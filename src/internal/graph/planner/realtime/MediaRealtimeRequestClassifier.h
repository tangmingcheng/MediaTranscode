#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

namespace media::ffmpeg::graph {

class MediaRealtimeRequestClassifier final {
public:
    static bool audioRequested(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool realtimeUrlInput(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool rawRtpInput(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool mpegTsUdpInput(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool unreliablePacketBoundary(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool separateRtpOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool muxedTransportOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool udpOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept;
    static bool rtpAvpOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept;

private:
    MediaRealtimeRequestClassifier() = delete;
};

} // namespace media::ffmpeg::graph
