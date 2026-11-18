#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupSession.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {
class MediaPreparedRtpSourceSession;

class MediaRtpClockGroupNode final : public FFmpegNodeRuntime {
public:
    explicit MediaRtpClockGroupNode(MediaNodeId nodeId);
    ::media::Status bindPreparedSession(std::shared_ptr<MediaPreparedRtpSourceSession> source);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    enum class StreamProcessOutcome {
        NoInput,
        Regular,
        Invalidation
    };

    struct StreamPending final {
        MediaBufferRef event;
        MediaBufferRef clock;
    };

    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Result<StreamProcessOutcome> processStream(
        MediaGraphExecutionContext& context,
        MediaStreamKind streamKind);
    ::media::Status fillPending(MediaGraphExecutionContext& context,
                                const char* portName,
                                MediaBufferRef& pending);
    ::media::Result<std::optional<MediaStreamKind>> pendingInvalidation(
        MediaGraphExecutionContext& context);
    ::media::Status publish(MediaGraphExecutionContext& context, std::int64_t observedAtNs);
    void resetState() noexcept;

    // Keeps preparation reservations until the handed-off clock is destroyed.
    std::shared_ptr<MediaPreparedRtpSourceSession> m_preparedSource;
    std::optional<MediaRtpClockGroupSession> m_session;
    StreamPending m_videoPending;
    StreamPending m_audioPending;
    bool m_preferAudio = false;
    bool m_configured = false;
    bool m_initialPublished = false;
};

} // namespace media::ffmpeg::graph
