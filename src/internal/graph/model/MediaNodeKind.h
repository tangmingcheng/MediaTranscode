#pragma once

namespace media::ffmpeg::graph {

enum class MediaNodeKind {
    Unknown = 0,

    /* Sources */
    FileInput = 1,
    RealtimeInput = 2,
    RealtimePacketSource = 3,

    /* Container / stream selection */
    Demux = 4,
    StreamSplit = 5,

    /* Split / route */
    PacketFanout = 6,
    FrameRoute = 7,

    /* Video branch */
    VideoDecode = 8,
    VideoTimestamp = 9,
    HardwareTransfer = 10,
    VideoFrameRate = 11,
    VideoFilter = 12,
    VideoEncode = 13,
    VideoPacketDrain = 14,

    /* Audio branch */
    AudioStrategy = 15,
    ReservedAudioCopy = 16,
    AudioDecode = 17,
    AudioResample = 18,
    AudioEncode = 19,
    ReservedAudioPacketNormalize = 20,
    AudioPacketDrain = 21,

    /* Merge / mux */
    PacketMerge = 22,
    FileMux = 23,
    RtpMux = 24,

    /* Outputs */
    FileOutput = 25,
    RtpOutput = 26,
    SdpWriter = 27,

    /* Lifecycle / control */
    EofBarrier = 28,
    Flush = 29,
    Finalize = 30,
    ControlSignal = 31,

    /* Metadata / diagnostics */
    CodecResolver = 32,
    MetadataProbe = 33,
    DebugDump = 34,
    TraceProbe = 35,

    /* New node kinds must only append values. Never insert in the middle. */
    ReservedAudioSourceConfig = 36,
    AudioCodecResolver = 37,
    PacketSourceConfig = 38,
    PacketNormalize = 39
};

} // namespace media::ffmpeg::graph
