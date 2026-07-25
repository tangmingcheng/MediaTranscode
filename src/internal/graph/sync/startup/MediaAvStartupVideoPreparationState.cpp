#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

#include <array>

namespace media::ffmpeg::graph {

::media::Result<std::shared_ptr<MediaAvStartupVideoPreparationState>>
MediaAvStartupVideoPreparationState::create(MediaAvSyncGroupKey groupKey)
{
    if (!groupKey.valid()) {
        return ::media::Result<std::shared_ptr<
            MediaAvStartupVideoPreparationState>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Video preparation state requires a valid sync group"));
    }
    return ::media::Result<std::shared_ptr<
        MediaAvStartupVideoPreparationState>>::success(
        std::shared_ptr<MediaAvStartupVideoPreparationState>(
            new MediaAvStartupVideoPreparationState(std::move(groupKey))));
}

MediaAvStartupVideoPreparationState::MediaAvStartupVideoPreparationState(
    MediaAvSyncGroupKey groupKey)
    : m_groupKey(std::move(groupKey))
{
}

::media::Status MediaAvStartupVideoPreparationState::failureLocked(
    const char* operation) const
{
    if (m_terminalError) return ::media::Status::failure(*m_terminalError);
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        std::string("Video preparation state rejects ") + operation));
}

::media::Status MediaAvStartupVideoPreparationState::validateIdentityLocked(
    std::uint64_t generation,
    std::uint64_t releaseIdentity) const
{
    if (m_terminalError) return ::media::Status::failure(*m_terminalError);
    if (generation == 0 || releaseIdentity == 0 ||
        generation != m_generation || releaseIdentity != m_releaseIdentity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video preparation state rejects mismatched release identity"));
    }
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::begin(
    std::uint64_t generation,
    std::uint64_t releaseIdentity,
    std::size_t videoUnitCount)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_terminalError || m_phase !=
            MediaAvStartupVideoPreparationPhase::Awaiting ||
        generation == 0 || releaseIdentity == 0 || videoUnitCount == 0) {
        return failureLocked("begin");
    }
    m_generation = generation;
    m_releaseIdentity = releaseIdentity;
    m_videoUnitCount = videoUnitCount;
    m_phase = MediaAvStartupVideoPreparationPhase::Feeding;
    return ::media::Status::success();
}

MediaAvStartupVideoPreparationState::VideoReservation
MediaAvStartupVideoPreparationState::reserveNextVideoUnit(
    std::uint64_t generation,
    std::uint64_t releaseIdentity)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (auto identity = validateIdentityLocked(generation, releaseIdentity);
        !identity) {
        return VideoReservation::failure(identity.error());
    }
    if (m_phase != MediaAvStartupVideoPreparationPhase::Feeding) {
        if (m_phase == MediaAvStartupVideoPreparationPhase::FilterReady &&
            !m_reservedVideoUnit) {
            return VideoReservation::success({
                MediaAvStartupVideoReservationKind::NoReservation,
                std::nullopt});
        }
        if (m_phase != MediaAvStartupVideoPreparationPhase::FilterReady) {
            return VideoReservation::failure(
                failureLocked("video reservation").error());
        }
    }
    if (m_reservedVideoUnit) {
        return VideoReservation::success({
            MediaAvStartupVideoReservationKind::Reserved,
            m_reservedVideoUnit});
    }
    if (m_committedVideoUnits == m_videoUnitCount) {
        return VideoReservation::success({
            MediaAvStartupVideoReservationKind::NoReservation,
            std::nullopt});
    }
    m_reservedVideoUnit = m_committedVideoUnits;
    return VideoReservation::success({
        MediaAvStartupVideoReservationKind::Reserved,
        m_reservedVideoUnit});
}

::media::Status MediaAvStartupVideoPreparationState::commitVideoUnit(
    std::uint64_t generation,
    std::uint64_t releaseIdentity,
    std::size_t index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (auto identity = validateIdentityLocked(generation, releaseIdentity);
        !identity) return identity;
    if ((m_phase != MediaAvStartupVideoPreparationPhase::Feeding &&
         m_phase != MediaAvStartupVideoPreparationPhase::FilterReady) ||
        !m_reservedVideoUnit || *m_reservedVideoUnit != index ||
        index != m_committedVideoUnits) {
        return failureLocked("video reservation commit");
    }
    ++m_committedVideoUnits;
    m_reservedVideoUnit.reset();
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::markFilterReady(
    std::uint64_t generation,
    std::uint64_t releaseIdentity,
    MediaOutputCapacityReservationHandle reservation)
{
    std::shared_ptr<MediaNodeWakeup> wakeup;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto identity = validateIdentityLocked(generation, releaseIdentity);
            !identity) return identity;
        if (m_phase != MediaAvStartupVideoPreparationPhase::Feeding ||
            !reservation.valid() || m_filterOutputReservation.valid()) {
            return failureLocked("filter readiness");
        }
        m_filterOutputReservation = std::move(reservation);
        m_phase = MediaAvStartupVideoPreparationPhase::FilterReady;
        wakeup = m_sequencerWakeup.lock();
    }
    if (wakeup) wakeup->notify();
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::registerExtractorOutputs(
    std::uint64_t generation,
    std::uint64_t releaseIdentity,
    MediaOutputCapacityReservationHandle reservation)
{
    std::shared_ptr<MediaNodeWakeup> wakeup;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto identity = validateIdentityLocked(generation, releaseIdentity);
            !identity) return identity;
        if (m_phase != MediaAvStartupVideoPreparationPhase::FilterReady ||
            !reservation.valid() || m_extractorOutputsReservation.valid()) {
            return failureLocked("extractor output reservation");
        }
        m_extractorOutputsReservation = std::move(reservation);
        wakeup = m_sequencerWakeup.lock();
    }
    if (wakeup) wakeup->notify();
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::publishInitialAnchor(
    std::uint64_t generation,
    std::uint64_t releaseIdentity,
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin)
{
    std::shared_ptr<MediaNodeWakeup> extractor;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto identity = validateIdentityLocked(generation, releaseIdentity);
            !identity) return identity;
        if (m_phase != MediaAvStartupVideoPreparationPhase::FilterReady ||
            !m_extractorOutputsReservation.valid() || m_anchoredEpoch ||
            epoch.generation != generation ||
            audioOrigin.generation != generation ||
            audioOrigin.sourceStart != epoch.sourceStart ||
            audioOrigin.masterRelease != epoch.masterRelease) {
            return failureLocked("initial epoch anchor");
        }
        m_anchoredEpoch = epoch;
        m_anchoredAudioOrigin = audioOrigin;
        extractor = m_extractorWakeup.lock();
    }
    if (extractor) extractor->notify();
    return ::media::Status::success();
}

::media::Status
MediaAvStartupVideoPreparationState::acknowledgeExtractorReanchor(
    std::uint64_t generation,
    std::uint64_t releaseIdentity)
{
    std::shared_ptr<MediaNodeWakeup> sequencer;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto identity = validateIdentityLocked(generation, releaseIdentity);
            !identity) return identity;
        if (m_phase != MediaAvStartupVideoPreparationPhase::FilterReady ||
            !m_anchoredEpoch || !m_anchoredAudioOrigin ||
            m_extractorOutputsReanchored) {
            return failureLocked("extractor reanchor acknowledgement");
        }
        m_extractorOutputsReanchored = true;
        sequencer = m_sequencerWakeup.lock();
    }
    if (sequencer) sequencer->notify();
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::authorizeRelease(
    std::uint64_t generation,
    std::uint64_t releaseIdentity,
    const MediaReservedOutputTransaction::Authorization& activation)
{
    std::shared_ptr<MediaNodeWakeup> filter;
    std::shared_ptr<MediaNodeWakeup> extractor;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (auto identity = validateIdentityLocked(generation, releaseIdentity);
            !identity) return identity;
        if (m_phase != MediaAvStartupVideoPreparationPhase::FilterReady ||
            !m_filterOutputReservation.valid() ||
            !m_extractorOutputsReservation.valid() || !m_anchoredEpoch ||
            !m_anchoredAudioOrigin || !m_extractorOutputsReanchored) {
            return failureLocked("release authorization");
        }
        const std::array<MediaOutputCapacityReservationHandle, 2> handles{
            m_filterOutputReservation, m_extractorOutputsReservation};
        if (auto authorized = MediaReservedOutputTransaction::authorize(
                handles, activation); !authorized) {
            return authorized;
        }
        m_phase = MediaAvStartupVideoPreparationPhase::ReleaseCommitted;
        filter = m_filterWakeup.lock();
        extractor = m_extractorWakeup.lock();
    }
    if (filter) filter->notify();
    if (extractor) extractor->notify();
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::cancel()
{
    std::shared_ptr<MediaNodeWakeup> sequencer;
    std::shared_ptr<MediaNodeWakeup> filter;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_phase == MediaAvStartupVideoPreparationPhase::Cancelled) {
            return ::media::Status::success();
        }
        if (!m_terminalError) {
            m_terminalError = ::media::ErrorInfo::cancelled(
                "Video preparation state was cancelled");
        }
        m_phase = MediaAvStartupVideoPreparationPhase::Cancelled;
        m_generation = 0;
        m_releaseIdentity = 0;
        m_videoUnitCount = 0;
        m_reservedVideoUnit.reset();
        m_filterOutputReservation = {};
        m_extractorOutputsReservation = {};
        m_anchoredEpoch.reset();
        m_anchoredAudioOrigin.reset();
        m_extractorOutputsReanchored = false;
        sequencer = m_sequencerWakeup.lock();
        filter = m_filterWakeup.lock();
    }
    if (sequencer) sequencer->notify();
    if (filter) filter->notify();
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::bindSequencerWakeup(
    const std::shared_ptr<MediaNodeWakeup>& wakeup)
{
    if (!wakeup) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "Video preparation state requires a sequencer wakeup"));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sequencerWakeup = wakeup;
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::bindFilterWakeup(
    const std::shared_ptr<MediaNodeWakeup>& wakeup)
{
    if (!wakeup) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "Video preparation state requires a filter wakeup"));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filterWakeup = wakeup;
    return ::media::Status::success();
}

::media::Status MediaAvStartupVideoPreparationState::bindExtractorWakeup(
    const std::shared_ptr<MediaNodeWakeup>& wakeup)
{
    if (!wakeup) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "Video preparation state requires an extractor wakeup"));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_extractorWakeup = wakeup;
    return ::media::Status::success();
}

MediaAvStartupVideoPreparationSnapshot
MediaAvStartupVideoPreparationState::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return {m_groupKey, m_phase, m_generation, m_releaseIdentity,
            m_committedVideoUnits, m_videoUnitCount,
            m_filterOutputReservation.valid(),
            m_extractorOutputsReservation.valid(), m_anchoredEpoch,
            m_anchoredAudioOrigin, m_extractorOutputsReanchored};
}

} // namespace media::ffmpeg::graph
