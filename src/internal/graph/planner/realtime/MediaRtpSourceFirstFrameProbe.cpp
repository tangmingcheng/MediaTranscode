#include "internal/graph/planner/realtime/MediaRtpSourceFirstFrameProbe.h"

#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"
#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"
#include "internal/graph/protocol/codec/MediaVideoNalUnitScanner.h"
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

::media::Status checkDeadline(const MediaRtpSourceFirstFrameProbePlan& plan)
{
    if (plan.cancellation.stop_requested()) return ::media::Status::failure(
        ::media::ErrorInfo::cancelled("RTP first-frame preparation was cancelled"));
    if (Clock::now() >= plan.deadline) return ::media::Status::failure(
        ::media::ErrorInfo::ioFailure("RTP first-frame preparation reached its original deadline"));
    return ::media::Status::success();
}

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

struct ParameterBinding final {
    const MediaRtpVideoSignalingFacts& expected;
    bool vps = false;
    bool sps = false;
    bool pps = false;
    bool conflict = false;

    static void visit(void* opaque, std::span<const std::uint8_t> nal)
    {
        auto& self = *static_cast<ParameterBinding*>(opaque);
        while (!nal.empty() && nal.back() == 0) nal = nal.first(nal.size() - 1);
        if (nal.empty()) { self.conflict = true; return; }
        const std::vector<std::uint8_t>* expected = nullptr;
        if (const auto* h264 = std::get_if<MediaH264SignalingFacts>(&self.expected)) {
            switch (nal.front() & 0x1f) {
            case 7: expected = &h264->sps; self.sps = true; break;
            case 8: expected = &h264->pps; self.pps = true; break;
            default: self.conflict = true; return;
            }
        } else {
            const auto& hevc = std::get<MediaHevcSignalingFacts>(self.expected);
            switch ((nal.front() >> 1) & 0x3f) {
            case 32: expected = &hevc.vps; self.vps = true; break;
            case 33: expected = &hevc.sps; self.sps = true; break;
            case 34: expected = &hevc.pps; self.pps = true; break;
            default: self.conflict = true; return;
            }
        }
        self.conflict = self.conflict ||
            !std::equal(nal.begin(), nal.end(), expected->begin(), expected->end());
    }
};

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
    ParameterBinding binding{plan.signaling.facts};
    auto scanned = MediaVideoNalUnitScanner::visit(
        {parameters.extradata, static_cast<std::size_t>(parameters.extradata_size)},
        h264 ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc,
        MediaEncodedPacketLayout::startCodeDelimited(), &binding, ParameterBinding::visit);
    if (!scanned) return scanned;
    if (binding.conflict || !binding.sps || !binding.pps || (!h264 && !binding.vps)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP first-frame extradata differs from the authoritative signaling sets"));
    }
    return ::media::Status::success();
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
    for (const auto& datagram : lease.datagrams()) {
        if (auto status = checkDeadline(plan); !status) return status;
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
    auto detected = observer.value().detected(lease.datagrams().size(), 0, 0);
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

::media::Result<MediaRtpSourceFirstFrameEvidence> MediaRtpSourceFirstFrameProbe::probe(
    const MediaRawRtpProbeLease& lease, const MediaRtpSourceFirstFrameProbePlan& plan,
    CodecResolverDecoderContextBuildResult decoder,
    const std::shared_ptr<MediaPreparationStorageBudget>& budget)
{
    try {
        if (!budget || !decoder.context || !avcodec_is_open(decoder.context.get()) ||
            !plan.source.decoder.outputFrame || lease.datagrams().empty() ||
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
        if (auto status = checkDeadline(plan); !status) return EvidenceResult::failure(status.error());
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
            decoder, *plan.source.decoder.outputFrame);
        if (!storage) return EvidenceResult::failure(storage.error());
        auto reserved = budget->reserve(storage.value().maximumBytes);
        if (!reserved) return EvidenceResult::failure(reserved.error());
        MediaRtpSourceFirstFrameEvidence evidence(std::move(reserved).value(), std::move(decoder));
        {
            auto configured = parseRtpVideoSignalingFacts(plan.depacketizer.codecName, plan.depacketizer.fmtp);
            if (!configured) return EvidenceResult::failure(configured.error());
            if (!equalSignaling(configured.value(), plan.signaling.facts)) return EvidenceResult::failure(
                ::media::ErrorInfo::invalidArgument("RTP depacketizer signaling differs from prepared source"));
        }
        auto signaling = verifySnapshotSignaling(lease, plan, storage.value());
        if (!signaling) return EvidenceResult::failure(signaling.error());
        evidence.inspectedDatagrams = lease.datagrams().size();
        if (!signaling.value()) {
            evidence.insufficiency = "Snapshot has no complete matching parameter-set evidence";
            return EvidenceResult::success(std::move(evidence));
        }
        auto depacketizer = MediaRtpDepacketizerFactory::create(
            plan.depacketizer, storage.value().maximumAccessUnitBytes);
        if (!depacketizer) return EvidenceResult::failure(depacketizer.error());
        // Snapshot start is not an AU boundary. The shared continuity state
        // discards the first unknown timestamp, even when its marker is set.
        depacketizer.value()->discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
        auto codec = makeMediaVideoDecoderCodecApi();
        auto frame = ::media::ffmpeg::makeFrame();
        if (!codec || !frame) return EvidenceResult::failure(
            ::media::ErrorInfo::allocationFailed("RTP first-frame codec/frame allocation failed"));
        bool interrupted = false;
        const auto receive = [&]() -> ::media::Result<bool> {
            if (auto status = checkDeadline(plan); !status)
                return ::media::Result<bool>::failure(status.error());
            const int received = codec->receiveFrame(evidence.decoder.context.get(), frame.get());
            if (received == AVERROR(EAGAIN)) return ::media::Result<bool>::success(false);
            if (received < 0) return ::media::Result<bool>::failure(
                FFmpegGraphError::fromCode(received, "RTP probe avcodec_receive_frame"));
            auto valid = validateFirstFrame(plan, evidence.decoder, *frame);
            if (!valid) return ::media::Result<bool>::failure(valid.error());
            evidence.firstFrame = std::move(frame);
            evidence.disposition = MediaRtpSourceFirstFrameDisposition::FirstFrame;
            return ::media::Result<bool>::success(true);
        };
        const auto waitCadence = [&]() -> ::media::Status {
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
            return checkDeadline(plan);
        };
        auto decoded = visitSnapshot(lease, plan, storage.value(), [&](const MediaRtpReorderResult& ordered)
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
                for (auto& unit : units.value().accessUnits) {
                    if (!unit.packet || unit.packet->size <= 0 ||
                        static_cast<std::size_t>(unit.packet->size) > storage.value().maximumAccessUnitBytes ||
                        unit.packet->opaque || unit.packet->opaque_ref ||
                        (unit.packet->flags & AV_PKT_FLAG_CORRUPT) ||
                        av_cmp_q({unit.timeBase.num, unit.timeBase.den},
                                 {plan.timeBase.num, plan.timeBase.den}) != 0)
                        return Result::failure(::media::ErrorInfo::invalidArgument("RTP probe AU violates byte/token contract"));
                    if (!evidence.firstRandomAccessTimestamp) {
                        if (!(unit.packet->flags & AV_PKT_FLAG_KEY)) continue;
                        evidence.firstRandomAccessTimestamp = unit.rtpTimestamp;
                    }
                    // Do not release or replace pending input on EAGAIN.
                    while (unit.packet) {
                        if (auto status = checkDeadline(plan); !status) return Result::failure(status.error());
                        const int sent = codec->sendPacket(evidence.decoder.context.get(), unit.packet.get());
                        if (sent < 0 && sent != AVERROR(EAGAIN)) return Result::failure(
                            FFmpegGraphError::fromCode(sent, "RTP probe avcodec_send_packet"));
                        if (sent == 0) {
                            unit.packet.reset();
                            ++evidence.submittedAccessUnits;
                            evidence.decoderMayHavePendingOutput = true;
                        }
                        auto output = receive();
                        if (!output) return output;
                        if (output.value()) return Result::success(false);
                        if (unit.packet) {
                            if (auto waited = waitCadence(); !waited) return Result::failure(waited.error());
                        }
                    }
                }
            }
            return Result::success(true);
        });
        if (!decoded) return EvidenceResult::failure(decoded.error());
        if (!evidence.firstFrame && !interrupted && evidence.submittedAccessUnits &&
            plan.source.decoderReceiveInterval) {
            // One source-cadence completion opportunity, then explicit finite
            // evidence exhaustion; no null packet, drain, or hidden reacquire.
            if (auto waited = waitCadence(); !waited) return EvidenceResult::failure(waited.error());
            auto output = receive();
            if (!output) return EvidenceResult::failure(output.error());
        }
        if (!evidence.firstFrame) evidence.insufficiency = interrupted
            ? "Snapshot has a sequence gap after decoder submission; reset before replay"
            : "Snapshot has no complete random-access prefix yielding a first frame";
        return EvidenceResult::success(std::move(evidence));
    } catch (const std::bad_alloc&) {
        return EvidenceResult::failure(::media::ErrorInfo::allocationFailed(
            "RTP first-frame preparation allocation failed"));
    }
}

} // namespace media::ffmpeg::graph
