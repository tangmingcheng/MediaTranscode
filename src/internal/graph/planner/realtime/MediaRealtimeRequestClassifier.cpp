#include "internal/graph/planner/realtime/MediaRealtimeRequestClassifier.h"

namespace media::ffmpeg::graph {

bool MediaRealtimeRequestClassifier::realtimeUrlInput(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return request.input.type == RealtimeInputType::Url;
}

bool MediaRealtimeRequestClassifier::rawRtpInput(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return request.input.type == RealtimeInputType::RtpPort;
}

bool MediaRealtimeRequestClassifier::mpegTsUdpInput(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return request.input.type == RealtimeInputType::MpegTsUdp;
}

bool MediaRealtimeRequestClassifier::unreliablePacketBoundary(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return rawRtpInput(request) || mpegTsUdpInput(request);
}

bool MediaRealtimeRequestClassifier::separateStreamsOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return request.output.streamLayout == RealtimeOutputStreamLayout::SeparateStreams;
}

bool MediaRealtimeRequestClassifier::muxedTransportOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return request.output.streamLayout == RealtimeOutputStreamLayout::MuxedTransportStream;
}

bool MediaRealtimeRequestClassifier::udpOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return request.output.transport == MediaOutputTransportKind::UdpDatagrams;
}

bool MediaRealtimeRequestClassifier::rtpAvpOutput(const MediaRealtimeRtpTranscodeRequest& request) noexcept
{
    return request.output.transport == MediaOutputTransportKind::RtpAvp;
}

} // namespace media::ffmpeg::graph
