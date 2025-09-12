#include <vart/tensor_buffer.hpp>
#include <xir/tensor/tensor.hpp>
#include <vector>
#include <cstring>

// tb comes from child_pred->tb (VvasInferPrediction*)
static inline void dump_tensorbuf(const TensorBuf* tb, GstInferencePrediction **ret_root_pred) {
    *ret_root_pred = nullptr;   // not implemented yet

    if (!tb) { g_printerr("TensorBuf is null\n"); return; }

    // Each entry in tb->ptr[] is a vart::TensorBuffer*
    for (int i = 0; i < tb->size; ++i) {
        auto* vtb = static_cast<vart::TensorBuffer*>(tb->ptr[i]);
        if (!vtb) { g_printerr("ptr[%d] is null\n", i); continue; }

        // Get a contiguous view of the whole tensor
        auto view = vtb->data({});                 // empty index => full tensor
        auto* base    = reinterpret_cast<uint8_t*>(view.first);
        size_t nbytes = view.second;
        g_printerr("nbytes=%d\n", (int) nbytes);

        // Always sync before CPU read
        vtb->sync_for_read(0, nbytes);

        // Copy to a temporary vector so we don't touch device memory directly
        std::vector<uint8_t> bytes(nbytes);
        std::memcpy(bytes.data(), base, nbytes);

        // Quick peek: first 64 raw bytes
        for (size_t b = 0; b < std::min<size_t>(64, nbytes); ++b)
            g_printerr("%02x ", bytes[b]);
        g_printerr("\n");

        const float* f = reinterpret_cast<const float*>(bytes.data());
        size_t count = nbytes / sizeof(float);
        g_printerr("fp32 head: ");
        for (size_t k = 0; k < std::min<size_t>(32, count); ++k)
            g_printerr("%g ", f[k]);
        g_printerr("\n");
    }
}
