#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"

#include "internal/graph/nodes/audio/AudioCodecResolverNode.h"
#include "internal/graph/nodes/audio/AudioDecodeNode.h"
#include "internal/graph/nodes/audio/AudioEncodeNode.h"
#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/nodes/audio/MediaAudioStartupTrimNode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageStagePreparation.h"
#include "internal/graph/nodes/control/ControlSignalNode.h"
#include "internal/graph/nodes/debug/DebugDumpNode.h"
#include "internal/graph/nodes/debug/TraceProbeNode.h"
#include "internal/graph/nodes/demux/DemuxNode.h"
#include "internal/graph/nodes/demux/MpegTsDemuxNode.h"
#include "internal/graph/nodes/demux/StreamSplitNode.h"
#include "internal/graph/nodes/input/FileInputNode.h"
#include "internal/graph/nodes/input/RawRtpInputNode.h"
#include "internal/graph/nodes/input/RealtimeInputNode.h"
#include "internal/graph/nodes/lifecycle/EofBarrierNode.h"
#include "internal/graph/nodes/lifecycle/FinalizeNode.h"
#include "internal/graph/nodes/lifecycle/FlushNode.h"
#include "internal/graph/nodes/merge/PacketMergeNode.h"
#include "internal/graph/nodes/metadata/CodecResolverNode.h"
#include "internal/graph/nodes/metadata/MetadataProbeNode.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h"
#include "internal/graph/nodes/mux/RtpMuxNode.h"
#include "internal/graph/nodes/output/FileOutputNode.h"
#include "internal/graph/nodes/output/RtpOutputNode.h"
#include "internal/graph/nodes/output/SdpWriterNode.h"
#include "internal/graph/nodes/output/MediaRtpSdpPublisherNode.h"
#include "internal/graph/nodes/output/MediaMpegTsRtpSdpPublisherNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h"
#include "internal/graph/nodes/packet/PacketNormalizeNode.h"
#include "internal/graph/nodes/packet/PacketSourceConfigNode.h"
#include "internal/graph/nodes/packet/PacketStartGateNode.h"
#include "internal/graph/nodes/route/FrameRouteNode.h"
#include "internal/graph/nodes/split/PacketFanoutNode.h"
#include "internal/graph/nodes/sync/MediaRtpClockGroupNode.h"
#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"
#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNode.h"
#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"
#include "internal/graph/nodes/sync/MediaRtpClockSnapshotFanoutNode.h"
#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h"
#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNodePreparation.h"
#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/nodes/sync/MediaVideoOutputSchedulerNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"
#include "internal/graph/nodes/sync/MediaLockedPacketGateNode.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h"
#include "internal/graph/nodes/sync/MediaAvStartupClockNode.h"
#include "internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h"
#include "internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.h"
#include "internal/graph/nodes/sync/MediaSourceClockStateFanoutNode.h"
#include "internal/graph/nodes/sync/MediaAudioDriftControllerNode.h"
#include "internal/graph/nodes/sync/MediaEncodedAudioCanonicalizerNode.h"
#include "internal/graph/nodes/sync/MediaScheduledOutputRouterNode.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/video/HardwareTransferNode.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoEncodeNode.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/nodes/video/VideoFrameRateNode.h"
#include "internal/graph/nodes/video/VideoTimestampNode.h"
#include "internal/graph/sync/lineage/MediaVideoLineageStagePreparation.h"
#include "internal/graph/sync/lineage/MediaVideoFrameRateState.h"
#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSessionFactory.h"
#include "internal/graph/runtime/filesystem/MediaPlatformAtomicFileReplacePort.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderSocket.h"

#include <new>

namespace media::ffmpeg::graph {
namespace {

template <typename Node>
std::optional<MediaRuntimeGenerationPurgeRegistration>
fixedGenerationPurgeRegistration(
    MediaRuntimeNode& runtime,
    MediaAvGenerationParticipant participant)
{
    auto* node = dynamic_cast<Node*>(&runtime);
    if (!node) return std::nullopt;
    return MediaRuntimeGenerationPurgeRegistration{
        participant,
        {std::string(Node::generationPurgeIdentity()),
         node->generationPurgeTarget()}};
}

::media::Result<MediaAvSyncGroupKey> requiredSyncGroup(
    const MediaNode& node,
    const char* nodeName,
    const char* optionName)
{
    auto group = requiredNodeOption(&node.options, nodeName, optionName);
    if (!group) {
        return ::media::Result<MediaAvSyncGroupKey>::failure(group.error());
    }
    MediaAvSyncGroupKey groupKey(std::move(group).value());
    if (!groupKey.valid()) {
        return ::media::Result<MediaAvSyncGroupKey>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(nodeName) + " requires a valid planned sync group"));
    }
    return ::media::Result<MediaAvSyncGroupKey>::success(std::move(groupKey));
}

template <typename Node>
::media::Result<std::unique_ptr<MediaRuntimeNode>> createVideoLineageStage(
    const MediaNode& node)
{
    auto prepared = prepareMediaVideoLineageStage(
        node, Node::generationPurgeIdentity());
    if (!prepared) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            prepared.error());
    }
    if (prepared.value()) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<Node>(node.id, std::move(prepared).value()));
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<Node>(node.id));
}

::media::Result<std::unique_ptr<MediaRuntimeNode>> createVideoFrameRateStage(
    const MediaNode& node,
    const std::shared_ptr<MediaAvStartupVideoPreparationState>& preparationState)
{
    auto capacity = prepareMediaVideoLineageStageCapacity(
        node, VideoFrameRateNode::generationPurgeIdentity());
    if (!capacity) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            capacity.error());
    }
    std::optional<MediaAvStartupVideoPreparationCapability> preparation;
    if (node.options.value("video.startup_preparation.owner") == "1") {
        if (!capacity.value()) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video startup preparation owner requires canonical lineage capacity"));
        }
        if (!preparationState) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Planned video startup preparation owner requires shared state"));
        }
        auto issued = MediaAvStartupVideoPreparationCapability::issue(
            preparationState,
            MediaAvStartupVideoPreparationRole::OutputReadiness);
        if (!issued) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                issued.error());
        }
        preparation.emplace(std::move(issued).value());
    }
    if (capacity.value()) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<VideoFrameRateNode>(
                node.id, std::make_shared<MediaVideoFrameRateState>(true),
                std::move(preparation)));
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<VideoFrameRateNode>(node.id));
}

::media::Result<MediaAudioLineageExecutionMode> requireAudioLineageMode(
    const MediaNode& node)
{
    if (!node.options.has(std::string(MediaAudioLineageModeOptionKey))) {
        return ::media::Result<MediaAudioLineageExecutionMode>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio runtime stage requires explicit audio lineage mode"));
    }
    const std::string mode = node.options.value(
        std::string(MediaAudioLineageModeOptionKey));
    if (mode == mediaAudioLineageExecutionModeName(
                    MediaAudioLineageExecutionMode::LegacyPlainPacket)) {
        return ::media::Result<MediaAudioLineageExecutionMode>::success(
            MediaAudioLineageExecutionMode::LegacyPlainPacket);
    }
    if (mode == mediaAudioLineageExecutionModeName(
                    MediaAudioLineageExecutionMode::SynchronizedReleasedAudio)) {
        return ::media::Result<MediaAudioLineageExecutionMode>::success(
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio);
    }
    return ::media::Result<MediaAudioLineageExecutionMode>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Audio runtime stage rejects unknown audio lineage mode"));
}

template <typename Node>
::media::Result<std::unique_ptr<MediaRuntimeNode>> createAudioLineageStage(
    const MediaNode& node)
{
    auto mode = requireAudioLineageMode(node);
    if (!mode) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(mode.error());
    }
    auto prepared = prepareMediaAudioLineageStage(
        node, Node::generationPurgeIdentity());
    if (!prepared) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            prepared.error());
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<Node>(
            node.id, mode.value(),
            std::make_shared<typename Node::LineageState>(
                mode.value(), prepared.value().capacity)));
}

::media::Result<std::unique_ptr<MediaRuntimeNode>> createAudioStartupTrimStage(
    const MediaNode& node)
{
    auto mode = requireAudioLineageMode(node);
    if (!mode || mode.value() !=
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            mode ? ::media::ErrorInfo::invalidArgument(
                       "Audio startup trim requires synchronized audio lineage mode")
                 : mode.error());
    }
    auto prepared = prepareMediaAudioLineageStage(
        node, MediaAudioStartupTrimNode::generationPurgeIdentity());
    if (!prepared) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            prepared.error());
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<MediaAudioStartupTrimNode>(
            node.id,
            std::make_shared<MediaAudioStartupTrimNode::LineageState>(
                mode.value(), prepared.value().capacity)));
}

} // namespace

::media::Result<std::unique_ptr<MediaRuntimeNode>> MediaRuntimeNodeFactory::create(const MediaNode& node)
{
    return create(node, nullptr, nullptr, nullptr);
}

::media::Result<std::unique_ptr<MediaRuntimeNode>> MediaRuntimeNodeFactory::create(
    const MediaNode& node,
    MediaPreparedRealtimeInputBinding* binding)
{
    return create(node, binding, nullptr, nullptr);
}

::media::Result<std::unique_ptr<MediaRuntimeNode>> MediaRuntimeNodeFactory::create(
    const MediaNode& node,
    MediaPreparedRealtimeInputBinding* binding,
    const std::shared_ptr<MediaAvStartupVideoPreparationState>&
        videoPreparationState)
{
    return create(node, binding, videoPreparationState, nullptr);
}

::media::Result<std::unique_ptr<MediaRuntimeNode>> MediaRuntimeNodeFactory::create(
    const MediaNode& node,
    MediaPreparedRealtimeInputBinding* binding,
    const std::shared_ptr<MediaAvStartupVideoPreparationState>&
        videoPreparationState,
    const std::shared_ptr<MediaProtocolOutputRuntimeAuthority>&
        protocolOutputAuthority)
{
    switch (node.kind) {
    case MediaNodeKind::FileInput:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<FileInputNode>(node.id));
    case MediaNodeKind::RealtimeInput:
        if (!binding || binding->nodeId != node.id || !binding->prepared.valid()) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                ::media::ErrorInfo::notInitialized("RealtimeInput runtime requires prepared node binding"));
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<RealtimeInputNode>(node.id, binding->expectedKind,
                                                std::move(binding->prepared)));
    case MediaNodeKind::RawRtpInput:
    {
        auto requiresPrepared = requiredBoolNodeOption(
            &node.options, "RawRtpInputNode",
            "rtp.prepared_input_required");
        if (!requiresPrepared) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                requiresPrepared.error());
        }
        if (requiresPrepared.value()) {
            if (!binding || binding->nodeId != node.id ||
                binding->expectedKind != MediaPreparedRealtimeInputKind::RawRtp ||
                !binding->prepared.valid()) {
                return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "RawRtpInput runtime requires exact prepared raw RTP binding"));
            }
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
                std::make_unique<RawRtpInputNode>(
                    node.id, std::move(binding->prepared)));
        }
        if (binding) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "node-owned RawRtpInput rejects prepared binding"));
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<RawRtpInputNode>(node.id));
    }
    case MediaNodeKind::Demux:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<DemuxNode>(node.id));
    case MediaNodeKind::MpegTsDemux:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<MpegTsDemuxNode>(node.id));
    case MediaNodeKind::StreamSplit:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<StreamSplitNode>(node.id));
    case MediaNodeKind::PacketFanout:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<PacketFanoutNode>(node.id));
    case MediaNodeKind::FrameRoute:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<FrameRouteNode>(node.id));
    case MediaNodeKind::VideoDecode:
        return createVideoLineageStage<VideoDecodeNode>(node);
    case MediaNodeKind::VideoTimestamp:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<VideoTimestampNode>(node.id));
    case MediaNodeKind::HardwareTransfer:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<HardwareTransferNode>(node.id));
    case MediaNodeKind::VideoFrameRate:
        return createVideoFrameRateStage(node, videoPreparationState);
    case MediaNodeKind::VideoFilter:
    {
        auto prepared = prepareMediaVideoLineageStage(
            node, VideoFilterNode::generationPurgeIdentity());
        if (!prepared) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                prepared.error());
        }
        if (!videoPreparationState) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
                std::make_unique<VideoFilterNode>(
                    node.id, std::move(prepared).value()));
        }
        auto capability = MediaAvStartupVideoPreparationCapability::issue(
            videoPreparationState,
            MediaAvStartupVideoPreparationRole::OutputReadiness);
        if (!capability) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                capability.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<VideoFilterNode>(
                node.id, std::move(prepared).value(),
                std::move(capability).value()));
    }
    case MediaNodeKind::VideoEncode:
        return createVideoLineageStage<VideoEncodeNode>(node);
    case MediaNodeKind::AudioCodecResolver:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<AudioCodecResolverNode>(node.id));
    case MediaNodeKind::AudioDecode:
        return createAudioLineageStage<AudioDecodeNode>(node);
    case MediaNodeKind::AudioStartupTrim:
        return createAudioStartupTrimStage(node);
    case MediaNodeKind::AudioResample:
        return createAudioLineageStage<AudioResampleNode>(node);
    case MediaNodeKind::AudioEncode:
        return createAudioLineageStage<AudioEncodeNode>(node);
    case MediaNodeKind::PacketSourceConfig:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<PacketSourceConfigNode>(node.id));
    case MediaNodeKind::PacketNormalize:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<PacketNormalizeNode>(node.id));
    case MediaNodeKind::PacketStartGate:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<PacketStartGateNode>(node.id));
    case MediaNodeKind::RtpClockGroup:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<MediaRtpClockGroupNode>(node.id));
    case MediaNodeKind::RtpPacketClockBinder:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaRtpPacketClockBinderNode>(node.id));
    case MediaNodeKind::DemuxPacketClockBinder:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Demux packet clock binder requires compiler injection"));
    case MediaNodeKind::RtpClockSnapshotFanout:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaRtpClockSnapshotFanoutNode>(node.id));
    case MediaNodeKind::AvStartupCoordinator: {
        auto prepared = prepareMediaAvStartupCoordinatorNode(node);
        if (!prepared) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                prepared.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAvStartupCoordinatorNode>(
                node.id, std::move(prepared).value()));
    }
    case MediaNodeKind::AvOutputScheduler:
        if (!protocolOutputAuthority ||
            protocolOutputAuthority->streamSet() !=
                MediaTranscodeStreamSet::AudioVideo) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V scheduler requires compiler-injected output authority"));
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAvOutputSchedulerNode>(
                node.id, protocolOutputAuthority));
    case MediaNodeKind::VideoOutputScheduler:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly scheduler requires compiler-injected output authority"));
    case MediaNodeKind::PlaybackEpochBinder:
    {
        auto group = requiredSyncGroup(
            node, "MediaPlaybackEpochBinderNode",
            "playback_epoch_binder.sync_group");
        if (!group) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                group.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaPlaybackEpochBinderNode>(
                node.id, std::move(group).value()));
    }
    case MediaNodeKind::CanonicalInput:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaCanonicalInputNode>(node.id));
    case MediaNodeKind::LockedPacketGate:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaLockedPacketGateNode>(node.id));
    case MediaNodeKind::AvBoundReleaseExtractor: {
        auto group = requiredSyncGroup(
            node,
            "MediaAvBoundReleaseExtractorNode",
            "av_bound_release_extractor.sync_group");
        if (!group) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                group.error());
        }
        auto encodedAudioBranch = requiredNodeOption(
            &node.options,
            "MediaAvBoundReleaseExtractorNode",
            "av_bound_release_extractor.audio_branch_mode");
        if (!encodedAudioBranch) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                encodedAudioBranch.error());
        }
        MediaBranchMode audioBranchMode = MediaBranchMode::Drop;
        if (!parseMediaBranchMode(encodedAudioBranch.value(), audioBranchMode) ||
            (audioBranchMode != MediaBranchMode::CopyPacket &&
             audioBranchMode != MediaBranchMode::TranscodeFrame)) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaAvBoundReleaseExtractorNode requires a planned audio branch mode"));
        }
        if (videoPreparationState) {
            auto capability = MediaAvStartupVideoPreparationCapability::issue(
                videoPreparationState,
                MediaAvStartupVideoPreparationRole::ExtractorFeed);
            if (!capability) {
                return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                    capability.error());
            }
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
                std::make_unique<MediaAvBoundReleaseExtractorNode>(
                    node.id,
                    std::move(group).value(),
                    audioBranchMode,
                    std::move(capability).value()));
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAvBoundReleaseExtractorNode>(
                node.id, std::move(group).value(), audioBranchMode));
    }
    case MediaNodeKind::ActivatedStartupReleaseSequencer:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Activation release sequencer requires compiler-issued activation authority"));
    case MediaNodeKind::RtpSourceClockStateAdapter:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaRtpSourceClockStateAdapterNode>(node.id));
    case MediaNodeKind::AvStartupClock:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAvStartupClockNode>(node.id));
    case MediaNodeKind::SourceClockStateFanout:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaSourceClockStateFanoutNode>(node.id));
    case MediaNodeKind::AudioDriftController:
    {
        auto group = requiredSyncGroup(
            node, "MediaAudioDriftControllerNode",
            "audio_drift_controller.sync_group");
        if (!group) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                group.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAudioDriftControllerNode>(
                node.id, std::move(group).value()));
    }
    case MediaNodeKind::EncodedAudioCanonicalizer:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaEncodedAudioCanonicalizerNode>(node.id));
    case MediaNodeKind::ScheduledOutputRouter:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaScheduledOutputRouterNode>(node.id));
    case MediaNodeKind::ProjectMpegTsPlanSource:
    {
        auto decoded = MediaProjectMpegTsPlanSourceNodePlanCodec::decode(node);
        if (!decoded) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                decoded.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaProjectMpegTsPlanSourceNode>(
                node.id, std::move(decoded.value().sessionKey),
                decoded.value().streamSet,
                std::move(decoded.value().outputPlan),
                protocolOutputAuthority));
    }
    case MediaNodeKind::ScheduledTsAccessUnitAdapter:
    {
        auto session = requiredNodeOption(
            &node.options, "MediaScheduledTsAccessUnitAdapterNode",
            "scheduled_ts_adapter.session");
        auto streamSet = requiredNodeOption(
            &node.options, "MediaScheduledTsAccessUnitAdapterNode",
            "scheduled_ts_adapter.stream_set");
        if (!session || !streamSet) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                session ? streamSet.error() : session.error());
        }
        auto decodedStreamSet = MediaTranscodeStreamSetCodec::decode(
            streamSet.value());
        MediaProtocolOutputSessionKey sessionKey(std::move(session).value());
        if (!sessionKey.valid() || !decodedStreamSet) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                decodedStreamSet
                    ? ::media::ErrorInfo::invalidArgument(
                          "Scheduled TS adapter requires an explicit session")
                    : decodedStreamSet.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaScheduledTsAccessUnitAdapterNode>(
                node.id, std::move(sessionKey), decodedStreamSet.value(),
                protocolOutputAuthority));
    }
    case MediaNodeKind::PacketMerge:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<PacketMergeNode>(node.id));
    case MediaNodeKind::FileMux: {
        auto kindValue = requiredNodeOption(
            &node.options, "MediaRuntimeNodeFactory",
            MediaTranscodeOptionKey::MuxSessionKind);
        if (!kindValue) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                kindValue.error());
        }
        auto kind = parseMediaMuxSessionKindOption(kindValue.value());
        if (!kind) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                kind.error());
        }
        if (kind.value() == MediaMuxSessionKind::ProjectMpegTs) {
            auto generationSession =
                std::make_shared<ProjectMpegTsGenerationSessionState>();
            auto generationState =
                std::make_shared<MediaProtocolOutputGenerationState>(
                    std::string(FileMuxNode::generationPurgeIdentity()),
                    generationSession);
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
                std::make_unique<FileMuxNode>(
                    node.id, std::move(generationState),
                    protocolOutputAuthority));
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<FileMuxNode>(node.id));
    }
    case MediaNodeKind::RtpMux:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<RtpMuxNode>(node.id));
    case MediaNodeKind::FileOutput:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<FileOutputNode>(node.id));
    case MediaNodeKind::RtpOutput:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<RtpOutputNode>(node.id));
    case MediaNodeKind::SdpWriter:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<SdpWriterNode>(node.id));
    case MediaNodeKind::RtpSdpPublisher:
    {
        auto path = requiredNodeOption(
            &node.options, "MediaRtpSdpPublisherNode", "sdp.path");
        auto streamSet = requiredNodeOption(
            &node.options, "MediaRtpSdpPublisherNode", "sdp.stream_set");
        if (!path || !streamSet) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                path ? streamSet.error() : path.error());
        }
        auto decoded = MediaTranscodeStreamSetCodec::decode(
            streamSet.value());
        if (!decoded) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                decoded.error());
        }
        auto publisher = MediaRtpSdpPublisherNode::create(
            node.id, decoded.value(), std::move(path).value(),
            std::make_unique<MediaPlatformAtomicFileReplacePort>());
        return publisher
            ? ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
                  std::move(publisher).value())
            : ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                  publisher.error());
    }
    case MediaNodeKind::ScheduledRtpSender:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender requires compiler-injected output authority"));
    case MediaNodeKind::MpegTsRtpSdpPublisher:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "MP2T SDP publisher requires compiler-injected sync-group runtime"));
    case MediaNodeKind::EofBarrier:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<EofBarrierNode>(node.id));
    case MediaNodeKind::Flush:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<FlushNode>(node.id));
    case MediaNodeKind::Finalize:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<FinalizeNode>(node.id));
    case MediaNodeKind::ControlSignal:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<ControlSignalNode>(node.id));
    case MediaNodeKind::CodecResolver:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<CodecResolverNode>(node.id));
    case MediaNodeKind::MetadataProbe:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<MetadataProbeNode>(node.id));
    case MediaNodeKind::DebugDump:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<DebugDumpNode>(node.id));
    case MediaNodeKind::TraceProbe:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<TraceProbeNode>(node.id));
    default:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::unsupported("MediaRuntimeNodeFactory unsupported node kind"));
    }
}

::media::Result<std::unique_ptr<MediaRuntimeNode>>
MediaRuntimeNodeFactory::createActivatedStartupReleaseSequencer(
    const MediaNode& node,
    MediaPlaybackEpochActivationCapability capability,
    const std::shared_ptr<MediaAvStartupVideoPreparationState>&
        videoPreparationState)
{
    if (node.kind != MediaNodeKind::ActivatedStartupReleaseSequencer) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Activation release sequencer factory requires the sequencer node kind"));
    }
    auto group = requiredSyncGroup(
        node, "MediaActivatedStartupReleaseSequencerNode",
        "activated_startup_release_sequencer.sync_group");
    if (!group) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            group.error());
    }
    auto outputLead = requiredPositiveInt64NodeOption(
        &node.options, "MediaActivatedStartupReleaseSequencerNode",
        "activated_startup_release_sequencer.output_lead_ns");
    if (!outputLead) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            outputLead.error());
    }
    std::optional<MediaAvStartupVideoPreparationCapability> preparation;
    if (videoPreparationState) {
        auto issued = MediaAvStartupVideoPreparationCapability::issue(
            videoPreparationState,
            MediaAvStartupVideoPreparationRole::SequencerActivation);
        if (!issued) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                issued.error());
        }
        preparation.emplace(std::move(issued).value());
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<MediaActivatedStartupReleaseSequencerNode>(
            node.id, std::move(group).value(), std::move(capability),
            MediaRunningTime::fromNanoseconds(outputLead.value()),
            std::move(preparation)));
}

::media::Result<std::unique_ptr<MediaRuntimeNode>>
MediaRuntimeNodeFactory::createScheduledRtpSender(
    const MediaNode& node,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
{
    if (node.kind != MediaNodeKind::ScheduledRtpSender) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender factory requires the sender node kind"));
    }
    auto decoded = MediaScheduledRtpSenderNodePlanCodec::decode(node);
    if (!decoded) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            decoded.error());
    }
    if (!authority ||
        authority->sessionKey() != decoded.value().sessionKey ||
        authority->streamSet() != decoded.value().streamSet) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender requires the exact protocol output authority"));
    }
    auto socketRuntime = MediaSocketRuntime::create();
    if (!socketRuntime) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            socketRuntime.error());
    }
    try {
        MediaScheduledRtpSenderNodeDependencies dependencies{
            std::move(authority),
            std::make_unique<MediaUdpDatagramSenderSocketFactory>(
                std::move(socketRuntime).value()),
            std::make_unique<ScheduledRtpMuxFfmpegSessionFactory>()};
        auto created = MediaScheduledRtpSenderNode::create(
            node.id,
            std::move(decoded.value().sessionKey),
            decoded.value().streamSet,
            std::move(decoded.value().output),
            std::move(decoded.value().sdp),
            std::move(dependencies));
        if (!created) {
            return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
                created.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::move(created).value());
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Scheduled RTP sender production dependencies"));
    }
}

::media::Result<std::unique_ptr<MediaRuntimeNode>>
MediaRuntimeNodeFactory::createVideoOutputScheduler(
    const MediaNode& node,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
{
    if (node.kind != MediaNodeKind::VideoOutputScheduler || !authority ||
        authority->streamSet() != MediaTranscodeStreamSet::VideoOnly) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoOnly scheduler factory requires its exact output authority"));
    }
    auto videoAuthority =
        std::dynamic_pointer_cast<MediaVideoProtocolOutputRuntimeAuthority>(
            std::move(authority));
    if (!videoAuthority) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "VideoOnly scheduler requires the fixed-generation video authority"));
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<MediaVideoOutputSchedulerNode>(
            node.id, std::move(videoAuthority)));
}

::media::Result<std::unique_ptr<MediaRuntimeNode>>
MediaRuntimeNodeFactory::createMpegTsRtpSdpPublisher(
    const MediaNode& node,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
{
    if (node.kind != MediaNodeKind::MpegTsRtpSdpPublisher) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T SDP publisher factory requires its exact node kind"));
    }
    auto session = requiredNodeOption(
        &node.options, "MediaMpegTsRtpSdpPublisherNode",
        "mpegts_rtp_sdp.session");
    auto streamSet = requiredNodeOption(
        &node.options, "MediaMpegTsRtpSdpPublisherNode",
        "mpegts_rtp_sdp.stream_set");
    if (!session || !streamSet) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            session ? streamSet.error() : session.error());
    }
    auto decodedStreamSet = MediaTranscodeStreamSetCodec::decode(
        streamSet.value());
    MediaProtocolOutputSessionKey sessionKey(std::move(session).value());
    if (!authority || !sessionKey.valid() || !decodedStreamSet ||
        authority->sessionKey() != sessionKey ||
        (decodedStreamSet &&
         authority->streamSet() != decodedStreamSet.value())) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "MP2T SDP publisher requires the exact output authority"));
    }
    try {
        auto created = MediaMpegTsRtpSdpPublisherNode::create(
            node.id, std::move(sessionKey), decodedStreamSet.value(),
            std::move(authority),
            std::make_unique<MediaPlatformAtomicFileReplacePort>());
        if (!created) {
            return ::media::Result<
                std::unique_ptr<MediaRuntimeNode>>::failure(
                created.error());
        }
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::move(created).value());
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MP2T SDP publisher production dependencies"));
    }
}

::media::Result<std::unique_ptr<MediaRuntimeNode>>
MediaRuntimeNodeFactory::createDemuxPacketClockBinder(
    const MediaNode& node,
    const MediaDecodedDemuxPacketClockBinderNodePlan& decoded,
    std::shared_ptr<MediaDemuxTimestampClockMapper> mapper,
    std::shared_ptr<MediaAvSyncGroupRuntime> syncGroup)
{
    if (node.kind != MediaNodeKind::DemuxPacketClockBinder ||
        !mapper || !syncGroup || !syncGroup->clock()) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Demux packet clock binder injection requires its exact node, mapper, and sync group"));
    }
    const MediaRational expected =
        decoded.stream == MediaScheduledStream::Video
        ? mapper->config().videoTimeBase
        : mapper->config().audioTimeBase;
    if (decoded.groupKey != syncGroup->key() ||
        decoded.mapper != mapper->config() ||
        decoded.streamTimeBase.num != expected.num ||
        decoded.streamTimeBase.den != expected.den) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Demux packet clock binder injection disagrees with the planner product"));
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<MediaDemuxPacketClockBinderNode>(
            node.id,
            decoded.stream,
            expected,
            decoded.firstPacket,
            std::move(mapper),
            std::move(syncGroup)));
}

std::optional<MediaRuntimeGenerationPurgeRegistration>
MediaRuntimeNodeFactory::generationPurgeRegistration(
    MediaRuntimeNode& runtime)
{
    if (auto* binder =
            dynamic_cast<MediaDemuxPacketClockBinderNode*>(&runtime)) {
        return MediaRuntimeGenerationPurgeRegistration{
            MediaAvGenerationParticipant::CanonicalLineage,
            {std::string(binder->generationPurgeIdentity()),
             binder->generationPurgeTarget()}};
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<MediaAvStartupCoordinatorNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<VideoDecodeNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<VideoFrameRateNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<VideoFilterNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<VideoEncodeNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<AudioDecodeNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<MediaAudioStartupTrimNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<AudioResampleNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<AudioEncodeNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<
                MediaEncodedAudioCanonicalizerNode>(
                runtime,
                MediaAvGenerationParticipant::CanonicalLineage)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<MediaAudioDriftControllerNode>(
                runtime,
                MediaAvGenerationParticipant::AudioCorrection)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<MediaAvOutputSchedulerNode>(
                runtime,
                MediaAvGenerationParticipant::Scheduler)) {
        return registration;
    }
    if (auto* sender =
            dynamic_cast<MediaScheduledRtpSenderNode*>(&runtime)) {
        const std::string identity(sender->generationPurgeIdentity());
        if (identity == "rtp_video_output_generation_state") {
            return MediaRuntimeGenerationPurgeRegistration{
                MediaAvGenerationParticipant::RtpVideoOutput,
                {identity, sender->generationPurgeTarget()}};
        }
        if (identity == "rtp_audio_output_generation_state") {
            return MediaRuntimeGenerationPurgeRegistration{
                MediaAvGenerationParticipant::RtpAudioOutput,
                {identity, sender->generationPurgeTarget()}};
        }
        return std::nullopt;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<MediaProjectMpegTsPlanSourceNode>(
                runtime,
                MediaAvGenerationParticipant::ProjectMpegTsOutput)) {
        return registration;
    }
    if (auto registration =
            fixedGenerationPurgeRegistration<
                MediaScheduledTsAccessUnitAdapterNode>(
                runtime,
                MediaAvGenerationParticipant::ProjectMpegTsOutput)) {
        return registration;
    }
    if (auto* mux = dynamic_cast<FileMuxNode*>(&runtime)) {
        auto target = mux->generationPurgeTarget();
        if (target) {
            return MediaRuntimeGenerationPurgeRegistration{
                MediaAvGenerationParticipant::ProjectMpegTsOutput,
                {std::string(FileMuxNode::generationPurgeIdentity()),
                 std::move(target)}};
        }
    }
    return std::nullopt;
}

bool MediaRuntimeNodeFactory::supported(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::FileInput:
    case MediaNodeKind::RealtimeInput:
    case MediaNodeKind::RawRtpInput:
    case MediaNodeKind::Demux:
    case MediaNodeKind::MpegTsDemux:
    case MediaNodeKind::StreamSplit:
    case MediaNodeKind::PacketFanout:
    case MediaNodeKind::FrameRoute:
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::VideoTimestamp:
    case MediaNodeKind::HardwareTransfer:
    case MediaNodeKind::VideoFrameRate:
    case MediaNodeKind::VideoFilter:
    case MediaNodeKind::VideoEncode:
    case MediaNodeKind::AudioCodecResolver:
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
    case MediaNodeKind::AudioEncode:
    case MediaNodeKind::PacketSourceConfig:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::PacketStartGate:
    case MediaNodeKind::RtpClockGroup:
    case MediaNodeKind::RtpPacketClockBinder:
    case MediaNodeKind::DemuxPacketClockBinder:
    case MediaNodeKind::RtpClockSnapshotFanout:
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvOutputScheduler:
    case MediaNodeKind::VideoOutputScheduler:
    case MediaNodeKind::PlaybackEpochBinder:
    case MediaNodeKind::CanonicalInput:
    case MediaNodeKind::LockedPacketGate:
    case MediaNodeKind::AvBoundReleaseExtractor:
    case MediaNodeKind::ActivatedStartupReleaseSequencer:
    case MediaNodeKind::RtpSourceClockStateAdapter:
    case MediaNodeKind::AvStartupClock:
    case MediaNodeKind::SourceClockStateFanout:
    case MediaNodeKind::AudioDriftController:
    case MediaNodeKind::EncodedAudioCanonicalizer:
    case MediaNodeKind::ScheduledOutputRouter:
    case MediaNodeKind::ProjectMpegTsPlanSource:
    case MediaNodeKind::ScheduledTsAccessUnitAdapter:
    case MediaNodeKind::PacketMerge:
    case MediaNodeKind::FileMux:
    case MediaNodeKind::RtpMux:
    case MediaNodeKind::FileOutput:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
    case MediaNodeKind::ScheduledRtpSender:
    case MediaNodeKind::RtpSdpPublisher:
    case MediaNodeKind::MpegTsRtpSdpPublisher:
    case MediaNodeKind::EofBarrier:
    case MediaNodeKind::Flush:
    case MediaNodeKind::Finalize:
    case MediaNodeKind::ControlSignal:
    case MediaNodeKind::CodecResolver:
    case MediaNodeKind::MetadataProbe:
    case MediaNodeKind::DebugDump:
    case MediaNodeKind::TraceProbe:
        return true;
    default:
        return false;
    }
}

} // namespace media::ffmpeg::graph
