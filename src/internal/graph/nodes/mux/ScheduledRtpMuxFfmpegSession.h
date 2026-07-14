#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"
#include "internal/graph/protocol/rtp/MediaRtpTimestamp.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDatagramWriteAvio.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <vector>

struct AVFormatContext;
struct AVPacket;

namespace media::ffmpeg::graph {

class ScheduledRtpMuxFfmpegSession final {
public:
    explicit ScheduledRtpMuxFfmpegSession(FFmpegDatagramSink sink);
    ~ScheduledRtpMuxFfmpegSession();

    ScheduledRtpMuxFfmpegSession(const ScheduledRtpMuxFfmpegSession&) = delete;
    ScheduledRtpMuxFfmpegSession& operator=(const ScheduledRtpMuxFfmpegSession&) = delete;

    ::media::Status configure(ScheduledRtpMuxStreamConfig config);
    ::media::Status open();
    ::media::Status writeAccessUnit(const AVPacket& packet,
                                    MediaRtpTimestamp timestamp);
    ::media::Status writeTrailer();
    ::media::Status reset() noexcept;

private:
    enum class State {
        Empty,
        Configured,
        Open,
        TrailerWritten,
        Poisoned
    };

    ::media::Status emitMuxDatagram(std::span<const std::uint8_t> datagram);
    ::media::Status preserveCallbackFailure();
    void poison(::media::ErrorInfo error);
    void releaseOutput() noexcept;

    FFmpegDatagramSink m_sink;
    std::optional<ScheduledRtpMuxStreamConfig> m_config;
    AVFormatContext* m_context = nullptr;
    std::unique_ptr<FFmpegDatagramWriteAvio> m_avio;
    std::optional<MediaRtpTimestamp> m_activeTimestamp;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    std::vector<std::uint8_t> m_rewriteScratch;
    State m_state = State::Empty;
};

} // namespace media::ffmpeg::graph
