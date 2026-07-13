#pragma once

#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/startup/MediaAvStartupSelectionWork.h"

#include <list>
#include <map>
#include <tuple>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvStartupIndexedUnit final {
    const MediaAvStartupAccessUnit* unit;
    std::size_t arrivalOrdinal;
    MediaRunningTime coverageEnd;
};

class MediaAvStartupStreamStore final {
public:
    explicit MediaAvStartupStreamStore(MediaRunningTime maximumGap);

    bool empty() const noexcept;
    std::size_t size() const noexcept;
    std::uint64_t backSequence() const noexcept;
    ::media::Result<bool> append(MediaAvStartupAccessUnit unit);
    std::vector<MediaAvStartupIndexedUnit> presentationSnapshot() const;
    std::vector<MediaAvStartupUnitId> ids() const;
    std::vector<MediaAvStartupUnitId> prefixBefore(
        const MediaAvStartupAccessUnit* selected,
        MediaAvStartupSelectionWork& work) const;
    void appendSuffixSelections(const MediaAvStartupAccessUnit* selected,
                                std::uint32_t firstTrim,
                                std::vector<MediaAvStartupSelection>& output,
                                MediaAvStartupSelectionWork& work) const;
    void clear() noexcept;
    const MediaAvStartupSelectionWork& cumulativeCoverageWork() const noexcept;

private:
    struct Record final {
        MediaAvStartupAccessUnit unit;
        std::size_t arrivalOrdinal;
        MediaRunningTime end;
        MediaRunningTime expandedEnd;
    };
    using Arrival = std::list<Record>;
    using PresentationKey = std::tuple<std::int64_t, std::uint64_t, std::uint64_t>;

    struct CoverageState final {
        MediaRunningTime anchorStart;
        MediaRunningTime frontier;
        MediaRunningTime reachableThrough;
        std::map<PresentationKey, const Record*> pending;
    };

    void applyInterval(CoverageState& state, const Record& interval);
    void promoteReachable(CoverageState& state);

    MediaRunningTime m_maximumGap;
    Arrival m_arrival;
    std::map<PresentationKey, Arrival::iterator> m_presentation;
    std::map<const Record*, CoverageState> m_coverage;
    MediaAvStartupSelectionWork m_cumulativeCoverageWork;
    std::size_t m_nextOrdinal = 0;
};

} // namespace media::ffmpeg::graph
