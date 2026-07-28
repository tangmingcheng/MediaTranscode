#include "common/TestAssert.h"

#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/MediaAvSyncStateMachine.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaAvStartupCoordinatorTestAccess final {
    static std::uint64_t candidateOperations(
        const MediaAvStartupCoordinator& coordinator) noexcept
    {
        return coordinator.m_lastAttemptSelectionWork.candidateOperations;
    }

    static std::uint64_t cumulativeCoverageOperations(
        const MediaAvStartupCoordinator& coordinator) noexcept
    {
        return coordinator.m_cumulativeSelectionWork.coverageOperations;
    }

    static std::uint64_t cumulativeOrderedMutations(
        const MediaAvStartupCoordinator& coordinator) noexcept
    {
        return coordinator.m_cumulativeSelectionWork.orderedIndexMutations;
    }
};

} // namespace media::ffmpeg::graph

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

class SizedStartupPayload final : public MediaBuffer {
public:
    explicit SizedStartupPayload(std::uint64_t bytes) : m_bytes(bytes) {}
    MediaBufferType type() const noexcept override { return MediaBufferType::Event; }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override
    {
        return m_bytes;
    }

private:
    std::uint64_t m_bytes;
};

constexpr MediaRunningTime ns(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaAvStartupConfig startupConfig()
{
    return MediaAvStartupConfig{
        .requireVideoKeyFrame = true,
        .trimAudioToCommonStart = true,
        .allowDegradedClock = false,
        .topology = MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        .maximumWait = ns(10'000),
        .preroll = ns(100),
        .keyFrameWait = ns(5'000),
        .maximumAudioTrim = ns(100),
        .maximumInitialSkew = ns(40),
        .maximumGap = ns(80),
        .outputLead = ns(60),
        .videoCapacity = 16,
        .audioCapacity = 32,
        .videoByteCapacity = 1'600,
        .audioByteCapacity = 3'200,
        .maximumVideoUnitBytes = 100,
        .maximumAudioUnitBytes = 100,
        .videoIdentity = "video-main",
        .audioIdentity = "audio-main"};
}

MediaAvStartupAccessUnit video(std::uint64_t sequence,
                               std::int64_t pts,
                               bool keyFrame,
                               std::uint64_t generation = 7,
                               MediaSourceClockReadiness readiness =
                                   MediaSourceClockReadiness::Locked)
{
    return MediaAvStartupAccessUnit{
        .stream = MediaAvStartupStream::Video,
        .identity = "video-main",
        .sequence = sequence,
        .payloadBytes = 100,
        .presentationTime = readiness == MediaSourceClockReadiness::Locked
            ? std::optional<MediaRunningTime>(ns(pts))
            : std::nullopt,
        .duration = ns(40),
        .readiness = readiness,
        .generation = generation,
        .keyFrame = keyFrame,
        .audio = std::nullopt};
}

MediaAvStartupAccessUnit audio(std::uint64_t sequence,
                               std::int64_t pts,
                               std::int64_t duration,
                               std::uint32_t samples,
                               std::uint64_t generation = 7,
                               MediaSourceClockReadiness readiness =
                                   MediaSourceClockReadiness::Locked)
{
    return MediaAvStartupAccessUnit{
        .stream = MediaAvStartupStream::Audio,
        .identity = "audio-main",
        .sequence = sequence,
        .payloadBytes = 100,
        .presentationTime = readiness == MediaSourceClockReadiness::Locked
            ? std::optional<MediaRunningTime>(ns(pts))
            : std::nullopt,
        .duration = ns(duration),
        .readiness = readiness,
        .generation = generation,
        .keyFrame = false,
        .audio = MediaAvAudioSampleSpan{pts * 48, 48'000, samples}};
}

void expectNoRelease(TestContext& ctx,
                     const MediaAvSyncResult<MediaAvStartupDecision>& decision)
{
    EXPECT_TRUE(ctx, decision);
    if (decision) EXPECT_FALSE(ctx, decision.value().release.has_value());
}

void expectCode(TestContext& ctx,
                const MediaAvSyncError& error,
                MediaAvSyncErrorCode code)
{
    EXPECT_EQ(ctx, error.code(), code);
    EXPECT_TRUE(ctx, !error.operation().empty());
}

void testStateMachineRejectsIllegalTransitions(TestContext& ctx)
{
    MediaAvSyncStateMachine state(MediaAvSyncTopology::SeparateRtpToSeparateRtp);
    EXPECT_EQ(ctx, state.state(), MediaAvSyncState::Idle);
    EXPECT_TRUE(ctx, state.transition(MediaAvSyncEvent::BeginAcquisition, 7));
    EXPECT_EQ(ctx, state.state(), MediaAvSyncState::AcquiringClock);
    EXPECT_FALSE(ctx, state.transition(MediaAvSyncEvent::Release, 7));
    EXPECT_TRUE(ctx, state.transition(MediaAvSyncEvent::ClocksLocked, 7));
    EXPECT_TRUE(ctx, state.transition(MediaAvSyncEvent::StreamsPrimed, 7));
    EXPECT_TRUE(ctx, state.transition(MediaAvSyncEvent::Release, 7));
    EXPECT_TRUE(ctx, state.transition(MediaAvSyncEvent::Run, 7));
    EXPECT_FALSE(ctx, state.transition(MediaAvSyncEvent::ClocksLocked, 6));
    EXPECT_TRUE(ctx, state.transition(MediaAvSyncEvent::RequireReacquisition, 8));
    EXPECT_EQ(ctx, state.state(), MediaAvSyncState::AcquiringClock);
    EXPECT_EQ(ctx, state.generation(), std::optional<std::uint64_t>(8));
}

void testAudioFirstReleasesAtVideoKeyFrameAndTrimsSamples(TestContext& ctx)
{
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;

    expectNoRelease(ctx, coordinator.value().submit(audio(1, 0, 100, 4'800), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(audio(2, 100, 100, 4'800), ns(1)));
    expectNoRelease(ctx, coordinator.value().submit(video(3, 40, true), ns(2)));
    auto released = coordinator.value().submit(video(4, 100, false), ns(3));
    EXPECT_TRUE(ctx, released);
    if (!released || !released.value().release) return;
    const auto& batch = *released.value().release;
    EXPECT_EQ(ctx, batch.epoch.sourceStart, ns(40));
    EXPECT_EQ(ctx, batch.epoch.masterRelease, ns(63));
    EXPECT_EQ(ctx, batch.epoch.generation, static_cast<std::uint64_t>(7));
    EXPECT_TRUE(ctx, batch.video.size() >= 1);
    EXPECT_TRUE(ctx, batch.audio.size() >= 2);
    EXPECT_EQ(ctx, batch.video.front().id.sequence, static_cast<std::uint64_t>(3));
    EXPECT_EQ(ctx, batch.audio.front().id.sequence, static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, batch.audio.front().trimLeadingSamples,
              static_cast<std::uint32_t>(1'920));
}

void testStartupTrimUsesTheAbsoluteEpochSampleGrid(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumInitialSkew = ns(10);
    config.maximumGap = ns(1);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;

    auto videoUnit = video(1, 5, true);
    videoUnit.presentationTime =
        MediaRunningTime::fromNanoseconds(5'000'001);
    expectNoRelease(ctx, coordinator.value().submit(
                             std::move(videoUnit), ns(0)));
    auto released = coordinator.value().submit(
        audio(2, 0, 20, 960), ns(1));
    EXPECT_TRUE(ctx, released && released.value().release);
    if (!released || !released.value().release) return;
    EXPECT_EQ(ctx, released.value().release->epoch.sourceStart,
              MediaRunningTime::fromNanoseconds(5'000'001));
    EXPECT_EQ(ctx,
              released.value().release->audio.front().trimLeadingSamples,
              static_cast<std::uint32_t>(241));
}

void testStartupTrimUsesAuthoritativeCanonicalSampleSpan(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumInitialSkew = ns(10);
    config.maximumGap = ns(1);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;

    auto videoUnit = video(1, 5, true);
    videoUnit.presentationTime =
        MediaRunningTime::fromNanoseconds(5'000'001);
    expectNoRelease(ctx, coordinator.value().submit(
                             std::move(videoUnit), ns(0)));
    auto audioUnit = audio(2, 0, 20, 960);
    audioUnit.audio->firstSample = 240;
    auto released = coordinator.value().submit(
        std::move(audioUnit), ns(1));
    EXPECT_TRUE(ctx, released && released.value().release);
    if (!released || !released.value().release) return;
    EXPECT_EQ(ctx,
              released.value().release->audio.front().trimLeadingSamples,
              static_cast<std::uint32_t>(1));
}

void testAuthoritativeSampleSpanHandlesSignedBoundaries(TestContext& ctx)
{
    const MediaAvAudioSampleSpan negativeSpan{
        -48'001, 48'000, 960};
    auto negativeTrim = calculateMediaAvAudioTrimSamples(
        ns(-1'000), negativeSpan);
    EXPECT_TRUE(ctx, negativeTrim);
    if (negativeTrim) {
        EXPECT_EQ(ctx, negativeTrim.value(), static_cast<std::uint32_t>(1));
    }

    const MediaAvAudioSampleSpan minimumSpan{
        std::numeric_limits<std::int64_t>::min(), 48'000, 960};
    EXPECT_FALSE(ctx, calculateMediaAvAudioTrimSamples(
                          ns(-1'000), minimumSpan));

    auto overflowCoordinator =
        MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, overflowCoordinator);
    if (!overflowCoordinator) return;
    auto overflowSpan = audio(2, 0, 20, 960);
    overflowSpan.audio->firstSample =
        std::numeric_limits<std::int64_t>::max() - 959;
    auto rejected = overflowCoordinator.value().submit(
        std::move(overflowSpan), ns(1));
    EXPECT_FALSE(ctx, rejected);
}

void testVideoFirstWaitsForCommonWindowAndReleasesOnce(TestContext& ctx)
{
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;

    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(2, 100, false), ns(1)));
    expectNoRelease(ctx, coordinator.value().submit(audio(3, 20, 40, 1'920), ns(2)));
    expectNoRelease(ctx, coordinator.value().submit(audio(4, 60, 40, 1'920), ns(3)));
    auto released = coordinator.value().submit(audio(5, 100, 40, 1'920), ns(4));
    EXPECT_TRUE(ctx, released && released.value().release);
    if (!released || !released.value().release) return;
    EXPECT_EQ(ctx, released.value().release->epoch.sourceStart, ns(20));

    auto pass = coordinator.value().submit(video(7, 200, false), ns(6));
    EXPECT_TRUE(ctx, pass);
    if (pass) {
        EXPECT_FALSE(ctx, pass.value().release.has_value());
        EXPECT_EQ(ctx, pass.value().disposition, MediaAvStartupDisposition::PassThrough);
    }
}

void testRunningDropsRepeatedOrRegressedPerStreamSequence(TestContext& ctx)
{
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;

    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(2, 100, false), ns(1)));
    expectNoRelease(ctx, coordinator.value().submit(audio(3, 20, 40, 1'920), ns(2)));
    expectNoRelease(ctx, coordinator.value().submit(audio(4, 60, 40, 1'920), ns(3)));
    auto released = coordinator.value().submit(audio(5, 100, 40, 1'920), ns(4));
    EXPECT_TRUE(ctx, released && released.value().release);
    if (!released || !released.value().release) return;

    auto repeatedVideo = coordinator.value().submit(
        video(2, 100, false), ns(5));
    EXPECT_TRUE(ctx, repeatedVideo);
    if (repeatedVideo) {
        EXPECT_EQ(ctx, repeatedVideo.value().disposition,
                  MediaAvStartupDisposition::DroppedDuplicateOrRegressed);
    }

    auto regressedAudio = coordinator.value().submit(
        audio(4, 60, 40, 1'920), ns(6));
    EXPECT_TRUE(ctx, regressedAudio);
    if (regressedAudio) {
        EXPECT_EQ(ctx, regressedAudio.value().disposition,
                  MediaAvStartupDisposition::DroppedDuplicateOrRegressed);
    }

    auto nextVideo = coordinator.value().submit(
        video(6, 140, false), ns(7));
    EXPECT_TRUE(ctx, nextVideo);
    if (nextVideo) {
        EXPECT_EQ(ctx, nextVideo.value().disposition,
                  MediaAvStartupDisposition::PassThrough);
    }
    auto nextAudio = coordinator.value().submit(
        audio(7, 140, 40, 1'920), ns(8));
    EXPECT_TRUE(ctx, nextAudio);
    if (nextAudio) {
        EXPECT_EQ(ctx, nextAudio.value().disposition,
                  MediaAvStartupDisposition::PassThrough);
    }
}

void testRequiresLockedSameGenerationAndPurgesOldPackets(TestContext& ctx)
{
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;

    auto acquiring = coordinator.value().submit(
        audio(1, 0, 20, 960, 7, MediaSourceClockReadiness::Acquiring), ns(0));
    EXPECT_TRUE(ctx, acquiring);
    if (acquiring) EXPECT_EQ(ctx, acquiring.value().disposition,
                             MediaAvStartupDisposition::DroppedNotReady);
    expectNoRelease(ctx, coordinator.value().submit(video(2, 0, true, 7), ns(1)));
    expectNoRelease(ctx, coordinator.value().submit(audio(3, 0, 100, 4'800, 8), ns(2)));
    EXPECT_EQ(ctx, coordinator.value().generation(), std::optional<std::uint64_t>(8));

    auto old = coordinator.value().submit(video(4, 100, true, 7), ns(3));
    EXPECT_TRUE(ctx, old);
    if (old) {
        EXPECT_EQ(ctx, old.value().disposition,
                  MediaAvStartupDisposition::DroppedOldGeneration);
        EXPECT_EQ(ctx, old.value().purged.front(),
                  (MediaAvStartupUnitId{MediaAvStartupStream::Video, 7, 4}));
    }
    for (std::uint64_t sequence = 40; sequence < 56; ++sequence) {
        auto late = coordinator.value().submit(video(sequence, 100, true, 7), ns(3));
        EXPECT_TRUE(ctx, late);
        if (late) EXPECT_EQ(ctx, late.value().disposition,
                            MediaAvStartupDisposition::DroppedOldGeneration);
    }
    expectNoRelease(ctx, coordinator.value().submit(video(5, 0, true, 8), ns(4)));
    expectNoRelease(ctx, coordinator.value().submit(audio(6, 100, 100, 4'800, 8), ns(5)));
    auto release = coordinator.value().submit(video(7, 100, false, 8), ns(6));
    EXPECT_TRUE(ctx, release && release.value().release);
    if (release && release.value().release) {
        for (const auto& selected : release.value().release->video) {
            EXPECT_TRUE(ctx, selected.id.sequence >= 5);
        }
        for (const auto& selected : release.value().release->audio) {
            EXPECT_TRUE(ctx, selected.id.sequence >= 3);
        }
    }
}

void testReacquireClosesGateUntilBothStreamsRelock(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    auto first = coordinator.value().submit(audio(2, 0, 20, 960), ns(1));
    EXPECT_TRUE(ctx, first && first.value().release);

    auto reacquire = coordinator.value().submit(audio(
        4, 30, 20, 960, 8, MediaSourceClockReadiness::ReacquireRequired), ns(3));
    EXPECT_TRUE(ctx, reacquire);
    if (reacquire) EXPECT_EQ(ctx, reacquire.value().disposition,
                             MediaAvStartupDisposition::DroppedNotReady);
    EXPECT_EQ(ctx, coordinator.value().state(), MediaAvSyncState::AcquiringClock);
    expectNoRelease(ctx, coordinator.value().submit(video(5, 40, true, 8), ns(4)));
    expectNoRelease(ctx, coordinator.value().submit(video(6, 60, false, 8), ns(5)));
    auto second = coordinator.value().submit(audio(7, 40, 20, 960, 8), ns(6));
    EXPECT_TRUE(ctx, second && second.value().release);
    if (second && second.value().release) {
        EXPECT_EQ(ctx, second.value().release->epoch.generation,
                  static_cast<std::uint64_t>(8));
    }
}

void testTimeoutEofErrorAndLifecycle(TestContext& ctx)
{
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    auto timedOut = coordinator.value().poll(ns(10'000));
    EXPECT_FALSE(ctx, timedOut);
    if (!timedOut) expectCode(ctx, timedOut.error(), MediaAvSyncErrorCode::StartupTimeout);
    EXPECT_EQ(ctx, coordinator.value().state(), MediaAvSyncState::Failed);

    EXPECT_TRUE(ctx, coordinator.value().reset());
    expectNoRelease(ctx, coordinator.value().submit(audio(2, 0, 20, 960), ns(0)));
    auto eof = coordinator.value().endOfStream(MediaAvStartupStream::Video);
    EXPECT_FALSE(ctx, eof);
    if (!eof) expectCode(ctx, eof.error(), MediaAvSyncErrorCode::EofBeforeRelease);
    EXPECT_EQ(ctx, coordinator.value().state(), MediaAvSyncState::Failed);

    EXPECT_TRUE(ctx, coordinator.value().reset());
    EXPECT_FALSE(ctx, coordinator.value().fail("decoder failed"));
    EXPECT_EQ(ctx, coordinator.value().state(), MediaAvSyncState::Failed);
    EXPECT_TRUE(ctx, coordinator.value().reset());
    coordinator.value().stop();
    EXPECT_EQ(ctx, coordinator.value().state(), MediaAvSyncState::Stopped);
    EXPECT_TRUE(ctx, coordinator.value().reset());
    coordinator.value().abort();
    EXPECT_EQ(ctx, coordinator.value().state(), MediaAvSyncState::Aborted);
    auto afterAbort = coordinator.value().submit(video(9, 0, true), ns(0));
    EXPECT_FALSE(ctx, afterAbort);
    if (!afterAbort) expectCode(ctx, afterAbort.error(), MediaAvSyncErrorCode::StartupAborted);
}

void testConfigRejectsMissingPolicySemantics(TestContext& ctx)
{
    auto config = startupConfig();
    config.videoCapacity = 0;
    EXPECT_FALSE(ctx, MediaAvStartupCoordinator::create(config));
    config = startupConfig();
    config.videoIdentity.clear();
    EXPECT_FALSE(ctx, MediaAvStartupCoordinator::create(config));
    config = startupConfig();
    config.maximumAudioTrim = ns(101);
    EXPECT_FALSE(ctx, MediaAvStartupCoordinator::create(config));
    config = startupConfig();
    config.videoByteCapacity = 0;
    EXPECT_FALSE(ctx, MediaAvStartupCoordinator::create(config));
    config = startupConfig();
    ++config.videoByteCapacity;
    EXPECT_FALSE(ctx, MediaAvStartupCoordinator::create(config));
    config = startupConfig();
    config.allowDegradedClock = true;
    EXPECT_FALSE(ctx, MediaAvStartupCoordinator::create(config));
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (coordinator) {
        auto wrongIdentity = video(1, 0, true);
        wrongIdentity.identity = "audio-main";
        EXPECT_FALSE(ctx, coordinator.value().submit(std::move(wrongIdentity), ns(0)));
    }
}

void testKeyFrameWaitAndInitialSkewAreEnforced(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(audio(1, 100, 100, 4'800), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(2, 0, true), ns(1)));
    expectNoRelease(ctx, coordinator.value().submit(video(3, 40, false), ns(2)));
    auto release = coordinator.value().submit(video(4, 80, true), ns(3));
    EXPECT_TRUE(ctx, release && release.value().release);
    if (release && release.value().release) {
        EXPECT_EQ(ctx, release.value().release->video.front().id.sequence,
                  static_cast<std::uint64_t>(4));
    }

    coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(6, 0, false), ns(0)));
    EXPECT_FALSE(ctx, coordinator.value().poll(ns(5'000)));
    EXPECT_EQ(ctx, coordinator.value().state(), MediaAvSyncState::Failed);
}

void testByteCapacityAndDegradedReadinessAreExplicit(TestContext& ctx)
{
    auto config = startupConfig();
    config.videoCapacity = 1;
    config.videoByteCapacity = 100;
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    auto exceeded = coordinator.value().submit(video(2, 40, false), ns(1));
    EXPECT_FALSE(ctx, exceeded);
    if (!exceeded) {
        expectCode(ctx, exceeded.error(), MediaAvSyncErrorCode::StartupCapacityExceeded);
    }

    coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    auto degraded = video(3, 0, true, 7, MediaSourceClockReadiness::Degraded);
    auto denied = coordinator.value().submit(std::move(degraded), ns(0));
    EXPECT_TRUE(ctx, denied);
    if (denied) {
        EXPECT_EQ(ctx, denied.value().disposition,
                  MediaAvStartupDisposition::DroppedNotReady);
    }
}

void testPurgesExactUnitIdsAcrossClockLossAndGenerationAdvance(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    auto lost = coordinator.value().submit(audio(
        2, 0, 20, 960, 7, MediaSourceClockReadiness::Acquiring), ns(1));
    EXPECT_TRUE(ctx, lost);
    if (lost) {
        EXPECT_EQ(ctx, lost.value().purged.size(), static_cast<std::size_t>(2));
        EXPECT_EQ(ctx, lost.value().purged.front(),
                  (MediaAvStartupUnitId{MediaAvStartupStream::Video, 7, 1}));
    }
    expectNoRelease(ctx, coordinator.value().submit(video(3, 0, true), ns(2)));
    auto advanced = coordinator.value().submit(audio(4, 0, 20, 960, 8), ns(3));
    EXPECT_TRUE(ctx, advanced);
    if (advanced) {
        EXPECT_EQ(ctx, advanced.value().purged.size(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, advanced.value().purged.front(),
                  (MediaAvStartupUnitId{MediaAvStartupStream::Video, 7, 3}));
    }
}

void testCommonWindowScansLaterKeyFrameAndRequiresContinuousPreroll(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(2, 80, true), ns(1)));
    auto release = coordinator.value().submit(audio(3, 100, 20, 960), ns(2));
    EXPECT_TRUE(ctx, release && release.value().release);
    if (release && release.value().release) {
        EXPECT_EQ(ctx, release.value().release->video.front().id.sequence,
                  static_cast<std::uint64_t>(2));
    }

    config = startupConfig();
    config.maximumGap = ns(10);
    coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(audio(10, 0, 200, 9'600), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(11, 0, true), ns(1)));
    auto gap = coordinator.value().submit(video(12, 100, false), ns(2));
    EXPECT_TRUE(ctx, gap && !gap.value().release);

    coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(audio(20, 0, 200, 9'600), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(21, 0, true), ns(1)));
    expectNoRelease(ctx, coordinator.value().submit(video(22, 100, false), ns(2)));
    expectNoRelease(ctx, coordinator.value().submit(video(23, 40, false), ns(3)));
    auto continuous = coordinator.value().submit(video(24, 80, false), ns(4));
    EXPECT_TRUE(ctx, continuous && continuous.value().release);
    if (continuous && continuous.value().release) {
        const auto& selected = continuous.value().release->video;
        EXPECT_EQ(ctx, selected.size(), static_cast<std::size_t>(4));
        EXPECT_EQ(ctx, selected[0].id.sequence, static_cast<std::uint64_t>(21));
        EXPECT_EQ(ctx, selected[1].id.sequence, static_cast<std::uint64_t>(22));
        EXPECT_EQ(ctx, selected[2].id.sequence, static_cast<std::uint64_t>(23));
        EXPECT_EQ(ctx, selected[3].id.sequence, static_cast<std::uint64_t>(24));
    }
}

void testNestedIntervalsAndLaterAudioCandidateFormCommonWindow(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(100);
    config.maximumAudioTrim = ns(100);
    config.maximumGap = ns(5);
    config.maximumInitialSkew = ns(40);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;

    auto firstVideo = video(1, 0, true);
    firstVideo.duration = ns(100);
    auto nestedVideo = video(2, 10, false);
    nestedVideo.duration = ns(10);
    auto extendingVideo = video(3, 90, false);
    extendingVideo.duration = ns(110);
    expectNoRelease(ctx, coordinator.value().submit(std::move(firstVideo), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(std::move(nestedVideo), ns(1)));
    expectNoRelease(ctx, coordinator.value().submit(std::move(extendingVideo), ns(2)));
    auto release = coordinator.value().submit(audio(4, 0, 200, 9'600), ns(3));
    EXPECT_TRUE(ctx, release && release.value().release);

    config.preroll = ns(20);
    config.maximumAudioTrim = ns(20);
    config.maximumGap = ns(5);
    coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    auto key = video(10, 100, true);
    key.duration = ns(40);
    expectNoRelease(ctx, coordinator.value().submit(std::move(key), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(audio(11, 70, 10, 480), ns(1)));
    auto later = coordinator.value().submit(audio(12, 90, 40, 1'920), ns(2));
    EXPECT_TRUE(ctx, later && later.value().release);
    if (later && later.value().release) {
        EXPECT_EQ(ctx, later.value().release->audio.front().id.sequence,
                  static_cast<std::uint64_t>(12));
    }
}

void testReleasePurgesEveryUnselectedPrefixIdentity(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(2, 80, true), ns(1)));
    auto released = coordinator.value().submit(audio(3, 100, 20, 960), ns(2));
    EXPECT_TRUE(ctx, released && released.value().release);
    if (!released) return;
    EXPECT_EQ(ctx, released.value().purged.size(), static_cast<std::size_t>(1));
    if (!released.value().purged.empty()) {
        EXPECT_EQ(ctx, released.value().purged.front(),
                  (MediaAvStartupUnitId{MediaAvStartupStream::Video, 7, 1}));
    }
}

void testRunningRejectsMediaAfterThatStreamEof(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(0)));
    auto release = coordinator.value().submit(audio(2, 0, 20, 960), ns(1));
    EXPECT_TRUE(ctx, release && release.value().release);
    EXPECT_TRUE(ctx, coordinator.value().endOfStream(MediaAvStartupStream::Video));
    auto lateVideo = coordinator.value().submit(video(3, 40, false), ns(2));
    EXPECT_FALSE(ctx, lateVideo);
    if (!lateVideo) {
        expectCode(ctx, lateVideo.error(), MediaAvSyncErrorCode::StartupInvalidTransition);
    }
    auto liveAudio = coordinator.value().submit(audio(4, 20, 20, 960), ns(3));
    EXPECT_TRUE(ctx, liveAudio);
}

void testPresentationIndexRemainsBoundedAtCapacity256(TestContext& ctx)
{
    auto config = startupConfig();
    config.videoCapacity = 256;
    config.videoByteCapacity = 25'600;
    config.audioCapacity = 256;
    config.audioByteCapacity = 25'600;
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    for (std::uint64_t sequence = 1; sequence <= 256; ++sequence) {
        auto accepted = coordinator.value().submit(
            video(sequence, static_cast<std::int64_t>(sequence * 40), false),
            ns(static_cast<std::int64_t>(sequence)));
        EXPECT_TRUE(ctx, accepted);
        if (!accepted) return;
    }
}

void testPresentationSelectionHasBoundedWorstCaseOperations(TestContext& ctx)
{
    auto config = startupConfig();
    config.videoCapacity = 256;
    config.videoByteCapacity = 25'600;
    config.audioCapacity = 256;
    config.audioByteCapacity = 25'600;
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    for (std::uint64_t sequence = 1; sequence <= 256; ++sequence) {
        const auto pts = sequence == 1
            ? 0
            : static_cast<std::int64_t>((258 - sequence) * 5);
        auto unit = video(sequence, pts, false);
        unit.duration = ns(5);
        expectNoRelease(ctx, coordinator.value().submit(
            std::move(unit), ns(static_cast<std::int64_t>(sequence))));
    }
    for (std::uint64_t sequence = 257; sequence <= 512; ++sequence) {
        const auto streamOrdinal = sequence - 256;
        const auto pts = streamOrdinal == 1
            ? 0
            : static_cast<std::int64_t>((258 - streamOrdinal) * 5);
        expectNoRelease(ctx, coordinator.value().submit(
            audio(sequence, pts, 5, 240),
            ns(static_cast<std::int64_t>(sequence))));
    }
    EXPECT_TRUE(ctx,
        MediaAvStartupCoordinatorTestAccess::candidateOperations(
            coordinator.value()) <= 768);
    const auto coverageOperations =
        MediaAvStartupCoordinatorTestAccess::cumulativeCoverageOperations(
            coordinator.value());
    const auto orderedMutations =
        MediaAvStartupCoordinatorTestAccess::cumulativeOrderedMutations(
            coordinator.value());
    EXPECT_EQ(ctx, coverageOperations, static_cast<std::uint64_t>(65'788));
    EXPECT_EQ(ctx, orderedMutations, static_cast<std::uint64_t>(1'016));
    EXPECT_TRUE(ctx, coverageOperations <= 131'072);
    EXPECT_TRUE(ctx, orderedMutations <= 131'072);
}

void testNegativePresentationTimesDoNotUseZeroAsCoverage(TestContext& ctx)
{
    auto config = startupConfig();
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, -200, true), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(2, -160, false), ns(1)));
    auto premature = coordinator.value().submit(audio(3, -200, 100, 4'800), ns(2));
    expectNoRelease(ctx, premature);
    auto finalVideo = video(4, -120, false);
    finalVideo.duration = ns(20);
    auto released = coordinator.value().submit(std::move(finalVideo), ns(3));
    EXPECT_TRUE(ctx, released && released.value().release);
}

void testCoverageExcludesUnitsBeforeSelectedArrivalSuffix(TestContext& ctx)
{
    auto config = startupConfig();
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    auto purgedPrefix = video(1, 40, false);
    purgedPrefix.duration = ns(100);
    expectNoRelease(ctx, coordinator.value().submit(std::move(purgedPrefix), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(video(2, 0, true), ns(1)));
    auto premature = coordinator.value().submit(audio(3, 0, 100, 4'800), ns(2));
    expectNoRelease(ctx, premature);
    auto suffix = video(4, 40, false);
    suffix.duration = ns(60);
    auto released = coordinator.value().submit(std::move(suffix), ns(3));
    EXPECT_TRUE(ctx, released && released.value().release);
    if (released && released.value().release) {
        EXPECT_EQ(ctx, released.value().release->video.front().id.sequence,
                  static_cast<std::uint64_t>(2));
        EXPECT_EQ(ctx, released.value().purged.front().sequence,
                  static_cast<std::uint64_t>(1));
    }
}

void testEqualPresentationTimesKeepStableUnitIdentityOrder(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(20);
    config.maximumAudioTrim = ns(20);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    auto first = video(1, 0, true);
    first.duration = ns(20);
    auto second = video(2, 0, false);
    second.duration = ns(20);
    expectNoRelease(ctx, coordinator.value().submit(std::move(first), ns(0)));
    expectNoRelease(ctx, coordinator.value().submit(std::move(second), ns(1)));
    auto released = coordinator.value().submit(audio(3, 0, 20, 960), ns(2));
    EXPECT_TRUE(ctx, released && released.value().release);
    if (released && released.value().release) {
        EXPECT_EQ(ctx, released.value().release->video.size(),
                  static_cast<std::size_t>(2));
        if (released.value().release->video.size() != 2) return;
        EXPECT_EQ(ctx, released.value().release->video[0].id.sequence,
                  static_cast<std::uint64_t>(1));
        EXPECT_EQ(ctx, released.value().release->video[1].id.sequence,
                  static_cast<std::uint64_t>(2));
    }
}

void testCoverageArithmeticOverflowIsTyped(TestContext& ctx)
{
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    auto overflowing = video(1, 0, true);
    overflowing.presentationTime = MediaRunningTime::fromNanoseconds(
        std::numeric_limits<std::int64_t>::max() - 10);
    overflowing.duration = MediaRunningTime::fromNanoseconds(20);
    auto result = coordinator.value().submit(std::move(overflowing), ns(0));
    EXPECT_FALSE(ctx, result);
    if (!result) expectCode(ctx, result.error(), MediaAvSyncErrorCode::TimeOverflow);
}

void testGlobalWatermarkPreventsLateEventTimeRegression(TestContext& ctx)
{
    auto config = startupConfig();
    config.preroll = ns(10);
    config.maximumAudioTrim = ns(10);
    config.maximumGap = ns(5);
    auto coordinator = MediaAvStartupCoordinator::create(config);
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator) return;
    expectNoRelease(ctx, coordinator.value().submit(video(1, 0, true), ns(1'000)));
    auto release = coordinator.value().submit(audio(2, 0, 20, 960), ns(1));
    EXPECT_TRUE(ctx, release && release.value().release);
    if (release && release.value().release) {
        EXPECT_EQ(ctx, release.value().release->epoch.masterRelease, ns(1'060));
    }
}

void testEnvelopeUsesTrustedPayloadFootprint(TestContext& ctx)
{
    auto unit = video(1, 0, true);
    unit.payloadBytes = 1;
    auto envelope = MediaAvStartupEnvelopeBuffer::create(
        makeMediaBufferRef<SizedStartupPayload>(100), std::move(unit), ns(0));
    EXPECT_TRUE(ctx, envelope);
    if (envelope) {
        const auto* typed = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(
            envelope.value().get());
        EXPECT_TRUE(ctx, typed != nullptr);
        if (typed) EXPECT_EQ(ctx, typed->unit().payloadBytes,
                             static_cast<std::uint64_t>(100));
    }
    auto unsized = MediaAvStartupEnvelopeBuffer::create(
        makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Flush),
        video(2, 0, true), ns(0));
    EXPECT_FALSE(ctx, unsized);
    auto mismatchedAudio = audio(3, 0, 20, 1);
    auto mismatchedEnvelope = MediaAvStartupEnvelopeBuffer::create(
        makeMediaBufferRef<SizedStartupPayload>(100), mismatchedAudio, ns(0));
    EXPECT_FALSE(ctx, mismatchedEnvelope);
    auto coordinator = MediaAvStartupCoordinator::create(startupConfig());
    EXPECT_TRUE(ctx, coordinator);
    if (coordinator) {
        auto rejected = coordinator.value().submit(std::move(mismatchedAudio), ns(0));
        EXPECT_FALSE(ctx, rejected);
        if (!rejected) expectCode(ctx, rejected.error(), MediaAvSyncErrorCode::InvalidDuration);
    }
}

void testFfmpegPacketFootprintIncludesSideDataOnlyPayload(TestContext& ctx)
{
    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    EXPECT_TRUE(ctx, av_packet_new_side_data(packet.get(), AV_PKT_DATA_NEW_EXTRADATA, 7) != nullptr);
    FFmpegPacketBuffer sideDataOnly(std::move(packet), std::nullopt);
    EXPECT_EQ(ctx, sideDataOnly.payloadFootprintBytes(),
              std::optional<std::uint64_t>(7));

    packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    EXPECT_TRUE(ctx, av_new_packet(packet.get(), 5) >= 0);
    EXPECT_TRUE(ctx, av_packet_new_side_data(packet.get(), AV_PKT_DATA_NEW_EXTRADATA, 7) != nullptr);
    FFmpegPacketBuffer combined(std::move(packet), std::nullopt);
    EXPECT_EQ(ctx, combined.payloadFootprintBytes(),
              std::optional<std::uint64_t>(12));

    packet = ::media::ffmpeg::makePacket();
    packet->size = 5;
    FFmpegPacketBuffer missingMainData(std::move(packet), std::nullopt);
    EXPECT_FALSE(ctx, missingMainData.payloadFootprintBytes().has_value());
    missingMainData.packet()->size = 0;

    packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, av_new_packet(packet.get(), 1) >= 0);
    EXPECT_TRUE(ctx, av_packet_new_side_data(
        packet.get(), AV_PKT_DATA_NEW_EXTRADATA, 1) != nullptr);
    av_freep(&packet->side_data[0].data);
    packet->side_data[0].size = 7;
    FFmpegPacketBuffer missingSideData(std::move(packet), std::nullopt);
    EXPECT_FALSE(ctx, missingSideData.payloadFootprintBytes().has_value());

    packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, av_new_packet(packet.get(), 1) >= 0);
    EXPECT_TRUE(ctx, av_packet_new_side_data(
        packet.get(), AV_PKT_DATA_NEW_EXTRADATA, 1) != nullptr);
    packet->side_data[0].size = std::numeric_limits<std::size_t>::max();
    FFmpegPacketBuffer overflowingMetadata(std::move(packet), std::nullopt);
    EXPECT_FALSE(ctx, overflowingMetadata.payloadFootprintBytes().has_value());
}

void setNodePolicy(MediaGraph& graph, MediaNodeId node)
{
    graph.setNodeOption(node, "av_startup.require_video_key_frame", "1");
    graph.setNodeOption(node, "av_startup.trim_audio_to_common_start", "1");
    graph.setNodeOption(node, "av_startup.allow_degraded_clock", "0");
    graph.setNodeOption(node, "av_startup.topology", "separate_rtp");
    graph.setNodeOption(node, "av_startup.maximum_wait_ns", "10000000000");
    graph.setNodeOption(node, "av_startup.preroll_ns", "100000000");
    graph.setNodeOption(node, "av_startup.key_frame_wait_ns", "5000000000");
    graph.setNodeOption(node, "av_startup.maximum_audio_trim_ns", "100000000");
    graph.setNodeOption(node, "av_startup.maximum_initial_skew_ns", "40000000");
    graph.setNodeOption(node, "av_startup.maximum_gap_ns", "80000000");
    graph.setNodeOption(node, "av_startup.output_lead_ns", "60000000");
    graph.setNodeOption(node, "av_startup.output_audio_sample_rate", "48000");
    graph.setNodeOption(node, "av_startup.video_capacity", "16");
    graph.setNodeOption(node, "av_startup.audio_capacity", "32");
    graph.setNodeOption(node, "av_startup.video_byte_capacity", "1600");
    graph.setNodeOption(node, "av_startup.audio_byte_capacity", "3200");
    graph.setNodeOption(node, "av_startup.maximum_video_unit_bytes", "100");
    graph.setNodeOption(node, "av_startup.maximum_audio_unit_bytes", "100");
    graph.setNodeOption(node, "av_startup.video_identity", "video-main");
    graph.setNodeOption(node, "av_startup.audio_identity", "audio-main");
    graph.setNodeOption(node, "av_startup.sync_group", "startup-group");
}

struct StartupNodeHarness final {
    MediaGraph graph;
    MediaNodeId coordinator;
    MediaGraphExecutionContext execution;
    std::unique_ptr<MediaRuntimeNode> runtime;
    MediaAvStartupCoordinatorNode* coordinatorRuntime = nullptr;
    std::shared_ptr<MediaAvStartupGenerationState> generationState;

    bool initialize(TestContext& ctx, std::size_t edgeCapacity = 64)
    {
        const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(edgeCapacity);
        const auto videoSource = graph.addNode(MediaNodeKind::DebugDump, "startup.video");
        const auto audioSource = graph.addNode(MediaNodeKind::DebugDump, "startup.audio");
        const auto clockSource = graph.addNode(MediaNodeKind::DebugDump, "startup.clock");
        coordinator = graph.addNode(MediaNodeKind::AvStartupCoordinator,
                                    "startup.coordinator");
        const auto sink = graph.addNode(MediaNodeKind::DebugDump, "startup.sink");
        graph.addOutputPort(videoSource, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(audioSource, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(clockSource, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(coordinator, "video", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(coordinator, "audio", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(coordinator, "clock", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addOutputPort(coordinator, "release", MediaStreamKind::Metadata,
                            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.addInputPort(sink, "in", MediaStreamKind::Metadata,
                           MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        graph.connect(videoSource, "out", coordinator, "video", "startup video", policy);
        graph.connect(audioSource, "out", coordinator, "audio", "startup audio", policy);
        graph.connect(clockSource, "out", coordinator, "clock", "startup clock", policy);
        graph.connect(coordinator, "release", sink, "in", "atomic release", policy);
        setNodePolicy(graph, coordinator);
        EXPECT_TRUE(ctx, execution.compile(graph));
        auto created = MediaRuntimeNodeFactory::create(*graph.findNode(coordinator));
        EXPECT_TRUE(ctx, created);
        if (!created) return false;
        runtime = std::move(created).value();
        coordinatorRuntime = dynamic_cast<MediaAvStartupCoordinatorNode*>(runtime.get());
        EXPECT_TRUE(ctx, coordinatorRuntime != nullptr);
        if (!coordinatorRuntime) return false;
        EXPECT_EQ(ctx, coordinatorRuntime->generationPurgeIdentity(),
                  MediaAvStartupGenerationState::plannedIdentity());
        generationState = std::dynamic_pointer_cast<MediaAvStartupGenerationState>(
            coordinatorRuntime->generationPurgeTarget());
        EXPECT_TRUE(ctx, generationState != nullptr);
        if (!generationState) return false;
        EXPECT_TRUE(ctx, runtime->start(execution));
        return true;
    }

    ::media::Status tick(std::int64_t nowMs)
    {
        return execution.findInputChannel(coordinator, "clock")->push(
            makeMediaBufferRef<MediaAvStartupClockBuffer>(ns(nowMs)));
    }

    ::media::Status push(const char* port,
                         MediaAvStartupAccessUnit unit,
                         std::int64_t observedAtMs)
    {
        auto envelope = MediaAvStartupEnvelopeBuffer::create(
            makeMediaBufferRef<SizedStartupPayload>(100),
            std::move(unit), ns(observedAtMs));
        if (!envelope) return ::media::Status::failure(envelope.error());
        return execution.findInputChannel(coordinator, port)->push(
            std::move(envelope).value());
    }

    ::media::Status control(const char* port, MediaControlBufferKind kind)
    {
        return execution.findInputChannel(coordinator, port)->push(
            makeMediaBufferRef<MediaControlBuffer>(kind));
    }
};

void testFactoryRejectsIncompleteStartupConfiguration(TestContext& ctx)
{
    MediaGraph graph;
    const auto coordinator = graph.addNode(MediaNodeKind::AvStartupCoordinator,
                                           "startup.incomplete");
    graph.addInputPort(coordinator, "video", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "audio", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "clock", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(coordinator, "release", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    EXPECT_FALSE(ctx, MediaRuntimeNodeFactory::create(*graph.findNode(coordinator)));
    MediaGraphRuntime runtime;
    EXPECT_FALSE(ctx, runtime.compile(std::move(graph)));
}

void testGenerationTargetIdentitySurvivesLifecycleCleanup(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    auto external = harness.generationState;
    const auto* identity = external.get();
    MediaAvStartupAccessUnit unit{
        MediaAvStartupStream::Video, "video-main", 1, 100, ns(0), ns(40),
        MediaSourceClockReadiness::Locked, 7, true, std::nullopt};
    const MediaAvStartupUnitId id{MediaAvStartupStream::Video, 7, 1};
    EXPECT_TRUE(ctx, external->store(MediaAvSyncGroupKey("startup-group"), unit,
                                     makeMediaBufferRef<SizedStartupPayload>(100)));
    EXPECT_TRUE(ctx, harness.runtime->stop(harness.execution));
    EXPECT_TRUE(ctx, harness.coordinatorRuntime->generationPurgeTarget().get() == identity);
    EXPECT_FALSE(ctx, external->take(id));

    EXPECT_TRUE(ctx, harness.runtime->start(harness.execution));
    EXPECT_TRUE(ctx, harness.coordinatorRuntime->generationPurgeTarget().get() == identity);
    EXPECT_TRUE(ctx, external->store(MediaAvSyncGroupKey("startup-group"), unit,
                                     makeMediaBufferRef<SizedStartupPayload>(100)));
    harness.runtime->abort(harness.execution);
    EXPECT_TRUE(ctx, harness.coordinatorRuntime->generationPurgeTarget().get() == identity);
    EXPECT_FALSE(ctx, external->take(id));

    harness.runtime.reset();
    EXPECT_FALSE(ctx, external->take(id));
    auto recreated = MediaRuntimeNodeFactory::create(
        *harness.graph.findNode(harness.coordinator));
    EXPECT_TRUE(ctx, recreated);
    if (recreated) {
        const auto* next = dynamic_cast<const MediaAvStartupCoordinatorNode*>(
            recreated.value().get());
        EXPECT_TRUE(ctx, next != nullptr);
        if (next) {
            EXPECT_TRUE(ctx, next->generationPurgeTarget().get() != identity);
        }
    }
}

void testNodeWaitsWithoutClockOrMedia(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    for (int index = 0; index < 4; ++index) {
        auto result = harness.runtime->process(harness.execution);
        EXPECT_TRUE(ctx, result);
        if (result) EXPECT_EQ(ctx, result.value().state, MediaNodeProcessState::Waiting);
    }
}

void testNodeReportsProgressWhenActivatingClockBarrier(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.tick(10'000));
    auto activated = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, activated);
    if (activated) {
        EXPECT_EQ(ctx, activated.value().state, MediaNodeProcessState::Progress);
    }
}

void testNodeProcessesDeadlineMediaBeforeEqualClockTick(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("video", video(1, 0, false, 7,
        MediaSourceClockReadiness::Acquiring), 0));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.push("audio", audio(2, 0, 200, 9'600), 10'000));
    EXPECT_TRUE(ctx, harness.push("video", video(3, 0, true), 10'000));
    EXPECT_TRUE(ctx, harness.push("video", video(4, 40, false), 10'000));
    EXPECT_TRUE(ctx, harness.push("video", video(5, 80, false), 10'000));
    EXPECT_TRUE(ctx, harness.tick(10'000));
    auto* output = harness.execution.findOutputChannel(harness.coordinator, "release");
    for (int index = 0; index < 5; ++index) {
        EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    }
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
}

void testNodeDrainsCapacityOneMediaSnapshotBeforeDeadlineClock(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx, 1)) return;
    EXPECT_TRUE(ctx, harness.push("video", video(1, 0, false, 7,
        MediaSourceClockReadiness::Acquiring), 0));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.tick(10'000));
    auto barrierActivated = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, barrierActivated);
    if (!barrierActivated) return;
    EXPECT_TRUE(ctx, harness.push("audio", audio(2, 0, 200, 9'600), 10'001));
    EXPECT_TRUE(ctx, harness.push("video", video(3, 0, true), 10'001));
    auto audioProcessed = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, audioProcessed);
    if (!audioProcessed) return;
    auto videoProcessed = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, videoProcessed);
    if (!videoProcessed) return;
    const MediaAvStartupUnitId keyFrame{
        MediaAvStartupStream::Video, 7, 3};
    EXPECT_TRUE(ctx, harness.generationState->take(keyFrame));
}

void testNodeClockBarrierDrainsSnapshotWithoutTakingFutureMedia(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("video", video(1, 0, false, 7,
        MediaSourceClockReadiness::Acquiring), 0));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    for (std::uint64_t sequence = 2; sequence <= 65; ++sequence) {
        EXPECT_TRUE(ctx, harness.push("video", video(sequence, 0, true), 10'001));
    }
    EXPECT_TRUE(ctx, harness.tick(10'000));
    auto mediaProcessed = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, mediaProcessed);
    auto* videoInput = harness.execution.findInputChannel(
        harness.coordinator, "video");
    EXPECT_TRUE(ctx, videoInput != nullptr);
    if (videoInput) {
        EXPECT_EQ(ctx, videoInput->size(), static_cast<std::size_t>(63));
    }
}

void testNodeProcessesQueuedReleaseMediaBeforeTerminalControl(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("audio", audio(1, 0, 200, 9'600), 0));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.push("video", video(2, 0, true), 1));
    EXPECT_TRUE(ctx, harness.push("video", video(3, 40, false), 2));
    EXPECT_TRUE(ctx, harness.push("video", video(4, 80, false), 3));
    EXPECT_TRUE(ctx, harness.control("audio", MediaControlBufferKind::Eof));
    auto* output = harness.execution.findOutputChannel(harness.coordinator, "release");
    EXPECT_TRUE(ctx, output != nullptr);
    if (!output) return;
    for (int index = 0; index < 2; ++index) {
        EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
        EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    }
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
}

void testNodeFreezesNewIntakeWhileDrainingClockWatermark(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("video", video(1, 0, false, 7,
        MediaSourceClockReadiness::Acquiring), 0));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    for (std::uint64_t index = 0; index < 4; ++index) {
        EXPECT_TRUE(ctx, harness.push("video", video(10 + index, 0, false, 7,
            MediaSourceClockReadiness::Acquiring), 9'999));
        EXPECT_TRUE(ctx, harness.push("audio", audio(20 + index, 0, 20, 960, 7,
            MediaSourceClockReadiness::Acquiring), 9'999));
    }
    EXPECT_TRUE(ctx, harness.tick(10'000));
    bool observedTimeout = false;
    for (std::uint64_t index = 0; index < 12; ++index) {
        auto processed = harness.runtime->process(harness.execution);
        if (!processed) {
            observedTimeout = true;
            break;
        }
        EXPECT_TRUE(ctx, harness.push("video", video(100 + index, 0, false, 7,
            MediaSourceClockReadiness::Acquiring), 9'999));
        EXPECT_TRUE(ctx, harness.push("audio", audio(200 + index, 0, 20, 960, 7,
            MediaSourceClockReadiness::Acquiring), 9'999));
    }
    EXPECT_TRUE(ctx, observedTimeout);
}

void testNodeRejectsPerStreamEventTimeRegression(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("video", video(1, 0, true), 10));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.push("video", video(2, 40, false), 9));
    EXPECT_FALSE(ctx, harness.runtime->process(harness.execution));
}

void testNodeDropsRepeatedSequenceAndContinues(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    auto* output = harness.execution.findOutputChannel(
        harness.coordinator, "release");
    EXPECT_TRUE(ctx, output != nullptr);
    if (!output) return;

    EXPECT_TRUE(ctx, harness.tick(4));
    EXPECT_TRUE(ctx, harness.push(
        "audio", audio(1, 0, 100, 4'800), 4));
    EXPECT_TRUE(ctx, harness.push(
        "audio", audio(2, 100, 100, 4'800), 4));
    EXPECT_TRUE(ctx, harness.push("video", video(3, 40, true), 4));
    EXPECT_TRUE(ctx, harness.push("video", video(4, 100, false), 4));
    EXPECT_TRUE(ctx, harness.push("video", video(5, 150, false), 4));
    auto* videoInput = harness.execution.findInputChannel(
        harness.coordinator, "video");
    auto* audioInput = harness.execution.findInputChannel(
        harness.coordinator, "audio");
    auto* clockInput = harness.execution.findInputChannel(
        harness.coordinator, "clock");
    EXPECT_TRUE(ctx, videoInput && audioInput && clockInput);
    if (!videoInput || !audioInput || !clockInput) return;
    for (int index = 0;
         index < 16 &&
         (videoInput->size() + audioInput->size() + clockInput->size()) > 0;
         ++index) {
        EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    }
    const auto baselineOutputs = output->size();
    EXPECT_TRUE(ctx, baselineOutputs > 0);

    EXPECT_TRUE(ctx, harness.push("video", video(4, 100, false), 5));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_EQ(ctx, output->size(), baselineOutputs);

    EXPECT_TRUE(ctx, harness.push("video", video(6, 200, false), 6));
    for (int index = 0;
         index < 3 && output->size() == baselineOutputs;
         ++index) {
        EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    }
    EXPECT_EQ(ctx, output->size(), baselineOutputs + 1);
}

void testNodePublishesOneImmutablePairedEnvelope(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(16);
    const auto videoSource = graph.addNode(MediaNodeKind::DebugDump, "startup.video");
    const auto audioSource = graph.addNode(MediaNodeKind::DebugDump, "startup.audio");
    const auto clockSource = graph.addNode(MediaNodeKind::DebugDump, "startup.clock");
    const auto coordinator = graph.addNode(MediaNodeKind::AvStartupCoordinator,
                                           "startup.coordinator");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "startup.sink");
    graph.addOutputPort(videoSource, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(audioSource, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(clockSource, "out", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "video", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "audio", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "clock", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(coordinator, "release", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(sink, "in", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.connect(videoSource, "out", coordinator, "video", "startup video", policy);
    graph.connect(audioSource, "out", coordinator, "audio", "startup audio", policy);
    graph.connect(clockSource, "out", coordinator, "clock", "startup clock", policy);
    graph.connect(coordinator, "release", sink, "in", "atomic release", policy);
    setNodePolicy(graph, coordinator);
    graph.setNodeOption(
        coordinator, "av_startup.output_audio_sample_rate", "44100");

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    auto runtime = MediaRuntimeNodeFactory::create(*graph.findNode(coordinator));
    EXPECT_TRUE(ctx, runtime);
    if (!runtime) return;
    EXPECT_TRUE(ctx, runtime.value()->start(execution));
    auto tick = [&](std::int64_t nowMs) {
        return execution.findInputChannel(coordinator, "clock")->push(
            makeMediaBufferRef<MediaAvStartupClockBuffer>(ns(nowMs)));
    };
    auto push = [&](const char* port, MediaAvStartupAccessUnit unit) {
        auto envelope = MediaAvStartupEnvelopeBuffer::create(
            makeMediaBufferRef<SizedStartupPayload>(100), std::move(unit), ns(4));
        if (!envelope) return ::media::Status::failure(envelope.error());
        return execution.findInputChannel(coordinator, port)->push(
            std::move(envelope).value());
    };
    EXPECT_TRUE(ctx, tick(4));
    EXPECT_TRUE(ctx, push("audio", audio(1, 0, 100, 4'800)));
    EXPECT_TRUE(ctx, push("audio", audio(2, 100, 100, 4'800)));
    EXPECT_TRUE(ctx, push("video", video(3, 40, true)));
    EXPECT_TRUE(ctx, push("video", video(4, 100, false)));
    EXPECT_TRUE(ctx, push("video", video(5, 150, false)));

    MediaChannel* output = execution.findOutputChannel(coordinator, "release");
    EXPECT_TRUE(ctx, output != nullptr);
    if (!output) return;
    for (int i = 0; i < 8 && output->size() == 0; ++i) {
        auto processed = runtime.value()->process(execution);
        EXPECT_TRUE(ctx, processed);
        if (!processed) return;
    }
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
    MediaBufferRef emitted;
    EXPECT_TRUE(ctx, output->tryPop(emitted));
    const auto* release = dynamic_cast<const MediaAvStartupReleaseBuffer*>(emitted.get());
    EXPECT_TRUE(ctx, release != nullptr);
    if (release) {
        EXPECT_EQ(ctx, release->groupKey(), MediaAvSyncGroupKey("startup-group"));
        EXPECT_EQ(ctx, release->releaseKind(),
                  MediaAvStartupReleaseKind::InitialAtomicRelease);
        EXPECT_EQ(ctx, release->epoch().generation, static_cast<std::uint64_t>(7));
        EXPECT_EQ(ctx, release->audioOrigin().outputSampleRate, 44'100);
        EXPECT_TRUE(ctx, !release->video().empty());
        EXPECT_TRUE(ctx, !release->audio().empty());
        EXPECT_EQ(ctx, release->audio().front().trimLeadingSamples,
                  static_cast<std::uint32_t>(1'920));
    }
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, runtime.value()->stop(execution));
}

void testNodeDropsOldEnvelopeAfterAcknowledgedGenerationPurge(
    TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("audio", audio(1, 0, 200, 9'600), 0));
    EXPECT_TRUE(ctx, harness.push("video", video(2, 0, true), 1));
    EXPECT_TRUE(ctx, harness.push("video", video(3, 40, false), 2));
    EXPECT_TRUE(ctx, harness.push("video", video(4, 80, false), 3));
    MediaChannel* output = harness.execution.findOutputChannel(
        harness.coordinator, "release");
    EXPECT_TRUE(ctx, output != nullptr);
    if (!output) return;
    for (int index = 0; index < 8 && output->size() == 0; ++index) {
        EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    }
    MediaBufferRef initial;
    EXPECT_TRUE(ctx, output->tryPop(initial));
    EXPECT_TRUE(ctx, harness.generationState->purge(
                         MediaAvGenerationPurge{7, 8, 1}));

    EXPECT_TRUE(ctx, harness.push(
                         "audio", audio(10, 0, 200, 9'600, 8), 4));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.push(
                         "video", video(100, 120, false, 7), 5));
    const auto dropped = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, dropped);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));
}

void testNodeFinishesAfterOneBackpressuredTerminalControl(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("audio", audio(1, 0, 200, 9'600), 0));
    EXPECT_TRUE(ctx, harness.push("video", video(2, 0, true), 1));
    EXPECT_TRUE(ctx, harness.push("video", video(3, 40, false), 2));
    EXPECT_TRUE(ctx, harness.push("video", video(4, 80, false), 3));
    MediaChannel* output = harness.execution.findOutputChannel(
        harness.coordinator, "release");
    EXPECT_TRUE(ctx, output != nullptr);
    if (!output) return;
    for (int index = 0; index < 8 && output->size() == 0; ++index) {
        EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    }
    MediaBufferRef release;
    EXPECT_TRUE(ctx, output->tryPop(release));
    EXPECT_TRUE(ctx, harness.control("video", MediaControlBufferKind::Eof));
    auto firstEof = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, firstEof);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(0));

    for (std::size_t index = 0; index < output->capacity(); ++index) {
        EXPECT_TRUE(ctx, output->push(makeMediaBufferRef<MediaAvStartupClockBuffer>(ns(10))));
    }
    EXPECT_TRUE(ctx, harness.control("audio", MediaControlBufferKind::Eof));
    auto terminal = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, terminal);
    if (terminal) EXPECT_EQ(ctx, terminal.value().state, MediaNodeProcessState::Progress);
    MediaBufferRef blocker;
    while (output->tryPop(blocker)) {}
    auto retry = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, retry);
    EXPECT_EQ(ctx, output->size(), static_cast<std::size_t>(1));
    MediaBufferRef eof;
    EXPECT_TRUE(ctx, output->tryPop(eof));
    const auto* control = dynamic_cast<const MediaControlBuffer*>(eof.get());
    EXPECT_TRUE(ctx, control != nullptr && control->controlKind() == MediaControlBufferKind::Eof);
    auto finished = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, finished);
    if (finished) EXPECT_EQ(ctx, finished.value().state, MediaNodeProcessState::Finished);
    EXPECT_TRUE(ctx, output->closed());
}

void testNodeTerminalSnapshotCannotBeStarvedByContinuousClock(TestContext& ctx)
{
    StartupNodeHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("audio", audio(1, 0, 200, 9'600), 0));
    EXPECT_TRUE(ctx, harness.push("video", video(2, 0, true), 1));
    EXPECT_TRUE(ctx, harness.push("video", video(3, 40, false), 2));
    EXPECT_TRUE(ctx, harness.push("video", video(4, 80, false), 3));
    MediaChannel* output = harness.execution.findOutputChannel(
        harness.coordinator, "release");
    EXPECT_TRUE(ctx, output != nullptr);
    if (!output) return;
    for (int index = 0; index < 8 && output->size() == 0; ++index) {
        EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    }
    MediaBufferRef release;
    EXPECT_TRUE(ctx, output->tryPop(release));
    for (std::int64_t now = 100; now < 104; ++now) {
        EXPECT_TRUE(ctx, harness.tick(now));
    }
    EXPECT_TRUE(ctx, harness.control("video", MediaControlBufferKind::Eof));
    EXPECT_TRUE(ctx, harness.control("audio", MediaControlBufferKind::Eof));
    bool finished = false;
    for (std::int64_t step = 0; step < 16 && !finished; ++step) {
        EXPECT_TRUE(ctx, harness.tick(104 + step));
        auto processed = harness.runtime->process(harness.execution);
        EXPECT_TRUE(ctx, processed);
        if (!processed) return;
        finished = processed.value().state == MediaNodeProcessState::Finished;
    }
    EXPECT_TRUE(ctx, finished);
}

void testNodeFailsClosedWithoutCommonCanonicalEnvelope(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(2);
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "startup.source");
    const auto clockSource = graph.addNode(MediaNodeKind::DebugDump, "startup.clock");
    const auto coordinator = graph.addNode(MediaNodeKind::AvStartupCoordinator,
                                           "startup.coordinator");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "startup.sink");
    graph.addOutputPort(source, "video", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(source, "audio", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(clockSource, "clock", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "video", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "audio", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(coordinator, "clock", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(coordinator, "release", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(sink, "in", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.connect(source, "video", coordinator, "video", "video", policy);
    graph.connect(source, "audio", coordinator, "audio", "audio", policy);
    graph.connect(clockSource, "clock", coordinator, "clock", "clock", policy);
    graph.connect(coordinator, "release", sink, "in", "release", policy);
    setNodePolicy(graph, coordinator);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    auto runtime = MediaRuntimeNodeFactory::create(*graph.findNode(coordinator));
    EXPECT_TRUE(ctx, runtime);
    if (!runtime) return;
    EXPECT_TRUE(ctx, runtime.value()->start(execution));
    EXPECT_TRUE(ctx, execution.findInputChannel(coordinator, "video")->push(
        makeMediaBufferRef<MediaRtpClockGroupBuffer>(MediaRtpClockGroupSnapshot{
            MediaRtpClockGroupState::Acquiring, 0, std::nullopt})));
    EXPECT_FALSE(ctx, runtime.value()->process(execution));
    runtime.value()->abort(execution);
}

} // namespace

void runAvStartupCoordinatorTests(TestContext& ctx)
{
    testStateMachineRejectsIllegalTransitions(ctx);
    testAudioFirstReleasesAtVideoKeyFrameAndTrimsSamples(ctx);
    testStartupTrimUsesTheAbsoluteEpochSampleGrid(ctx);
    testStartupTrimUsesAuthoritativeCanonicalSampleSpan(ctx);
    testAuthoritativeSampleSpanHandlesSignedBoundaries(ctx);
    testVideoFirstWaitsForCommonWindowAndReleasesOnce(ctx);
    testRunningDropsRepeatedOrRegressedPerStreamSequence(ctx);
    testRequiresLockedSameGenerationAndPurgesOldPackets(ctx);
    testReacquireClosesGateUntilBothStreamsRelock(ctx);
    testTimeoutEofErrorAndLifecycle(ctx);
    testConfigRejectsMissingPolicySemantics(ctx);
    testKeyFrameWaitAndInitialSkewAreEnforced(ctx);
    testByteCapacityAndDegradedReadinessAreExplicit(ctx);
    testPurgesExactUnitIdsAcrossClockLossAndGenerationAdvance(ctx);
    testCommonWindowScansLaterKeyFrameAndRequiresContinuousPreroll(ctx);
    testNestedIntervalsAndLaterAudioCandidateFormCommonWindow(ctx);
    testReleasePurgesEveryUnselectedPrefixIdentity(ctx);
    testRunningRejectsMediaAfterThatStreamEof(ctx);
    testPresentationIndexRemainsBoundedAtCapacity256(ctx);
    testPresentationSelectionHasBoundedWorstCaseOperations(ctx);
    testNegativePresentationTimesDoNotUseZeroAsCoverage(ctx);
    testCoverageExcludesUnitsBeforeSelectedArrivalSuffix(ctx);
    testEqualPresentationTimesKeepStableUnitIdentityOrder(ctx);
    testCoverageArithmeticOverflowIsTyped(ctx);
    testGlobalWatermarkPreventsLateEventTimeRegression(ctx);
    testEnvelopeUsesTrustedPayloadFootprint(ctx);
    testFfmpegPacketFootprintIncludesSideDataOnlyPayload(ctx);
    testFactoryRejectsIncompleteStartupConfiguration(ctx);
    testGenerationTargetIdentitySurvivesLifecycleCleanup(ctx);
    testNodeWaitsWithoutClockOrMedia(ctx);
    testNodeReportsProgressWhenActivatingClockBarrier(ctx);
    testNodeProcessesDeadlineMediaBeforeEqualClockTick(ctx);
    testNodeDrainsCapacityOneMediaSnapshotBeforeDeadlineClock(ctx);
    testNodeClockBarrierDrainsSnapshotWithoutTakingFutureMedia(ctx);
    testNodeProcessesQueuedReleaseMediaBeforeTerminalControl(ctx);
    testNodeFreezesNewIntakeWhileDrainingClockWatermark(ctx);
    testNodeRejectsPerStreamEventTimeRegression(ctx);
    testNodeDropsRepeatedSequenceAndContinues(ctx);
    testNodePublishesOneImmutablePairedEnvelope(ctx);
    testNodeDropsOldEnvelopeAfterAcknowledgedGenerationPurge(ctx);
    testNodeFinishesAfterOneBackpressuredTerminalControl(ctx);
    testNodeTerminalSnapshotCannotBeStarvedByContinuousClock(ctx);
    testNodeFailsClosedWithoutCommonCanonicalEnvelope(ctx);
}
