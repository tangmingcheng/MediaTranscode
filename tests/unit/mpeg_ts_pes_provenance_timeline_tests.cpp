#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsPesProvenanceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsReturnedPesCursor.h"

#include <array>
#include <cstdint>
#include <span>

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

namespace {

constexpr std::array<std::uint8_t, 6> PesStart{0x00, 0x00, 0x01, 0xE0, 0x00, 0x00};
constexpr std::array<std::uint8_t, 4> NotPes{0x12, 0x34, 0x56, 0x78};

MediaTsPacketPrefixView packet(std::uint64_t offset,
                               std::uint16_t pid,
                               bool payloadStart,
                               std::span<const std::uint8_t> payload = {})
{
    return MediaTsPacketPrefixView{
        .byteOffset = offset,
        .pid = pid,
        .payloadUnitStart = payloadStart,
        .pesStart = payload.size() >= 3 && payload[0] == 0 &&
                    payload[1] == 0 && payload[2] == 1};
}

MediaTsPesProvenanceTimeline createTimeline(TestContext& ctx,
                                            std::size_t capacity = 8,
                                            std::uint64_t regression = 188 * 8)
{
    auto created = MediaTsPesProvenanceTimeline::create(188, capacity, regression);
    EXPECT_TRUE(ctx, created);
    return std::move(created).value();
}

void testReadAheadPreservesContainingPesOrigin(TestContext& ctx)
{
    auto timeline = createTimeline(ctx);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));

    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x102, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(376, 0x101, false)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(564, 0x101, true, PesStart)));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));

    auto oldVideo = timeline.resolveAnchor(376, 0x101);
    EXPECT_TRUE(ctx, oldVideo);
    if (oldVideo) {
        EXPECT_EQ(ctx, oldVideo.value().originByteOffset, std::optional<std::uint64_t>(0));
        EXPECT_EQ(ctx, oldVideo.value().validity, MediaTsPesProvenanceValidity::Valid);
    }
    auto newVideo = timeline.resolveAnchor(564, 0x101);
    EXPECT_TRUE(ctx, newVideo);
    if (newVideo) {
        EXPECT_EQ(ctx, newVideo.value().originByteOffset, std::optional<std::uint64_t>(564));
    }
}

void testContinuityInvalidatesUntilValidPesStart(TestContext& ctx)
{
    auto timeline = createTimeline(ctx);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    const std::array<std::uint16_t, 1> selected{0x101};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onContinuityEvent(MediaTsContinuityEvent{
        188, 0x101, MediaTsContinuityEventReason::CounterLoss}, false));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x101, false)));
    const auto malformedPrefix = packet(376, 0x101, true, NotPes);
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(malformedPrefix));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(564, 0x101, true, PesStart)));
    auto invalid = timeline.resolveAnchor(376, 0x101);
    EXPECT_TRUE(ctx, invalid);
    if (invalid) {
        EXPECT_EQ(ctx, invalid.value().originByteOffset, std::optional<std::uint64_t>{});
        EXPECT_EQ(ctx, invalid.value().validity, MediaTsPesProvenanceValidity::Invalid);
    }
    auto reacquired = timeline.resolveAnchor(564, 0x101);
    EXPECT_TRUE(ctx, reacquired);
    if (reacquired) {
        EXPECT_EQ(ctx, reacquired.value().originByteOffset, std::optional<std::uint64_t>(564));
        EXPECT_EQ(ctx, reacquired.value().validity, MediaTsPesProvenanceValidity::Valid);
    }
}

void testResolutionFailsClosedForInvalidIdentityAndPosition(TestContext& ctx)
{
    EXPECT_FALSE(ctx, MediaTsPesProvenanceTimeline::create(192, 8, 188));
    auto timeline = createTimeline(ctx, 8, 187);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(5, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(193, 0x101, false)));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    const std::array<std::uint16_t, 1> selected{0x101};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));

    EXPECT_FALSE(ctx, timeline.resolveAnchor(6, 0x101));
    EXPECT_FALSE(ctx, timeline.resolveAnchor(5, 0x102));
    EXPECT_FALSE(ctx, timeline.resolveAnchor(5, 0x777));
    EXPECT_TRUE(ctx, timeline.resolveAnchor(193, 0x101));
    EXPECT_FALSE(ctx, timeline.resolveAnchor(5, 0x101));
}

void testSelectionKeepsKnownInventoryIdempotent(TestContext& ctx)
{
    auto timeline = createTimeline(ctx);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    EXPECT_FALSE(ctx, timeline.trackPid(0x103));
}

void testSafeEvictionRejectsExpiredPesRange(TestContext& ctx)
{
    auto timeline = createTimeline(ctx, 2, 0);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    const std::array<std::uint16_t, 1> selected{0x101};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.resolveAnchor(188, 0x101));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(376, 0x101, true, PesStart)));

    EXPECT_FALSE(ctx, timeline.resolveAnchor(0, 0x101));
    auto current = timeline.resolveAnchor(376, 0x101);
    EXPECT_TRUE(ctx, current);
    if (current) {
        EXPECT_EQ(ctx, current.value().originByteOffset, std::optional<std::uint64_t>(376));
    }
}

void testCapacityFailureDoesNotPartiallyCloseCurrentRange(TestContext& ctx)
{
    auto timeline = createTimeline(ctx, 1, 188);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    const std::array<std::uint16_t, 1> selected{0x101};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    EXPECT_FALSE(ctx, timeline.onPacketPrefix(packet(188, 0x101, true, PesStart)));
    EXPECT_FALSE(ctx, timeline.onPacketPrefix(packet(188, 0x101, true, PesStart)));

    auto retained = timeline.resolveAnchor(0, 0x101);
    EXPECT_TRUE(ctx, retained);
    if (retained) {
        EXPECT_EQ(ctx, retained.value().originByteOffset, std::optional<std::uint64_t>(0));
    }
}

MediaTsRuntimeBinding runtimeBinding()
{
    return MediaTsRuntimeBinding{
        MediaTsPacketOriginPolicy::PerStreamPesCarry,
        MediaTsRuntimeStreamBinding{0, 0x101},
        MediaTsRuntimeStreamBinding{1, 0x102}, 0x103, 8};
}

void testRuntimeBindingDefinesSelectedPesBoundaryPids(TestContext& ctx)
{
    const auto binding = runtimeBinding();
    EXPECT_TRUE(ctx, binding.requiresSelectedPesBoundary(binding.video.pid));
    EXPECT_TRUE(ctx, binding.requiresSelectedPesBoundary(binding.audio.pid));
    EXPECT_TRUE(ctx, binding.requiresSelectedPesBoundary(binding.pcrPid));
    EXPECT_FALSE(ctx, binding.requiresSelectedPesBoundary(0x777));
}

void testProgramSourceBoundaryInvalidatesEveryOpenSelectedPes(TestContext& ctx)
{
    auto timeline = createTimeline(ctx, 16);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x102, true, PesStart)));

    auto cursor = MediaTsReturnedPesCursor::create(runtimeBinding());
    EXPECT_TRUE(ctx, cursor);
    if (!cursor) return;
    const auto resolve = [&timeline](std::uint64_t position, std::uint16_t pid) {
        return timeline.resolveAnchor(position, pid);
    };
    const auto state = [&timeline](const MediaTsPesProvenanceAnchor& anchor) {
        return timeline.stateForAnchor(anchor);
    };
    EXPECT_TRUE(ctx, cursor.value().resolve(0, 0, resolve, state));
    EXPECT_TRUE(ctx, cursor.value().resolve(1, 188, resolve, state));

    EXPECT_TRUE(ctx, timeline.onSourceClockBoundary(376));
    auto videoCarry = cursor.value().resolve(0, -1, resolve, state);
    auto audioCarry = cursor.value().resolve(1, -1, resolve, state);
    EXPECT_TRUE(ctx, videoCarry && audioCarry);
    if (videoCarry && audioCarry) {
        EXPECT_EQ(ctx, videoCarry.value().readiness,
                  MediaSourceClockReadiness::ReacquireRequired);
        EXPECT_EQ(ctx, audioCarry.value().readiness,
                  MediaSourceClockReadiness::ReacquireRequired);
        EXPECT_EQ(ctx, videoCarry.value().evidenceByteOffset,
                  std::optional<std::uint64_t>{376});
        EXPECT_EQ(ctx, audioCarry.value().evidenceByteOffset,
                  std::optional<std::uint64_t>{376});
    }
}

void testProgramSourceBoundaryPreservesClosedHistory(TestContext& ctx)
{
    auto timeline = createTimeline(ctx, 16);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    const auto closedVideo = timeline.resolveAnchor(0, 0x101);
    EXPECT_TRUE(ctx, closedVideo);
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(376, 0x102, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onSourceClockBoundary(564));

    if (closedVideo) {
        const auto retained = timeline.stateForAnchor(closedVideo.value());
        EXPECT_TRUE(ctx, retained);
        if (retained) {
            EXPECT_EQ(ctx, retained.value().validity,
                      MediaTsPesProvenanceValidity::Valid);
            EXPECT_EQ(ctx, retained.value().stateEvidenceByteOffset,
                      std::uint64_t{0});
        }
    }
}

void testProbeBoundariesRebuildOnlyCrossingSelectedPes(TestContext& ctx)
{
    auto timeline = createTimeline(ctx, 32);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    EXPECT_TRUE(ctx, timeline.trackPid(0x777));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x102, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(376, 0x777, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(564, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(940, 0x777, false)));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));

    const std::array<std::uint64_t, 1> sourceBoundaries{752};
    EXPECT_TRUE(ctx, timeline.replaySourceClockBoundaries(sourceBoundaries));

    const auto closedVideo = timeline.resolveAnchor(0, 0x101);
    const auto currentVideo = timeline.resolveAnchor(564, 0x101);
    const auto currentAudio = timeline.resolveAnchor(188, 0x102);
    EXPECT_TRUE(ctx, closedVideo && currentVideo && currentAudio);
    if (closedVideo && currentVideo && currentAudio) {
        EXPECT_EQ(ctx, closedVideo.value().validity,
                  MediaTsPesProvenanceValidity::Valid);
        EXPECT_EQ(ctx, currentVideo.value().validity,
                  MediaTsPesProvenanceValidity::Invalid);
        EXPECT_EQ(ctx, currentVideo.value().stateEvidenceByteOffset,
                  std::uint64_t{752});
        EXPECT_EQ(ctx, currentAudio.value().validity,
                  MediaTsPesProvenanceValidity::Invalid);
        EXPECT_EQ(ctx, currentAudio.value().stateEvidenceByteOffset,
                  std::uint64_t{752});
    }
}

void testReturnedCursorCarriesEachStreamPesIndependently(TestContext& ctx)
{
    auto timeline = createTimeline(ctx);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x102, true, PesStart)));
    auto cursor = MediaTsReturnedPesCursor::create(runtimeBinding());
    EXPECT_TRUE(ctx, cursor);
    if (!cursor) return;
    const auto resolve = [&timeline](std::uint64_t position, std::uint16_t pid) {
        return timeline.resolveAnchor(position, pid);
    };
    const auto state = [&timeline](const MediaTsPesProvenanceAnchor& anchor) {
        return timeline.stateForAnchor(anchor);
    };
    auto videoAnchor = cursor.value().resolve(0, 0, resolve, state);
    auto audioAnchor = cursor.value().resolve(1, 188, resolve, state);
    auto videoContinuation1 = cursor.value().resolve(0, -1, resolve, state);
    auto audioContinuation = cursor.value().resolve(1, -1, resolve, state);
    auto videoContinuation2 = cursor.value().resolve(0, -1, resolve, state);
    EXPECT_TRUE(ctx, videoAnchor && audioAnchor && videoContinuation1 &&
                     audioContinuation && videoContinuation2);
    if (videoContinuation1 && audioContinuation && videoContinuation2) {
        EXPECT_EQ(ctx, videoContinuation1.value().originByteOffset,
                  std::optional<std::uint64_t>{0});
        EXPECT_EQ(ctx, videoContinuation2.value().originByteOffset,
                  std::optional<std::uint64_t>{0});
        EXPECT_EQ(ctx, audioContinuation.value().originByteOffset,
                  std::optional<std::uint64_t>{188});
    }
}

void testReturnedCursorAcquiresAndReacquiresWithoutLatestFallback(TestContext& ctx)
{
    auto timeline = createTimeline(ctx);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    auto cursor = MediaTsReturnedPesCursor::create(runtimeBinding());
    EXPECT_TRUE(ctx, cursor);
    if (!cursor) return;
    const auto resolve = [&timeline](std::uint64_t position, std::uint16_t pid) {
        return timeline.resolveAnchor(position, pid);
    };
    const auto state = [&timeline](const MediaTsPesProvenanceAnchor& anchor) {
        return timeline.stateForAnchor(anchor);
    };
    auto firstContinuation = cursor.value().resolve(0, -1, resolve, state);
    EXPECT_TRUE(ctx, firstContinuation);
    if (firstContinuation) {
        EXPECT_EQ(ctx, firstContinuation.value().readiness,
                  MediaSourceClockReadiness::Acquiring);
        EXPECT_FALSE(ctx, firstContinuation.value().originByteOffset.has_value());
    }
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    auto locked = cursor.value().resolve(0, 0, resolve, state);
    EXPECT_TRUE(ctx, locked);
    EXPECT_TRUE(ctx, timeline.onContinuityEvent(MediaTsContinuityEvent{
        188, 0x101, MediaTsContinuityEventReason::CounterLoss}, false));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x101, false)));
    auto invalidContinuation = cursor.value().resolve(0, -1, resolve, state);
    EXPECT_TRUE(ctx, invalidContinuation);
    if (invalidContinuation) {
        EXPECT_EQ(ctx, invalidContinuation.value().readiness,
                  MediaSourceClockReadiness::ReacquireRequired);
        EXPECT_EQ(ctx, invalidContinuation.value().evidenceByteOffset,
                  std::optional<std::uint64_t>{188});
        EXPECT_FALSE(ctx, invalidContinuation.value().originByteOffset.has_value());
    }
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(376, 0x101, true, PesStart)));
    auto continuationBeforeAnchor = cursor.value().resolve(0, -1, resolve, state);
    EXPECT_TRUE(ctx, continuationBeforeAnchor);
    if (continuationBeforeAnchor) {
        EXPECT_EQ(ctx, continuationBeforeAnchor.value().readiness,
                  MediaSourceClockReadiness::ReacquireRequired);
        EXPECT_FALSE(ctx, continuationBeforeAnchor.value().originByteOffset.has_value());
    }
    auto reacquired = cursor.value().resolve(0, 376, resolve, state);
    EXPECT_TRUE(ctx, reacquired);
    if (reacquired) {
        EXPECT_EQ(ctx, reacquired.value().readiness, MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, reacquired.value().originByteOffset,
                  std::optional<std::uint64_t>{376});
    }
}

void testFuturePesFaultDoesNotInvalidateClosedActivePes(TestContext& ctx)
{
    auto timeline = createTimeline(ctx);
    EXPECT_TRUE(ctx, timeline.trackPid(0x101));
    EXPECT_TRUE(ctx, timeline.trackPid(0x102));
    const std::array<std::uint16_t, 2> selected{0x101, 0x102};
    EXPECT_TRUE(ctx, timeline.configureSelectedPids(selected));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(0, 0x101, true, PesStart)));
    auto cursor = MediaTsReturnedPesCursor::create(runtimeBinding());
    EXPECT_TRUE(ctx, cursor);
    if (!cursor) return;
    const auto resolve = [&timeline](std::uint64_t position, std::uint16_t pid) {
        return timeline.resolveAnchor(position, pid);
    };
    const auto state = [&timeline](const MediaTsPesProvenanceAnchor& anchor) {
        return timeline.stateForAnchor(anchor);
    };
    auto oldPes = cursor.value().resolve(0, 0, resolve, state);
    EXPECT_TRUE(ctx, oldPes);

    EXPECT_TRUE(ctx, timeline.onContinuityEvent(MediaTsContinuityEvent{
        188, 0x101, MediaTsContinuityEventReason::CounterLoss}, true));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(188, 0x101, true, PesStart)));
    auto delayedAtBoundary = cursor.value().resolve(0, -1, resolve, state);
    EXPECT_TRUE(ctx, delayedAtBoundary);
    if (delayedAtBoundary) {
        EXPECT_EQ(ctx, delayedAtBoundary.value().readiness,
                  MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, delayedAtBoundary.value().originByteOffset,
                  std::optional<std::uint64_t>{0});
    }
    EXPECT_TRUE(ctx, timeline.onContinuityEvent(MediaTsContinuityEvent{
        376, 0x101, MediaTsContinuityEventReason::CounterLoss}, false));
    EXPECT_TRUE(ctx, timeline.onPacketPrefix(packet(376, 0x101, false)));
    auto delayedOldPesPacket = cursor.value().resolve(0, -1, resolve, state);
    EXPECT_TRUE(ctx, delayedOldPesPacket);
    if (delayedOldPesPacket) {
        EXPECT_EQ(ctx, delayedOldPesPacket.value().readiness,
                  MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, delayedOldPesPacket.value().originByteOffset,
                  std::optional<std::uint64_t>{0});
        EXPECT_EQ(ctx, delayedOldPesPacket.value().evidenceByteOffset,
                  std::optional<std::uint64_t>{0});
    }
}

} // namespace

void runMpegTsPesProvenanceTimelineTests(TestContext& ctx)
{
    testReadAheadPreservesContainingPesOrigin(ctx);
    testContinuityInvalidatesUntilValidPesStart(ctx);
    testResolutionFailsClosedForInvalidIdentityAndPosition(ctx);
    testSelectionKeepsKnownInventoryIdempotent(ctx);
    testSafeEvictionRejectsExpiredPesRange(ctx);
    testCapacityFailureDoesNotPartiallyCloseCurrentRange(ctx);
    testRuntimeBindingDefinesSelectedPesBoundaryPids(ctx);
    testProgramSourceBoundaryInvalidatesEveryOpenSelectedPes(ctx);
    testProgramSourceBoundaryPreservesClosedHistory(ctx);
    testProbeBoundariesRebuildOnlyCrossingSelectedPes(ctx);
    testReturnedCursorCarriesEachStreamPesIndependently(ctx);
    testReturnedCursorAcquiresAndReacquiresWithoutLatestFallback(ctx);
    testFuturePesFaultDoesNotInvalidateClosedActivePes(ctx);
}
