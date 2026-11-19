#include "internal/graph/builder/codec/MediaDecoderInputRetentionOptionCodec.h"
#include "internal/graph/planner/realtime/MediaRtpSourceFirstFrameProbe.h"
#include "internal/graph/builder/codec/MediaVideoDecoderPlanOptionCodec.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include <new>

namespace media::ffmpeg::graph {

::media::Status MediaRtpSourceFirstFrameProbe::restartDecoder(
    MediaRtpSourceFirstFrameEvidence& evidence, const FFmpegInputStreamSnapshot& source,
    const MediaVideoSourcePlan& plan, const MediaPreparationControl& control)
try {
    if (auto status = control.check("RTP decoder replay preparation"); !status) return status;
    if (!evidence.decoder.context || !evidence.storage || !source.codecParametersComplete() ||
        !plan.decoder.outputFrame || !plan.decoder.preparedInputRetention)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP replay requires the admitted decoder, source and retention plan"));
    auto options = MediaVideoDecoderPlanOptionCodec::encode(plan.decoder, std::nullopt);
    if (!options) return ::media::Status::failure(options.error());
    const auto facts = std::move(evidence.decoder.runtimeFacts);
    const auto threads = evidence.decoder.context->thread_count;
    const auto threadType = evidence.decoder.context->active_thread_type;
    const auto timeBase = evidence.decoder.context->pkt_timebase;
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
    const auto copyOpaque = evidence.decoder.context->flags & AV_CODEC_FLAG_COPY_OPAQUE;
#endif
    auto device = std::move(evidence.decoder.hardwareDevice);
    // Retain only the device, not a frame pool or codec-internal output queue.
    // The evidence lease outlives both the old context and a failed replacement.
    evidence.firstFrame.reset();
    evidence.decoder = {};
    evidence.identityProbe.reset();
    if (auto status = control.check("RTP decoder reopening"); !status) return status;
    auto replacement = CodecResolverDecoderContextBuilder::build({source.codec.parameters(),
        source.time, &options.value(), device.get(), plan.decoderLineagePropagation, control});
    if (!replacement) return ::media::Status::failure(replacement.error());
    auto& reopened = replacement.value();
    const auto& context = *reopened.context;
    if (reopened.runtimeFacts.decoderName != facts.decoderName ||
        reopened.runtimeFacts.hardwareAccelerationFlags != facts.hardwareAccelerationFlags ||
        reopened.runtimeFacts.suppliedHardwareFramesContext != facts.suppliedHardwareFramesContext ||
        context.thread_count != threads || context.active_thread_type != threadType ||
        context.pkt_timebase.num != timeBase.num || context.pkt_timebase.den != timeBase.den ||
        static_cast<bool>(reopened.hardwareDevice) != static_cast<bool>(device) ||
        (device && reopened.hardwareDevice->data != device->data)
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
        || (context.flags & AV_CODEC_FLAG_COPY_OPAQUE) != copyOpaque
#endif
        )
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Reopened RTP decoder differs from its prepared execution facts"));
    auto retention = MediaDecoderInputRetentionAdapter::readAfterOpen(context, plan.decoder.hwaccelName);
    if (!retention) return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "Reopened RTP decoder has no authoritative input retention"));
    if (auto status = MediaDecoderInputRetentionOptionCodec::validateOpened(context, options.value()); !status)
        return status;
    auto storage = MediaVideoDecoderPreparationStoragePlanner::plan(
        evidence.maximumAccessUnitBytes, *retention, reopened, evidence.frameContract);
    if (!storage) return ::media::Status::failure(storage.error());
    if (storage.value().packetBytes > evidence.decoderStorage.packetBytes ||
        storage.value().ownerBytes > evidence.decoderStorage.ownerBytes ||
        storage.value().softwareFrameBytes > evidence.decoderStorage.softwareFrameBytes)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Reopened RTP decoder exceeds its admitted preparation storage"));
    if (auto status = control.check("RTP decoder replay handoff"); !status) return status;
    evidence.decoder = std::move(reopened);
    evidence.decoderMayHavePendingOutput = false;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode,
        "rtp_decoder_replay stage=reopened decoder=" + facts.decoderName +
        " source_stream=" + std::to_string(source.index) +
        " prior_context_destroyed=1 same_device=1 packet_bytes=" +
        std::to_string(storage.value().packetBytes) + " owner_bytes=" +
        std::to_string(storage.value().ownerBytes));
    return ::media::Status::success();
} catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
        "RTP decoder replay preparation allocation failed"));
}

::media::Result<std::shared_ptr<MediaPreparedVideoDecoder>>
MediaRtpSourceFirstFrameProbe::prepareReplay(MediaRtpSourceFirstFrameEvidence evidence,
    const FFmpegInputStreamSnapshot& source, const MediaVideoSourcePlan& plan,
    const MediaPreparationControl& control)
{
    using Result = ::media::Result<std::shared_ptr<MediaPreparedVideoDecoder>>;
    if (evidence.disposition != MediaRtpSourceFirstFrameDisposition::FirstFrame || !evidence.firstFrame)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP replay handoff requires validated first-frame evidence"));
    if (auto status = restartDecoder(evidence, source, plan, control); !status)
        return Result::failure(status.error());
    return MediaPreparedVideoDecoder::create(std::move(evidence.decoder),
        std::move(evidence.storage), source, std::move(evidence.frameContract));
}

} // namespace media::ffmpeg::graph
