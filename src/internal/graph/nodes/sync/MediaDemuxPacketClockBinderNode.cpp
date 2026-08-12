#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNode.h"

#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaDemuxClockBinderGenerationIdentities.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

::media::Status requireDemuxTerminalControl(
    const MediaControlBufferClassification& classification)
{
    if (classification.flow != MediaControlBufferFlow::Terminal ||
        (classification.kind != MediaControlBufferKind::Eof &&
         classification.kind != MediaControlBufferKind::Abort)) {
        return ::media::Status::failure(
            invalid("Demux clock binder requires EOF or Abort terminal control"));
    }
    return ::media::Status::success();
}

} // namespace

class MediaDemuxPacketClockBinderState final
    : public MediaAvGenerationPurgeTarget {
public:
    MediaDemuxPacketClockBinderState(
        std::shared_ptr<MediaDemuxTimestampClockMapper> mapper,
        MediaScheduledStream stream)
        : m_mapper(std::move(mapper))
        , m_stream(stream)
        , generation(m_mapper->config().initialGeneration)
    {
    }

    ::media::Status purge(const MediaAvGenerationPurge& purge) override
    {
        std::lock_guard lock(mutex);
        if (generation != purge.oldGeneration ||
            purge.nextGeneration <= purge.oldGeneration ||
            purge.transitionSequence == 0 ||
            (lastTransitionSequence &&
             purge.transitionSequence <= *lastTransitionSequence)) {
            return ::media::Status::failure(
                invalid("Demux clock binder purge requires its exact planned transition"));
        }
        auto purged = m_mapper->purgeParticipant(m_stream, purge);
        if (!purged) return purged;
        pendingInput.reset();
        publishedClockRevision.reset();
        generation = purge.nextGeneration;
        lastTransitionSequence = purge.transitionSequence;
        return ::media::Status::success();
    }

    ::media::Status resetLifecycle()
    {
        std::lock_guard lock(mutex);
        if (m_stream == MediaScheduledStream::Video) {
            auto reset = m_mapper->resetLifecycle();
            if (!reset) return reset;
        }
        pendingInput.reset();
        publishedClockRevision.reset();
        generation = m_mapper->config().initialGeneration;
        lastTransitionSequence.reset();
        return ::media::Status::success();
    }

private:
    friend class MediaDemuxPacketClockBinderNode;

    std::shared_ptr<MediaDemuxTimestampClockMapper> m_mapper;
    const MediaScheduledStream m_stream;
    mutable std::mutex mutex;
    MediaBufferRef pendingInput;
    std::optional<std::uint64_t> publishedClockRevision;
    std::uint64_t generation;
    std::optional<std::uint64_t> lastTransitionSequence;
};

MediaDemuxPacketClockBinderNode::MediaDemuxPacketClockBinderNode(
    MediaNodeId nodeId,
    MediaScheduledStream stream,
    MediaRational plannedTimeBase,
    MediaPreparedDemuxFirstPacketEvidence firstPacket,
    std::shared_ptr<MediaDemuxTimestampClockMapper> mapper,
    std::shared_ptr<MediaAvSyncGroupRuntime> syncGroup)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaDemuxPacketClockBinderNode")
    , m_stream(stream)
    , m_streamKind(
          stream == MediaScheduledStream::Video
              ? MediaStreamKind::Video
              : MediaStreamKind::Audio)
    , m_plannedTimeBase(plannedTimeBase)
    , m_firstPacket(std::move(firstPacket))
    , m_mapper(std::move(mapper))
    , m_syncGroup(std::move(syncGroup))
    , m_state(std::make_shared<MediaDemuxPacketClockBinderState>(
          m_mapper, stream))
{
}

MediaNodeKind MediaDemuxPacketClockBinderNode::staticKind() noexcept
{
    return MediaNodeKind::DemuxPacketClockBinder;
}

std::string_view
MediaDemuxPacketClockBinderNode::generationPurgeIdentity() const noexcept
{
    return m_stream == MediaScheduledStream::Video
        ? MediaDemuxVideoClockBinderGenerationIdentity
        : MediaDemuxAudioClockBinderGenerationIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaDemuxPacketClockBinderNode::generationPurgeTarget() const noexcept
{
    return m_state;
}

::media::Status MediaDemuxPacketClockBinderNode::start(
    MediaGraphExecutionContext& context)
{
    if (!m_mapper || !m_syncGroup || !m_syncGroup->clock() ||
        m_plannedTimeBase.num <= 0 || m_plannedTimeBase.den <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Demux clock binder requires compiler-injected mapper and sync group"));
    }
    if (auto reset = resetLifecycle(); !reset) return reset;
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaDemuxPacketClockBinderNode::stop(
    MediaGraphExecutionContext& context)
{
    auto reset = resetLifecycle();
    auto stopped = FFmpegNodeRuntime::stop(context);
    return !reset ? reset : stopped;
}

void MediaDemuxPacketClockBinderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetLifecycle();
    FFmpegNodeRuntime::abort(context);
}

bool MediaDemuxPacketClockBinderNode::pendingOutputIsCurrent(
    const MediaBufferRef& buffer) const noexcept
{
    auto evidence = outputCommitEvidence(buffer);
    return evidence &&
        m_mapper->outputIsCurrent(evidence.value());
}

::media::Result<MediaDemuxTimestampOutputCommitEvidence>
MediaDemuxPacketClockBinderNode::outputCommitEvidence(
    const MediaBufferRef& buffer) const
{
    using Result =
        ::media::Result<MediaDemuxTimestampOutputCommitEvidence>;
    if (!buffer) {
        return Result::failure(
            invalid("Demux binder output evidence requires a buffer"));
    }
    if (const auto* packet =
            dynamic_cast<const FFmpegPacketBuffer*>(buffer.get())) {
        if (packet->sourceTiming()) {
            return Result::success(
                MediaDemuxTimestampOutputCommitEvidence{
                    MediaSourceClockReadiness::Locked,
                    packet->sourceTiming()->generation});
        }
        return Result::failure(
            invalid("Demux binder packet output lacks generation evidence"));
    }
    if (const auto* state =
            dynamic_cast<const MediaSourceClockStateBuffer*>(buffer.get())) {
        return Result::success(
            MediaDemuxTimestampOutputCommitEvidence{
                state->readiness(), state->generation()});
    }
    auto classified = classifyMediaControlBuffer(buffer);
    if (!classified) {
        return Result::failure(classified.error());
    }
    if (auto terminal =
            requireDemuxTerminalControl(classified.value());
        !terminal) {
        return Result::failure(terminal.error());
    }
    const auto generation =
        classified.value().control->generation();
    if (!generation) {
        return Result::failure(
            invalid("Demux binder terminal output lacks generation evidence"));
    }
    return Result::success(
        MediaDemuxTimestampOutputCommitEvidence{
            MediaSourceClockReadiness::Locked, *generation});
}

::media::Result<MediaOutputCommitReservation>
MediaDemuxPacketClockBinderNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    auto evidence = outputCommitEvidence(buffer);
    if (!evidence) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
            evidence.error());
    }
    auto reserved = m_mapper->reserveOutputCommit(evidence.value());
    return reserved
        ? ::media::Result<MediaOutputCommitReservation>::success(
              MediaOutputCommitReservation::hold(
                  std::move(reserved).value()))
        : ::media::Result<MediaOutputCommitReservation>::failure(
              reserved.error());
}

::media::Result<MediaBufferRef>
MediaDemuxPacketClockBinderNode::timedPacket(
    MediaBufferRef buffer,
    std::uint64_t generation)
{
    auto* source = dynamic_cast<FFmpegPacketBuffer*>(buffer.get());
    if (!source || !source->packet() || source->sourceTiming() ||
        source->streamKind() != m_streamKind) {
        return ::media::Result<MediaBufferRef>::failure(
            invalid("Demux clock binder requires one untimed matching packet"));
    }
    AVPacket* packet = source->packet();
    if (!m_firstPacketValidated) {
        const auto& provenance = source->demuxProvenance();
        if (!provenance ||
            provenance->origin !=
                MediaDemuxPacketOrigin::PostFindStreamInfoPreparedRead ||
            provenance->ordinal != m_firstPacket.preparedReadOrdinal ||
            packet->stream_index != m_firstPacket.streamIndex ||
            packet->pts != m_firstPacket.pts ||
            packet->dts != m_firstPacket.dts ||
            packet->duration != m_firstPacket.duration) {
            return ::media::Result<MediaBufferRef>::failure(
                invalid("Demux clock binder first packet disagrees with prepared evidence"));
        }
        m_firstPacketValidated = true;
    }
    MediaTimeDescriptor time = source->timeDescriptor();
    const auto timeBaseStatus = [&](int numerator, int denominator,
                                    const char* sourceName) {
        const bool absent = numerator == 0 && denominator == 1;
        if (absent) return ::media::Status::success();
        if (numerator <= 0 || denominator <= 0) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    std::string("Demux clock binder rejects malformed ") +
                    sourceName + " time base"));
        }
        if (numerator != m_plannedTimeBase.num ||
            denominator != m_plannedTimeBase.den) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    std::string("Demux clock binder rejects conflicting ") +
                    sourceName + " time base"));
        }
        return ::media::Status::success();
    };
    auto descriptorTimeBase = timeBaseStatus(
        time.timeBase.num, time.timeBase.den, "descriptor");
    auto packetTimeBase = timeBaseStatus(
        packet->time_base.num, packet->time_base.den, "packet");
    if (packet->pts == AV_NOPTS_VALUE || packet->dts == AV_NOPTS_VALUE ||
        packet->duration <= 0 || !descriptorTimeBase || !packetTimeBase) {
        return ::media::Result<MediaBufferRef>::failure(
            !descriptorTimeBase ? descriptorTimeBase.error()
            : !packetTimeBase ? packetTimeBase.error()
            : invalid("Demux clock binder rejects absent PTS/DTS/duration"));
    }
    time.timeBase = m_plannedTimeBase;

    auto mapped = m_mapper->mapPacket(
        m_stream,
        packet->pts,
        packet->dts,
        AVRational{m_plannedTimeBase.num, m_plannedTimeBase.den},
        packet->duration,
        generation);
    if (!mapped) {
        return ::media::Result<MediaBufferRef>::failure(mapped.error());
    }
    const auto& timestamp = mapped.value();
    if (!timestamp.decodeTime() || !timestamp.duration() ||
        timestamp.duration()->nanoseconds() <= 0 ||
        timestamp.generation() != generation ||
        timestamp.confidence() != MediaTimeMappingConfidence::Locked) {
        return ::media::Result<MediaBufferRef>::failure(
            invalid("Demux clock binder requires complete locked mapped evidence"));
    }

    const MediaFormatDescriptor format = source->formatDescriptor();
    const MediaHardwareDescriptor hardware = source->hardwareDescriptor();
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        source->takePacket(),
        m_streamKind,
        MediaPacketSourceTiming{
            timestamp.presentationTime().nanoseconds(),
            timestamp.decodeTime()->nanoseconds(),
            MediaSourceClockReadiness::Locked,
            generation,
            timestamp.duration()->nanoseconds()});
    if (!wrapped) return wrapped;
    wrapped.value()->setFormatDescriptor(format);
    wrapped.value()->setTimeDescriptor(time);
    wrapped.value()->setHardwareDescriptor(hardware);
    return wrapped;
}

::media::Result<MediaNodeProcessResult>
MediaDemuxPacketClockBinderNode::processPacket(
    MediaGraphExecutionContext& context,
    MediaBufferRef buffer)
{
    auto timed = timedPacket(buffer, m_state->generation);
    if (!timed) {
        const auto snapshot = m_mapper->snapshot();
        if (snapshot.readiness ==
                MediaSourceClockReadiness::Acquiring &&
            timed.error().code == ::media::ErrorCode::NotInitialized) {
            m_state->pendingInput = std::move(buffer);
            return processProgress();
        }
        if (snapshot.readiness ==
                MediaSourceClockReadiness::ReacquireRequired &&
            timed.error().code == ::media::ErrorCode::WouldBlock) {
            return processProgress();
        }
        return ::media::Result<MediaNodeProcessResult>::failure(
            timed.error());
    }
    return processProgress(
        emitOutput(context, "packet", timed.value()));
}

::media::Result<MediaNodeProcessResult>
MediaDemuxPacketClockBinderNode::publishClockState(
    MediaGraphExecutionContext& context)
{
    const auto snapshot = m_mapper->snapshot();
    if (m_stream != MediaScheduledStream::Video ||
        !snapshot.hasTimestampEvidence ||
        m_state->publishedClockRevision == snapshot.revision) {
        return processWaiting();
    }
    const bool discontinuity =
        snapshot.readiness ==
        MediaSourceClockReadiness::ReacquireRequired;
    MediaBufferRef state = makeMediaBufferRef<MediaSourceClockStateBuffer>(
        snapshot.readiness, snapshot.generation, discontinuity);
    m_state->publishedClockRevision = snapshot.revision;
    return processProgress(emitOutput(context, "state", state));
}

::media::Result<MediaNodeProcessResult>
MediaDemuxPacketClockBinderNode::processTerminal(
    MediaGraphExecutionContext& context,
    MediaBufferRef terminal)
{
    auto classified = classifyMediaControlBuffer(terminal);
    if (!classified) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            classified.error());
    }
    if (auto valid =
            requireDemuxTerminalControl(classified.value());
        !valid) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            valid.error());
    }
    if (m_state->pendingInput ||
        m_mapper->snapshot().readiness !=
            MediaSourceClockReadiness::Locked) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            invalid("Demux clock binder cannot terminate before common clock lock"));
    }
    MediaBufferRef generatedTerminal =
        makeMediaBufferRef<MediaControlBuffer>(
            classified.value().kind, m_state->generation);
    return m_stream == MediaScheduledStream::Video
        ? processFinished(
              broadcastControlToAllOutputs(
                  context, generatedTerminal))
        : processFinished(
              emitOutput(context, "packet", generatedTerminal));
}

::media::Result<MediaNodeProcessResult>
MediaDemuxPacketClockBinderNode::onProcess(
    MediaGraphExecutionContext& context)
{
    std::lock_guard lock(m_state->mutex);
    if (m_mapper->snapshot().transitionPending) {
        return processWaiting();
    }
    if (m_stream == MediaScheduledStream::Video) {
        const auto snapshot = m_mapper->snapshot();
        if (snapshot.hasTimestampEvidence &&
            m_state->publishedClockRevision != snapshot.revision) {
            return publishClockState(context);
        }
    }
    if (m_state->pendingInput) {
        const auto snapshot = m_mapper->snapshot();
        if (snapshot.readiness != MediaSourceClockReadiness::Locked) {
            return processWaiting();
        }
        MediaBufferRef pending = std::move(m_state->pendingInput);
        return processPacket(context, std::move(pending));
    }

    auto packet = tryPopInputOptional(context, "packet");
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            packet.error());
    }
    if (!packet.value()) return processWaiting();
    MediaBufferRef input = std::move(*packet.value());
    if (dynamic_cast<const MediaControlBuffer*>(input.get())) {
        return processTerminal(context, std::move(input));
    }
    return processPacket(context, std::move(input));
}

::media::Status MediaDemuxPacketClockBinderNode::resetLifecycle()
{
    m_firstPacketValidated = false;
    return m_state->resetLifecycle();
}

} // namespace media::ffmpeg::graph
