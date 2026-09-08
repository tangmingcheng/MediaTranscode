#include "internal/graph/nodes/demux/MpegTsDemuxNode.h"

#include "internal/graph/nodes/demux/MediaTsDemuxNodePlanDecoder.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramContractValidator.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketPayloadFootprint.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <limits>

namespace media::ffmpeg::graph {
namespace {

std::optional<std::uint64_t> packetTimestamp(std::int64_t value)
{
    if (value == AV_NOPTS_VALUE) return std::nullopt;
    constexpr std::int64_t modulus = std::int64_t{1} << 33;
    const auto normalized = value % modulus;
    return static_cast<std::uint64_t>(normalized < 0 ? normalized + modulus : normalized);
}

::media::Result<MediaBufferRef> wrapTimedPacket(
    ::media::ffmpeg::PacketPtr packet,
    MediaStreamKind streamKind,
    MediaPacketSourceTiming timing,
    MediaRational plannedTimeBase)
{
    auto buffer = FFmpegBufferFactory::wrapPacket(
        std::move(packet), streamKind, std::move(timing));
    if (!buffer) return ::media::Result<MediaBufferRef>::failure(buffer.error());
    const auto* wrapped = dynamic_cast<const FFmpegPacketBuffer*>(buffer.value().get());
    if (!wrapped || !wrapped->packet()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MpegTsDemuxNode lost wrapped packet ownership"));
    }
    buffer.value()->setTimeDescriptor(MediaTimeDescriptor{plannedTimeBase});
    return buffer;
}

} // namespace

void MpegTsDemuxNode::StreamClock::discardBefore(std::uint64_t generation)
{
    mappers.erase(mappers.begin(), mappers.lower_bound(generation));
}

MpegTsDemuxNode::MpegTsDemuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MpegTsDemuxNode")
{
}

MediaNodeKind MpegTsDemuxNode::staticKind() noexcept
{
    return MediaNodeKind::MpegTsDemux;
}

::media::Status MpegTsDemuxNode::bind(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Status::failure(input.error());
    if (!input.value()) return ::media::Status::success();
    if (m_session) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode rejects duplicate binding"));
    }
    auto* prepared = dynamic_cast<MediaTsPreparedInputBuffer*>(input.value()->get());
    if (!prepared) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode requires MediaTsPreparedInputBuffer"));
    }
    auto session = prepared->takeSession();
    if (!session) return ::media::Status::failure(session.error());
    const auto& runtimeContract = session.value()->runtimeContract();
    if (!runtimeContract.originBinding) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MpegTsDemuxNode requires a typed runtime binding"));
    }
    if (auto snapshots = MediaTsProgramContractValidator::validateSnapshots(
            session.value()->programSnapshots(),
            session.value()->programInventory());
        !snapshots) {
        return snapshots;
    }
    auto plan = MediaTsDemuxNodePlanDecoder::decode(
        nodeOptions(context), *runtimeContract.originBinding);
    if (!plan) return ::media::Status::failure(plan.error());
    if (runtimeContract.packetStride != plan.value().packetStride ||
        runtimeContract.evidenceCapacity != plan.value().evidenceCapacity ||
        runtimeContract.maximumPositionRegressionBytes !=
            plan.value().maximumPositionRegressionBytes ||
        runtimeContract.pesProvenanceCapacity !=
            plan.value().pesProvenanceCapacity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MpegTsDemuxNode materialized runtime session violates the planned contract"));
    }
    auto projection = MediaTsClockProjection::create(
        plan.value().clockPolicy, plan.value().projectionCapacity,
        plan.value().maximumPositionRegressionBytes,
        plan.value().initialSourceGeneration,
        plan.value().initialRawTransportGeneration);
    if (!projection) return ::media::Status::failure(projection.error());
    auto preflight = session.value()->evidenceSnapshotAfter(std::nullopt);
    if (!preflight) return ::media::Status::failure(preflight.error());
    if (auto status = projection.value().replay(preflight.value()); !status) return status;
    auto retention = MediaTsInitialAcquiringPacketBuffer::create(
        std::move(plan.value().retention));
    if (!retention) return ::media::Status::failure(retention.error());
    m_binding = *runtimeContract.originBinding;
    m_policy = std::move(plan.value().clockPolicy);
    m_initialSourceGeneration = plan.value().initialSourceGeneration;
    m_acquiringPackets.emplace(std::move(retention).value());
    m_projection = std::move(projection).value();
    m_session = std::move(session).value();
    return ::media::Status::success();
}

::media::Result<MediaPacketSourceTiming> MpegTsDemuxNode::timingFor(
    const AVPacket& packet, const MediaTsClockProjectionCheckpoint& checkpoint, StreamClock& clock)
{
    MediaPacketSourceTiming timing{std::nullopt, std::nullopt, checkpoint.readiness, checkpoint.generation};
    if (checkpoint.readiness != MediaSourceClockReadiness::Locked) {
        return ::media::Result<MediaPacketSourceTiming>::success(timing);
    }
    auto mapper = clock.mappers.find(checkpoint.generation);
    if (mapper == clock.mappers.end()) {
        auto created = MediaTsSourceClockMapper::create(checkpoint.calibration);
        if (!created) return ::media::Result<MediaPacketSourceTiming>::failure(created.error());
        mapper = clock.mappers.emplace(checkpoint.generation, std::move(created).value()).first;
    }
    auto mapped = mapper->second.map(packetTimestamp(packet.pts), packetTimestamp(packet.dts));
    if (!mapped) return ::media::Result<MediaPacketSourceTiming>::failure(mapped.error());
    if (mapped.value().presentationTime) timing.presentationNs = mapped.value().presentationTime->nanoseconds();
    if (mapped.value().decodeTime) timing.decodeNs = mapped.value().decodeTime->nanoseconds();
    return ::media::Result<MediaPacketSourceTiming>::success(timing);
}

::media::Result<MediaTsClockProjectionCheckpoint>
MpegTsDemuxNode::sourceClockCheckpoint(std::uint64_t packetPosition)
{
    if (auto status = m_session->observePacketPosition(packetPosition); !status) {
        return ::media::Result<MediaTsClockProjectionCheckpoint>::failure(status.error());
    }
    if (auto status = m_projection->observePacketPosition(packetPosition); !status) {
        return ::media::Result<MediaTsClockProjectionCheckpoint>::failure(status.error());
    }
    auto incremental = m_session->evidenceSnapshotAfter(m_projection->lastReplayedOffset());
    if (!incremental) {
        return ::media::Result<MediaTsClockProjectionCheckpoint>::failure(
            incremental.error());
    }
    if (auto status = m_projection->replay(incremental.value()); !status) {
        return ::media::Result<MediaTsClockProjectionCheckpoint>::failure(status.error());
    }
    const auto oldestGeneration = m_projection->oldestRetainedGeneration();
    m_videoClock.discardBefore(oldestGeneration);
    m_audioClock.discardBefore(oldestGeneration);
    return m_projection->atOrBefore(packetPosition);
}

::media::Status MpegTsDemuxNode::enqueueLockedPacket(
    ::media::ffmpeg::PacketPtr packet,
    MediaStreamKind streamKind,
    const MediaTsClockProjectionCheckpoint& checkpoint,
    MediaGraphPayloadReservation reservation)
{
    auto timing = timingFor(
        *packet, checkpoint,
        streamKind == MediaStreamKind::Video ? m_videoClock : m_audioClock);
    if (!timing) return ::media::Status::failure(timing.error());
    const MediaRational timeBase{
        packet->time_base.num, packet->time_base.den};
    auto buffer = wrapTimedPacket(
        std::move(packet), streamKind, timing.value(), timeBase);
    if (!buffer) return ::media::Status::failure(buffer.error());
    if (auto status = reservation.attachTo(*buffer.value()); !status) {
        return status;
    }
    return m_acquiringPackets->stageSingleReplay(
        std::move(buffer).value(), streamKind);
}

::media::Status MpegTsDemuxNode::prepareLockedBatch(
    ::media::ffmpeg::PacketPtr packet,
    MediaStreamKind streamKind,
    const MediaTsClockProjectionCheckpoint& checkpoint,
    MediaGraphPayloadReservation reservation)
{
    StreamClock videoClock = m_videoClock;
    StreamClock audioClock = m_audioClock;
    auto stage = m_acquiringPackets->stageReplay(
        *packet, streamKind, std::move(reservation),
        [&](const AVPacket& source,
            MediaStreamKind kind,
            const MediaGraphPayloadReservation& sourceReservation)
            -> ::media::Result<MediaBufferRef> {
        auto timing = timingFor(
            source, checkpoint,
            kind == MediaStreamKind::Video ? videoClock : audioClock);
        if (!timing) return ::media::Result<MediaBufferRef>::failure(timing.error());
        ::media::ffmpeg::PacketPtr clone(av_packet_clone(&source));
        if (!clone) {
            return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::allocationFailed(
                "MpegTsDemuxNode failed to clone retained acquiring packet"));
        }
        auto buffer = wrapTimedPacket(
            std::move(clone), kind, timing.value(),
            MediaRational{source.time_base.num, source.time_base.den});
        if (!buffer) return ::media::Result<MediaBufferRef>::failure(buffer.error());
        if (auto status = sourceReservation.shareWithAliasingBuffer(
                *buffer.value()); !status) {
            return ::media::Result<MediaBufferRef>::failure(status.error());
        }
        return ::media::Result<MediaBufferRef>::success(std::move(buffer).value());
    });
    if (!stage) return stage;
    m_videoClock = std::move(videoClock);
    m_audioClock = std::move(audioClock);
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MpegTsDemuxNode::emitReadyPacket(
    MediaGraphExecutionContext& context)
{
    const auto& ready = m_acquiringPackets->nextReplay();
    auto status = emitOutput(
        context,
        ready.streamKind == MediaStreamKind::Video ? "video" : "audio",
        ready.buffer);
    if (status || retainsPendingOutput(ready.buffer)) {
        m_acquiringPackets->popReplay();
    }
    return processProgress(status);
}

::media::Status MpegTsDemuxNode::rejectDuplicateBinding(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Status::failure(input.error());
    if (!input.value()) return ::media::Status::success();
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode rejects duplicate binding"));
}

::media::Result<MediaNodeProcessResult> MpegTsDemuxNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_session) {
        if (auto status = bind(context); !status) return processProgress(status);
        if (!m_session) return processWaiting();
    } else if (auto status = rejectDuplicateBinding(context); !status) {
        return processProgress(status);
    }
    if (m_eofSent) return processFinished();
    if (m_acquiringPackets->hasReplay()) return emitReadyPacket(context);
    auto reservation = context.reservePayload(
        nodeId(), MediaStreamKind::Any, MediaPayloadKind::Packet);
    if (!reservation) {
        return processProgress(
            ::media::Status::failure(reservation.error()));
    }
    auto read = m_session->readFrame();
    if (!read) return ::media::Result<MediaNodeProcessResult>::failure(read.error());
    auto envelope = std::move(read).value();
    if (envelope.state == MediaTsReadFrameState::Discarded) {
        return processProgress();
    }
    if (envelope.state == MediaTsReadFrameState::Waiting) return processWaiting();
    if (envelope.state == MediaTsReadFrameState::EndOfStream) {
        if (m_acquiringPackets && !m_acquiringPackets->empty()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MpegTsDemuxNode reached EOF before initial clock lock"));
        }
        return processFinished(emitEof(context));
    }
    if (!envelope.packet) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode frame envelope requires a packet"));
    auto packet = std::move(envelope.packet);
    if (!m_binding) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "MpegTsDemuxNode typed stream binding is unavailable"));
    }
    const auto plannedStreamKind =
        MediaTsRuntimeBindingCodec::streamKindForIndex(
            *m_binding, packet->stream_index);
    if (!plannedStreamKind) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode packet stream/PID mismatch"));
    const MediaStreamKind streamKind = *plannedStreamKind;
    const auto plannedTimeBase = MediaTsRuntimeBindingCodec::timeBaseForIndex(
        *m_binding, packet->stream_index);
    if (!plannedTimeBase) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MpegTsDemuxNode selected stream time base is unavailable"));
    }
    if (packet->time_base.num > 0 && packet->time_base.den > 0 &&
        (packet->time_base.num != plannedTimeBase->num ||
         packet->time_base.den != plannedTimeBase->den)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MpegTsDemuxNode packet time base conflicts with typed runtime binding"));
    }
    packet->time_base = AVRational{plannedTimeBase->num, plannedTimeBase->den};
    const auto footprint = ffmpegPacketPayloadFootprintBytes(*packet);
    if (!footprint || *footprint == 0) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "MpegTsDemuxNode packet lacks an exact payload footprint"));
    }
    if (auto status = reservation.value().shrinkToActual(*footprint);
        !status) {
        return ::media::Result<MediaNodeProcessResult>::failure(status.error());
    }
    if (envelope.provenance.readiness == MediaSourceClockReadiness::Acquiring) {
        if (envelope.provenance.evidenceByteOffset ||
            envelope.provenance.originByteOffset) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MpegTsDemuxNode acquiring provenance cannot identify a PES"));
        }
        if (auto status = m_acquiringPackets->retain(
                std::move(packet), streamKind,
                std::move(reservation).value()); !status) {
            return processProgress(status);
        }
        MediaBufferRef state = makeMediaBufferRef<MediaSourceClockStateBuffer>(
            MediaSourceClockReadiness::Acquiring,
            m_initialSourceGeneration, false);
        if (context.findOutputChannel(nodeId(), "clock")) {
            return processProgress(emitOutput(context, "clock", state));
        }
        return processProgress();
    }
    if (!envelope.provenance.evidenceByteOffset) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MpegTsDemuxNode resolved PES provenance requires evidence"));
    }
    auto checkpoint = sourceClockCheckpoint(
        *envelope.provenance.evidenceByteOffset);
    if (!checkpoint) {
        return ::media::Result<MediaNodeProcessResult>::failure(checkpoint.error());
    }
    const bool invalidPesProvenance =
        envelope.provenance.readiness ==
        MediaSourceClockReadiness::ReacquireRequired;
    if (m_lockedProjectionGeneration &&
        checkpoint.value().readiness ==
            MediaSourceClockReadiness::ReacquireRequired &&
        checkpoint.value().generation <= *m_lockedProjectionGeneration) {
        return processProgress();
    }
    if (invalidPesProvenance &&
        checkpoint.value().readiness ==
            MediaSourceClockReadiness::Locked &&
        !m_reacquiringSourceGeneration) {
        return processProgress();
    }
    const bool reacquiring =
        checkpoint.value().readiness ==
            MediaSourceClockReadiness::ReacquireRequired ||
        (invalidPesProvenance && m_reacquiringSourceGeneration);
    if (reacquiring) {
        if (!m_lockedSourceGeneration || !m_lockedProjectionGeneration ||
            checkpoint.value().generation <=
                *m_lockedProjectionGeneration) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MpegTsDemuxNode reacquisition requires a future clock generation"));
        }
        if (auto status = m_acquiringPackets->retain(
                std::move(packet), streamKind,
                std::move(reservation).value()); !status) {
            return processProgress(status);
        }
        if (m_reacquiringSourceGeneration) return processProgress();
        if (*m_lockedSourceGeneration ==
                std::numeric_limits<std::uint64_t>::max()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MpegTsDemuxNode source generation overflows"));
        }
        m_reacquiringSourceGeneration = *m_lockedSourceGeneration + 1;
        MediaBufferRef state = makeMediaBufferRef<MediaSourceClockStateBuffer>(
            MediaSourceClockReadiness::ReacquireRequired,
            *m_lockedSourceGeneration, true);
        if (context.findOutputChannel(nodeId(), "clock")) {
            return processProgress(emitOutput(context, "clock", state));
        }
        return processProgress();
    }
    if (!envelope.provenance.originByteOffset ||
        *envelope.provenance.originByteOffset !=
            *envelope.provenance.evidenceByteOffset ||
        checkpoint.value().readiness != MediaSourceClockReadiness::Locked) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MpegTsDemuxNode locked PES provenance requires locked exact origin"));
    }
    if (m_reacquiringSourceGeneration &&
        m_lockedProjectionGeneration &&
        checkpoint.value().generation <=
            *m_lockedProjectionGeneration) {
        return processProgress();
    }
    if (m_reacquiringSourceGeneration &&
        !m_lockedProjectionGeneration) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MpegTsDemuxNode locked an unexpected clock generation"));
    }
    if (m_lockedSourceGeneration && !m_reacquiringSourceGeneration &&
        (!m_lockedProjectionGeneration ||
         *m_lockedProjectionGeneration != checkpoint.value().generation)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MpegTsDemuxNode clock generation changed without reacquisition"));
    }
    auto outputCheckpoint = checkpoint.value();
    outputCheckpoint.generation = m_reacquiringSourceGeneration
        .value_or(m_lockedSourceGeneration.value_or(
            checkpoint.value().generation));
    ::media::Status prepared =
        m_lockedSourceGeneration && !m_reacquiringSourceGeneration
        ? enqueueLockedPacket(
              std::move(packet), streamKind, outputCheckpoint,
              std::move(reservation).value())
        : prepareLockedBatch(
              std::move(packet), streamKind, outputCheckpoint,
              std::move(reservation).value());
    if (!prepared) return processProgress(prepared);
    m_lockedSourceGeneration = outputCheckpoint.generation;
    m_lockedProjectionGeneration = checkpoint.value().generation;
    m_reacquiringSourceGeneration.reset();
    MediaBufferRef state = makeMediaBufferRef<MediaSourceClockStateBuffer>(
        MediaSourceClockReadiness::Locked, outputCheckpoint.generation, false);
    if (context.findOutputChannel(nodeId(), "clock")) {
        return processProgress(emitOutput(context, "clock", state));
    }
    return emitReadyPacket(context);
}

::media::Status MpegTsDemuxNode::emitEof(MediaGraphExecutionContext& context)
{
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    if (!eof) return ::media::Status::failure(eof.error());
    m_eofSent = true;
    return broadcastControlToAllOutputs(context, eof.value());
}

void MpegTsDemuxNode::reset() noexcept
{
    if (m_session) (void)m_session->close();
    m_session.reset(); m_projection.reset(); m_policy.reset(); m_binding.reset();
    m_initialSourceGeneration = 0;
    m_videoClock = {}; m_audioClock = {};
    m_acquiringPackets.reset();
    m_lockedSourceGeneration.reset();
    m_lockedProjectionGeneration.reset();
    m_reacquiringSourceGeneration.reset();
    m_eofSent = false; m_aborted = false;
}

::media::Status MpegTsDemuxNode::stop(MediaGraphExecutionContext& context)
{
    reset();
    return FFmpegNodeRuntime::stop(context);
}

void MpegTsDemuxNode::interrupt(MediaGraphExecutionContext&) noexcept
{
    m_aborted = true;
    if (m_session) m_session->cancel();
}

void MpegTsDemuxNode::abort(MediaGraphExecutionContext& context) noexcept
{
    interrupt(context);
    reset();
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
