#pragma once

namespace media::ffmpeg::graph {

enum class RealtimeInputType {
    Url = 0,
    Sdp = 1,
    RtpPort = 2,
    MpegTsUdp = 3,
    ExternalMpegTsPacket = 4,
    ExternalRtpPacket = 5
};

enum class RealtimeInputStreamLayout {
    SessionDescribed = 0,
    SeparateStreams = 1,
    MuxedTransportStream = 2,
    BundledStream = 3
};

enum class RealtimeOutputStreamLayout {
    SeparateStreams = 0,
    BundledStream = 1,
    MuxedTransportStream = 2
};

} // namespace media::ffmpeg::graph
