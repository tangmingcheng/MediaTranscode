#pragma once
#include "internal/graph/runtime/ffmpeg/platform/MediaCudaCanvasResources.h"
#include "internal/graph/diagnostics/MediaCanvasBatchTiming.h"
#include <cmath>

namespace media::ffmpeg::graph::cuda_canvas {
// Two driver-owned events per adapter. No per-frame allocation or ownership of media.
class BatchTiming final {
public:
    BatchTiming() = default;
    BatchTiming(const BatchTiming&) = delete;
    BatchTiming& operator=(const BatchTiming&) = delete;
    ~BatchTiming() {
        if (!start_ && !end_) return;
        ContextScope scope(push_, pop_);
        const auto opened = scope.open(context_);
        if (opened != CUDA_SUCCESS) { reportCleanup(opened, "timing_destroy_context_push"); return; }
        if (end_) reportCleanup(destroy_(end_), "timing_end_destroy");
        if (start_) reportCleanup(destroy_(start_), "timing_start_destroy");
    }
    ::media::Status initialize(const Library& library, CUcontext context) {
        auto* create = library.symbol<tcuEventCreate>("cuEventCreate");
        record_ = library.symbol<tcuEventRecord>("cuEventRecord");
        elapsed_ = library.symbol<Elapsed>("cuEventElapsedTime");
        destroy_ = library.symbol<tcuEventDestroy_v2>("cuEventDestroy_v2");
        push_ = library.symbol<tcuCtxPushCurrent_v2>("cuCtxPushCurrent_v2");
        pop_ = library.symbol<tcuCtxPopCurrent_v2>("cuCtxPopCurrent_v2");
        context_ = context;
        preparation_ = "exports_unavailable";
        if (!context || !create || !record_ || !elapsed_ || !destroy_ || !push_ || !pop_)
            return ::media::Status::success();
        ContextScope scope(push_, pop_);
        preparation_ = "context_push";
        auto code = scope.open(context_);
        preparationResult_ = static_cast<int>(code);
        // An actual context operation failure is not an absent timing capability.
        // Do not continue preparation with an unverified CUDA execution context.
        if (code != CUDA_SUCCESS) return status(code, "CUDA canvas timing context push");
        preparation_ = "start_create";
        code = create(&start_, CU_EVENT_DEFAULT);
        preparationResult_ = static_cast<int>(code);
        if (code == CUDA_SUCCESS) {
            preparation_ = "end_create";
            code = create(&end_, CU_EVENT_DEFAULT);
            preparationResult_ = static_cast<int>(code);
        }
        // Close explicitly even after a partial event allocation. The scope's
        // destructor is only a cleanup retry, never evidence that restore passed.
        const auto restored = scope.close();
        if (restored != CUDA_SUCCESS) {
            if (code != CUDA_SUCCESS) reportCleanup(code, preparation_);
            return status(restored, "CUDA canvas timing context restore");
        }
        if (code != CUDA_SUCCESS) return ::media::Status::success();
        preparation_ = "ready";
        ready_ = true;
        return ::media::Status::success();
    }
    MediaCanvasBatchTiming begin(CUstream stream) noexcept {
        MediaCanvasBatchTiming result;
        result.process = GetCurrentProcessId(); result.thread = GetCurrentThreadId();
        result.context = reinterpret_cast<std::uintptr_t>(context_);
        result.stream = reinterpret_cast<std::uintptr_t>(stream);
        result.preparation = preparation_; result.preparationResult = preparationResult_;
        if (ready_) result.startResult = static_cast<int>(record_(start_, stream));
        return result;
    }
    void end(CUstream stream, MediaCanvasBatchTiming& sample) noexcept {
        if (sample.startResult && *sample.startResult == CUDA_SUCCESS)
            sample.endResult = static_cast<int>(record_(end_, stream));
    }
    void read(MediaCanvasBatchTiming& sample) noexcept {
        if (!sample.completionResult || *sample.completionResult != CUDA_SUCCESS ||
            !sample.startResult || *sample.startResult != CUDA_SUCCESS ||
            !sample.endResult || *sample.endResult != CUDA_SUCCESS) return;
        float elapsed = 0;
        const auto code = elapsed_(&elapsed, start_, end_);
        sample.elapsedResult = static_cast<int>(code);
        if (code == CUDA_SUCCESS && std::isfinite(elapsed) && elapsed >= 0) sample.gpuMilliseconds = elapsed;
    }
private:
    using Elapsed = CUresult CUDAAPI(float*, CUevent, CUevent);
    CUcontext context_ = nullptr;
    CUevent start_ = nullptr, end_ = nullptr;
    tcuCtxPushCurrent_v2* push_ = nullptr;
    tcuCtxPopCurrent_v2* pop_ = nullptr;
    tcuEventRecord* record_ = nullptr;
    tcuEventDestroy_v2* destroy_ = nullptr;
    Elapsed* elapsed_ = nullptr;
    const char* preparation_ = "not_prepared";
    std::optional<int> preparationResult_;
    bool ready_ = false;
};
}
