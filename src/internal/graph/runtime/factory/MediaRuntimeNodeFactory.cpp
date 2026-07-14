#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"

#include "internal/graph/nodes/audio/AudioCodecResolverNode.h"
#include "internal/graph/nodes/audio/AudioDecodeNode.h"
#include "internal/graph/nodes/audio/AudioEncodeNode.h"
#include "internal/graph/nodes/audio/AudioResampleNode.h"
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
#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h"
#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/nodes/video/HardwareTransferNode.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoEncodeNode.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/nodes/video/VideoFrameRateNode.h"
#include "internal/graph/nodes/video/VideoTimestampNode.h"

namespace media::ffmpeg::graph {

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
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<VideoDecodeNode>(node.id));
    case MediaNodeKind::VideoTimestamp:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<VideoTimestampNode>(node.id));
    case MediaNodeKind::HardwareTransfer:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<HardwareTransferNode>(node.id));
    case MediaNodeKind::VideoFrameRate:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<VideoFrameRateNode>(node.id));
    case MediaNodeKind::VideoFilter:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<VideoFilterNode>(node.id));
    case MediaNodeKind::VideoEncode:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<VideoEncodeNode>(node.id));
    case MediaNodeKind::AudioCodecResolver:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<AudioCodecResolverNode>(node.id));
    case MediaNodeKind::AudioDecode:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<AudioDecodeNode>(node.id));
    case MediaNodeKind::AudioResample:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<AudioResampleNode>(node.id));
    case MediaNodeKind::AudioEncode:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<AudioEncodeNode>(node.id));
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
    case MediaNodeKind::AvStartupCoordinator:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(std::make_unique<MediaAvStartupCoordinatorNode>(node.id));
    case MediaNodeKind::AvOutputScheduler:
        return ::media::Result<std::unique_ptr<MediaRuntimeNode>>::success(
            std::make_unique<MediaAvOutputSchedulerNode>(node.id));
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
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvOutputScheduler:
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
