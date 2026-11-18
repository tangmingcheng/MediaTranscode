#include "internal/graph/runtime/session/MediaRtpPacketClockSession.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
extern "C" {
#include <libavutil/avutil.h>
}
#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpPacketClockSession> MediaRtpPacketClockSession::create(
    MediaRtpPacketClockSessionConfig config, MediaLineageAllocationScope allocation)
try {
    using Result = ::media::Result<MediaRtpPacketClockSession>;
    if (!MediaLineageAllocator<std::byte>{allocation}.resource() ||
        (config.stream != MediaStreamKind::Video && config.stream != MediaStreamKind::Audio) ||
        (config.members != MediaTranscodeStreamSet::VideoOnly && config.members != MediaTranscodeStreamSet::AudioVideo) ||
        (config.stream == MediaStreamKind::Audio && config.members != MediaTranscodeStreamSet::AudioVideo) ||
        !config.acquiringCapacity ||
        (config.stream == MediaStreamKind::Video) != config.videoClockRate.has_value() ||
        (config.stream == MediaStreamKind::Video) != config.videoTiming.has_value() ||
        (config.videoClockRate && *config.videoClockRate <= 0))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP clock binding requires complete stream and capacity facts"));
    if (config.videoTiming) {
        const auto& timing = *config.videoTiming;
        if ((timing.product != MediaRtpVideoTimingProduct::CanonicalInterval &&
             timing.product != MediaRtpVideoTimingProduct::SourcePresentation) ||
            (timing.product == MediaRtpVideoTimingProduct::SourcePresentation) != timing.sourceIdentity.has_value() ||
            (timing.sourceIdentity && timing.sourceIdentity->empty()))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTP video timing requires an explicit product and matching source identity"));
    }
    auto deadline = MediaInitialClockAcquisitionDeadline::create(config.acquisitionTimeout);
    if (!deadline) return Result::failure(deadline.error());
    return Result::success(MediaRtpPacketClockSession(config, std::move(deadline).value(), std::move(allocation)));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtpPacketClockSession>::failure(
        ::media::ErrorInfo::allocationFailed("RTP clock binder allocation failed"));
}

MediaRtpPacketClockSession::MediaRtpPacketClockSession(
    MediaRtpPacketClockSessionConfig config, MediaInitialClockAcquisitionDeadline deadline,
    MediaLineageAllocationScope allocation)
    : m_allocation(std::move(allocation)),
      m_acquiringPackets(makeMediaLineageUnique<std::pmr::deque<MediaBufferRef>>(m_allocation.scope())),
      m_acquisitionDeadline(std::move(deadline)), m_acquiringCapacity(config.acquiringCapacity),
      m_videoClockRate(config.videoClockRate), m_videoTiming(std::move(config.videoTiming)),
      m_streamKind(config.stream), m_scheduledStream(config.stream == MediaStreamKind::Video
          ? MediaScheduledStream::Video : MediaScheduledStream::Audio) {}

::media::Result<MediaRtpPacketClockSession::PacketDisposition>
MediaRtpPacketClockSession::classifyPacket(const MediaBufferRef& buffer) const
{
    using Result = ::media::Result<PacketDisposition>;
    const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    if (!packet || packet->streamKind() != m_streamKind || !packet->rtpProvenance())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP binder requires matching raw packet provenance"));
    const auto& raw = *packet->rtpProvenance();
    if (raw.ingressSequence == 0)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP binder rejects zero ingress sequence"));
    if (m_ingressCutoff && raw.ingressSequence <= *m_ingressCutoff)
        return Result::success(PacketDisposition::Retired);
    if (!m_lockedSnapshot) return Result::success(PacketDisposition::AwaitClock);
    const auto& calibration = m_streamKind == MediaStreamKind::Video
        ? m_lockedSnapshot->locked->video : *m_lockedSnapshot->locked->audio;
    if (raw.ingressGeneration < calibration.generation)
        return Result::success(PacketDisposition::Retired);
    if (raw.ingressGeneration > calibration.generation)
        return Result::success(PacketDisposition::AwaitClock);
    if (raw.ssrc != calibration.ssrc)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP binder packet SSRC conflicts with its raw generation calibration"));
    return Result::success(PacketDisposition::Bind);
}

::media::Result<MediaBufferRef> MediaRtpPacketClockSession::timedPacket(
    MediaBufferRef buffer,
    std::uint64_t& extendedTimestamp)
{
    auto* source = dynamic_cast<FFmpegPacketBuffer*>(buffer.get());
    if (!source || !source->packet() || source->sourceTiming() ||
        source->streamKind() != m_streamKind ||
        source->packet()->pts == AV_NOPTS_VALUE || source->packet()->pts < 0 ||
        source->packet()->pts > std::numeric_limits<std::uint32_t>::max() ||
        (source->packet()->dts != AV_NOPTS_VALUE &&
         source->packet()->dts != source->packet()->pts)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet binder requires untimed matching packets with raw 32-bit RTP PTS and no distinct DTS"));
    }
    const MediaTimeDescriptor time = source->timeDescriptor();
    const MediaRtpSourceClockCalibration& calibration =
        m_scheduledStream == MediaScheduledStream::Video
        ? m_lockedSnapshot->locked->video
        : *m_lockedSnapshot->locked->audio;
    if (!time.hasKnownTimeBase() || time.timeBase.num != 1 ||
        (m_streamKind == MediaStreamKind::Video &&
         time.timeBase.den != *m_videoClockRate)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet binder requires the planned packet time base"));
    }
    auto aligned = m_timestampAligner.align(
        calibration, static_cast<std::uint32_t>(source->packet()->pts));
    if (!aligned) {
        return ::media::Result<MediaBufferRef>::failure(aligned.error());
    }
    extendedTimestamp = aligned.value();
    auto timing = m_projector.project(
        *m_lockedSnapshot, m_scheduledStream, extendedTimestamp);
    if (!timing) {
        return ::media::Result<MediaBufferRef>::failure(timing.error());
    }
    const bool sourcePresentation = m_videoTiming &&
        m_videoTiming->product == MediaRtpVideoTimingProduct::SourcePresentation;
    std::shared_ptr<const MediaVideoDecodeLineage> lineage;
    if (sourcePresentation) {
        if (!source->rtpProvenance() || !source->rtpProvenance()->ingressSequence ||
            !timing.value().presentationNs)
            return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
                "RTP source presentation requires AU provenance and mapped PTS"));
        timing.value().decodeNs.reset();
        timing.value().durationNs.reset();
        try {
            lineage = std::allocate_shared<const MediaVideoDecodeLineage>(
                std::pmr::polymorphic_allocator<MediaVideoDecodeLineage>{m_allocation.resource()}, MediaVideoDecodeLineage{
                {*m_videoTiming->sourceIdentity,
                 MediaSourceAccessUnitSequence(source->rtpProvenance()->ingressSequence)},
                MediaRunningTime::fromNanoseconds(*timing.value().presentationNs),
                std::nullopt, std::nullopt, MediaTimeMappingConfidence::Locked,
                timing.value().generation, static_cast<std::int64_t>(extendedTimestamp), time.timeBase,
                {calibration.extendedRtpAnchor, calibration.continuousSourceAnchor,
                 calibration.rateNumeratorNs, calibration.rateDenominatorTicks,
                 m_lockedSnapshot->locked->commonSourceEpoch}});
        } catch (const std::bad_alloc&) {
            return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::allocationFailed(
                "RTP source AU identity allocation failed"));
        }
    }
    auto packet = source->takePacket();
    const std::int64_t materializedTimestamp =
        static_cast<std::int64_t>(extendedTimestamp);
    packet->pts = materializedTimestamp;
    if (sourcePresentation) {
        packet->dts = AV_NOPTS_VALUE;
        packet->duration = 0; // FFmpeg unknown duration, not a zero-length interval.
    } else if (packet->dts != AV_NOPTS_VALUE) {
        packet->dts = materializedTimestamp;
    }
    const MediaFormatDescriptor format = source->formatDescriptor();
    const MediaHardwareDescriptor hardware = source->hardwareDescriptor();
    const bool inputKey = source->isKeyFrame();
    auto wrapped = FFmpegBufferFactory::wrapPacket(*m_allocation.resource(),
        std::move(packet), m_streamKind, std::move(timing).value());
    if (!wrapped) return wrapped;
    if (auto status = source->transferPayloadOwnershipTo(*wrapped.value()); !status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    if (m_streamKind == MediaStreamKind::Video && inputKey &&
        !m_keyTraceEmitted) {
        m_keyTraceEmitted = true;
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            std::string("rtp_key_trace stage=clock_binder input=") +
                (inputKey ? "1" : "0") + " output=" +
                (wrapped.value()->isKeyFrame() ? "1" : "0"));
    }
    wrapped.value()->setFormatDescriptor(format);
    wrapped.value()->setTimeDescriptor(time);
    wrapped.value()->setHardwareDescriptor(hardware);
    if (lineage)
        return MediaVideoDecodeBuffer::create(*m_allocation.resource(), std::move(wrapped).value(), std::move(lineage), std::nullopt);
    return wrapped;
}

::media::Result<MediaBufferRef> MediaRtpPacketClockSession::bindPacket(
    MediaBufferRef buffer)
{
    std::uint64_t timestamp = 0;
    auto timed = timedPacket(std::move(buffer), timestamp);
    if (!timed) return ::media::Result<MediaBufferRef>::failure(timed.error());
    if (m_streamKind == MediaStreamKind::Audio ||
        m_videoTiming->product == MediaRtpVideoTimingProduct::SourcePresentation) {
        return timed;
    }
    if (!m_videoLookahead) {
        m_videoLookahead = std::move(timed).value();
        m_videoLookaheadTimestamp = timestamp;
        return ::media::Result<MediaBufferRef>::success({});
    }
    if (!m_videoLookaheadTimestamp || timestamp <= *m_videoLookaheadTimestamp ||
        timestamp - *m_videoLookaheadTimestamp >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument("RTP video binder requires positive representable timestamp delta"));
    }
    const std::int64_t delta = static_cast<std::int64_t>(
        timestamp - *m_videoLookaheadTimestamp);
    auto* prior = dynamic_cast<FFmpegPacketBuffer*>(m_videoLookahead.get());
    if (!prior || !prior->packet()) {
        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument("RTP video binder lost lookahead packet ownership"));
    }
    prior->packet()->duration = delta;
    m_videoLookahead->setTimestamps(
        prior->packet()->pts, prior->packet()->dts, delta);
    MediaBufferRef output = std::move(m_videoLookahead);
    m_videoLookahead = std::move(timed).value();
    m_videoLookaheadTimestamp = timestamp;
    m_lastPositiveVideoDelta = delta;
    return ::media::Result<MediaBufferRef>::success(std::move(output));
}

::media::Result<MediaBufferRef> MediaRtpPacketClockSession::finishPacketInput(
    MediaBufferRef terminal)
{
    if (!m_lockedSnapshot || !m_acquiringPackets->empty()) {
        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument("RTP packet binder cannot terminate before buffered packets are clock-bound"));
    }
    if (m_streamKind == MediaStreamKind::Video && m_videoLookahead) {
        if (!m_lastPositiveVideoDelta) {
            return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument("RTP video binder terminal packet has no observed positive duration"));
        }
        auto* packet = dynamic_cast<FFmpegPacketBuffer*>(m_videoLookahead.get());
        if (!packet || !packet->packet()) {
            return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument("RTP video binder lost terminal lookahead packet ownership"));
        }
        packet->packet()->duration = *m_lastPositiveVideoDelta;
        m_videoLookahead->setTimestamps(
            packet->packet()->pts, packet->packet()->dts,
            *m_lastPositiveVideoDelta);
        MediaBufferRef output = std::move(m_videoLookahead);
        m_videoLookaheadTimestamp.reset();
        m_pendingTerminal = std::move(terminal);
        return ::media::Result<MediaBufferRef>::success(std::move(output));
    }
    m_pendingTerminal = std::move(terminal);
    return ::media::Result<MediaBufferRef>::success({});
}

::media::Status MediaRtpPacketClockSession::acceptClock(
    const MediaRtpClockGroupSnapshot& snapshot, MediaTranscodeStreamSet members)
{
    if (auto status = snapshot.validateMembers(members); !status)
        return status;
    if (snapshot.state != MediaRtpClockGroupState::Locked) {
        if (m_lockedSnapshot) {
            invalidateClockProjection();
        }
        return ::media::Status::success();
    }
    if (snapshot.groupGeneration == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("RTP packet binder requires locked clock evidence"));
    }
    if (m_lockedSnapshot &&
        m_lockedSnapshot->groupGeneration != snapshot.groupGeneration) {
        invalidateClockProjection();
    }
    m_lockedSnapshot = snapshot;
    bool awaitingClock = false;
    for (const auto& packet : *m_acquiringPackets) {
        auto disposition = classifyPacket(packet);
        if (!disposition) return ::media::Status::failure(disposition.error());
        awaitingClock = awaitingClock || disposition.value() == PacketDisposition::AwaitClock;
    }
    if (!awaitingClock) m_acquisitionDeadline->clear();
    return ::media::Status::success();
}

::media::Status MediaRtpPacketClockSession::bufferAcquiring(
    MediaBufferRef buffer, std::optional<MediaRunningTime> observedAt)
{
    if (!m_acquisitionDeadline->deadline()) {
        if (!observedAt) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "RTP acquisition requires an observed master-clock time"));
        if (auto status = m_acquisitionDeadline->establish(*observedAt); !status) return status;
    }
    if (m_acquiringPackets->size() >= m_acquiringCapacity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP packet binder acquiring capacity exhausted: buffered=" +
            std::to_string(m_acquiringPackets->size()) +
            " capacity=" + std::to_string(m_acquiringCapacity)));
    }
    m_acquiringPackets->push_back(std::move(buffer));
    m_acquiringHighWater = (std::max)(m_acquiringHighWater, m_acquiringPackets->size());
    return ::media::Status::success();
}

void MediaRtpPacketClockSession::invalidateClockProjection() noexcept
{
    m_lockedSnapshot.reset();
    m_videoLookahead.reset();
    m_videoLookaheadTimestamp.reset();
    m_lastPositiveVideoDelta.reset();
    m_pendingTerminal.reset();
}

} // namespace media::ffmpeg::graph
