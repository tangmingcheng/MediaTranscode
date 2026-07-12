#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaRtpClockGroupNode final : public FFmpegNodeRuntime {
public:
    explicit MediaRtpClockGroupNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Result<bool> processPort(MediaGraphExecutionContext& context,
                                      const char* portName,
                                      MediaStreamKind streamKind,
                                      bool clockPort);
    ::media::Status processClock(const MediaRtpIngressEventBuffer& event,
                                 MediaStreamKind streamKind);
    ::media::Status publish(MediaGraphExecutionContext& context, std::int64_t observedAtNs);
    void resetState() noexcept;

    MediaRtpSourceClockMapperConfig m_videoConfig{};
    MediaRtpSourceClockMapperConfig m_audioConfig{};
    std::unique_ptr<MediaRtpSourceClockMapper> m_videoMapper;
    std::unique_ptr<MediaRtpSourceClockMapper> m_audioMapper;
    std::unique_ptr<MediaRtpClockGroupValidator> m_validator;
    std::optional<std::uint64_t> m_videoGeneration;
    std::optional<std::uint64_t> m_audioGeneration;
    std::optional<std::uint64_t> m_minimumVideoGeneration;
    std::optional<std::uint64_t> m_minimumAudioGeneration;
    bool m_configured = false;
    bool m_initialPublished = false;
};

} // namespace media::ffmpeg::graph
