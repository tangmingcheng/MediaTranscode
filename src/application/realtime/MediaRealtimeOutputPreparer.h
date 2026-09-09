#pragma once

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshot.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/planner/capability/MediaDecoderRuntimeFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoEncodingGroupContract.h"

struct AVBufferRef;

namespace media::ffmpeg::graph {

struct MediaRealtimeOutputPreparationRequest final {
    const MediaRealtimeVideoOutputRequest& output;
    const MediaRealtimeRtpTranscodeRequest& sessionRequest;
    const MediaRealtimeVideoSessionFacts& sessionPlan;
    const FFmpegInputStreamSnapshot& sourceSnapshot;
    AVBufferRef* liveFrames;
    const MediaGraph& graph;
    MediaEndpoint formatSource;
    MediaSharedVideoDecodeEndpoints sharedDecode;
    std::string prefix;
    std::uint64_t sourceGeneration;
    const MediaDecoderRuntimeFacts& decoderFacts;
};

struct MediaPreparedRealtimeOutput final {
    MediaRealtimeRtpTranscodePlan plan;
    MediaRealtimeVideoOutputBranchGraph branch;
    MediaBufferRef encoder;
    MediaRealtimeVideoEncodingGroupContract encodingContract;
};

class MediaRealtimeOutputPreparer final {
public:
    static ::media::Result<MediaPreparedRealtimeOutput> prepare(
        const MediaRealtimeOutputPreparationRequest& request);
};

} // namespace media::ffmpeg::graph
