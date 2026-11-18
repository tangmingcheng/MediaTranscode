#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <limits>

namespace media::ffmpeg::graph {

// Source identity is the already-validated identity in the aggregate source plan.
// Keep only scalars here: a miss may outlive the source generation and its frames.
struct MediaAggregateFrameObservation final {
    std::uint64_t sourceSequence;
    std::int64_t presentationNs;
    std::optional<std::int64_t> durationNs;
    std::optional<std::uint64_t> decodedSequence;
};

struct MediaAggregateGenerationObservation final {
    std::uint64_t generation = 0;
    std::optional<std::int64_t> firstRealCursor;
    std::optional<std::int64_t> firstRealSteadyNs;
    std::optional<std::int64_t> firstCandidateSteadyNs;
    std::optional<std::int64_t> firstCandidateBeginNs;
    std::optional<std::int64_t> firstCandidateEndNs;
    std::optional<MediaAggregateFrameObservation> firstCandidateFrame;
    std::optional<MediaAggregateFrameObservation> firstRealFrame;
    std::uint64_t prepareExpired = 0;
    std::uint64_t prepareOldGeneration = 0;
    std::optional<std::int64_t> lastPrepareBeginNs;
    std::optional<std::int64_t> lastPrepareEndNs;
    bool incomplete = false;

    void enter(std::uint64_t value) noexcept {
        if (generation != value) { *this = {}; generation = value; }
    }
    void count(std::uint64_t& value) noexcept {
        if (value == (std::numeric_limits<std::uint64_t>::max)()) incomplete = true;
        else ++value;
    }
};

// First attempted interval miss after a real contribution. Not publication
// evidence. Channel depth is a concurrent snapshot and can include controls;
// it does not prove that an available frame covers the requested target.
struct MediaAggregateIntervalMissObservation final {
    std::int64_t cursor;
    std::int64_t targetNs;
    std::int64_t steadyNs;
    std::uint64_t generation;
    std::size_t candidatesBefore;
    std::size_t candidatesAfter;
    std::size_t expiredCandidates;
    std::optional<std::size_t> channelDepth;
    std::optional<std::int64_t> lastExaminedBeginNs;
    std::optional<std::int64_t> lastExaminedEndNs;
    std::int64_t epochSourceStartNs = 0;
    std::int64_t epochMasterReleaseNs = 0;
    MediaAggregateGenerationObservation generationAtMiss;
    std::optional<std::int64_t> firstRealCursor;
    std::optional<std::int64_t> firstRealSteadyNs;
    std::optional<std::int64_t> firstCandidateSteadyNs;
    std::optional<std::int64_t> firstCandidateBeginNs;
    std::optional<std::int64_t> firstCandidateEndNs;
    std::optional<MediaAggregateFrameObservation> firstCandidateFrame;
    std::optional<MediaAggregateFrameObservation> firstRealFrame;
    std::optional<std::uint64_t> committedGeneration;
    std::optional<std::int64_t> committedSteadyNs;
};

} // namespace media::ffmpeg::graph
