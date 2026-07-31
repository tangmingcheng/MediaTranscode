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
    DiagnosticRecord,
    OutputByteSink,
    ProjectMpegTsRuntimePlan,
    TsAccessUnit
};

} // namespace media::ffmpeg::graph
