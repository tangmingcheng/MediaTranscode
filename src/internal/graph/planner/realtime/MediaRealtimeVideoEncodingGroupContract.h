#pragma once

#include "internal/graph/model/MediaVideoJoinPlan.h"

#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/core/MediaNodeId.h"

#include "internal/graph/planner/capability/MediaVideoEncoderReadback.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeVideoEncodingStageContract final {
    std::string codecName;
    std::string implementation;
    std::string filterName;
    std::string hardwareAdapter;
    std::optional<MediaHardwareDescriptor> inputFrame;
    std::optional<MediaHardwareDescriptor> outputFrame;
    friend bool operator==(const MediaRealtimeVideoEncodingStageContract&,
                           const MediaRealtimeVideoEncodingStageContract&) = default;
};

// Protocol targets and session identities do not belong to encoder equivalence.
struct MediaRealtimeVideoEncodingGroupContract final {
    MediaNodeId sourceFanout;
    std::uint64_t sourceGeneration;
    int sourceStreamIndex;
    MediaRealtimeVideoEncodingStageContract decoder;
    MediaRealtimeVideoEncodingStageContract filter;
    MediaRealtimeVideoEncodingStageContract encoder;
    MediaEncoderOpenContract open;
    MediaPreparedEncoderEmissionEnvelope emission;
    MediaEncodedPacketLayout packetLayout;
    MediaHardwareTransferDirection transfer;
    MediaVideoLineagePropagation decoderLineage;
    MediaVideoLineagePropagation encoderLineage;
    MediaVideoFilterImplementation filterImplementation;
    MediaVideoEncoderAbortPolicy abortPolicy;
    bool filterActive;
    bool synthesizeMissingTimestamps;
    MediaRational sourceTimeBase;
    MediaRational sourceFrameRate;
    MediaRational maximumFrameDuplicationGap;
    MediaVideoEncoderReadback readback;
    friend bool operator==(const MediaRealtimeVideoEncodingGroupContract&,
                           const MediaRealtimeVideoEncodingGroupContract&) = default;
};

class MediaRealtimeVideoEncodingGroupContractPlanner final {
public:
    static ::media::Result<MediaVideoJoinPlan> joinPlan(const MediaPipelinePlan& pipeline);
    static ::media::Result<MediaRealtimeVideoEncodingGroupContract> plan(
        const MediaPipelinePlan& pipeline,
        MediaNodeId sourceFanout,
        std::uint64_t sourceGeneration,
        MediaRational sourceTimeBase,
        MediaRational sourceFrameRate,
        const MediaVideoEncoderReadback& retainedEncoder);
};

} // namespace media::ffmpeg::graph
