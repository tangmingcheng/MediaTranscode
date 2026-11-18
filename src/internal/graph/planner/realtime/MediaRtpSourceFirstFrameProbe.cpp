#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/planner/realtime/MediaRtpSourceFirstFrameProbe.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"

#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoParameterSetValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCodecApi.h"

extern "C" {
#include <libavutil/hwcontext.h>
}

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <new>

namespace media::ffmpeg::graph {
namespace {
using Clock = std::chrono::steady_clock;
using EvidenceResult = ::media::Result<MediaRtpSourceFirstFrameEvidence>;

bool equalSignaling(const MediaRtpVideoSignalingFacts& left,
                    const MediaRtpVideoSignalingFacts& right)
{
    if (left.index() != right.index()) return false;
    if (const auto* a = std::get_if<MediaH264SignalingFacts>(&left)) {
        const auto& b = std::get<MediaH264SignalingFacts>(right);
        return a->sps == b.sps && a->pps == b.pps && a->profileLevelId == b.profileLevelId;
    }
    const auto& a = std::get<MediaHevcSignalingFacts>(left);
    const auto& b = std::get<MediaHevcSignalingFacts>(right);
    return a.vps == b.vps && a.sps == b.sps && a.pps == b.pps;
}

::media::Status validateBinding(const MediaRtpSourceFirstFrameProbePlan& plan,
                                const AVCodecContext& decoder)
{
    const auto& parameters = plan.codecParameters;
    const bool h264 = plan.signaling.codecName == "h264";
    if ((h264 && !std::holds_alternative<MediaH264SignalingFacts>(plan.signaling.facts)) ||
        (!h264 && !std::holds_alternative<MediaHevcSignalingFacts>(plan.signaling.facts)) ||
        parameters.codec_id != (h264 ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC) ||
        parameters.codec_type != AVMEDIA_TYPE_VIDEO || decoder.codec_id != parameters.codec_id ||
        decoder.width != parameters.width || decoder.height != parameters.height ||
        av_cmp_q(decoder.pkt_timebase, {plan.timeBase.num, plan.timeBase.den}) != 0 ||
        parameters.width <= 0 || parameters.height <= 0 ||
        parameters.extradata_size <= 0 || !parameters.extradata || !decoder.extradata ||
        decoder.extradata_size != parameters.extradata_size ||
        std::memcmp(decoder.extradata, parameters.extradata, parameters.extradata_size) != 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP first-frame decoder differs from the prepared codec-parameter snapshot"));
    }
    return MediaRtpVideoParameterSetValidator::validateBinding(
        {parameters.extradata, static_cast<std::size_t>(parameters.extradata_size)},
        plan.signaling.facts, true);
}

// One bounded reorder owner per pass. Arrival timestamps are replayed as
// evidence; CPU time never advances a network gap deadline.
template<class Consumer>
::media::Status visitSnapshot(const MediaRawRtpProbeLease& lease,
    const MediaRtpSourceFirstFrameProbePlan& plan,
    const MediaSourcePreparationStoragePlan& storage, Consumer&& consume)
{
    MediaRtpReorderBuffer reorder(plan.reorder);
    std::int64_t previousArrival = 0;
    for (std::size_t index = 0; index < lease.datagramCount(); ++index) {
        if (auto status = (MediaPreparationControl{plan.deadline, plan.cancellation}.check("RTP first-frame preparation")); !status) return status;
        auto borrowed = lease.datagram(index);
        if (!borrowed) return ::media::Status::failure(borrowed.error());
        const auto& datagram = borrowed.value();
        if (datagram.bytes.empty() || datagram.bytes.size() > storage.maximumDatagramBytes ||
            datagram.observedAtNs <= 0 || datagram.observedAtNs < previousArrival) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "RTP probe snapshot violates datagram capacity or arrival ordering"));
        }
        previousArrival = datagram.observedAtNs;
        const auto arrival = Clock::time_point(std::chrono::duration_cast<Clock::duration>(
            std::chrono::nanoseconds(datagram.observedAtNs)));
        auto ordered = [&]() -> ::media::Result<MediaRtpReorderResult> {
            if (datagram.channel == MediaRtpUdpChannel::Rtcp) return reorder.expire(arrival);
            auto packet = MediaRtpPacketParser::parse(datagram.bytes);
            if (!packet) return ::media::Result<MediaRtpReorderResult>::failure(packet.error());
            if (packet.value().ssrc != plan.signaling.ssrc ||
                packet.value().payloadType != plan.signaling.payloadType) {
                return ::media::Result<MediaRtpReorderResult>::failure(
                    ::media::ErrorInfo::invalidArgument("RTP probe snapshot source identity changed"));
            }
            return reorder.push(std::move(packet).value(), arrival);
        }();
        if (!ordered) return ::media::Status::failure(ordered.error());
        if (ordered.value().packets.size() > storage.maximumReorderedPackets) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "RTP probe reorder exceeded its planned packet bound"));
        }
        auto progress = consume(ordered.value());
        if (!progress) return ::media::Status::failure(progress.error());
        if (!progress.value()) break;
    }
    return ::media::Status::success();
}

::media::Result<bool> verifySnapshotSignaling(const MediaRawRtpProbeLease& lease,
    const MediaRtpSourceFirstFrameProbePlan& plan,
    const MediaSourcePreparationStoragePlan& storage)
{
    using Result = ::media::Result<bool>;
    auto observer = MediaRtpVideoSignalingObserver::create(plan.signaling.codecName,
        plan.signaling.payloadType, plan.signaling.clockRate, plan.packetization,
        storage.maximumAccessUnitBytes);
    if (!observer) return Result::failure(observer.error());
    bool complete = false;
    auto visited = visitSnapshot(lease, plan, storage, [&](const MediaRtpReorderResult& ordered) {
        for (const auto& gap : ordered.discontinuities) {
            if (gap.reason != MediaRtpDiscontinuityReason::SequenceGap)
                return Result::failure(::media::ErrorInfo::invalidArgument("RTP probe identity discontinuity"));
            observer.value().discontinuity();
        }
        for (const auto& packet : ordered.packets) {
            auto observation = observer.value().observe(packet);
            if (!observation) return Result::failure(observation.error());
            if (observation.value().epochChanged) return Result::failure(
                ::media::ErrorInfo::invalidArgument("RTP probe signaling epoch changed"));
            complete = complete || observation.value().complete;
        }
        return Result::success(true);
    });
    if (!visited) return Result::failure(visited.error());
    if (!complete) return Result::success(false);
    auto detected = observer.value().detected(lease.datagramCount(), 0, 0);
    if (!detected) return Result::failure(detected.error());
    if (!equalSignaling(detected.value().facts, plan.signaling.facts)) return Result::failure(
        ::media::ErrorInfo::invalidArgument("RTP probe snapshot parameter sets changed"));
    return Result::success(true);
}

::media::Status validateFirstFrame(const MediaRtpSourceFirstFrameProbePlan& plan,
    const CodecResolverDecoderContextBuildResult& decoder, const AVFrame& frame)
{
    if ((frame.flags & AV_FRAME_FLAG_CORRUPT) || frame.decode_error_flags) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("RTP first-frame decoder reported damaged media"));
    auto validated = MediaVideoFrameContractValidator::validate(
        frame, *plan.source.decoder.outputFrame, "RTP first-frame evidence");
    if (!validated) return ::media::Status::failure(validated.error());
    if (frame.hw_frames_ctx || plan.source.decoder.outputFrame->requiresHardwareFramesContext) {
        auto frames = MediaVideoFrameContractValidator::validateHardwareFrames(
            frame.hw_frames_ctx, *plan.source.decoder.outputFrame, "RTP first-frame evidence");
        if (!frames) return frames;
    }
    if (frame.sample_aspect_ratio.num <= 0 || frame.sample_aspect_ratio.den <= 0 ||
        av_cmp_q(frame.sample_aspect_ratio, {plan.sampleAspectRatio.num, plan.sampleAspectRatio.den}) != 0 ||
        (plan.source.decoder.effectiveColorRange &&
         frame.color_range != plan.source.decoder.effectiveColorRange->range)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP first-frame SAR or color differs from prepared source facts"));
    }
    if (decoder.hardwareDevice) {
        const auto* frames = frame.hw_frames_ctx
            ? reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data) : nullptr;
        if (!frames || !frames->device_ref ||
            frames->device_ref->data != decoder.hardwareDevice->data) {
            return ::media::Status::failure(::media::ErrorInfo::hardwareUnavailable(
                "RTP first-frame device differs from the opened decoder device"));
        }
    }
    return ::media::Status::success();
}

} // namespace

namespace {
::media::Result<MediaRtpSourceFirstFrameEvidence> probePreparedInput(
    const MediaRawRtpProbeLease* lease, std::span<const MediaBufferRef> packets,
    const MediaRtpSourceFirstFrameProbePlan& plan,
    MediaRtpSourceDecoderProbeOwner owner)
{
    try {
        auto& decoder = owner.decoder;
        const auto* identityGoal = std::get_if<MediaSourceIdentityProbeGoal>(&plan.goal);
        if (identityGoal && (lease || !identityGoal->budget || identityGoal->source.empty() || !identityGoal->generation ||
            (identityGoal->requirement != MediaSourceIdentityProbeRequirement::MappedFrame &&
             identityGoal->requirement != MediaSourceIdentityProbeRequirement::ReorderedFrames)))
            return EvidenceResult::failure(::media::ErrorInfo::invalidArgument(
                "Identity probe requires completed source AUs and explicit source storage"));
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
        if (identityGoal && (!decoder.context || !(decoder.context->flags & AV_CODEC_FLAG_COPY_OPAQUE)))
#else
        if (identityGoal)
#endif
            return EvidenceResult::failure(::media::ErrorInfo::unsupported("Identity probe requires opened COPY_OPAQUE decoder"));
        if ((lease && plan.input != MediaSourcePreparationInput::RawDatagrams) ||
            (!lease && plan.input != MediaSourcePreparationInput::CompletedAccessUnits) ||
            !owner.storage || !decoder.context || !avcodec_is_open(decoder.context.get()) ||
            !plan.source.decoder.outputFrame || (lease && lease->datagramCount() == 0) ||
            plan.timeBase.num <= 0 || plan.timeBase.den <= 0 ||
            plan.sampleAspectRatio.num <= 0 || plan.sampleAspectRatio.den <= 0 ||
            plan.signaling.clockRate <= 0 ||
            av_cmp_q({plan.timeBase.num, plan.timeBase.den}, {1, plan.signaling.clockRate}) != 0 ||
            plan.depacketizer.codecName != plan.signaling.codecName ||
            plan.depacketizer.payloadType != plan.signaling.payloadType ||
            plan.depacketizer.clockRate != plan.signaling.clockRate ||
            plan.reorder.payloadType != plan.signaling.payloadType ||
            plan.envelope.codecName != plan.signaling.codecName ||
            (plan.source.decoderReceiveInterval && plan.source.decoderReceiveInterval->nanoseconds() <= 0)) {
            return EvidenceResult::failure(::media::ErrorInfo::invalidArgument(
                "RTP first-frame probe requires a bound prepared source and opened decoder"));
        }
        if (auto status = (MediaPreparationControl{plan.deadline, plan.cancellation}.check("RTP first-frame preparation")); !status) return EvidenceResult::failure(status.error());
        if (auto status = validateBinding(plan, *decoder.context); !status)
            return EvidenceResult::failure(status.error());
        auto retention = MediaDecoderInputRetentionAdapter::readAfterOpen(
            *decoder.context, plan.source.decoder.hwaccelName);
        if (!retention) return EvidenceResult::failure(::media::ErrorInfo::unsupported(
            "RTP first-frame decoder has no authoritative input-retention facts"));
        if (const auto& planned = plan.source.decoder.preparedInputRetention; planned &&
            (planned->authority.empty() || retention->threadCount != planned->threadCount ||
             retention->threadType != planned->threadType ||
             retention->mainHandoffPackets > planned->mainHandoffPackets ||
             retention->frameWorkerPackets > planned->frameWorkerPackets ||
             retention->serialPrivatePackets > planned->serialPrivatePackets)) {
            return EvidenceResult::failure(::media::ErrorInfo::invalidArgument(
                "RTP first-frame decoder retention exceeds prepared source facts"));
        }
        auto storage = MediaSourcePreparationStoragePlanner::plan(
            plan.maximumDatagramBytes, plan.reorder, plan.depacketizer, plan.envelope, *retention,
            decoder, *plan.source.decoder.outputFrame, plan.input);
        if (!storage) return EvidenceResult::failure(storage.error());
        if (storage.value().maximumBytes > owner.storage.bytes())
            return EvidenceResult::failure(::media::ErrorInfo::invalidArgument(
                "RTP probe exceeds its retained decoder preparation admission"));
        auto frameContract = *plan.source.decoder.outputFrame;
        MediaRtpSourceFirstFrameEvidence evidence(std::move(owner.storage), std::move(decoder),
            std::move(frameContract), storage.value());
        if (identityGoal && packets.empty()) {
            evidence.insufficiency = "Snapshot has no completed source AUs yet";
            return EvidenceResult::success(std::move(evidence));
        }
        if (identityGoal) {
            auto identity = MediaRtpSourceIdentityProbe::create(packets.size(), *identityGoal,
                storage.value().softwareFrameBytes);
            if (!identity) return EvidenceResult::failure(identity.error());
            evidence.identityProbe = std::move(identity).value();
        }
        {
            auto configured = parseRtpVideoSignalingFacts(plan.depacketizer.codecName, plan.depacketizer.fmtp);
            if (!configured) return EvidenceResult::failure(configured.error());
            if (!equalSignaling(configured.value(), plan.signaling.facts)) return EvidenceResult::failure(
                ::media::ErrorInfo::invalidArgument("RTP depacketizer signaling differs from prepared source"));
        }
        if (lease) {
            auto signaling = verifySnapshotSignaling(*lease, plan, storage.value());
            if (!signaling) return EvidenceResult::failure(signaling.error());
            evidence.inspectedDatagrams = lease->datagramCount();
            if (!signaling.value()) {
                evidence.insufficiency = "Snapshot has no complete matching parameter-set evidence";
                return EvidenceResult::success(std::move(evidence));
            }
        }
        auto codec = makeMediaVideoDecoderCodecApi();
        auto frame = ::media::ffmpeg::makeFrame();
        if (!codec || !frame) return EvidenceResult::failure(
            ::media::ErrorInfo::allocationFailed("RTP first-frame codec/frame allocation failed"));
        bool interrupted = false;
        struct ReceiveProgress { bool complete; bool producedFrame; };
        const auto receive = [&]() -> ::media::Result<ReceiveProgress> {
            bool produced = false;
            if (auto status = (MediaPreparationControl{plan.deadline, plan.cancellation}.check("RTP first-frame preparation")); !status)
                return ::media::Result<ReceiveProgress>::failure(status.error());
            for (;;) {
                const int received = codec->receiveFrame(evidence.decoder.context.get(), frame.get());
                if (received == AVERROR(EAGAIN)) return ::media::Result<ReceiveProgress>::success({false, produced});
                if (received < 0) return ::media::Result<ReceiveProgress>::failure(
                    FFmpegGraphError::fromCode(received, "RTP probe avcodec_receive_frame"));
                produced = true;
                auto valid = validateFirstFrame(plan, evidence.decoder, *frame);
                if (!valid) return ::media::Result<ReceiveProgress>::failure(valid.error());
                if (evidence.identityProbe) {
                    auto matched = evidence.identityProbe->received(*frame, *evidence.decoder.context);
                    if (!matched) return ::media::Result<ReceiveProgress>::failure(matched.error());
                }
                if (!evidence.firstFrame) {
                    evidence.firstFrame = std::move(frame);
                    if (evidence.identityProbe && !evidence.identityProbe->complete()) {
                        frame = ::media::ffmpeg::makeFrame();
                        if (!frame) return ::media::Result<ReceiveProgress>::failure(
                            ::media::ErrorInfo::allocationFailed("Identity probe receive frame allocation failed"));
                    }
                } else av_frame_unref(frame.get());
                if (!evidence.identityProbe || evidence.identityProbe->complete()) {
                    evidence.disposition = MediaRtpSourceFirstFrameDisposition::FirstFrame;
                    return ::media::Result<ReceiveProgress>::success({true, true});
                }
                if (auto status = (MediaPreparationControl{plan.deadline, plan.cancellation}.check(
                        "RTP identity probe receive")); !status)
                    return ::media::Result<ReceiveProgress>::failure(status.error());
            }
        };
        const auto waitReceiveCheck = [&]() -> ::media::Status {
            if (!plan.source.decoderReceiveInterval) return ::media::Status::failure(
                ::media::ErrorInfo::internalError("RTP decoder returned EAGAIN on both API ends"));
            const auto now = Clock::now();
            const auto interval = std::chrono::nanoseconds(plan.source.decoderReceiveInterval->nanoseconds());
            const auto remaining = plan.deadline - now;
            const auto until = interval < remaining ? now + interval : plan.deadline;
            std::mutex mutex;
            std::condition_variable_any condition;
            std::unique_lock lock(mutex);
            condition.wait_until(lock, plan.cancellation, until, [] { return false; });
            return (MediaPreparationControl{plan.deadline, plan.cancellation}.check("RTP first-frame preparation"));
        };
        const auto submit = [&](const AVPacket& packet, MediaRational timeBase,
                                std::uint32_t rtpTimestamp,
                                std::shared_ptr<const MediaVideoDecodeLineage> lineage) -> ::media::Result<bool> {
            using Result = ::media::Result<bool>;
            if (!packet.data || packet.size <= 0 ||
                static_cast<std::size_t>(packet.size) > storage.value().maximumAccessUnitBytes ||
                packet.opaque || packet.opaque_ref || (packet.flags & AV_PKT_FLAG_CORRUPT) ||
                av_cmp_q({timeBase.num, timeBase.den}, {plan.timeBase.num, plan.timeBase.den}) != 0)
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "RTP probe AU violates byte/token contract"));
            if (auto valid = MediaRtpVideoParameterSetValidator::validateBinding(
                    {packet.data, static_cast<std::size_t>(packet.size)}, plan.signaling.facts, false); !valid)
                return Result::failure(valid.error());
            if (!evidence.firstRandomAccessTimestamp) {
                if (!(packet.flags & AV_PKT_FLAG_KEY)) return Result::success(true);
                evidence.firstRandomAccessTimestamp = rtpTimestamp;
            }
            ::media::ffmpeg::PacketPtr cloned;
            const AVPacket* submittedPacket = &packet;
            if (evidence.identityProbe) {
                auto prepared = evidence.identityProbe->prepare(packet, lineage);
                if (!prepared) return Result::failure(prepared.error());
                cloned = std::move(prepared).value();
                submittedPacket = cloned.get();
            }
            // The decoder borrows this reference; the source owner keeps its
            // original AVPacket and preparation reservation for runtime.
            bool pending = true;
            while (pending) {
                if (auto status = (MediaPreparationControl{plan.deadline, plan.cancellation}.check(
                        "RTP first-frame preparation")); !status) return Result::failure(status.error());
                const int sent = codec->sendPacket(evidence.decoder.context.get(), submittedPacket);
                if (sent < 0 && sent != AVERROR(EAGAIN)) return Result::failure(
                    FFmpegGraphError::fromCode(sent, "RTP probe avcodec_send_packet"));
                if (sent == 0) {
                    pending = false;
                    ++evidence.submittedAccessUnits;
                    if (evidence.identityProbe) {
                        auto recorded = evidence.identityProbe->submitted(*lineage);
                        if (!recorded) return Result::failure(recorded.error());
                    }
                    evidence.decoderMayHavePendingOutput = true;
                }
                auto output = receive();
                if (!output) return Result::failure(output.error());
                if (output.value().complete) return Result::success(false);
                if (pending && !output.value().producedFrame) {
                    if (auto waited = waitReceiveCheck(); !waited) return Result::failure(waited.error());
                }
            }
            return Result::success(true);
        };
        auto decoded = [&]() -> ::media::Status {
            if (!lease) {
                for (const auto& buffer : packets) {
                    const auto* packet = FFmpegPacketView::packet(buffer);
                    const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get());
                    const auto* canonical = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
                    if (!packet || packet->pts < 0 ||
                        (identityGoal ? (!source || source->decodedSequence().has_value())
                                      : (!canonical || canonical->streamKind() != MediaStreamKind::Video || !canonical->sourceTiming())))
                        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                            "Continuous RTP probe input differs from its planned timing product"));
                    const auto timeBase = identityGoal ? source->lineage()->packetTimeBase : canonical->timeDescriptor().timeBase;
                    auto progress = submit(*packet, timeBase, static_cast<std::uint32_t>(packet->pts),
                        identityGoal ? source->lineage() : nullptr);
                    if (!progress) return ::media::Status::failure(progress.error());
                    if (!progress.value()) break;
                }
                return ::media::Status::success();
            }
            auto depacketizer = MediaRtpDepacketizerFactory::create(
                plan.depacketizer, storage.value().maximumAccessUnitBytes, MediaRuntimeLineageAllocation{});
            if (!depacketizer) return ::media::Status::failure(depacketizer.error());
            // Snapshot start is not an AU boundary. Preserve the shared
            // continuity algorithm used by the raw probe path.
            depacketizer.value()->discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
            return visitSnapshot(*lease, plan, storage.value(), [&](const MediaRtpReorderResult& ordered)
                -> ::media::Result<bool> {
                using Result = ::media::Result<bool>;
                for (const auto& gap : ordered.discontinuities) {
                    depacketizer.value()->discontinuity(gap.reason);
                    if (evidence.submittedAccessUnits) {
                        interrupted = true;
                        return Result::success(false);
                    }
                }
                for (const auto& packet : ordered.packets) {
                    auto units = depacketizer.value()->push(packet);
                    if (!units) return Result::failure(units.error());
                    if (units.value().accessUnits.size() > storage.value().maximumAccessUnitsPerPush)
                        return Result::failure(::media::ErrorInfo::internalError("RTP probe AU count exceeds plan"));
                    for (const auto& unit : units.value().accessUnits) {
                        if (!unit.packet) return Result::failure(::media::ErrorInfo::invalidArgument(
                            "RTP probe AU has no packet"));
                        auto progress = submit(*unit.packet, unit.timeBase, unit.rtpTimestamp, nullptr);
                        if (!progress || !progress.value()) return progress;
                    }
                }
                return Result::success(true);
            });
        }();
        if (!decoded) return EvidenceResult::failure(decoded.error());
        if (evidence.disposition == MediaRtpSourceFirstFrameDisposition::NeedMoreEvidence && !interrupted && evidence.submittedAccessUnits &&
            plan.source.decoderReceiveInterval) {
            // One planned receive-check opportunity, then explicit finite
            // evidence exhaustion; no null packet, drain, or hidden reacquire.
            if (auto waited = waitReceiveCheck(); !waited) return EvidenceResult::failure(waited.error());
            auto output = receive();
            if (!output) return EvidenceResult::failure(output.error());
        }
        if (evidence.identityProbe && !evidence.identityProbe->complete())
            evidence.insufficiency = identityGoal->requirement == MediaSourceIdentityProbeRequirement::ReorderedFrames
                ? "Snapshot lacks verified input PTS and decoded identity reorder witnesses"
                : "Snapshot lacks a decoded frame with verified source identity";
        else if (!evidence.firstFrame) evidence.insufficiency = interrupted
            ? "Snapshot has a sequence gap after decoder submission; reset before replay"
            : "Snapshot has no complete random-access prefix yielding a first frame";
        return EvidenceResult::success(std::move(evidence));
    } catch (const std::bad_alloc&) {
        return EvidenceResult::failure(::media::ErrorInfo::allocationFailed(
            "RTP first-frame preparation allocation failed"));
    }
}

} // namespace

::media::Result<MediaRtpSourceFirstFrameEvidence> MediaRtpSourceFirstFrameProbe::probe(
    const MediaRawRtpProbeLease& lease, const MediaRtpSourceFirstFrameProbePlan& plan,
    MediaRtpSourceDecoderProbeOwner owner)
{
    return probePreparedInput(&lease, {}, plan, std::move(owner));
}

::media::Result<MediaRtpSourceFirstFrameEvidence> MediaRtpSourceFirstFrameProbe::probe(
    std::span<const MediaBufferRef> packets, const MediaRtpSourceFirstFrameProbePlan& plan,
    MediaRtpSourceDecoderProbeOwner owner)
{
    return probePreparedInput(nullptr, packets, plan, std::move(owner));
}

} // namespace media::ffmpeg::graph
