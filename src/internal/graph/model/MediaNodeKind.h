#pragma once

namespace media::ffmpeg::graph {

enum class MediaNodeKind {
    Unknown = 0,

    /* Sources */
    FileInput = 1,
    RealtimeInput = 2,
    ReservedNodeKind3 = 3,

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
    ReservedNodeKind14 = 14,

    /* Audio branch */
    ReservedNodeKind15 = 15,
    ReservedNodeKind16 = 16,
    AudioDecode = 17,
    AudioResample = 18,
    AudioEncode = 19,
    ReservedNodeKind20 = 20,
    ReservedNodeKind21 = 21,

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
    ReservedNodeKind36 = 36,
    AudioCodecResolver = 37,
    PacketSourceConfig = 38,
    PacketNormalize = 39,
    RawRtpInput = 40,
    ReservedNodeKind41 = 41,
    PacketStartGate = 42,
    RtpClockGroup = 43,
    MpegTsDemux = 44,
    AvStartupCoordinator = 45,
    AvOutputScheduler = 46,
    PlaybackEpochBinder = 47,
    CanonicalInput = 48,
    AvBoundReleaseExtractor = 49,
    AudioStartupTrim = 50,
    RtpPacketClockBinder = 51,
    RtpClockSnapshotFanout = 52,
    LockedPacketGate = 53,
    ReservedNodeKind54 = 54,
    RtpSourceClockStateAdapter = 55,
    AvStartupClock = 56,
    ActivatedStartupReleaseSequencer = 57,
    SourceClockStateFanout = 58,
    AudioDriftController = 59,
    EncodedAudioCanonicalizer = 60,
    ScheduledOutputRouter = 61,
    ScheduledRtpSender = 62,
    DualMediaSdpPublisher = 63,
    ProjectMpegTsPlanSource = 64,
    ScheduledTsAccessUnitAdapter = 65,
    DemuxPacketClockBinder = 66,
    MpegTsRtpSdpPublisher = 67,
    VideoOutputScheduler = 68
};

} // namespace media::ffmpeg::graph
