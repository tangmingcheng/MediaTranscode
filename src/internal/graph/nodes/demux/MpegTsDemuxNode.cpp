#include "internal/graph/nodes/demux/MpegTsDemuxNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> requiredGeneration(const MediaNodeOptions* options,
                                                   const char* key)
{
    auto value = requiredNonNegativeIntNodeOption(options, "MpegTsDemuxNode", key);
    if (!value) return ::media::Result<std::uint64_t>::failure(value.error());
    return ::media::Result<std::uint64_t>::success(static_cast<std::uint64_t>(value.value()));
}

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
    const auto* options = nodeOptions(context);
    auto program = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.program_number");
    auto pmt = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.pmt_pid");
    auto video = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.video_pid");
    auto audio = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.audio_pid");
    auto pcr = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.pcr_pid");
    auto gap = requiredPositiveInt64NodeOption(options, "MpegTsDemuxNode", "mpegts.maximum_pcr_gap_27mhz");
    auto packetStride = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.packet_stride");
    auto evidenceCapacity = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.evidence_timeline_capacity");
    auto capacity = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.projection_capacity");
    auto regression = requiredPositiveInt64NodeOption(options, "MpegTsDemuxNode", "mpegts.maximum_position_regression_bytes");
    auto provenanceCapacity = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.pes_provenance_capacity");
    auto originPolicyValue = requiredNonNegativeIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.packet_origin_policy");
    auto numerator = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.timestamp_time_base_num");
    auto denominator = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.timestamp_time_base_den");
    auto sourceGeneration = requiredGeneration(options, "mpegts.initial_source_generation");
    auto rawGeneration = requiredGeneration(options, "mpegts.initial_raw_generation");
    auto videoPacketCapacity = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.initial_acquiring_video_packet_capacity");
    auto audioPacketCapacity = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.initial_acquiring_audio_packet_capacity");
    auto videoByteCapacity = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", "mpegts.initial_acquiring_video_byte_capacity");
    auto audioByteCapacity = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", "mpegts.initial_acquiring_audio_byte_capacity");
    auto maximumVideoPacketBytes = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", "mpegts.maximum_acquiring_video_packet_bytes");
    auto maximumAudioPacketBytes = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", "mpegts.maximum_acquiring_audio_packet_bytes");
    if (!program || !pmt || !video || !audio || !pcr || !gap ||
        !packetStride || !evidenceCapacity || !capacity || !regression || !provenanceCapacity ||
        !originPolicyValue ||
        !numerator || !denominator || !sourceGeneration ||
        !rawGeneration || !videoPacketCapacity || !audioPacketCapacity ||
        !videoByteCapacity || !audioByteCapacity ||
        !maximumVideoPacketBytes || !maximumAudioPacketBytes) {
        const ::media::ErrorInfo* error = nullptr;
        if (!program) error = &program.error(); else if (!pmt) error = &pmt.error();
        else if (!video) error = &video.error(); else if (!audio) error = &audio.error();
        else if (!pcr) error = &pcr.error(); else if (!gap) error = &gap.error();
        else if (!packetStride) error = &packetStride.error(); else if (!evidenceCapacity) error = &evidenceCapacity.error();
        else if (!capacity) error = &capacity.error(); else if (!regression) error = &regression.error();
        else if (!provenanceCapacity) error = &provenanceCapacity.error();
        else if (!originPolicyValue) error = &originPolicyValue.error();
        else if (!numerator) error = &numerator.error(); else if (!denominator) error = &denominator.error();
        else if (!sourceGeneration) error = &sourceGeneration.error();
        else if (!rawGeneration) error = &rawGeneration.error();
        else if (!videoPacketCapacity) error = &videoPacketCapacity.error();
        else if (!audioPacketCapacity) error = &audioPacketCapacity.error();
        else if (!videoByteCapacity) error = &videoByteCapacity.error();
        else if (!audioByteCapacity) error = &audioByteCapacity.error();
        else if (!maximumVideoPacketBytes) error = &maximumVideoPacketBytes.error();
        else error = &maximumAudioPacketBytes.error();
        return ::media::Status::failure(*error);
    }
    if (numerator.value() != 1 || denominator.value() != 90'000) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode requires planned 1/90000 packet timestamps"));
    }
    m_packetTimeBase = MediaRational{
        numerator.value(), denominator.value()};
    if (originPolicyValue.value() !=
        static_cast<int>(MediaTsPacketOriginPolicy::PerStreamPesCarry)) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "MpegTsDemuxNode packet origin policy is unsupported"));
    }
    const auto originPolicy =
        static_cast<MediaTsPacketOriginPolicy>(originPolicyValue.value());
    MediaTsProgramClockPolicy policy{
        static_cast<std::uint16_t>(program.value()), static_cast<std::uint16_t>(pmt.value()),
        static_cast<std::uint16_t>(pcr.value()), static_cast<std::uint16_t>(video.value()),
        static_cast<std::uint16_t>(audio.value()), gap.value()};
    auto projection = MediaTsClockProjection::create(
        policy, static_cast<std::size_t>(capacity.value()),
        static_cast<std::uint64_t>(regression.value()), sourceGeneration.value(), rawGeneration.value());
    if (!projection) return ::media::Status::failure(projection.error());
    auto session = prepared->takeSession();
    if (!session) return ::media::Status::failure(session.error());
    const auto& programs = session.value()->programSnapshots();
    const auto selected = std::find_if(programs.begin(), programs.end(), [&](const auto& item) {
        return item.programNumber == program.value() && item.pmtPid == pmt.value() && item.pcrPid == pcr.value();
    });
    if (selected == programs.end()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MpegTsDemuxNode program identity mismatch"));
    }
    for (const auto& binding : selected->streamBindings) {
        if (binding.elementaryPid == video.value()) m_videoStreamIndex = binding.streamIndex;
        if (binding.elementaryPid == audio.value()) m_audioStreamIndex = binding.streamIndex;
    }
    if (m_videoStreamIndex < 0 || m_audioStreamIndex < 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MpegTsDemuxNode selected PID mismatch"));
    }
    const MediaTsRuntimeBinding expectedBinding{
        originPolicy,
        MediaTsRuntimeStreamBinding{
            m_videoStreamIndex, static_cast<std::uint16_t>(video.value())},
        MediaTsRuntimeStreamBinding{
            m_audioStreamIndex, static_cast<std::uint16_t>(audio.value())},
        static_cast<std::uint16_t>(pcr.value()),
        static_cast<std::size_t>(provenanceCapacity.value())};
    const auto& runtimeContract = session.value()->runtimeContract();
    if (runtimeContract.packetStride != static_cast<std::size_t>(packetStride.value()) ||
        runtimeContract.evidenceCapacity != static_cast<std::size_t>(evidenceCapacity.value()) ||
        runtimeContract.maximumPositionRegressionBytes !=
            static_cast<std::uint64_t>(regression.value()) ||
        runtimeContract.pesProvenanceCapacity !=
            static_cast<std::size_t>(provenanceCapacity.value()) ||
        runtimeContract.originBinding != expectedBinding) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MpegTsDemuxNode materialized runtime session violates the planned contract"));
    }
    auto preflight = session.value()->evidenceSnapshotAfter(std::nullopt);
    if (!preflight) return ::media::Status::failure(preflight.error());
    if (auto status = projection.value().replay(preflight.value()); !status) return status;
    m_policy = policy;
    m_initialSourceGeneration = sourceGeneration.value();
    auto retention = MediaTsInitialAcquiringPacketBuffer::create(
        MediaTsInitialPacketRetentionLimit{
            static_cast<std::size_t>(videoPacketCapacity.value()),
            static_cast<std::uint64_t>(videoByteCapacity.value()),
            static_cast<std::uint64_t>(maximumVideoPacketBytes.value())},
        MediaTsInitialPacketRetentionLimit{
            static_cast<std::size_t>(audioPacketCapacity.value()),
            static_cast<std::uint64_t>(audioByteCapacity.value()),
            static_cast<std::uint64_t>(maximumAudioPacketBytes.value())});
    if (!retention) return ::media::Status::failure(retention.error());
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
    const MediaTsClockProjectionCheckpoint& checkpoint)
{
    auto timing = timingFor(
        *packet, checkpoint,
        streamKind == MediaStreamKind::Video ? m_videoClock : m_audioClock);
    if (!timing) return ::media::Status::failure(timing.error());
    auto buffer = wrapTimedPacket(
        std::move(packet), streamKind, timing.value(), m_packetTimeBase);
    if (!buffer) return ::media::Status::failure(buffer.error());
    return m_acquiringPackets->stageSingleReplay(
        std::move(buffer).value(), streamKind);
}

::media::Status MpegTsDemuxNode::prepareLockedBatch(
    ::media::ffmpeg::PacketPtr packet,
    MediaStreamKind streamKind,
    const MediaTsClockProjectionCheckpoint& checkpoint)
{
    StreamClock videoClock = m_videoClock;
    StreamClock audioClock = m_audioClock;
    auto stage = m_acquiringPackets->stageReplay(
        *packet, streamKind,
        [&](const AVPacket& source,
            MediaStreamKind kind) -> ::media::Result<MediaBufferRef> {
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
            std::move(clone), kind, timing.value(), m_packetTimeBase);
        if (!buffer) return ::media::Result<MediaBufferRef>::failure(buffer.error());
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
    auto read = m_session->readFrame();
    if (!read) return ::media::Result<MediaNodeProcessResult>::failure(read.error());
    auto envelope = std::move(read).value();
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
    const bool video = packet->stream_index == m_videoStreamIndex;
    const bool audio = packet->stream_index == m_audioStreamIndex;
    if (!video && !audio) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode packet stream/PID mismatch"));
    const auto streamKind = video ? MediaStreamKind::Video : MediaStreamKind::Audio;
    if (packet->time_base.num <= 0 || packet->time_base.den <= 0) {
        packet->time_base = AVRational{
            m_packetTimeBase.num, m_packetTimeBase.den};
    }
    if (envelope.provenance.readiness == MediaSourceClockReadiness::Acquiring) {
        if (envelope.provenance.evidenceByteOffset ||
            envelope.provenance.originByteOffset) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MpegTsDemuxNode acquiring provenance cannot identify a PES"));
        }
        if (auto status = m_acquiringPackets->retain(
                std::move(packet), streamKind); !status) {
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
                std::move(packet), streamKind); !status) {
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
              std::move(packet), streamKind, outputCheckpoint)
        : prepareLockedBatch(
              std::move(packet), streamKind, outputCheckpoint);
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
    m_session.reset(); m_projection.reset(); m_policy.reset();
    m_packetTimeBase = {};
    m_videoStreamIndex = -1; m_audioStreamIndex = -1;
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
