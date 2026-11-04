#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "media_transcode/Result.h"
#include <ffnvcodec/dynlink_cuda.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sstream>

namespace media::ffmpeg::graph::cuda_canvas {

inline ::media::Status status(CUresult code, const char* operation)
{
    if (code == CUDA_SUCCESS) return ::media::Status::success();
    return ::media::Status::failure(::media::ErrorInfo::make(
        ::media::ErrorCode::HardwareUnavailable, operation, static_cast<int>(code)));
}

inline void reportCleanup(CUresult code, const char* operation) noexcept
{
    if (code == CUDA_SUCCESS) return;
    try {
        std::ostringstream out;
        out << "canvas_cuda_cleanup operation=" << operation << " native=" << static_cast<int>(code);
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::Summary,
            MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) {}
}

class Library final {
public:
    Library() : handle_(LoadLibraryExW(L"nvcuda.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {}
    ~Library() { if (handle_) FreeLibrary(handle_); }
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    template<class T> T* symbol(const char* name) const
    { return handle_ ? reinterpret_cast<T*>(GetProcAddress(handle_, name)) : nullptr; }
private:
    HMODULE handle_;
};

class ContextScope final {
public:
    ContextScope(tcuCtxPushCurrent_v2* push, tcuCtxPopCurrent_v2* pop)
        : push_(push), pop_(pop) {}
    ~ContextScope() { if (active_) reportCleanup(close(), "context_pop"); }
    ContextScope(const ContextScope&) = delete;
    ContextScope& operator=(const ContextScope&) = delete;
    CUresult open(CUcontext context) noexcept
    {
        const auto code = push_(context);
        if (code == CUDA_SUCCESS) { context_ = context; active_ = true; }
        return code;
    }
    CUresult close() noexcept
    {
        if (!active_) return CUDA_SUCCESS;
        CUcontext popped = nullptr;
        const auto code = pop_(&popped);
        if (code != CUDA_SUCCESS) return code;
        active_ = false;
        return popped == context_ ? CUDA_SUCCESS : CUDA_ERROR_INVALID_VALUE;
    }
private:
    tcuCtxPushCurrent_v2* push_;
    tcuCtxPopCurrent_v2* pop_;
    CUcontext context_ = nullptr;
    bool active_ = false;
};

// Also drains on exception unwinding before a caller can release source/target frames.
class CompletionScope final {
public:
    CompletionScope(tcuStreamSynchronize* synchronize, CUstream stream)
        : synchronize_(synchronize), stream_(stream) {}
    ~CompletionScope() { if (pending_) reportCleanup(synchronize_(stream_), "copy_unwind_completion"); }
    CompletionScope(const CompletionScope&) = delete;
    CompletionScope& operator=(const CompletionScope&) = delete;
    CUresult complete() noexcept
    {
        const auto code = synchronize_(stream_);
        pending_ = false;
        return code;
    }
private:
    tcuStreamSynchronize* synchronize_;
    CUstream stream_;
    bool pending_ = true;
};

// The adapter keeps its frames reference and driver library alive past this owner.
class OwnedStream final {
public:
    OwnedStream() = default;
    OwnedStream(const OwnedStream&) = delete;
    OwnedStream& operator=(const OwnedStream&) = delete;
    ~OwnedStream()
    {
        if (!stream_) return;
        ContextScope scope(push_, pop_);
        const auto opened = scope.open(context_);
        if (opened != CUDA_SUCCESS) { reportCleanup(opened, "stream_context_push"); return; }
        reportCleanup(sync_(stream_), "stream_completion");
        reportCleanup(destroy_(stream_), "stream_destroy");
    }
    ::media::Status initialize(const Library& library, CUcontext context)
    {
        using GetFlags = CUresult CUDAAPI(CUstream, unsigned int*);
        auto* create = library.symbol<tcuStreamCreate>("cuStreamCreate");
        auto* flags = library.symbol<GetFlags>("cuStreamGetFlags");
        push_ = library.symbol<tcuCtxPushCurrent_v2>("cuCtxPushCurrent_v2");
        pop_ = library.symbol<tcuCtxPopCurrent_v2>("cuCtxPopCurrent_v2");
        sync_ = library.symbol<tcuStreamSynchronize>("cuStreamSynchronize");
        destroy_ = library.symbol<tcuStreamDestroy_v2>("cuStreamDestroy_v2");
        if (!context || !create || !flags || !push_ || !pop_ || !sync_ || !destroy_)
            return ::media::Status::failure(::media::ErrorInfo::hardwareUnavailable(
                "CUDA canvas private stream exports or context unavailable"));
        context_ = context;
        ContextScope scope(push_, pop_);
        auto code = scope.open(context_);
        if (code != CUDA_SUCCESS) return status(code, "CUDA canvas stream context push");
        code = create(&stream_, CU_STREAM_NON_BLOCKING);
        if (code != CUDA_SUCCESS) return status(code, "CUDA canvas private stream create");
        unsigned int actual = 0;
        code = flags(stream_, &actual);
        if (code != CUDA_SUCCESS) return status(code, "CUDA canvas private stream flags");
        if (actual != CU_STREAM_NON_BLOCKING)
            return ::media::Status::failure(::media::ErrorInfo::hardwareUnavailable(
                "CUDA canvas private stream flags differ from nonblocking contract"));
        return status(scope.close(), "CUDA canvas stream context restore");
    }
    CUstream get() const noexcept { return stream_; }
private:
    CUcontext context_ = nullptr;
    CUstream stream_ = nullptr;
    tcuCtxPushCurrent_v2* push_ = nullptr;
    tcuCtxPopCurrent_v2* pop_ = nullptr;
    tcuStreamSynchronize* sync_ = nullptr;
    tcuStreamDestroy_v2* destroy_ = nullptr;
};

} // namespace media::ffmpeg::graph::cuda_canvas
