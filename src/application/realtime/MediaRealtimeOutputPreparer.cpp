#include "application/realtime/MediaRealtimeOutputPreparer.h"

#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"
#include "internal/graph/planner/capability/MediaEncoderEmissionPreflightAdapter.h"
#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/validation/MediaRealtimeVideoGraphShapeValidator.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedRealtimeOutput> MediaRealtimeOutputPreparer::prepare(
    const MediaRealtimeOutputPreparationRequest& request)
{
    using Result = ::media::Result<MediaPreparedRealtimeOutput>;
    const auto& snapshot = request.sourceSnapshot;
    if (request.prefix.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "output preparation requires a nonempty branch identity"));
    if (request.sourceGeneration == 0) return Result::failure(::media::ErrorInfo::notInitialized(
        "output preparation requires the active playback session epoch"));
    if (snapshot.streamKind != MediaStreamKind::Video ||
        snapshot.index != request.sessionPlan.videoPlan.sourceStreamIndex) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "output source snapshot stream identity differs from the running video session"));
    }
    const auto& source = request.sessionPlan.preparedVideoSource;
    if (source.streamIndex != snapshot.index || source.width <= 0 || source.height <= 0 ||
        !source.frameRate.isKnown() || !request.sessionPlan.sourceTimeBase.isKnown()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "output preparation lacks authoritative prepared source geometry, cadence or time base"));
    }
    const auto sameRate = [](MediaRational left, MediaRational right) {
        return av_cmp_q(AVRational{left.num, left.den}, AVRational{right.num, right.den}) == 0;
    };
    if ((snapshot.time.frameRate.isKnown() && !sameRate(snapshot.time.frameRate, source.frameRate)) ||
        (snapshot.time.timeBase.isKnown() &&
         !sameRate(snapshot.time.timeBase, request.sessionPlan.sourceTimeBase))) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "source snapshot timing contradicts its prepared session contract"));
    }
    auto codec = snapshot.cloneCodecParameters();
    if (!codec) return Result::failure(codec.error());
    if (request.sessionPlan.videoPlan.inputCodecName != avcodec_get_name(codec.value()->codec_id) ||
        (codec.value()->width > 0 && codec.value()->width != source.width) ||
        (codec.value()->height > 0 && codec.value()->height != source.height)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "source codec snapshot contradicts prepared decoder identity or geometry"));
    }
    // Raw RTP metadata carries codec/extradata and transport time base. Its
    // geometry and cadence come from the same prepared facts used to open the
    // shared decoder, not from an optional AVStream frame-rate guess.
    codec.value()->width = source.width;
    codec.value()->height = source.height;
    MediaTimeDescriptor sourceTime = snapshot.time;
    sourceTime.timeBase = request.sessionPlan.sourceTimeBase;
    sourceTime.frameRate = source.frameRate;
    MediaFormatDescriptor sourceFormat = snapshot.format;
    sourceFormat.video.size = {source.width, source.height};
    sourceFormat.video.frameRate = source.frameRate;
    sourceFormat.time = sourceTime;
    auto identity = request.sessionRequest;
    identity.mediaId += ":" + request.prefix;
    for (const auto& group : request.groups) {
        if (!group.witness || group.groupId == 0) return Result::failure(
            ::media::ErrorInfo::invalidArgument("Existing encoding group has no immutable witness"));
        const auto& witness = *group.witness;
        if (witness.actual.sourceFanout != request.sharedDecode.frame.node ||
            witness.actual.sourceGeneration != request.sourceGeneration ||
            witness.actual.sourceStreamIndex != source.streamIndex ||
            witness.actual.sourceTimeBase != request.sessionPlan.sourceTimeBase ||
            witness.actual.sourceFrameRate != source.frameRate ||
            (witness.sourceFramesOwner ? witness.sourceFramesOwner->data : nullptr) !=
                (request.liveFrames ? request.liveFrames->data : nullptr)) continue;
        auto matched = MediaRealtimeRtpTranscodePlanner::matchesEncodingRequest(
            request.output, identity, source, witness.pipeline);
        if (!matched) return Result::failure(matched.error());
        if (!matched.value()) continue;
        auto planned = MediaRealtimeRtpTranscodePlanner::planOutputForEncodingGroup(
            request.output, identity, request.sessionPlan, source,
            request.sourceGeneration, witness.pipeline);
        if (!planned) return Result::failure(planned.error());
        auto output = MediaRealtimeRtpTranscodeGraphBuilder::appendProtocolOutput(
            request.graph, planned.value(), request.prefix, group.encoded);
        if (!output) return Result::failure(output.error());
        if (auto status = MediaRealtimeVideoGraphShapeValidator::validateOutputBranch(
                *output.value().graph, output.value().nodeIds, request.sessionPlan,
                std::get<MediaRealtimeVideoRuntimePlan>(planned.value().runtime)); !status)
            return Result::failure(status.error());
        return Result::success({std::move(planned).value(), std::move(output).value(),
            MediaRealtimeExistingEncodingGroup{group.groupId}});
    }
    MediaHardwareCapabilityProbe probe([&](MediaPipelineChainPlan& candidate,
                                          const MediaPipelinePlannerOptions& options) {
        return MediaHardwareCapabilityProbe::validateOutputBranch(
            candidate, options, request.liveFrames, request.decoderFacts);
    });
    auto planned = MediaRealtimeRtpTranscodePlanner::planOutputBranch(
        request.output, identity, request.sessionPlan, source,
        request.sourceGeneration, probe);
    if (!planned) return Result::failure(planned.error());
    auto branch = MediaRealtimeRtpTranscodeGraphBuilder::appendEncodingGroup(
        request.graph, planned.value(), request.prefix,
        request.formatSource, request.sharedDecode);
    if (!branch) return Result::failure(branch.error());
    auto output = MediaRealtimeRtpTranscodeGraphBuilder::appendProtocolOutput(
        branch.value().graph, planned.value(), request.prefix, branch.value().segment.encoded);
    if (!output) return Result::failure(output.error());
    if (auto status = MediaRealtimeVideoGraphShapeValidator::validateOutputBranch(
            *output.value().graph, output.value().nodeIds,
            request.sessionPlan,
            std::get<MediaRealtimeVideoRuntimePlan>(planned.value().runtime)); !status) {
        return Result::failure(status.error());
    }
    if (branch.value().segment.nodeIds.empty()) return Result::failure(
        ::media::ErrorInfo::internalError("prepared output contains no executable nodes"));
    const auto* resolver = branch.value().graph.findNode(branch.value().segment.nodeIds.front());
    if (!resolver || resolver->kind != MediaNodeKind::CodecResolver) return Result::failure(
        ::media::ErrorInfo::internalError("prepared output has no encoder resolver"));
    AVBufferRef* device = nullptr;
    if (request.liveFrames) {
        if (!request.liveFrames->data) return Result::failure(
            ::media::ErrorInfo::invalidArgument("running decoder frames reference has no context"));
        device = reinterpret_cast<AVHWFramesContext*>(request.liveFrames->data)->device_ref;
    }
    CodecResolverEncoderContextBuildRequest encoderRequest;
    encoderRequest.codecParameters = codec.value().get();
    encoderRequest.sourceFormat = sourceFormat;
    encoderRequest.sourceTime = sourceTime;
    encoderRequest.options = &resolver->options;
    encoderRequest.hardwareDevice = device;
    auto encoder = CodecResolverEncoderContextBuilder::build(encoderRequest);
    if (!encoder) return Result::failure(encoder.error());
    const auto& stage = planned.value().videoPlan.selected.encoder;
    if (!stage.encoderRateControl || !stage.preparedEmission ||
        !stage.encodedPacketLayout || !stage.encoderOpenContract) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "prepared output lacks its encoder emission contract"));
    }
    auto readback = MediaEncoderEmissionPreflightAdapter::readAfterOpen(
        *encoder.value().context, *stage.encoderRateControl,
        stage.encoderOpenContract->frameRate, *stage.encodedPacketLayout,
        "retained-output-encoder:" + stage.ffmpegName,
        stage.preparedEmission->backend);
    if (!readback) return Result::failure(readback.error());
    const auto& actual = readback.value();
    const auto& admitted = *stage.preparedEmission;
    if (actual.maximumAccessUnitPayloadBytes > admitted.maximumAccessUnitPayloadBytes ||
        actual.maximumBurstPayloadBytes > admitted.maximumBurstPayloadBytes ||
        actual.maximumEncoderRetainedFrames > admitted.maximumEncoderRetainedFrames ||
        actual.peakPayloadBytesPerSecond > admitted.peakPayloadBytesPerSecond ||
        actual.sustainedPayloadBytesPerSecond != admitted.sustainedPayloadBytesPerSecond) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "retained output encoder exceeds its planned emission or retention envelope"));
    }
    auto encoderReadback = MediaVideoEncoderReadback::capture(*encoder.value().context);
    if (!encoderReadback) return Result::failure(encoderReadback.error());
    auto encodingContract = MediaRealtimeVideoEncodingGroupContractPlanner::plan(
        planned.value().videoPlan, request.sharedDecode.frame.node,
        request.sourceGeneration, request.sessionPlan.sourceTimeBase,
        source.frameRate, encoderReadback.value());
    if (!encodingContract) return Result::failure(encodingContract.error());
    auto wrapped = FFmpegBufferFactory::wrapCodecContext(std::move(encoder).value().context);
    if (!wrapped) return Result::failure(wrapped.error());
    auto join = MediaRealtimeVideoEncodingGroupContractPlanner::joinPlan(planned.value().videoPlan);
    if (!join) return Result::failure(join.error());
    ::media::ffmpeg::BufferRefPtr sourceFrames(request.liveFrames ? av_buffer_ref(request.liveFrames) : nullptr);
    if (request.liveFrames && !sourceFrames) return Result::failure(::media::ErrorInfo::allocationFailed("Encoding source frame context identity"));
    auto witness = std::make_shared<const MediaRealtimeVideoEncodingWitness>(
        MediaRealtimeVideoEncodingWitness{planned.value().videoPlan, std::move(encodingContract).value(),
            std::move(sourceFrames), std::move(join).value()});
    return Result::success(MediaPreparedRealtimeOutput{
        std::move(planned).value(), std::move(output).value(),
        MediaRealtimeNewEncodingGroup{std::move(branch.value().segment), std::move(wrapped).value(), std::move(witness)}});
}

} // namespace media::ffmpeg::graph
