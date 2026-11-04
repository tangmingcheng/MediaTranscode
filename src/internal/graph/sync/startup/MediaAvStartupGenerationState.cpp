#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"
#include <stdexcept>

namespace media::ffmpeg::graph {

MediaAvStartupGenerationState::MediaAvStartupGenerationState(
    MediaAvSyncGroupKey groupKey, MediaLineageAllocationScope allocation,
    MediaAvStartupGenerationStoragePlan storage)
    : m_allocation(std::move(allocation)), m_storage(std::move(storage)), m_groupKey(std::move(groupKey)),
      m_payloadResource(m_storage, 0, *MediaLineageAllocator<std::byte>{m_allocation}.resource()),
      m_seenResource(m_storage, 1, *MediaLineageAllocator<std::byte>{m_allocation}.resource()),
      m_payloads(&m_payloadResource), m_seen(&m_seenResource) {}

::media::Status MediaAvStartupGenerationState::prepareStorage()
try {
    const auto* prepared = std::get_if<MediaPreparedStartupGenerationStorage>(&m_storage.storage);
    if (m_storagePrepared || !m_storage.maximumElements ||
        (prepared != nullptr) != std::holds_alternative<MediaPreparedLineageAllocation>(m_allocation))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Startup generation storage requires its planned domain and one preparation"));
    if (prepared) {
        auto payloadPhase = m_payloadResource.beginRehash();
        if (!payloadPhase) return payloadPhase;
        m_payloads.rehash(prepared->hashes[0].geometry.bucketCount);
        auto payloadSeal = m_payloadResource.seal(m_payloads.bucket_count(), m_payloads.max_load_factor());
        if (!payloadSeal) return payloadSeal;
        auto seenPhase = m_seenResource.beginRehash();
        if (!seenPhase) return seenPhase;
        m_seen.rehash(prepared->hashes[1].geometry.bucketCount);
        auto seenSeal = m_seenResource.seal(m_seen.bucket_count(), m_seen.max_load_factor());
        if (!seenSeal) return seenSeal;
    }
    m_storagePrepared = true;
    return ::media::Status::success();
} catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Startup generation hash preparation allocation failed"));
} catch (const std::length_error&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Startup generation hash capacity is not representable"));
}

::media::Status MediaAvStartupGenerationState::store(
    const MediaAvSyncGroupKey& groupKey,
    const MediaAvStartupAccessUnit& unit,
    MediaBufferRef media)
try
{
    if (!m_storagePrepared)
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Startup generation ownership storage is not prepared"));
    if (groupKey != m_groupKey || !media || unit.generation == 0 ||
        unit.sequence == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup generation state rejects sync-group mismatch"));
    }
    if (m_generation && unit.generation != *m_generation) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup generation state requires an acknowledged generation transition"));
    }
    const MediaAvStartupUnitId id{unit.stream, unit.generation, unit.sequence};
    if (m_seen.contains(id)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup generation state rejects duplicate source sequence"));
    }
    if (unit.stream == MediaAvStartupStream::Audio) {
        if (!unit.audio || unit.audio->sampleRate == 0) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Startup generation state requires audio sample rate"));
        }
        const int rate = static_cast<int>(unit.audio->sampleRate);
        if (m_audioSampleRate && *m_audioSampleRate != rate) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Startup generation state rejects audio sample-rate changes"));
        }
    }
    // Production envelopes are Locked; startup retries erase their purged IDs,
    // and Running passes through without store. take intentionally retains seen.
    if (std::holds_alternative<MediaPreparedStartupGenerationStorage>(m_storage.storage) &&
        (m_seen.size() >= m_storage.maximumElements || m_payloads.size() >= m_storage.maximumElements))
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "Startup generation ownership exceeds its planned element capacity"));
    if (!m_seen.emplace(id).second) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup generation state rejects duplicate source sequence"));
    }
    try {
        if (!m_payloads.emplace(id, std::move(media)).second) {
            m_seen.erase(id);
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "Startup generation state payload ownership diverged from sequence history"));
        }
    } catch (const std::bad_alloc&) {
        m_seen.erase(id);
        throw;
    }
    if (!m_generation) m_generation = unit.generation;
    if (unit.stream == MediaAvStartupStream::Audio) {
        m_audioSampleRate = static_cast<int>(unit.audio->sampleRate);
    }
    return ::media::Status::success();
} catch (const std::bad_alloc&) {
    return ::media::Status::failure(
        ::media::ErrorInfo::allocationFailed("Startup media ownership storage allocation failed"));
}

::media::Result<MediaBufferRef> MediaAvStartupGenerationState::take(
    const MediaAvStartupUnitId& id)
{
    auto found = m_payloads.find(id);
    if (found == m_payloads.end()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::internalError(
                "Startup generation state lost buffered media"));
    }
    MediaBufferRef media = std::move(found->second);
    m_payloads.erase(found);
    return ::media::Result<MediaBufferRef>::success(std::move(media));
}

void MediaAvStartupGenerationState::erase(const MediaAvStartupUnitId& id) noexcept
{
    m_payloads.erase(id);
    m_seen.erase(id);
}

void MediaAvStartupGenerationState::erase(
    const MediaAvStartupIds& ids) noexcept
{
    for (const auto& id : ids) erase(id);
}

::media::Status MediaAvStartupGenerationState::purge(
    const MediaAvGenerationPurge& purge)
{
    if (purge.oldGeneration == 0 || purge.nextGeneration == 0 ||
        purge.transitionSequence == 0 ||
        purge.nextGeneration <= purge.oldGeneration ||
        !m_generation || *m_generation != purge.oldGeneration ||
        (m_lastTransitionSequence &&
         purge.transitionSequence <= *m_lastTransitionSequence)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup generation state rejects invalid purge transition"));
    }
    m_payloads.clear();
    m_seen.clear();
    m_audioSampleRate.reset();
    m_generation = purge.nextGeneration;
    m_lastTransitionSequence = purge.transitionSequence;
    return ::media::Status::success();
}

void MediaAvStartupGenerationState::reset() noexcept
{
    m_payloads.clear();
    m_seen.clear();
    m_audioSampleRate.reset();
    m_generation.reset();
    m_lastTransitionSequence.reset();
}

} // namespace media::ffmpeg::graph
