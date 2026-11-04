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
    MediaRunningTime maximumGap,
    MediaAvStartupIndexStoragePlan indexStorage, MediaLineageAllocationScope allocation)
    : m_allocation(std::move(allocation)), m_indexStorage(std::move(indexStorage)),
      m_arrivalResource(m_indexStorage, MediaAvStartupIndexClass::Arrival,
          *MediaLineageAllocator<std::byte>{m_allocation}.resource()),
      m_presentationResource(m_indexStorage, MediaAvStartupIndexClass::Presentation,
          *MediaLineageAllocator<std::byte>{m_allocation}.resource()),
      m_coverageResource(m_indexStorage, MediaAvStartupIndexClass::Coverage,
          *MediaLineageAllocator<std::byte>{m_allocation}.resource()),
      m_pendingResource(m_indexStorage, MediaAvStartupIndexClass::Pending,
          *MediaLineageAllocator<std::byte>{m_allocation}.resource()),
      m_snapshot(m_indexStorage.capacity, *MediaLineageAllocator<std::byte>{m_allocation}.resource()),
      m_maximumGap(maximumGap), m_readiness(m_indexStorage.readiness),
      m_arrival(&m_arrivalResource),
      m_presentation(&m_presentationResource), m_coverage(&m_coverageResource)
{
}

bool MediaAvStartupStreamStore::empty() const noexcept { return m_arrival.empty(); }
bool MediaAvStartupStreamStore::hasKeyFrame() const noexcept
{
    for (const auto& record : m_arrival) {
        if (record.unit.keyFrame) return true;
    }
    return false;
}

std::size_t MediaAvStartupStreamStore::size() const noexcept { return m_arrival.size(); }
std::uint64_t MediaAvStartupStreamStore::backSequence() const noexcept
{
    return m_arrival.back().unit.sequence;
}

::media::Result<bool> MediaAvStartupStreamStore::append(
    MediaAvStartupAccessUnit unit)
try
{
    if (!unit.presentationTime)
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "Startup store requires a mapped presentation"));
    std::optional<Interval> interval;
    if (m_readiness == MediaVideoStartupReadiness::IntervalCoverage) {
        const auto* duration = std::get_if<MediaRunningTime>(&unit.timing);
        if (!duration || duration->nanoseconds() <= 0)
            return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
                "Interval startup store requires positive duration"));
        auto end = unit.presentationTime->checkedAdd(*duration);
        if (!end) return ::media::Result<bool>::failure(end.error());
        auto expanded = end.value().checkedAdd(m_maximumGap);
        if (!expanded) return ::media::Result<bool>::failure(expanded.error());
        interval = Interval{end.value(), expanded.value()};
    } else if (m_readiness == MediaVideoStartupReadiness::DecodedPresentationSpan) {
        if (!std::holds_alternative<MediaAvStartupDisplayPoint>(unit.timing) ||
            (!m_arrival.empty() &&
                (*unit.presentationTime < *m_arrival.back().unit.presentationTime ||
                 unit.generation != m_arrival.back().unit.generation ||
                 unit.sequence <= m_arrival.back().unit.sequence)))
            return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
                "Display startup store requires ordered points from one generation"));
    } else {
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "Startup store has no planned readiness"));
    }
    m_arrival.push_back({std::move(unit), m_nextOrdinal++, interval});
    auto iterator = std::prev(m_arrival.end());
    const auto& stored = iterator->unit;
    const PresentationKey key{stored.presentationTime->nanoseconds(),
                              stored.generation, stored.sequence};
    if (!m_presentation.emplace(key, iterator).second) {
        m_arrival.pop_back();
        return ::media::Result<bool>::success(false);
    }
    const Record* appended = &*iterator;
    if (appended->interval) {
        for (auto& [candidate, state] : m_coverage) {
            (void)candidate;
            applyInterval(state, *appended);
        }
        m_coverage.emplace(appended, CoverageState{
            *stored.presentationTime, appended->interval->end,
            appended->interval->expandedEnd,
            Pending{&m_pendingResource}});
    }
    return ::media::Result<bool>::success(true);
} catch (const std::bad_alloc&) {
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::allocationFailed("Startup indexed storage allocation failed"));
}

void MediaAvStartupStreamStore::applyInterval(
    CoverageState& state,
    const Record& interval)
{
    ++m_cumulativeCoverageWork.coverageOperations;
    if (interval.interval->end <= state.anchorStart) return;
    if (*interval.unit.presentationTime <= state.reachableThrough) {
        if (interval.interval->end > state.frontier) {
            state.frontier = interval.interval->end;
            state.reachableThrough = interval.interval->expandedEnd;
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
        if (interval->interval->end > state.frontier) {
            state.frontier = interval->interval->end;
            state.reachableThrough = interval->interval->expandedEnd;
        }
    }
}

MediaAvStartupPresentationSnapshot
MediaAvStartupStreamStore::presentationSnapshot()
{
    m_snapshot.clear();
    for (const auto& [key, iterator] : m_presentation) {
        (void)key;
        if (m_readiness == MediaVideoStartupReadiness::DecodedPresentationSpan) {
            m_snapshot.push_back({&iterator->unit, iterator->arrivalOrdinal,
                MediaAvStartupDisplayFrontier{*m_arrival.back().unit.presentationTime}});
        } else {
            const auto coverage = m_coverage.find(&*iterator);
            m_snapshot.push_back({&iterator->unit, iterator->arrivalOrdinal,
                MediaAvStartupIntervalFrontier{coverage->second.frontier}});
        }
    }
    return m_snapshot.view();
}

void MediaAvStartupStreamStore::appendIds(MediaAvStartupIds& output) const
{
    for (const auto& record : m_arrival) output.push_back(idOf(record.unit));
}

::media::Result<std::size_t> MediaAvStartupStreamStore::prefixCount(
    const MediaAvStartupAccessUnit* selected, MediaAvStartupSelectionWork& work) const
{
    std::size_t count = 0;
    for (const auto& record : m_arrival) {
        ++work.releaseUnitsVisited;
        if (&record.unit == selected) return ::media::Result<std::size_t>::success(count);
        ++count;
    }
    return ::media::Result<std::size_t>::failure(::media::ErrorInfo::invalidArgument(
        "Startup selected unit is absent from arrival storage"));
}

void MediaAvStartupStreamStore::appendPrefixIds(
    const MediaAvStartupAccessUnit* selected, MediaAvStartupIds& output,
    MediaAvStartupSelectionWork& work) const
{
    for (const auto& record : m_arrival) {
        ++work.releaseUnitsVisited;
        if (&record.unit == selected) break;
        output.push_back(idOf(record.unit));
    }
}

void MediaAvStartupStreamStore::appendSuffixSelections(
    const MediaAvStartupAccessUnit* selected,
    std::uint32_t firstTrim,
    MediaAvStartupSelections& output,
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
    m_snapshot.clear();
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
