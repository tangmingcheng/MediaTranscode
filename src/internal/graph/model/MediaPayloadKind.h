#pragma once

namespace media::ffmpeg::graph {

enum class MediaPayloadKind {
    Unknown,

    FormatContext,
    StreamDescriptor,
    CodecContext,
    CodecParameters,

    Packet,
    Frame,

    TimeDescriptor,
    HardwareDescriptor,
    AudioLayoutDescriptor,
    VideoFormatDescriptor,

    ControlSignal,
    GraphEvent,
    DiagnosticRecord
};

} // namespace media::ffmpeg::graph
