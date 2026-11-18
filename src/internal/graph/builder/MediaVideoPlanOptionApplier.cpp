#include "internal/graph/builder/MediaVideoPlanOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/codec/MediaEncoderRateControlOptionAdapter.h"
#include "internal/graph/builder/codec/MediaVideoDecoderPlanOptionCodec.h"
#include "internal/graph/builder/codec/MediaVideoEncoderPlanOptionCodec.h"
#include "internal/graph/nodes/video/MediaVideoFilterExecutionPlanCodec.h"
#include "internal/graph/nodes/video/MediaVideoFrameReadinessPlanCodec.h"

#include <array>
#include <initializer_list>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaVideoPlanOptionApplier";

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

const char* transferDirectionName(MediaHardwareTransferDirection direction) noexcept
{
    switch (direction) {
    case MediaHardwareTransferDirection::Unknown:
        return "unknown";
    case MediaHardwareTransferDirection::None:
        return "none";
    case MediaHardwareTransferDirection::Upload:
        return "upload";
    case MediaHardwareTransferDirection::Download:
        return "download";
    case MediaHardwareTransferDirection::Map:
        return "map";
    case MediaHardwareTransferDirection::Unmap:
        return "unmap";
    }
    return "unknown";
}

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setFrameContractOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const std::string& prefix,
    const std::optional<MediaHardwareDescriptor>& contract)
{
    if (auto status = setOption(graph, nodeId, prefix + ".present", boolOption(contract.has_value())); !status) return status;
    if (!contract) {
        return ::media::Result<void>::success();
    }
    if (auto status = setOption(graph, nodeId, prefix + ".device", mediaHardwareDeviceKindName(contract->deviceKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(contract->frameKind)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".device_name", contract->deviceName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".pixel_format", contract->pixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".surface_pixel_format", contract->surfacePixelFormat); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".frames_context_name", contract->framesContextName); !status) return status;
    if (!contract->size.isValid()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaVideoPlanOptionApplier requires valid frame-contract dimensions"));
    }
    if (auto status = setOption(graph, nodeId, prefix + ".width", std::to_string(contract->size.width)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".height", std::to_string(contract->size.height)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".transfer_direction", transferDirectionName(contract->transferDirection)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".zero_copy", boolOption(contract->zeroCopyPreferred)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".requires_hw_device_ctx", boolOption(contract->requiresHardwareDeviceContext)); !status) return status;
    return setOption(graph, nodeId, prefix + ".requires_hw_frames_ctx", boolOption(contract->requiresHardwareFramesContext));
}

::media::Result<void> setStageOptions(MediaGraph& graph,
                                       MediaNodeId nodeId,
                                       const std::string& prefix,
                                       const MediaPipelineStagePlan& stage)
{
    if (auto status = setOption(graph, nodeId, prefix + ".component", stage.componentName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".codec", stage.codecName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".ffmpeg", stage.ffmpegName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".filter", stage.filterName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hwaccel", stage.hwaccelName); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".device", mediaHardwareDeviceKindName(stage.deviceKind())); !status) return status;
    const auto* contract = stage.frameContract();
    if (auto status = setOption(graph, nodeId, prefix + ".frame_kind", mediaHardwareFrameKindName(contract ? contract->frameKind : MediaHardwareFrameKind::Unknown)); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".hardware", boolOption(stage.hardware())); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".zero_copy", boolOption(stage.zeroCopy())); !status) return status;
    if (auto status = setOption(graph, nodeId, prefix + ".priority", std::to_string(stage.priority)); !status) return status;
    if (auto status = setFrameContractOptions(graph, nodeId, prefix + ".input", stage.inputFrame); !status) return status;
    return setFrameContractOptions(graph, nodeId, prefix + ".output", stage.outputFrame);
}

::media::Result<void> setChainOptions(MediaGraph& graph,
                                      MediaNodeId nodeId,
                                      const MediaPipelineChainPlan& chain)
{
    if (auto status = setOption(graph, nodeId, "pipeline.chain", chain.label); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.score", std::to_string(chain.score)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.zero_copy", boolOption(chain.zeroCopy)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.all_hardware", boolOption(chain.allHardware)); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.same_hardware_device", boolOption(chain.sameHardwareDevice)); !status) return status;
    return setOption(graph, nodeId, "pipeline.reason", chain.reason);
}

::media::Result<void> setFullPlanOptions(MediaGraph& graph,
                                         MediaNodeId nodeId,
                                         const MediaPipelinePlan& plan)
{
    const MediaPipelineChainPlan& chain = plan.selected;
    if (auto status = setChainOptions(graph, nodeId, chain); !status) return status;
    if (auto status = setOption(graph, nodeId, "pipeline.filter_active", boolOption(plan.filterActive)); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "decoder.pipeline", chain.decoder); !status) return status;
    if (auto status = setStageOptions(graph, nodeId, "filter.pipeline", chain.filter); !status) return status;
    return setStageOptions(graph, nodeId, "encoder.pipeline", chain.encoder);
}

::media::Result<void> applyOptions(MediaGraph& graph, MediaNodeId node,
                                  const MediaNodeOptions& options)
{
    for (const auto& [key, value] : options.values()) {
        if (auto status = setOption(graph, node, key, value); !status) return status;
    }
    return ::media::Result<void>::success();
}
// The resolver owns codec-open readback; the decode node owns the registry.
// Both consumers receive the same planner-encoded retention contract.
::media::Result<void> applyDecoderContracts(MediaGraph& graph,
    const MediaVideoTranscodeBranchNodes& nodes, const MediaNodeOptions& options)
{
    if (auto status = applyOptions(graph, nodes.codecResolver, options); !status) return status;
    // A shared-decoder output branch has no local decode node.
    if (!nodes.videoDecode.isValid()) return ::media::Result<void>::success();
    for (const auto& [key, value] : options.values()) {
        if (key.starts_with("decoder.opaque_retention.")) {
            if (auto status = setOption(graph, nodes.videoDecode, key, value); !status) return status;
        }
    }
    return ::media::Result<void>::success();
}

::media::Result<void> applyDuplicationBound(
    MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
    const std::optional<MediaRational>& maximumDuplicationGap)
{
    if (nodes.videoFrameRate.isValid()) {
        if (auto status = setOption(graph, nodes.videoFrameRate,
                "video.framerate.bound_duplication_gap",
                boolOption(maximumDuplicationGap.has_value())); !status) return status;
        if (maximumDuplicationGap) {
            const auto gap = *maximumDuplicationGap;
            if (!gap.isKnown() || gap.num <= 0 || gap.den <= 0) {
                return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
                    "Video frame-rate duplication gap contract is invalid"));
            }
            if (auto status = setOption(graph, nodes.videoFrameRate,
                    "video.framerate.maximum_duplication_gap_num",
                    std::to_string(gap.num)); !status) return status;
            if (auto status = setOption(graph, nodes.videoFrameRate,
                    "video.framerate.maximum_duplication_gap_den",
                    std::to_string(gap.den)); !status) return status;
        }
    }
    return ::media::Result<void>::success();
}

::media::Result<void> applySourceStageFacts(
    MediaGraph& graph, std::initializer_list<MediaNodeId> nodes,
    const MediaVideoSourcePlan& plan, const MediaNodeOptions& decoder)
{
    for (const auto id : nodes) {
        if (!id.isValid()) continue;
        if (auto status = setStageOptions(graph, id, "decoder.pipeline", plan.decoder); !status) return status;
        if (auto status = setStageOptions(graph, id, "filter.pipeline", plan.filter); !status) return status;
        for (const auto& [key, value] : decoder.values()) {
            if (key.starts_with("decoder.pipeline.input_retention.")) {
                if (auto status = setOption(graph, id, key, value); !status) return status;
            }
        }
    }
    return ::media::Result<void>::success();
}

::media::Result<void> applyDecoderPolling(
    MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
    const MediaVideoSourcePlan& source)
{
    if (nodes.videoDecode.isValid()) {
        if (auto status = setOption(graph, nodes.videoDecode,
                "video_decode.poll_output", boolOption(source.decoderReceiveInterval.has_value()));
            !status) return status;
        if (source.decoderReceiveInterval) {
            if (auto status = setOption(graph, nodes.videoDecode,
                    "video_decode.receive_interval_ns",
                    std::to_string(source.decoderReceiveInterval->nanoseconds()));
                !status) return status;
        }
    }

    return ::media::Result<void>::success();
}

::media::Result<void> applySourceExecution(
    MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
    const MediaVideoSourcePlan& source, MediaVideoFrameLineageProduct frameProduct)
{
    if (source.transferDirection == MediaHardwareTransferDirection::Unknown) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaVideoPlanOptionApplier requires planner-selected transfer direction"));
    }
    if (nodes.hardwareTransfer.isValid()) if (auto status = setOption(graph, nodes.hardwareTransfer, "transfer.direction", transferDirectionName(source.transferDirection)); !status) return status;
    if (nodes.videoFilter.isValid()) {
        if (source.filterReadiness) {
            auto readiness = MediaVideoFrameReadinessPlanCodec::encode(*source.filterReadiness);
            if (!readiness) return ::media::Result<void>::failure(readiness.error());
            if (auto status = applyOptions(graph, nodes.videoFilter, readiness.value()); !status) return status;
        }
        if (source.filterImplementation == MediaVideoFilterImplementation::Unknown ||
            source.filterImplementation == MediaVideoFilterImplementation::None) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaVideoPlanOptionApplier requires an active planner filter implementation"));
        }
        if (!source.filterExecution || !source.filterExecution->valid() ||
            source.filterExecution->frameProduct != frameProduct ||
            source.filterExecution->output.filterDescription != source.filter.filterName)
            return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
                "Source filter execution differs from its planner contract"));
        if (auto status = MediaVideoPlanOptionApplier::applyFilterExecutionPlan(graph, nodes.videoFilter, *source.filterExecution); !status) return status;
        if (auto status = setOption(graph, nodes.videoFilter, MediaTranscodeOptionKey::PlannedFilter, source.filter.filterName); !status) return status;
        if (auto status = setOption(graph, nodes.videoFilter, "filter.name", source.filter.filterName); !status) return status;
        if (auto status = setOption(graph, nodes.videoFilter, "filter.hwaccel", source.filter.hwaccelName); !status) return status;
        if (auto status = setOption(
                graph, nodes.videoFilter, "filter.pipeline.implementation",
                mediaVideoFilterImplementationName(source.filterImplementation));
            !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace

::media::Result<void> MediaVideoPlanOptionApplier::applySelectedPlan(
    MediaGraph& graph,
    const MediaVideoTranscodeBranchNodes& nodes,
    const MediaPipelinePlan& plan)
{
    if (plan.branchMode != MediaBranchMode::TranscodeFrame) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::unsupported("MediaVideoPlanOptionApplier requires transcode_frame video branch"));
    }

    const MediaPipelineChainPlan& chain = plan.selected;
    auto decoderOptions = MediaVideoDecoderPlanOptionCodec::encode(chain.decoder, std::nullopt);
    if (!decoderOptions) return ::media::Result<void>::failure(decoderOptions.error());
    if (auto status = applyDuplicationBound(graph, nodes, plan.maximumFrameDuplicationGap); !status) return status;
    std::vector<MediaNodeId> plannedNodes {
        nodes.codecResolver,
        nodes.videoDecode,
        nodes.hardwareTransfer,
        nodes.videoFrameRate,
        nodes.videoFilter,
        nodes.videoEncode,
    };
    if (nodes.videoTimestamp.isValid()) {
        plannedNodes.push_back(nodes.videoTimestamp);
    }

    if (auto status = applyDecoderPolling(graph, nodes, chain); !status) return status;

    for (MediaNodeId nodeId : plannedNodes) {
        if (!nodeId.isValid()) {
            continue;
        }
        if (auto status = setFullPlanOptions(graph, nodeId, plan); !status) return status;
        // Preserve the existing decoder retention diagnostics on every planned node.
        for (const auto& [key, value] : decoderOptions.value().values()) {
            if (key.starts_with("decoder.pipeline.input_retention.")) {
                if (auto status = setOption(graph, nodeId, key, value); !status) return status;
            }
        }
    }

    if (auto status = applyDecoderContracts(graph, nodes, decoderOptions.value()); !status) return status;
    if (auto status = setOption(graph, nodes.codecResolver, MediaTranscodeOptionKey::VideoCodec, plan.outputCodecName); !status) return status;
    if (auto status = applyEncoderPlan(graph, nodes, chain.encoder, chain.encoderAbortPolicy);
        !status) return status;
    if (nodes.sourceCopy.isValid()) {
        if (!plan.sharedSource || !plan.sharedSource->copy ||
            plan.sharedSource->copyImplementation == MediaVideoFilterImplementation::Unknown ||
            plan.sharedSource->copyImplementation == MediaVideoFilterImplementation::None)
            return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument("source copy lacks its planned filter implementation"));
        if (auto status = setFrameContractOptions(graph, nodes.sourceCopy, "filter.pipeline.input", chain.decoder.outputFrame); !status) return status;
        if (auto status = setFrameContractOptions(graph, nodes.sourceCopy, "filter.pipeline.output", chain.decoder.outputFrame); !status) return status;
        if (auto status = setOption(graph, nodes.sourceCopy, "filter.pipeline.filter", plan.sharedSource->copy->output.filterDescription); !status) return status;
        if (auto status = setOption(graph, nodes.sourceCopy, "filter.pipeline.implementation",
                mediaVideoFilterImplementationName(plan.sharedSource->copyImplementation)); !status) return status;
    }
    if (auto status = applySourceExecution(graph, nodes, chain, MediaVideoFrameLineageProduct::CanonicalInterval); !status) return status;
    if (nodes.videoTimestamp.isValid()) {
        if (auto status = setOption(graph, nodes.videoTimestamp, MediaTranscodeOptionKey::VideoSynthesizeMissingTimestamps, boolOption(plan.synthesizeMissingTimestamps)); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaVideoPlanOptionApplier::applyEncoderPlan(
    MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
    const MediaPipelineStagePlan& encoder, MediaVideoEncoderAbortPolicy abortPolicy)
{
    auto encoded = MediaVideoEncoderPlanOptionCodec::encode(encoder);
    if (!encoded) return ::media::Result<void>::failure(encoded.error());
    if (auto status = applyOptions(graph, nodes.codecResolver, encoded.value()); !status) return status;
    if (auto status = setStageOptions(graph, nodes.codecResolver, "encoder.pipeline", encoder); !status) return status;
    if (!nodes.videoEncode.isValid()) return ::media::Result<void>::success();
    if (abortPolicy == MediaVideoEncoderAbortPolicy::Unknown)
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "MediaVideoPlanOptionApplier requires planner encoder abort policy"));
    if (auto status = setStageOptions(graph, nodes.videoEncode, "encoder.pipeline", encoder); !status) return status;
    auto rateControl = MediaEncoderRateControlOptionAdapter::encode(*encoder.encoderRateControl);
    if (auto status = applyOptions(graph, nodes.videoEncode, rateControl); !status) return status;
    if (auto status = setOption(graph, nodes.videoEncode, "video_encode.abort_policy",
            mediaVideoEncoderAbortPolicyName(abortPolicy)); !status) return status;
    return setOption(graph, nodes.videoEncode, MediaTranscodeOptionKey::PlannedEncoder, encoder.ffmpegName);
}

::media::Result<void> MediaVideoPlanOptionApplier::applySourceDecoderPlan(
    MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
    const MediaVideoSourcePlan& plan, int sourceStreamIndex)
{
    if (!plan.available || sourceStreamIndex < 0 ||
        !nodes.codecResolver.isValid() || !nodes.videoDecode.isValid())
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Video source decoder requires its planned resolver and decoder"));
    auto decoder = MediaVideoDecoderPlanOptionCodec::encode(plan.decoder, std::nullopt);
    if (!decoder) return ::media::Result<void>::failure(decoder.error());
    if (auto status = applyDecoderContracts(graph, nodes, decoder.value()); !status) return status;
    if (auto status = applySourceStageFacts(graph, {nodes.codecResolver, nodes.videoDecode},
            plan, decoder.value()); !status) return status;
    if (plan.decoderReadiness) {
        if (!mediaVideoFrameProducerMatchesRole(*plan.decoderReadiness, MediaVideoFrameProducerRole::Decoder))
            return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
                "Decoder readiness does not describe its admitted producer operation"));
        auto readiness = MediaVideoFrameReadinessPlanCodec::encode(*plan.decoderReadiness);
        if (!readiness) return ::media::Result<void>::failure(readiness.error());
        if (auto status = applyOptions(graph, nodes.videoDecode, readiness.value()); !status) return status;
    }
    return applyDecoderPolling(graph, nodes, plan);
}

::media::Result<void> MediaVideoPlanOptionApplier::applySourceFramePlan(
    MediaGraph& graph, const MediaVideoTranscodeBranchNodes& nodes,
    const MediaVideoSourcePlan& plan, MediaVideoSourceTiming timing,
    const std::optional<MediaRational>& maximumDuplicationGap)
{
    const auto* cadence = std::get_if<MediaObservedVideoCadence>(&timing);
    if (!plan.available || !nodes.hardwareTransfer.isValid() ||
        plan.transferDirection == MediaHardwareTransferDirection::Unknown ||
        (cadence ? (cadence->rate.num <= 0 || cadence->rate.den <= 0 ||
                    !nodes.videoFrameRate.isValid())
                 : (nodes.videoFrameRate.isValid() || maximumDuplicationGap.has_value())) ||
        (maximumDuplicationGap && (maximumDuplicationGap->num <= 0 || maximumDuplicationGap->den <= 0)))
        return ::media::Result<void>::failure(::media::ErrorInfo::invalidArgument(
            "Video source frame processing differs from its planned timing product"));
    auto decoder = MediaVideoDecoderPlanOptionCodec::encode(plan.decoder, std::nullopt);
    if (!decoder) return ::media::Result<void>::failure(decoder.error());
    if (auto status = applySourceStageFacts(graph, {nodes.hardwareTransfer, nodes.videoFrameRate, nodes.videoFilter},
            plan, decoder.value()); !status) return status;
    if (cadence) {
        if (auto status = setOption(graph, nodes.videoFrameRate, MediaTranscodeOptionKey::VideoFpsNum, std::to_string(cadence->rate.num)); !status) return status;
        if (auto status = setOption(graph, nodes.videoFrameRate, MediaTranscodeOptionKey::VideoFpsDen, std::to_string(cadence->rate.den)); !status) return status;
        if (auto status = applyDuplicationBound(graph, nodes, maximumDuplicationGap); !status) return status;
    }
    return applySourceExecution(graph, nodes, plan, cadence
        ? MediaVideoFrameLineageProduct::CanonicalInterval : MediaVideoFrameLineageProduct::SourceDisplayPoint);
}

::media::Result<void> MediaVideoPlanOptionApplier::applyFilterExecutionPlan(
    MediaGraph& graph, MediaNodeId node, const MediaVideoFilterExecutionPlan& plan)
{
    auto encoded = MediaVideoFilterExecutionPlanCodec::encode(plan);
    if (!encoded) return ::media::Result<void>::failure(encoded.error());
    for (const auto& [key, value] : encoded.value().values()) {
        if (auto status = setOption(graph, node, key, value); !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
