#pragma once

#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/startup/MediaAvStartupSelectionWork.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

#include "internal/graph/runtime/resource/MediaAvStartupIndexMemoryResource.h"

#include <list>
#include <map>
#include <tuple>
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include <span>

namespace media::ffmpeg::graph {

struct MediaAvStartupIntervalFrontier final { MediaRunningTime end; };
struct MediaAvStartupDisplayFrontier final { MediaRunningTime lastPresentation; };
using MediaAvStartupReadinessEvidence = std::variant<
    MediaAvStartupIntervalFrontier, MediaAvStartupDisplayFrontier>;

struct MediaAvStartupIndexedUnit final {
    const MediaAvStartupAccessUnit* unit;
    std::size_t arrivalOrdinal;
    MediaAvStartupReadinessEvidence readiness;
};

using MediaAvStartupPresentationSnapshot = std::span<const MediaAvStartupIndexedUnit>;

class MediaAvStartupStreamStore final {
public:
    MediaAvStartupStreamStore(MediaRunningTime maximumGap,
        MediaAvStartupIndexStoragePlan indexStorage, MediaLineageAllocationScope allocation);
    MediaAvStartupStreamStore(const MediaAvStartupStreamStore&) = delete;
    MediaAvStartupStreamStore& operator=(const MediaAvStartupStreamStore&) = delete;
    MediaAvStartupStreamStore(MediaAvStartupStreamStore&&) = delete;
    MediaAvStartupStreamStore& operator=(MediaAvStartupStreamStore&&) = delete;
    static ::media::Result<MediaAvStartupIndexGeometries> allocationGeometry();

    bool empty() const noexcept;
    bool hasKeyFrame() const noexcept;
    std::size_t size() const noexcept;
    std::uint64_t backSequence() const noexcept;
    ::media::Result<bool> append(MediaAvStartupAccessUnit unit);
    // Borrowed until the next snapshot or clear; used only during one selection.
    MediaAvStartupPresentationSnapshot presentationSnapshot();
    void appendIds(MediaAvStartupIds& output) const;
    ::media::Result<std::size_t> prefixCount(const MediaAvStartupAccessUnit* selected,
        MediaAvStartupSelectionWork& work) const;
    void appendPrefixIds(const MediaAvStartupAccessUnit* selected,
        MediaAvStartupIds& output, MediaAvStartupSelectionWork& work) const;
    void appendSuffixSelections(const MediaAvStartupAccessUnit* selected,
                                std::uint32_t firstTrim,
                                MediaAvStartupSelections& output,
                                MediaAvStartupSelectionWork& work) const;
    void clear() noexcept;
    const MediaAvStartupSelectionWork& cumulativeCoverageWork() const noexcept;

private:
    struct Interval final {
        MediaRunningTime end;
        MediaRunningTime expandedEnd;
    };
    struct Record final {
        MediaAvStartupAccessUnit unit;
        std::size_t arrivalOrdinal;
        std::optional<Interval> interval;
    };
    using Arrival = std::pmr::list<Record>;
    using PresentationKey = std::tuple<std::int64_t, std::uint64_t, std::uint64_t>;

    using Presentation = std::pmr::map<PresentationKey, Arrival::iterator>;
    using Pending = std::pmr::map<PresentationKey, const Record*>;
    struct CoverageState final {
        MediaRunningTime anchorStart;
        MediaRunningTime frontier;
        MediaRunningTime reachableThrough;
        Pending pending;
    };

    using Coverage = std::pmr::map<const Record*, CoverageState>;

    void applyInterval(CoverageState& state, const Record& interval);
    void promoteReachable(CoverageState& state);

    MediaLineageAllocationScope m_allocation;
    MediaAvStartupIndexStoragePlan m_indexStorage;
    MediaAvStartupIndexMemoryResource m_arrivalResource;
    MediaAvStartupIndexMemoryResource m_presentationResource;
    MediaAvStartupIndexMemoryResource m_coverageResource;
    MediaAvStartupIndexMemoryResource m_pendingResource;
    MediaFixedCapacityStorage<MediaAvStartupIndexedUnit> m_snapshot;
    MediaRunningTime m_maximumGap;
    MediaVideoStartupReadiness m_readiness;
    Arrival m_arrival;
    Presentation m_presentation;
    Coverage m_coverage;
    MediaAvStartupSelectionWork m_cumulativeCoverageWork;
    std::size_t m_nextOrdinal = 0;
};

} // namespace media::ffmpeg::graph
