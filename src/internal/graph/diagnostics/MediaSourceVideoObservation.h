#pragma once
#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {
// Borrowed identity only. A pending codec input keeps its existing lineage/token
// owner alive; its lineage-state purge clears this view together with that input.
struct MediaSourceVideoObservationFacts final {
    std::string_view sourceIdentity;
    std::uint64_t generation;
    std::uint64_t sourceSequence;
    std::int64_t presentationNs;
    std::optional<std::int64_t> durationNs;
    std::optional<std::uint64_t> decodedSequence;
    std::int64_t pts;
    MediaRational timeBase;
    std::optional<MediaRational> frameTimeBase;
};
std::optional<MediaSourceVideoObservationFacts> mediaSourceVideoObservationFacts(
    const MediaBufferRef& buffer) noexcept;

enum class MediaSourceVideoObservationStage : std::size_t {
    Input, Accepted, WouldBlock, Resolved, OutputAttempt, Published,
    Release, ReleaseCommitted, Activation, ReleasePublished, Count
};
class MediaAvStartupReleaseBuffer;
struct MediaSourceVideoObservationContext final {
    std::string_view group;
    bool identityFromInput = false;
    std::optional<std::uint64_t> releaseIdentity;
    std::optional<std::uint64_t> transitionSequence;
    std::optional<std::int64_t> epochSourceStartNs;
    std::optional<std::int64_t> epochMasterReleaseNs;
    std::optional<std::size_t> videoCount;
    std::optional<std::int64_t> minimumPresentationNs;
    std::optional<std::int64_t> maximumEndNs;
    std::optional<int> preparationPhase;
};
// Owner-thread observation only: no media references, per-frame logging or
// unbounded history. Diagnostic allocation/logging failures never affect media.
class MediaSourceVideoObservation final {
public:
    void observe(MediaNodeId node, const char* nodeName, MediaSourceVideoObservationStage stage,
        const MediaSourceVideoObservationFacts& facts,
        const MediaSourceVideoObservationContext& context = {}) noexcept;
    void observe(MediaNodeId node, const char* nodeName, MediaSourceVideoObservationStage stage,
        const MediaBufferRef& buffer, const MediaSourceVideoObservationContext& context = {}) noexcept;
    void observeRelease(MediaNodeId node, const char* nodeName, MediaSourceVideoObservationStage stage,
        const MediaAvStartupReleaseBuffer& release, std::optional<std::uint64_t> releaseIdentity) noexcept;
    void unresolved() noexcept;
    void droppedStale() noexcept;
    void finish(MediaNodeId node, const char* nodeName, const char* reason) noexcept;
    void reset() noexcept { *this = {}; }
private:
    void increment(std::uint64_t& count) noexcept;
    void summary(MediaNodeId node, const char* nodeName, const char* reason) const noexcept;
    std::uint64_t m_generation = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(MediaSourceVideoObservationStage::Count)> m_counts{};
    std::uint64_t m_unknownLineage = 0;
    std::uint64_t m_stale = 0;
    bool m_incomplete = false;
};
} // namespace media::ffmpeg::graph
