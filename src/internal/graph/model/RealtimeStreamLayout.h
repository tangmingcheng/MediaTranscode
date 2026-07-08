#pragma once

namespace media::ffmpeg::graph {

enum class RealtimeInputType {
    Url,
    Sdp,
    RtpPort,
    MpegTsUdp,
    ExternalMpegTsPacket,
    ExternalRtpPacket
};

enum class RealtimeInputStreamLayout {
    SessionDescribed,
    SeparateStreams,
    MuxedTransportStream,
    BundledStream
};

enum class RealtimeOutputStreamLayout {
    SeparateStreams,
    BundledStream,
    MuxedTransportStream
};

} // namespace media::ffmpeg::graph
