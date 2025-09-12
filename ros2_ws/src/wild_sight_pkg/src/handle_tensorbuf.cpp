#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <utility>
#include <cstring>
#include <string>

#include <xir/tensor/tensor.hpp>
#include <vart/runner.hpp>
#include <vart/tensor_buffer.hpp>


// If you have VVAS/GST inference helpers, include them here
// #include "gstvvas_inferencemeta.h"   // adjust to your tree

// ----------------- TUNE THESE TO YOUR MODEL -----------------
static constexpr int   kNumClasses  = 3;     // from your training
static constexpr int   kNumAnchors  = 3;     // YOLOv5 default per head
static constexpr float kConfThresh  = 0.65f; // pre-NMS threshold 0.45
static constexpr float kNMSThresh   = 0.45f; // IoU for NMS
static constexpr float kStrides[4]  = {8.f, 16.f, 32.f, 64.f}; // 80/40/20/10 at 640
static constexpr int   kMinWH       = 2;      // drop tiny boxes early

// Anchors in **pixels** per head (replace with your Detect.anchors * stride values if needed)
static constexpr float kAnchors[4][kNumAnchors][2] = {
  {{10,13}, {16,30}, {33,23}},      // stride 8
  {{30,61}, {62,45}, {59,119}},     // stride 16
  {{116,90}, {156,198}, {373,326}}, // stride 32
  {{0,0}, {0,0}, {0,0}}             // stride 64 (if you really use 4 heads; otherwise remove)
};
// Decide at compile time how many heads you actually output
static constexpr int kNumHeads = 3; // set to 4 if you really have 10x10 head
// ------------------------------------------------------------

struct Det {
  float x1, y1, x2, y2; // corners in pixels
  float score;          // objectness * best_class_score
  int   cls;            // class id
};

// Sigmoid
static inline float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

// IoU for NMS
static float iou(const Det& a, const Det& b) {
  float ix1 = std::max(a.x1, b.x1);
  float iy1 = std::max(a.y1, b.y1);
  float ix2 = std::min(a.x2, b.x2);
  float iy2 = std::min(a.y2, b.y2);
  float iw  = std::max(0.f, ix2 - ix1);
  float ih  = std::max(0.f, iy2 - iy1);
  float inter = iw * ih;
  float ua = (a.x2 - a.x1) * (a.y2 - a.y1);
  float ub = (b.x2 - b.x1) * (b.y2 - b.y1);
  float uni = ua + ub - inter + 1e-6f;
  return inter / uni;
}

// NMS on a vector (keeps highest scores)
static std::vector<Det> nms(std::vector<Det> dets, float thr) {
  std::vector<Det> out;
  std::sort(dets.begin(), dets.end(), [](auto& A, auto& B){ return A.score > B.score; });
  std::vector<char> removed(dets.size(), 0);
  for (size_t i = 0; i < dets.size(); ++i) {
    if (removed[i]) continue;
    out.push_back(dets[i]);
    for (size_t j = i + 1; j < dets.size(); ++j) {
      if (!removed[j] && iou(dets[i], dets[j]) > thr) removed[j] = 1;
    }
  }
  return out;
}

static void decode_head(const float* f, int H, int W, int C, int head_idx,
                        std::vector<Det>& out) {
  // derive na/no from C and known nc
  if (C % (5 + kNumClasses) != 0) return;
  const int no = 5 + kNumClasses;
  const int na = C / no;

  // guard: anchors/strides present for this head?
  if (head_idx >= kNumHeads) return;
  const float stride = kStrides[head_idx];

  // quick layout probe: look at two adjacent floats along X
  // If channels-last (NHWC), advancing X jumps by C floats.
  // If channels-first (NCHW), advancing X jumps by 1 float per channel plane.
  // We’ll decide by comparing memory addresses for (y=0,x=0) and (y=0,x=1).
  const float* base = f;
  const float* step_x = f + C;       // NHWC hypothesis
  const float* step_c = f + 1;       // NCHW hypothesis (within channel plane)

  // Heuristic: in NCHW, the (x=0,y=0) anchor vector (no floats) is contiguous,
  // so reading p[0..no-1] from base, then from base+no should still be the same cell/anchor in NHWC but NOT in NCHW.
  // Instead of being too clever, just implement BOTH indexers and try the one that produces saner boxes.

  auto decode_NHWC = [&](void){
    for (int y=0; y<H; ++y){
      for (int x=0; x<W; ++x){
        const float* cell = f + ((y*W + x) * C);
        for (int a=0; a<na; ++a){
          const float* p = cell + a*no;
          float obj = sigmoid(p[4]);
          // best class
          int best_c=0; float best_ps=-1e9f;
          for (int c=0;c<kNumClasses;++c){ float s=sigmoid(p[5+c]); if (s>best_ps){best_ps=s; best_c=c;} }
          float conf = obj * best_ps;
          if (conf < kConfThresh) continue;

          float tx=p[0], ty=p[1], tw=p[2], th=p[3];
          float cx = (sigmoid(tx)*2.f - 0.5f + x) * stride;
          float cy = (sigmoid(ty)*2.f - 0.5f + y) * stride;
          float aw = kAnchors[head_idx][a][0], ah = kAnchors[head_idx][a][1];
          float w  = std::pow(sigmoid(tw)*2.f, 2.f) * aw;
          float h  = std::pow(sigmoid(th)*2.f, 2.f) * ah;

          if (w < kMinWH || h < kMinWH) continue;
          out.push_back({cx - 0.5f*w, cy - 0.5f*h, cx + 0.5f*w, cy + 0.5f*h, conf, best_c});
        }
      }
    }
  };

  auto decode_NCHW = [&](void){
    // NCHW layout is [N, C, H, W]; channels are grouped: [a0(no), a1(no), a2(no), ...]
    // For a cell (y,x), the start index of channel c is: ((c * H + y) * W + x)
    for (int y=0; y<H; ++y){
      for (int x=0; x<W; ++x){
        for (int a=0; a<na; ++a){
          int c0 = a * no;
          float tx = f[ ((c0+0) * H + y) * W + x ];
          float ty = f[ ((c0+1) * H + y) * W + x ];
          float tw = f[ ((c0+2) * H + y) * W + x ];
          float th = f[ ((c0+3) * H + y) * W + x ];
          float to = f[ ((c0+4) * H + y) * W + x ];

          // classes
          int best_c=0; float best_ps=-1e9f;
          for (int c=0;c<kNumClasses;++c){
            float logit = f[ ((c0+5+c)*H + y) * W + x ];
            float s = sigmoid(logit);
            if (s > best_ps){best_ps=s; best_c=c;}
          }
          float conf = sigmoid(to) * best_ps;
          if (conf < kConfThresh) continue;

          float cx = (sigmoid(tx)*2.f - 0.5f + x) * kStrides[head_idx];
          float cy = (sigmoid(ty)*2.f - 0.5f + y) * kStrides[head_idx];
          float aw = kAnchors[head_idx][a][0], ah = kAnchors[head_idx][a][1];
          float w  = std::pow(sigmoid(tw)*2.f, 2.f) * aw;
          float h  = std::pow(sigmoid(th)*2.f, 2.f) * ah;
          if (w < kMinWH || h < kMinWH) continue;
          out.push_back({cx - 0.5f*w, cy - 0.5f*h, cx + 0.5f*w, cy + 0.5f*h, conf, best_c});
        }
      }
    }
  };

  // Try NHWC first (VART often returns NHWC for TF-like graphs; PyTorch paths can be NCHW).
  //size_t before = out.size();
  //decode_NHWC();
  // If nothing reasonable came out, retry NCHW.
  //if (out.size() == before) decode_NCHW();
  //decode_NCHW();
  decode_NHWC();
}

// ---- MAIN ENTRY: replace your probe printer with this ----
static inline void handle_tensorbuf(const TensorBuf* tb,
                                    GstInferencePrediction **ret_root_pred,
                                    int frame_w, int frame_h) {
  // Collect raw detections from all heads
  std::vector<Det> dets;
  dets.reserve(4096);

  if (!tb) { g_printerr("TensorBuf is null\n"); *ret_root_pred = nullptr; return; }

  // Expect 4 heads in order: 80x80x24, 40x40x24, 20x20x24, 10x10x24
  const int num_heads = kNumHeads; //Ctb->size;
  for (int i = 0; i < num_heads; ++i) {
    auto* vtb = static_cast<vart::TensorBuffer*>(tb->ptr[i]);
    if (!vtb) { g_printerr("ptr[%d] is null\n", i); continue; }

#if 0
    // Read shape (assume NHWC)
    auto* tensor = vtb->get_tensor();
    auto shape = tensor->get_shape();
    // Supported shapes: [1,H,W,C] or [H,W,C]
    int H=0,W=0,C=0;
    if (shape.size() == 4) { H = shape[1]; W = shape[2]; C = shape[3]; }
    else if (shape.size() == 3) { H = shape[0]; W = shape[1]; C = shape[2]; }
    else { g_printerr("Unexpected rank (%zu) for head %d\n", shape.size(), i); continue; }
#else
    int H=80/(1<<i);
    int W=H;
    int C=24;
#endif

    // Map memory and sync
    auto view = vtb->data({});
    auto* base = reinterpret_cast<uint8_t*>(view.first);
    size_t nbytes = view.second;
    vtb->sync_for_read(0, nbytes);

    // Cast to FP32
    const float* f = reinterpret_cast<const float*>(base);
    size_t count = nbytes / sizeof(float);
    if (count != static_cast<size_t>(H*W*C)) {
      g_printerr("Size mismatch head %d: H=%d W=%d C=%d -> %zu floats, but buffer has %zu\n",
                 i, H, W, C, (size_t)H*W*C, count);
      continue;
    }

    // Decode this head
    decode_head(f, H, W, C, i, dets);
  }

  // Clip to frame
  for (auto& d : dets) {
    d.x1 = std::max(0.f, std::min(d.x1, (float)frame_w-1));
    d.y1 = std::max(0.f, std::min(d.y1, (float)frame_h-1));
    d.x2 = std::max(0.f, std::min(d.x2, (float)frame_w-1));
    d.y2 = std::max(0.f, std::min(d.y2, (float)frame_h-1));
  }

  // NMS
  auto kept = nms(std::move(dets), kNMSThresh);

  // --------- Build GstInferencePrediction tree (minimal) ----------
  // NOTE: Adjust the API names to your project headers if they differ.
  // Root node spans the full frame
  GstInferencePrediction* groot = gst_inference_prediction_new();   // or your project’s creator
  VvasInferPrediction *root = & groot->prediction;

  root->bbox.x = 0; root->bbox.y = 0;
  root->bbox.width  = frame_w;
  root->bbox.height = frame_h;
  //root->enabled = TRUE;

  for (const auto& d : kept) {
    GstInferencePrediction* gchild = gst_inference_prediction_new();
    VvasInferPrediction *child = & gchild->prediction;

    child->bbox.x = (gint)std::round(d.x1);
    child->bbox.y = (gint)std::round(d.y1);
    child->bbox.width  = (gint)std::round(d.x2 - d.x1);
    child->bbox.height = (gint)std::round(d.y2 - d.y1);

    GstInferenceClassification *gcls = gst_inference_classification_new();
    VvasInferClassification *cls = &gcls->classification;

    cls->class_id = d.cls;
    cls->class_prob = d.score;

    gst_inference_prediction_append_classification(gchild, gcls);
    //gst_inference_classification_unref(gcls);

    g_printerr("x=%d, y=%d, w=%d, h=%d, cls=%d, scr=%f\n", child->bbox.x, child->bbox.y, child->bbox.width, child->bbox.height, d.cls, d.score);

    // append to root
    gst_inference_prediction_append(groot, gchild);
    //gst_inference_prediction_unref(gchild);
  }

  *ret_root_pred = groot;
}
