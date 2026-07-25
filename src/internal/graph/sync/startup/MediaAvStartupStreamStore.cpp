#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaAvStartupUnitId idOf(const MediaAvStartupAccessUnit& unit) noexcept
{
    return {unit.stream, unit.generation, unit.sequence};
}

} // namespace

MediaAvStartupStreamStore::MediaAvStartupStreamStore(
    MediaRunningTime maximumGap)
    : m_maximumGap(maximumGap)
{
}

bool MediaAvStartupStreamStore::empty() const noexcept { return m_arrival.empty(); }
std::size_t MediaAvStartupStreamStore::size() const noexcept { return m_arrival.size(); }
std::uint64_t MediaAvStartupStreamStore::backSequence() const noexcept
{
    return m_arrival.back().unit.sequence;
}

::media::Result<bool> MediaAvStartupStreamStore::append(
    MediaAvStartupAccessUnit unit)
{
    auto end = unit.presentationTime->checkedAdd(unit.duration);
    if (!end) return ::media::Result<bool>::failure(end.error());
    auto expandedEnd = end.value().checkedAdd(m_maximumGap);
    if (!expandedEnd) {
        return ::media::Result<bool>::failure(expandedEnd.error());
    }
    m_arrival.push_back(
        {std::move(unit), m_nextOrdinal++, end.value(), expandedEnd.value()});
    auto iterator = std::prev(m_arrival.end());
    const auto& stored = iterator->unit;
    const PresentationKey key{stored.presentationTime->nanoseconds(),
                              stored.generation, stored.sequence};
    if (!m_presentation.emplace(key, iterator).second) {
        m_arrival.pop_back();
        return ::media::Result<bool>::success(false);
    }
    const Record* appended = &*iterator;
    for (auto& [candidate, state] : m_coverage) {
        (void)candidate;
        applyInterval(state, *appended);
    }
    m_coverage.emplace(appended, CoverageState{
        *stored.presentationTime, appended->end, appended->expandedEnd, {}});
    return ::media::Result<bool>::success(true);
}

void MediaAvStartupStreamStore::applyInterval(
    CoverageState& state,
    const Record& interval)
{
    ++m_cumulativeCoverageWork.coverageOperations;
    if (interval.end <= state.anchorStart) return;
    if (*interval.unit.presentationTime <= state.reachableThrough) {
        if (interval.end > state.frontier) {
            state.frontier = interval.end;
            state.reachableThrough = interval.expandedEnd;
            promoteReachable(state);
        }
        return;
    }
    const PresentationKey key{interval.unit.presentationTime->nanoseconds(),
                              interval.unit.generation, interval.unit.sequence};
    state.pending.emplace(key, &interval);
    ++m_cumulativeCoverageWork.orderedIndexMutations;
}

void MediaAvStartupStreamStore::promoteReachable(CoverageState& state)
{
    while (!state.pending.empty()) {
        auto first = state.pending.begin();
        const Record* interval = first->second;
        if (*interval->unit.presentationTime > state.reachableThrough) break;
        state.pending.erase(first);
        ++m_cumulativeCoverageWork.orderedIndexMutations;
        ++m_cumulativeCoverageWork.coverageOperations;
        if (interval->end > state.frontier) {
            state.frontier = interval->end;
            state.reachableThrough = interval->expandedEnd;
        }
    }
}

std::vector<MediaAvStartupIndexedUnit>
MediaAvStartupStreamStore::presentationSnapshot() const
{
    std::vector<MediaAvStartupIndexedUnit> snapshot;
    snapshot.reserve(m_presentation.size());
    for (const auto& [key, iterator] : m_presentation) {
        (void)key;
        const auto coverage = m_coverage.find(&*iterator);
        snapshot.push_back(
            {&iterator->unit, iterator->arrivalOrdinal, coverage->second.frontier});
    }
    return snapshot;
}

std::vector<MediaAvStartupUnitId> MediaAvStartupStreamStore::ids() const
{
    std::vector<MediaAvStartupUnitId> result;
    result.reserve(m_arrival.size());
    for (const auto& record : m_arrival) result.push_back(idOf(record.unit));
    return result;
}

std::vector<MediaAvStartupUnitId> MediaAvStartupStreamStore::prefixBefore(
    const MediaAvStartupAccessUnit* selected,
    MediaAvStartupSelectionWork& work) const
{
    std::vector<MediaAvStartupUnitId> result;
    for (const auto& record : m_arrival) {
        ++work.releaseUnitsVisited;
        if (&record.unit == selected) break;
        result.push_back(idOf(record.unit));
    }
    return result;
}

void MediaAvStartupStreamStore::appendSuffixSelections(
    const MediaAvStartupAccessUnit* selected,
    std::uint32_t firstTrim,
    std::vector<MediaAvStartupSelection>& output,
    MediaAvStartupSelectionWork& work) const
{
    bool append = false;
    bool first = true;
    for (const auto& record : m_arrival) {
        ++work.releaseUnitsVisited;
        if (&record.unit == selected) append = true;
        if (!append) continue;
        output.push_back({idOf(record.unit), first ? firstTrim : 0});
        first = false;
    }
}

void MediaAvStartupStreamStore::clear() noexcept
{
    m_coverage.clear();
    m_presentation.clear();
    m_arrival.clear();
    m_nextOrdinal = 0;
    m_cumulativeCoverageWork = {};
}

const MediaAvStartupSelectionWork&
MediaAvStartupStreamStore::cumulativeCoverageWork() const noexcept
{
    return m_cumulativeCoverageWork;
}

} // namespace media::ffmpeg::graph
