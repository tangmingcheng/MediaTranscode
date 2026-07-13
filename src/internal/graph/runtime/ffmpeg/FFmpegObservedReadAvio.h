#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/dict.h>
}

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <span>
#include <string>

namespace media::ffmpeg::graph {

class FFmpegObservedByteSink {
public:
    virtual ~FFmpegObservedByteSink() = default;
    virtual ::media::Status onBytes(std::uint64_t absoluteOffset,
                                    std::span<const std::uint8_t> bytes) = 0;
};

enum class FFmpegObservedAvioLifecycleEvent {
    FormatClosed,
    OuterClosed,
    InnerClosed
};

class FFmpegObservedAvioLifecycleSink {
public:
    virtual ~FFmpegObservedAvioLifecycleSink() = default;
    virtual void onLifecycleEvent(FFmpegObservedAvioLifecycleEvent event) noexcept = 0;
};

class FFmpegAvioInterruptState final {
public:
    void cancel() noexcept { m_cancelled.store(true, std::memory_order_release); }
    bool cancelled() const noexcept { return m_cancelled.load(std::memory_order_acquire); }

private:
    std::atomic_bool m_cancelled{false};
};

class FFmpegProtocolAvioOpener {
public:
    virtual ~FFmpegProtocolAvioOpener() = default;
    virtual ::media::Result<AVIOContext*> open(
        const std::string& protocolUrl,
        AVDictionary** protocolOptions,
        const AVIOInterruptCB* interrupt) = 0;
    virtual void close(AVIOContext** context) noexcept = 0;
    virtual void requestInterrupt(AVIOContext*) noexcept {}
};

class FFmpegObservedReadAvio final {
public:
    static ::media::Result<std::unique_ptr<FFmpegObservedReadAvio>> open(
        const std::string& protocolUrl,
        AVDictionary* protocolOptions,
        std::size_t bufferBytes,
        FFmpegObservedByteSink& observer,
        FFmpegAvioInterruptState& interruptState,
        FFmpegObservedAvioLifecycleSink* lifecycleSink = nullptr);
    static ::media::Result<std::unique_ptr<FFmpegObservedReadAvio>> open(
        const std::string& protocolUrl,
        AVDictionary* protocolOptions,
        std::size_t bufferBytes,
        FFmpegObservedByteSink& observer,
        FFmpegAvioInterruptState& interruptState,
        FFmpegProtocolAvioOpener& opener,
        FFmpegObservedAvioLifecycleSink* lifecycleSink = nullptr);

    ~FFmpegObservedReadAvio();
    FFmpegObservedReadAvio(const FFmpegObservedReadAvio&) = delete;
    FFmpegObservedReadAvio& operator=(const FFmpegObservedReadAvio&) = delete;

    AVIOContext* outer() noexcept { return m_outer; }
    std::optional<::media::ErrorInfo> observerFailure() const;
    ::media::Status status() const;
    void close() noexcept;

private:
    FFmpegObservedReadAvio(FFmpegObservedByteSink& observer,
                           FFmpegAvioInterruptState& interruptState,
                           FFmpegProtocolAvioOpener& opener,
                           FFmpegObservedAvioLifecycleSink* lifecycleSink) noexcept;
    static int read(void* opaque, std::uint8_t* destination, int requested);
    static int interrupt(void* opaque) noexcept;

    FFmpegObservedByteSink& m_observer;
    FFmpegAvioInterruptState& m_interruptState;
    FFmpegProtocolAvioOpener& m_opener;
    FFmpegObservedAvioLifecycleSink* m_lifecycleSink = nullptr;
    AVIOContext* m_inner = nullptr;
    AVIOContext* m_outer = nullptr;
    std::uint64_t m_offset = 0;
    mutable std::mutex m_stateMutex;
    std::condition_variable m_callbacksDone;
    std::mutex m_readMutex;
    std::size_t m_activeCallbacks = 0;
    bool m_closing = false;
    bool m_closed = false;
    std::optional<::media::ErrorInfo> m_observerFailure;
};

} // namespace media::ffmpeg::graph
