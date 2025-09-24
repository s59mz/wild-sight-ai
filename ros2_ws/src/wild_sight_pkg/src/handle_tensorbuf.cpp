/*
# Wild-Sight-AI
# Smart Following Camera with Animal Detection
#   for Kria KR260 Board
#
# Created by: Matjaz Zibert S59MZ - September 2025
#
# YOLOv5 P6 Decoder
#   - Input: 4 lanes of raw tensors from DPU
#   - Outout: pointer to populated GstInferencePrediction datastructure
#
#   - This function decodes raw tensors and calculates
#     boundary boxes of detected 3 classes: Animal, Person, Vehicle
#     Then populates child nodes of the VvasInferPrediction data structures
#
# Hackster.io Project link:
#     https://www.hackster.io/matjaz4/wildsight-ai-real-time-human-wildlife-conflict-detection-ff65fa
*/


/* -----------------------------------------------------------
 * Minimal helpers (keep or remove if you already have them)
 * -----------------------------------------------------------*/
struct Det {
  float x1, y1, x2, y2;
  int   cls;
  float score;
};

static inline float iou(const Det& a, const Det& b) {
  float xx1 = std::max(a.x1, b.x1);
  float yy1 = std::max(a.y1, b.y1);
  float xx2 = std::min(a.x2, b.x2);
  float yy2 = std::min(a.y2, b.y2);
  float w = std::max(0.f, xx2 - xx1);
  float h = std::max(0.f, yy2 - yy1);
  float inter = w * h;
  float u = (a.x2-a.x1)*(a.y2-a.y1) + (b.x2-b.x1)*(b.y2-b.y1) - inter;
  return u <= 0 ? 0.f : inter/u;
}

static inline std::vector<Det> nms(std::vector<Det>&& in, float iou_th) {
  std::vector<Det> out;
  std::sort(in.begin(), in.end(), [](auto& A, auto& B){ return A.score > B.score; });
  std::vector<char> sup(in.size(), 0);
  for (size_t i=0;i<in.size();++i) {
    if (sup[i]) continue;
    out.push_back(in[i]);
    for (size_t j=i+1;j<in.size();++j)
      if (!sup[j] && iou(in[i], in[j]) > iou_th) sup[j]=1;
  }
  return out;
}

static inline float sigmoid(float x) {
  return 1.f / (1.f + std::exp(-x));
}

/* -----------------------------------------------------------
 * Anchors (PIXELS) & strides per head (order MUST match heads)
 * P3 80x80 s=8,  P4 40x40 s=16,  P5 20x20 s=32,  P6 10x10 s=64
 * Converted from your printed Detect.anchors (in grid units).
 * -----------------------------------------------------------*/
static constexpr int   kNumHeads    = 4;
static constexpr int   kNumAnchors  = 3;
static constexpr int   kNumClasses  = 3;
static constexpr float kConfThresh  = 0.85;    // tweak as needed
static constexpr float kNMSThresh   = 0.85f;    // tweak as needed

static const float kStrides[kNumHeads] = { 8.f, 16.f, 32.f, 64.f };

#if 0
tensor([[[ 2.37500,  3.37500],
         [ 5.50000,  5.00000],
         [ 4.75000, 11.75000]],

        [[ 6.00000,  4.25000],
         [ 5.37500,  9.50000],
         [11.25000,  8.56250]],

        [[ 4.37500,  9.40625],
         [ 9.46875,  8.25000],
         [ 7.43750, 16.93750]],

        [[ 6.81250,  9.60938],
         [11.54688,  5.93750],
         [14.45312, 12.37500]]])
#endif
// [head][anchor][2] = (aw, ah) in PIXELS
static const float kAnchors[kNumHeads][kNumAnchors][2] = {
  // P3 / 56x32 / s=8
  { { 19.0f, 27.0f }, { 44.0f, 40.0f }, { 38.0f, 94.0f } },
  // P4 / 28x16 / s=16
  { { 96.0f, 68.0f }, { 86.0f,152.0f }, {180.0f,137.0f } },
  // P5 / 14x8 / s=32
  { {140.0f,301.0f }, {303.0f,264.0f }, {238.0f,542.0f } },
  // P6 / 7x4 / s=64
  { {436.0f,615.0f }, {739.0f,380.0f }, {925.0f,792.0f } }
};

// Expected spatial sizes per head for 576x320 (matching above order)
// static const int kHeadH[kNumHeads] = {40, 20, 10,  5};
// static const int kHeadW[kNumHeads] = {72, 36, 18,  9};

// Expected spatial sizes per head for 448x256 (matching above order)
static const int kHeadH[kNumHeads] = {32, 16,  8,  4};
static const int kHeadW[kNumHeads] = {56, 28, 14,  7};

// Expected spatial sizes per head for 320x256 (matching above order)
// static const int kHeadH[kNumHeads] = {32, 16,  8,  4};
// static const int kHeadW[kNumHeads] = {40, 20, 10,  5};

/* -----------------------------------------------------------
 * Decode a single NHWC head (H x W x 24 floats)
 * layout per cell (anchor-major): 8 floats
 *   0:tx,1:ty,2:tw,3:th,4:to, 5..(4+kNumClasses-1): class logits
 * -----------------------------------------------------------*/
static inline void decode_head_nhwc(const float* f,
                                    int head_idx, int H, int W,
                                    int frame_w, int frame_h,
                                    std::vector<Det>& dets)
{
  //  g_printerr("idx=%d, H=%d, W=%d, fw=%d, fh=%d\n", head_idx, H, W, frame_w, frame_h);
  const float stride = kStrides[head_idx];
  const int   C      = kNumAnchors * (5 + kNumClasses); // 3*(5+3)=24
  const int   step_a = (5 + kNumClasses);               // 8
  const int   step_c = 1;                               // NHWC contiguous per channel
  const int   step_w = C;                               // channels per x
  const int   step_h = W * C;                           // channels per row

  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const int base = y * step_h + x * step_w;
      for (int a = 0; a < kNumAnchors; ++a) {
        const int off = base + a * step_a * step_c; // anchor offset
        float cc[3];

        const float tx = f[off + 0];
        const float ty = f[off + 1];
        const float tw = f[off + 2];
        const float th = f[off + 3];

        const float to = f[off + 4];

        cc[0] = f[off + 5];
        cc[1] = f[off + 6];
        cc[2] = f[off + 7];



        // class logits (kNumClasses)
        float best_logit = cc[0];
        int   best_id    = 0;
        for (int c = 1; c < kNumClasses; ++c) {
          float v = cc[c];
          if (v > best_logit) { best_logit = v; best_id = c; }
        }

        const float obj  = sigmoid(to);
        const float prob = obj * sigmoid(best_logit);
        if (prob < kConfThresh) continue;

        // YOLOv5 decode
        const float cx = (sigmoid(tx) * 2.f - 0.5f + float(x)) * stride;
        const float cy = (sigmoid(ty) * 2.f - 0.5f + float(y)) * stride;
        const float aw = kAnchors[head_idx][a][0];
        const float ah = kAnchors[head_idx][a][1];
        const float bw = std::pow(sigmoid(tw) * 2.f, 2.f) * aw;
        const float bh = std::pow(sigmoid(th) * 2.f, 2.f) * ah;

        Det d;
        d.x1 = cx - bw * 0.5f;
        d.y1 = cy - bh * 0.5f;
        d.x2 = cx + bw * 0.5f;
        d.y2 = cy + bh * 0.5f;
        d.cls = best_id;
        d.score = prob;

        // Optional: quick reject if completely outside frame
        if (d.x2 <= 0 || d.y2 <= 0 || d.x1 >= frame_w || d.y1 >= frame_h) continue;

        dets.emplace_back(std::move(d));
      }
    }
  }
}

/* -----------------------------------------------------------
 * Main entry: read 4 heads (NHWC), decode, NMS, attach to meta
 * -----------------------------------------------------------*/
static inline void handle_tensorbuf(const TensorBuf* tb,
                                    GstInferencePrediction **ret_root_pred,
                                    int frame_w, int frame_h)
{
  *ret_root_pred = nullptr;

  std::vector<Det> dets;
  dets.reserve(4096);

  if (!tb) { g_printerr("TensorBuf is null\n"); return; }

  int num_heads = std::min(tb->size, kNumHeads);
  num_heads = 2;   // only first two lanes have sense for this resolution

  for (int i = 0; i < num_heads; ++i) {
    auto* vtb = static_cast<vart::TensorBuffer*>(tb->ptr[i]);
    if (!vtb) { g_printerr("ptr[%d] is null\n", i); continue; }

    const int H = kHeadH[i];
    const int W = kHeadW[i];
    const int C = kNumAnchors * (5 + kNumClasses); // 24

    auto view = vtb->data({});
    auto* base = reinterpret_cast<uint8_t*>(view.first);
    size_t nbytes = view.second;
    vtb->sync_for_read(0, nbytes);

    const size_t expected = (size_t)H * W * C * sizeof(float);
    if (nbytes != expected) {
      g_printerr("Head %d size mismatch: H=%d W=%d C=%d -> %zu bytes, buffer=%zu bytes\n",
                 i, H, W, C, expected, nbytes);
      continue;
    }

    const float* f = reinterpret_cast<const float*>(base);

#if 0
    // debug print raw tensor
    g_printerr("Lane=%d, nbytes=%d\n", i, (int) nbytes);

    for (int j=0; j<128; j++) {
        g_printerr("%08X ", (int) base[j]);
    }
    g_printerr("\n");

    if (i==3){
      g_printerr("Head %d: H=%d W=%d C=%d -> %zu bytes, buffer=%zu bytes\n",
                 i, H, W, C, expected, nbytes);
      for (int j=0; j<nbytes/4; j++) {
        g_printerr("%.2f ", f[j]);
      }
      g_printerr("\n");
    }
#endif

    decode_head_nhwc(f, i, H, W, frame_w, frame_h, dets);
  }

  // Clip to frame
  for (auto& d : dets) {
    d.x1 = std::max(0.f, std::min(d.x1, (float)frame_w - 1));
    d.y1 = std::max(0.f, std::min(d.y1, (float)frame_h - 1));
    d.x2 = std::max(0.f, std::min(d.x2, (float)frame_w - 1));
    d.y2 = std::max(0.f, std::min(d.y2, (float)frame_h - 1));
  }

  // NMS
  auto kept = nms(std::move(dets), kNMSThresh);

  // ------- Build GstInferencePrediction tree -------
  GstInferencePrediction* groot = gst_inference_prediction_new();
  VvasInferPrediction *root = &groot->prediction;
  root->bbox.x = 0; root->bbox.y = 0;
  root->bbox.width = frame_w; root->bbox.height = frame_h;

  for (const auto& d : kept) {
    if (d.cls != 0) continue;  // show animals only for debugg

    GstInferencePrediction* gchild = gst_inference_prediction_new();
    VvasInferPrediction *child = &gchild->prediction;

    child->bbox.x = (gint)std::round(d.x1);
    child->bbox.y = (gint)std::round(d.y1);
    child->bbox.width  = (gint)std::round(d.x2 - d.x1);
    child->bbox.height = (gint)std::round(d.y2 - d.y1);

    // g_printerr("x=%d, y=%d, w=%d, h=%d, cls=%d, scr=%f\n", child->bbox.x, child->bbox.y, child->bbox.width, child->bbox.height, d.cls, d.score);

    GstInferenceClassification *gcls = gst_inference_classification_new();
    VvasInferClassification *cls = &gcls->classification;
    cls->class_id = d.cls;
    cls->class_prob = d.score;

    gst_inference_prediction_append_classification(gchild, gcls);
    gst_inference_prediction_append(groot, gchild);
  }

  *ret_root_pred = groot;
}
