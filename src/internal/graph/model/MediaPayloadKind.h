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
    TsAccessUnit,
    ScheduledDatagramBatch,
    WireDatagramBatch,
    ScheduledWireDatagramBatch,
    DatagramShapingPlan
};

} // namespace media::ffmpeg::graph
