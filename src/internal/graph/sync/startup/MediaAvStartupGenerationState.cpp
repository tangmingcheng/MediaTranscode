#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"

namespace media::ffmpeg::graph {

MediaAvStartupGenerationState::MediaAvStartupGenerationState(
    MediaAvSyncGroupKey groupKey)
    : m_groupKey(std::move(groupKey)) {}

::media::Status MediaAvStartupGenerationState::store(
    const MediaAvSyncGroupKey& groupKey,
    const MediaAvStartupAccessUnit& unit,
    MediaBufferRef media)
{
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
    if (!m_seen.emplace(id).second) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup generation state rejects duplicate source sequence"));
    }
    if (!m_payloads.emplace(id, std::move(media)).second) {
        m_seen.erase(id);
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "Startup generation state payload ownership diverged from sequence history"));
    }
    if (!m_generation) m_generation = unit.generation;
    if (unit.stream == MediaAvStartupStream::Audio) {
        m_audioSampleRate = static_cast<int>(unit.audio->sampleRate);
    }
    return ::media::Status::success();
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
}

void MediaAvStartupGenerationState::erase(
    const std::vector<MediaAvStartupUnitId>& ids) noexcept
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
