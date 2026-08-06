#pragma once

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsUnexpectedElementaryPidPolicy.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <unordered_set>

namespace media::ffmpeg::graph {

enum class MediaTsPesProvenanceValidity {
    Valid,
    Invalid
};

struct MediaTsPesProvenanceAnchor final {
    std::uint16_t pid = 0;
    std::uint64_t rangeByteOffset = 0;
    std::uint64_t stateEvidenceByteOffset = 0;
    std::optional<std::uint64_t> originByteOffset;
    MediaTsPesProvenanceValidity validity = MediaTsPesProvenanceValidity::Invalid;
};

class MediaTsPesProvenanceTimeline final {
public:
    static ::media::Result<MediaTsPesProvenanceTimeline> create(
        std::size_t packetStride,
        std::size_t capacity,
        std::uint64_t maximumPositionRegressionBytes);

    ::media::Status trackPid(std::uint16_t pid);
    ::media::Status configureSelectedPids(
        std::span<const std::uint16_t> pids,
        MediaTsUnexpectedElementaryPidPolicy unexpectedPidPolicy);
    ::media::Status onPacketPrefix(const MediaTsPacketPrefixView& packet);
    ::media::Status onContinuityEvent(const MediaTsContinuityEvent& event,
                                      bool beginsPayloadUnit);
    ::media::Status onSourceClockBoundary(std::uint64_t byteOffset);
    ::media::Status replaySourceClockBoundaries(
        std::span<const std::uint64_t> byteOffsets);
    ::media::Result<MediaTsPesProvenanceAnchor> resolveAnchor(
        std::uint64_t packetPosition,
        std::uint16_t pid) const;
    ::media::Result<MediaTsPesProvenanceAnchor> stateForAnchor(
        const MediaTsPesProvenanceAnchor& anchor) const;

private:
    enum class BoundaryApplication {
        Live,
        Historical
    };

    struct Range final {
        std::uint64_t startByteOffset = 0;
        std::optional<std::uint64_t> endByteOffset;
        std::uint16_t pid = 0;
        std::uint64_t stateEvidenceByteOffset = 0;
        std::optional<std::uint64_t> originByteOffset;
        MediaTsPesProvenanceValidity validity = MediaTsPesProvenanceValidity::Invalid;
    };

    struct SelectionConfiguration final {
        std::unordered_set<std::uint16_t> selectedPids;
        std::unordered_set<std::uint16_t> knownPids;
        MediaTsUnexpectedElementaryPidPolicy unexpectedPidPolicy;
    };

    MediaTsPesProvenanceTimeline(std::size_t capacity,
                                 std::uint64_t maximumPositionRegressionBytes) noexcept;
    ::media::Status validateObservedOffset(std::uint64_t byteOffset);
    ::media::Status appendRange(Range range);
    ::media::Status invalidate(std::uint64_t byteOffset,
                               std::uint16_t pid);
    ::media::Status applySourceClockBoundary(std::uint64_t byteOffset);
    ::media::Status invalidateSelectedRangesAt(
        std::uint64_t byteOffset,
        BoundaryApplication application);
    Range* openRange(std::uint16_t pid);
    void evictSafeRanges() const;

    std::size_t m_capacity;
    std::uint64_t m_maximumPositionRegressionBytes;
    std::optional<std::uint64_t> m_alignmentOrigin;
    std::optional<std::uint64_t> m_lastPacketOffset;
    std::optional<std::uint64_t> m_lastSourceClockBoundaryOffset;
    mutable std::optional<std::uint64_t> m_queryHighWatermark;
    std::unordered_set<std::uint16_t> m_trackedPids;
    std::optional<SelectionConfiguration> m_selection;
    mutable std::deque<Range> m_ranges;
};

} // namespace media::ffmpeg::graph
