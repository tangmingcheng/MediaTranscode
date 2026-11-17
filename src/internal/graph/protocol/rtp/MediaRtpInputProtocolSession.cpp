#include "internal/graph/protocol/rtp/MediaRtpInputProtocolSession.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/depacketizer/MediaAacRtpDepacketizer.h"
#include "internal/graph/utils/MediaAsciiStringUtils.h"

#include <algorithm>
#include <new>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpInputProtocolSession> MediaRtpInputProtocolSession::create(
    MediaRtpDepacketizerConfig config, MediaPreparedRtpAccessUnitEnvelope envelope,
    MediaRtpReorderConfig reorder, MediaRtpInputClockSessionConfig clock,
    std::unique_ptr<MediaRtpDepacketizer> depacketizer,
        MediaLineageAllocationScope allocation) try
{
    using Result = ::media::Result<MediaRtpInputProtocolSession>;
    if (auto status = MediaRtpDepacketizerFactory::validate(config); !status)
        return Result::failure(status.error());
    if (auto status = envelope.validate(); !status) return Result::failure(status.error());
    if (!depacketizer || !reorder.windowPackets ||
        reorder.windowPackets > (std::numeric_limits<std::size_t>::max)() / 2 - 1 ||
        reorder.maximumDelay.count() <= 0 ||
        reorder.payloadType != config.payloadType || envelope.streamKind != config.streamKind ||
        lowercaseAscii(envelope.codecName) != lowercaseAscii(config.codecName))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP protocol session requires matching planned protocol contracts"));
    auto session = MediaRtpInputClockSession::create(clock, allocation);
    if (!session) return Result::failure(session.error());
    return Result::success(MediaRtpInputProtocolSession(std::move(config), std::move(envelope),
        reorder, std::move(session).value(), std::move(depacketizer), std::move(allocation)));
}
catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtpInputProtocolSession>::failure(
        ::media::ErrorInfo::allocationFailed("RTP protocol session allocation failed"));
}


MediaRtpInputProtocolSession::MediaRtpInputProtocolSession(
    MediaRtpDepacketizerConfig config, MediaPreparedRtpAccessUnitEnvelope envelope,
    MediaRtpReorderConfig reorder, MediaRtpInputClockSession clock,
    std::unique_ptr<MediaRtpDepacketizer> depacketizer, MediaLineageAllocationScope allocation)
    : m_config(std::move(config)), m_envelope(std::move(envelope)),
      m_reorder(makeMediaLineageUnique<MediaRtpReorderBuffer>(allocation, reorder)),
      m_clock(std::move(clock)), m_depacketizer(std::move(depacketizer)), m_allocation(std::move(allocation)),
      // polymorphic_allocator::new_object injects the same resource into the
      // PMR deque constructor through uses_allocator construction.
      m_pending(makeMediaLineageUnique<std::pmr::deque<PendingItem>>(m_allocation)),
      m_pendingCapacity(2 * (reorder.windowPackets + 1)) {}

::media::Status MediaRtpInputProtocolSession::observeRtp(
    MediaRtpPacket packet, std::int64_t observedAtNs)
{
    if (observedAtNs <= 0 || hasPending()) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "RTP protocol observation requires a positive arrival time and drained pending items"));
    const auto generationBefore = m_clock.observeMedia(packet.ssrc);
    if (!generationBefore) return ::media::Status::failure(generationBefore.error());
    auto reordered = m_reorder->push(std::move(packet),
        std::chrono::steady_clock::time_point(std::chrono::nanoseconds(observedAtNs)));
    if (!reordered) return ::media::Status::failure(reordered.error());
    return enqueueReordered(std::move(reordered).value(), generationBefore.value());
}

::media::Status MediaRtpInputProtocolSession::expire(std::chrono::steady_clock::time_point now)
{
    if (hasPending()) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "RTP protocol expiry requires drained pending items"));
    auto reordered = m_reorder->expire(now);
    if (!reordered) return ::media::Status::failure(reordered.error());
    return enqueueReordered(std::move(reordered).value(), m_clock.generation());
}

::media::Status MediaRtpInputProtocolSession::enqueueReordered(
    MediaRtpReorderResult reordered, std::uint64_t generationBefore)
try {
    if (reordered.packets.size() > m_pendingCapacity ||
        reordered.discontinuities.size() > m_pendingCapacity - reordered.packets.size())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP reorder result exceeds pending owner capacity"));
    auto next = reordered.packets.begin();
    for (const auto& discontinuity : reordered.discontinuities) {
        while (next != reordered.packets.end() && next->sequenceNumber != discontinuity.resumedSequence)
            m_pending->emplace_back(std::move(*next++));
        m_pending->emplace_back(std::pair{discontinuity, generationBefore});
    }
    for (; next != reordered.packets.end(); ++next) m_pending->emplace_back(std::move(*next));
    return ::media::Status::success();
} catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("RTP pending owner allocation failed"));
}

bool MediaRtpInputProtocolSession::hasPending() const noexcept { return !m_pending->empty(); }
const MediaRtpInputProtocolSession::PendingItem& MediaRtpInputProtocolSession::pendingFront() const
{ return m_pending->front(); }
void MediaRtpInputProtocolSession::consumeFront() { m_pending->pop_front(); }

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputProtocolSession::prepareDiscontinuity(
    std::uint64_t generationBefore) const
{ return m_clock.prepareDiscontinuity(generationBefore); }

::media::Status MediaRtpInputProtocolSession::commitDiscontinuity(
    MediaRtpDiscontinuityReason reason, MediaRtpPreparedClockObservation prepared)
{
    if (auto status = m_clock.commitClockObservation(std::move(prepared)); !status) return status;
    m_depacketizer->discontinuity(reason);
    requireKeyframe();
    releasePayloadState();
    return ::media::Status::success();
}

::media::Result<MediaRtpPacketPreparation> MediaRtpInputProtocolSession::preparePacket(
    const MediaRtpPacket& packet, std::size_t heldReservations) const
try {
    using Result = ::media::Result<MediaRtpPacketPreparation>;
    if (m_waitingForClockEvidence) return Result::success(MediaRtpPacketAllocationAction{true, false,
        MediaLineageVector<std::uint64_t>{MediaLineageAllocator<std::uint64_t>{m_allocation}}});
    const auto codec = lowercaseAscii(m_config.codecName);
    MediaRtpPacketAllocationAction action{false,
        (codec == "h264" || codec == "hevc") && m_reservedAccessUnitTimestamp &&
        *m_reservedAccessUnitTimestamp != packet.timestamp,
        MediaLineageVector<std::uint64_t>{MediaLineageAllocator<std::uint64_t>{m_allocation}}};
    // A held AAC fragment still requires header, marker and continuity
    // validation before its existing reservation authorizes another push.
    if (codec == "aac") {
        const auto* aac = dynamic_cast<const MediaAacRtpDepacketizer*>(m_depacketizer.get());
        if (!aac) return Result::failure(::media::ErrorInfo::internalError(
            "AAC admission requires its AAC depacketizer owner"));
        auto prepared = aac->preparePacket(packet, heldReservations ? std::nullopt
            : std::optional<std::uint64_t>{m_envelope.maximumAccessUnitsPerPush});
        if (const auto* rejected = std::get_if<MediaRtpPacketRejection>(&prepared))
            return Result::success(*rejected);
        const auto& headers = std::get<MediaAacRtpPacketPlan>(prepared).headers;
        if (heldReservations) return Result::success(std::move(action));
        try {
            action.reserveBytes.reserve(headers.accessUnits.size());
            for (const auto& unit : headers.accessUnits) action.reserveBytes.push_back(unit.size);
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed("RTP payload credit request"));
        }
        return Result::success(std::move(action));
    }
    if (heldReservations && !action.releaseExisting) return Result::success(std::move(action));
    try {
        if (codec == "opus") action.reserveBytes.push_back(packet.payload.size());
        else action.reserveBytes.push_back(m_envelope.maximumAccessUnitBytes);
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("RTP payload credit request"));
    }
    return Result::success(std::move(action));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtpPacketPreparation>::failure(
        ::media::ErrorInfo::allocationFailed("RTP payload request owner allocation failed"));
}

::media::Status MediaRtpInputProtocolSession::commitPacketRejection(
    const MediaRtpPacketRejection& rejection)
{
    auto* aac = dynamic_cast<MediaAacRtpDepacketizer*>(m_depacketizer.get());
    if (!aac) return ::media::Status::failure(::media::ErrorInfo::internalError(
        "Prepared AAC rejection requires its AAC depacketizer owner"));
    auto failure = aac->commitRejection(rejection);
    releasePayloadState();
    return failure;
}

::media::Result<MediaRtpPacketCompletion> MediaRtpInputProtocolSession::commitPacket(
    const MediaRtpPacket& packet, std::size_t admittedReservations)
try {
    using Result = ::media::Result<MediaRtpPacketCompletion>;
    if (!admittedReservations || m_waitingForClockEvidence)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP depacketizer push requires admitted payload reservations"));
    const auto codec = lowercaseAscii(m_config.codecName);
    if (codec != "aac" && codec != "opus") m_reservedAccessUnitTimestamp = packet.timestamp;
    auto media = m_depacketizer->push(packet);
    if (!media) { releasePayloadState(); return Result::failure(media.error()); }
    const auto count = media.value().accessUnits.size();
    if (count > m_envelope.maximumAccessUnitsPerPush || (count && count != admittedReservations)) {
        releasePayloadState();
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP depacketizer completion conflicts with its prepared payload reservation"));
    }
    const bool release = count || packet.marker;
    if (release) releasePayloadState();
    return Result::success({std::move(media).value(), release});
} catch (const std::bad_alloc&) {
    releasePayloadState();
    return ::media::Result<MediaRtpPacketCompletion>::failure(
        ::media::ErrorInfo::allocationFailed("RTP completion owner allocation failed"));
}

MediaRtpAccessUnitAdmission MediaRtpInputProtocolSession::admitAccessUnit(bool keyFrame) noexcept
{
    if (m_waitingForKeyFrame && !keyFrame) return {false, false};
    const bool recovered = m_waitingForKeyFrame;
    m_waitingForKeyFrame = false;
    return {true, recovered};
}

void MediaRtpInputProtocolSession::releasePayloadState() noexcept
{ m_reservedAccessUnitTimestamp.reset(); }
void MediaRtpInputProtocolSession::requireKeyframe() noexcept
{ m_waitingForKeyFrame = *m_config.waitForKeyFrameAfterLoss; }
void MediaRtpInputProtocolSession::resetAssembly() noexcept
{
    m_reorder->reset();
    m_pending->clear();
    m_depacketizer->discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    releasePayloadState();
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputProtocolSession::prepareRtcp(
    std::span<const std::uint8_t> bytes, std::int64_t observedAtNs, std::int64_t evaluationAtNs,
    MediaRtpByeHandling byeHandling) const
{
    return m_clock.prepareRtcp(bytes, observedAtNs, evaluationAtNs, byeHandling);
}

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputProtocolSession::prepareEvidence(std::int64_t now) const
{ return m_clock.prepareEvidence(now); }

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputProtocolSession::prepareClockTransition(std::int64_t now) const
{ return m_clock.prepareClockTransition(now); }

::media::Result<MediaRtpPreparedClockObservation> MediaRtpInputProtocolSession::prepareMemberRetirement(
    std::uint64_t expectedIngressGeneration, std::uint32_t expectedSsrc,
    MediaRtpSourceUnavailableReason reason, std::int64_t observedAtNs) const
{ return m_clock.prepareMemberRetirement(expectedIngressGeneration, expectedSsrc, reason, observedAtNs); }

::media::Status MediaRtpInputProtocolSession::commitClockObservation(MediaRtpPreparedClockObservation&& prepared)
{
    const bool invalidated = prepared.tracker.invalidated || prepared.retireAssembly;
    const bool evidence = prepared.evidencePublished;
    if (auto status = m_clock.commitClockObservation(std::move(prepared)); !status) return status;
    if (invalidated) {
        m_waitingForClockEvidence = true;
        requireKeyframe();
        resetAssembly();
    } else if (evidence) m_waitingForClockEvidence = false;
    return ::media::Status::success();
}

::media::Result<int> MediaRtpInputProtocolSession::receiveTimeoutMs(
    std::chrono::steady_clock::time_point reorderNow, std::int64_t clockNowNs,
    int plannedMaximumMs) const
{
    if (const auto deadline = hasPending() ? std::nullopt : m_reorder->nextDeadline()) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - reorderNow);
        plannedMaximumMs = std::min(plannedMaximumMs,
            remaining.count() > 0 ? static_cast<int>(remaining.count()) : 1);
    }
    return m_clock.receiveTimeoutMs(clockNowNs, plannedMaximumMs);
}
std::optional<std::int64_t> MediaRtpInputProtocolSession::nextClockDeadlineNs() const noexcept
{ return m_clock.nextDeadlineNs(); }

std::uint64_t MediaRtpInputProtocolSession::generation() const noexcept { return m_clock.generation(); }
const MediaRtpInputClockSessionConfig& MediaRtpInputProtocolSession::clockConfig() const noexcept
{ return m_clock.config(); }

} // namespace media::ffmpeg::graph
