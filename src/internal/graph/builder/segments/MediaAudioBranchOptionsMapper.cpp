#include "internal/graph/builder/segments/MediaAudioBranchOptionsMapper.h"

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"

namespace media::ffmpeg::graph {

MediaAudioPacketCopyBranchOptions makeAudioPacketCopyBranchOptions(const MediaAudioBranchSegmentOptions& options)
{
    MediaAudioPacketCopyBranchOptions copyOptions;
    copyOptions.prefix = options.prefix + ".copy";
    copyOptions.plan = options.plan;
    copyOptions.queues = options.queues;
    copyOptions.edgePolicies = options.edgePolicies;
    copyOptions.formatSourceNode = options.formatSourceNode;
    copyOptions.formatSourcePort = options.formatSourcePort;
    copyOptions.packetSourceNode = options.packetSourceNode;
    copyOptions.packetSourcePort = options.packetSourcePort;
    copyOptions.normalizePackets = options.normalizeInputPackets;
    return copyOptions;
}

MediaAudioEncodeBranchOptions makeAudioEncodeBranchOptions(const MediaAudioBranchSegmentOptions& options)
{
    MediaAudioEncodeBranchOptions encodeOptions;
    encodeOptions.prefix = options.prefix + ".encode";
    encodeOptions.plan = options.plan;
    encodeOptions.queues = options.queues;
    encodeOptions.edgePolicies = options.edgePolicies;
    encodeOptions.formatSourceNode = options.formatSourceNode;
    encodeOptions.formatSourcePort = options.formatSourcePort;
    encodeOptions.packetSourceNode = options.packetSourceNode;
    encodeOptions.packetSourcePort = options.packetSourcePort;
    encodeOptions.normalizePackets = options.normalizeInputPackets;
    encodeOptions.correctionMode = options.correctionMode;
    encodeOptions.lineageMode = options.lineageMode;
    encodeOptions.lineageCapacity = options.lineageCapacity;
    encodeOptions.correctionGeneration = options.correctionGeneration;
    encodeOptions.correctionLookaheadWindows = options.correctionLookaheadWindows;
    encodeOptions.syncGroup = options.syncGroup;
    return encodeOptions;
}

::media::Status mapSynchronizedAudioBranchOptions(
    const MediaRealtimeAvSyncRuntimePlan& runtime,
    MediaAudioBranchSegmentOptions& options)
{
    if (options.correctionMode || options.lineageMode ||
        options.lineageCapacity || options.correctionGeneration ||
        options.correctionLookaheadWindows || options.syncGroup) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "synchronized audio branch mapping requires empty execution options"));
    }

    if (std::holds_alternative<MediaSynchronizedAudioPacketCopyBounds>(
            runtime.componentBounds)) {
        if (runtime.audioPipeline.branchMode != MediaBranchMode::CopyPacket ||
            runtime.audioCorrection) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "synchronized packet-copy bounds conflict with runtime audio facts"));
        }
        options.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
        options.lineageMode =
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
        return ::media::Status::success();
    }

    if (!std::holds_alternative<MediaSynchronizedAudioFrameTranscodeBounds>(
            runtime.componentBounds) ||
        runtime.audioPipeline.branchMode != MediaBranchMode::TranscodeFrame ||
        !runtime.audioCorrection || runtime.queues.frame == 0 ||
        !runtime.synchronization.audioServo.correctionLookaheadWindows) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "synchronized frame-transcode bounds are incomplete"));
    }
    options.correctionMode =
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
    options.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    options.lineageCapacity = runtime.queues.frame;
    options.correctionGeneration = MediaFirstLockedSourceGeneration;
    options.correctionLookaheadWindows =
        runtime.synchronization.audioServo.correctionLookaheadWindows;
    options.syncGroup = runtime.groupKey;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
