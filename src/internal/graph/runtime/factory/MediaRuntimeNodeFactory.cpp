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
#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/nodes/mux/RtpMuxNode.h"
#include "internal/graph/nodes/output/FileOutputNode.h"
#include "internal/graph/nodes/output/RtpOutputNode.h"
#include "internal/graph/nodes/output/SdpWriterNode.h"
#include "internal/graph/nodes/packet/AvPacketStartBarrierNode.h"
#include "internal/graph/nodes/packet/PacketNormalizeNode.h"
#include "internal/graph/nodes/packet/PacketSourceConfigNode.h"
#include "internal/graph/nodes/packet/PacketStartGateNode.h"
#include "internal/graph/nodes/route/FrameRouteNode.h"
#include "internal/graph/nodes/split/PacketFanoutNode.h"
#include "internal/graph/nodes/sync/MediaRtpClockGroupNode.h"
#include "internal/graph/nodes/sync/MediaRtpPacketClockBinderNode.h"
#include "internal/graph/nodes/sync/MediaRtpClockSnapshotFanoutNode.h"
#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h"
#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNodePreparation.h"
#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"
#include "internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/video/HardwareTransferNode.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoEncodeNode.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/nodes/video/VideoFrameRateNode.h"
#include "internal/graph/nodes/video/VideoTimestampNode.h"
#include "internal/graph/sync/lineage/MediaVideoLineageStagePreparation.h"
#include "internal/graph/sync/lineage/MediaVideoFrameRateState.h"

namespace media::ffmpeg::graph {
namespace {

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
    const MediaNode& node)
{
    auto capacity = prepareMediaVideoLineageStageCapacity(
        node, VideoFrameRateNode::generationPurgeIdentity());
    if (!capacity) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            capacity.error());
    }
    if (capacity.value()) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<VideoFrameRateNode>(
                node.id, std::make_shared<MediaVideoFrameRateState>(true)));
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
    return create(node, nullptr);
}

::media::Result<std::unique_ptr<MediaRuntimeNode>> MediaRuntimeNodeFactory::create(
    const MediaNode& node,
    MediaPreparedRealtimeInputBinding* binding)
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
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<RawRtpInputNode>(node.id));
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
        return createVideoFrameRateStage(node);
    case MediaNodeKind::VideoFilter:
        return createVideoLineageStage<VideoFilterNode>(node);
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
    case MediaNodeKind::AvPacketStartBarrier:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<AvPacketStartBarrierNode>(node.id));
    case MediaNodeKind::PacketStartGate:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<PacketStartGateNode>(node.id));
    case MediaNodeKind::RtpClockGroup:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<MediaRtpClockGroupNode>(node.id));
    case MediaNodeKind::RtpPacketClockBinder:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaRtpPacketClockBinderNode>(node.id));
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
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAvOutputSchedulerNode>(node.id));
    case MediaNodeKind::PlaybackEpochBinder:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::notInitialized(
                "PlaybackEpochBinder requires compiler-issued activation authority"));
    case MediaNodeKind::CanonicalInput:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaCanonicalInputNode>(node.id));
    case MediaNodeKind::AvBoundReleaseExtractor:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAvBoundReleaseExtractorNode>(node.id));
    case MediaNodeKind::PacketMerge:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<PacketMergeNode>(node.id));
    case MediaNodeKind::FileMux:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<FileMuxNode>(node.id));
    case MediaNodeKind::RtpMux:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<RtpMuxNode>(node.id));
    case MediaNodeKind::FileOutput:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<FileOutputNode>(node.id));
    case MediaNodeKind::RtpOutput:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<RtpOutputNode>(node.id));
    case MediaNodeKind::SdpWriter:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<SdpWriterNode>(node.id));
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
MediaRuntimeNodeFactory::createPlaybackEpochBinder(
    const MediaNode& node,
    MediaPlaybackEpochActivationCapability capability)
{
    if (node.kind != MediaNodeKind::PlaybackEpochBinder) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch binder factory requires the binder node kind"));
    }
    auto group = requiredNodeOption(
        &node.options, "MediaPlaybackEpochBinderNode",
        "playback_epoch_binder.sync_group");
    if (!group) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            group.error());
    }
    MediaAvSyncGroupKey groupKey(std::move(group).value());
    if (!groupKey.valid()) {
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch binder requires a valid planned sync group"));
    }
    return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
        std::make_unique<MediaPlaybackEpochBinderNode>(
            node.id, std::move(groupKey), std::move(capability)));
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
    case MediaNodeKind::AudioResample:
    case MediaNodeKind::AudioEncode:
    case MediaNodeKind::PacketSourceConfig:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::AvPacketStartBarrier:
    case MediaNodeKind::PacketStartGate:
    case MediaNodeKind::RtpClockGroup:
    case MediaNodeKind::RtpPacketClockBinder:
    case MediaNodeKind::RtpClockSnapshotFanout:
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvOutputScheduler:
    case MediaNodeKind::PlaybackEpochBinder:
    case MediaNodeKind::CanonicalInput:
    case MediaNodeKind::AvBoundReleaseExtractor:
    case MediaNodeKind::PacketMerge:
    case MediaNodeKind::FileMux:
    case MediaNodeKind::RtpMux:
    case MediaNodeKind::FileOutput:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
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
