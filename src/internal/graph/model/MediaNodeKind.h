#pragma once

namespace media::ffmpeg::graph {

enum class MediaNodeKind {
    Unknown,

    /* Sources */
    FileInput,
    RealtimeInput,
    RealtimePacketSource,

    /* Container / stream selection */
    Demux,
    StreamSplit,

    /* Split / route */
    PacketFanout,
    FrameRoute,

    /* Video branch */
    VideoDecode,
    VideoTimestamp,
    HardwareTransfer,
    VideoFrameRate,
    VideoFilter,
    VideoEncode,
    VideoPacketDrain,

    /* Audio branch */
    AudioStrategy,
    AudioCopy,
    AudioDecode,
    AudioResample,
    AudioEncode,
    AudioPacketNormalize,
    AudioPacketDrain,

    /* Merge / mux */
    PacketMerge,
    FileMux,
    RtpMux,

    /* Outputs */
    FileOutput,
    RtpOutput,
    SdpWriter,

    /* Lifecycle / control */
    EofBarrier,
    Flush,
    Finalize,
    ControlSignal,

    /* Metadata / diagnostics */
    CodecResolver,
    MetadataProbe,
    DebugDump,
    TraceProbe
};

} // namespace media::ffmpeg::graph
