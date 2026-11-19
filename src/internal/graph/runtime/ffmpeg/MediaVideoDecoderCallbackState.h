#pragma once

#include "internal/graph/runtime/resource/MediaPreparationControl.h"
#include "internal/graph/runtime/threading/MediaGraphWorkerExitToken.h"
#include <memory>
#include <variant>
extern "C" {
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg::graph {
using MediaDecoderCancellationSource = std::variant<MediaPreparationControl,
    std::shared_ptr<const MediaGraphWorkerExitToken>>;

// Codec-owner thread only, including phase handoff. Backend polling reads only
// this state; requestStop publishes through the shared execution token.
class MediaVideoDecoderCallbackState final {
public:
    MediaVideoDecoderCallbackState(AVPixelFormat format, MediaDecoderCancellationSource source);
    const AVPixelFormat pixelFormat;
    bool interrupted() const noexcept;
    ::media::Status bindExecution(std::shared_ptr<const MediaGraphWorkerExitToken> token);
    ::media::ErrorInfo error(int nativeCode, std::string operation) const;
    static int interruptCallback(void* opaque) noexcept;
private:
    enum class Reason { None, Cancelled, Deadline };
    Reason reason() const noexcept;
    const MediaDecoderCancellationSource m_source;
    std::shared_ptr<const MediaGraphWorkerExitToken> m_execution;
};
} // namespace media::ffmpeg::graph
