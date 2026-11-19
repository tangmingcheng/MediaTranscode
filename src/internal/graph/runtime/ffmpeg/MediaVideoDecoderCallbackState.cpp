#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCallbackState.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include <utility>
extern "C" {
#include <libavutil/error.h>
}

namespace media::ffmpeg::graph {
MediaVideoDecoderCallbackState::MediaVideoDecoderCallbackState(
    AVPixelFormat format, MediaDecoderCancellationSource source)
    : pixelFormat(format), m_source(std::move(source)) {}

MediaVideoDecoderCallbackState::Reason MediaVideoDecoderCallbackState::reason() const noexcept
{
    // Once handed to the decoder worker, only its coordinated stop publication
    // may interrupt decoding. RunControl's preparation token is signalled before
    // the executor publishes all worker stops and must not race that ordering.
    if (m_execution) return m_execution->stopRequested() ? Reason::Cancelled : Reason::None;
    if (const auto* prepared = std::get_if<MediaPreparationControl>(&m_source)) {
        if (prepared->cancellation.stop_requested()) return Reason::Cancelled;
        if (std::chrono::steady_clock::now() >= prepared->deadline) return Reason::Deadline;
    } else {
        const auto& token = std::get<std::shared_ptr<const MediaGraphWorkerExitToken>>(m_source);
        if (token && token->stopRequested()) return Reason::Cancelled;
    }
    return Reason::None;
}

bool MediaVideoDecoderCallbackState::interrupted() const noexcept
{ return reason() != Reason::None; }

int MediaVideoDecoderCallbackState::interruptCallback(void* opaque) noexcept
{ return static_cast<MediaVideoDecoderCallbackState*>(opaque)->interrupted() ? 1 : 0; }

::media::Status MediaVideoDecoderCallbackState::bindExecution(
    std::shared_ptr<const MediaGraphWorkerExitToken> token)
{
    if (!token) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "Decoder cancellation handoff requires its compiled worker token"));
    if (m_execution && m_execution != token) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("Decoder cancellation owner cannot change execution"));
    // Preparation completion and runtime-start admission own the old source.
    // Inside a running worker, even the first bind must obey its stop ordering.
    m_execution = std::move(token);
    if (interrupted()) return ::media::Status::failure(error(AVERROR_EXIT, "Decoder execution handoff"));
    return ::media::Status::success();
}

::media::ErrorInfo MediaVideoDecoderCallbackState::error(int nativeCode, std::string operation) const
{
    if (nativeCode == AVERROR_EXIT) {
        switch (reason()) {
        case Reason::Cancelled:
            return {::media::ErrorCode::Cancelled, nativeCode, std::move(operation) + " was cancelled"};
        case Reason::Deadline:
            return {::media::ErrorCode::IoFailure, nativeCode, std::move(operation) + " reached its preparation deadline"};
        case Reason::None: break;
        }
    }
    return FFmpegGraphError::fromCode(nativeCode, std::move(operation));
}
} // namespace media::ffmpeg::graph
