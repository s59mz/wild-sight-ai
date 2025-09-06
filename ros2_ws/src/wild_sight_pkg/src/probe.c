#include <gst/gst.h>
#include <mutex>
#include <vector>
#include <cstring>

// VVAS inference headers — names can vary slightly by release
#include "gstvvasinference.h"      // meta API
#include "vvas_infer_prediction.h" // prediction tree

GstElement* xinfer = /* your vvas_xinfer element */;
GstPad* srcpad = gst_element_get_static_pad(xinfer, "src");
gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BUFFER, yolo_rawtensor_probe, nullptr, nullptr);
gst_object_unref(srcpad);

static GstPadProbeReturn infer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
  if (!(info->type & GST_PAD_PROBE_TYPE_BUFFER)) return GST_PAD_PROBE_OK;
  GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
  if (!buf) return GST_PAD_PROBE_OK;

  // 1) Get VVAS inference meta from buffer
  GstMeta *meta = gst_buffer_get_meta(buf, GST_VVAS_INFERENCE_META_API_TYPE);
  if (!meta) return GST_PAD_PROBE_OK;

  auto *infer_meta = reinterpret_cast<GstVvasInferenceMeta *>(meta);
  GstInferencePrediction *root = &infer_meta->prediction; // root of the tree

  // 2) Find a child node that carries the raw tensor (label often "TensorBuf")
  GstInferencePrediction *tensor_node = nullptr;

  // Traverse (you can also use vvas_infer_prediction_foreach if available)
  for (GList *l = root->prediction.children; l != nullptr; l = l->next) {
    auto *child = reinterpret_cast<GstInferencePrediction *>(l->data);
    if (!child) continue;

    // Heuristics:
    // - raw tensor nodes typically have Class == -1 and Label == "TensorBuf"
    // - more robust: check child->sub_buffer != nullptr
    if (child->sub_buffer != nullptr) {
      tensor_node = child;
      break;
    }
  }

  if (!tensor_node || !tensor_node->sub_buffer) {
    // No raw tensor attached on this buffer
    return GST_PAD_PROBE_OK;
  }

  // 3) Map the raw tensor bytes
  GstMapInfo map;
  if (!gst_buffer_map(tensor_node->sub_buffer, &map, GST_MAP_READ)) {
    g_printerr("Failed to map raw tensor buffer\n");
    return GST_PAD_PROBE_OK;
  }

  // 4) Interpret the tensor
  // Your runner reports input NHWC and output shape [1, 25500, 8].
  // Dtype is usually INT8 (xint8) after quantization; sometimes FLOAT.
  const size_t B = 1, A = 25500, NO = 8;
  const size_t expected_bytes_int8 = B * A * NO * sizeof(int8_t);

  if (map.size == expected_bytes_int8) {
    const int8_t *data = reinterpret_cast<const int8_t *>(map.data);
    // Example: convert to float with scale (if you need dequantization)
    // float scale = ...; // from quant_info if needed; often handled by postproc thresholds
    // std::vector<float> tensor(B*A*NO);
    // for (size_t i=0;i<B*A*NO;i++) tensor[i] = data[i] * scale;

    // 5) Call your CPU postproc: sigmoid + decode + NMS
    // yolo5_postprocess_int8(data, A, NO, anchors, strides, boxes_out);

  } else if (map.size == B * A * NO * sizeof(float)) {
    const float *data = reinterpret_cast<const float *>(map.data);
    // yolo5_postprocess_float(data, A, NO, anchors, strides, boxes_out);
  } else {
    g_printerr("Unexpected tensor size: %zu bytes (expected ~%zu)\n",
               map.size, expected_bytes_int8);
  }

  gst_buffer_unmap(tensor_node->sub_buffer, &map);

  // 6) (Optional) attach your decoded boxes back as VVAS predictions
  //     so your renderer (airender) can draw them.

  return GST_PAD_PROBE_OK;
}

