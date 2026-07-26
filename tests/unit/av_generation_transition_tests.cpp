#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaAvGenerationTransitionCoordinator.h"
#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAvEpochTransitionServiceTestAccess final {
    static ::media::Status activateInitial(
        const std::shared_ptr<MediaAvEpochTransitionService>& service,
        std::uint64_t generation)
    {
        return service->activateInitial(
            MediaPlaybackEpoch{MediaRunningTime::fromNanoseconds(10'000'000),
                               MediaRunningTime::fromNanoseconds(20'000'000),
                               generation},
            MediaAudioPlaybackOrigin{
                generation,
                MediaRunningTime::fromNanoseconds(10'000'000),
                MediaRunningTime::fromNanoseconds(20'000'000),
                0,
                48'000});
    }

    static ::media::Status activateNext(
        const std::shared_ptr<MediaAvEpochTransitionService>& service,
        std::uint64_t generation,
        std::uint64_t transitionSequence)
    {
        return service->activateNextAfter(
            transitionSequence,
            MediaPlaybackEpoch{MediaRunningTime::fromNanoseconds(30'000'000),
                               MediaRunningTime::fromNanoseconds(40'000'000),
                               generation},
            MediaAudioPlaybackOrigin{
                generation,
                MediaRunningTime::fromNanoseconds(30'000'000),
                MediaRunningTime::fromNanoseconds(40'000'000),
                0,
                48'000});
    }
};

} // namespace media::ffmpeg::graph

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaAvGenerationTransitionPlan transitionPlan()
{
    return MediaAvGenerationTransitionPlan{
        {{MediaAvGenerationParticipant::CanonicalLineage,
          {"decoder", "filter"}},
         {MediaAvGenerationParticipant::Scheduler, {"scheduler"}}},
        ms(500), ms(100)};
}

class RecordingPurgeTarget final : public MediaAvGenerationPurgeTarget {
public:
    explicit RecordingPurgeTarget(
        std::optional<::media::ErrorInfo> failure = std::nullopt)
        : m_failure(std::move(failure))
    {
    }

    ::media::Status purge(const MediaAvGenerationPurge& purge) override
    {
        ++calls;
        lastPurge = purge;
        return m_failure
            ? ::media::Status::failure(*m_failure)
            : ::media::Status::success();
    }

    std::uint64_t purges() const noexcept
    {
        return static_cast<std::uint64_t>(calls);
    }

    int calls = 0;
    std::optional<MediaAvGenerationPurge> lastPurge;

private:
    std::optional<::media::ErrorInfo> m_failure;
};

void testParticipantGroupRequiresExactSealedChildSet(TestContext& ctx)
{
    const MediaAvGenerationParticipantPlan plan{
        MediaAvGenerationParticipant::CanonicalLineage,
        {"decoder", "filter"}};
    auto created = MediaAvGenerationParticipantGroup::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto group = std::move(created).value();
    auto decoder = std::make_shared<RecordingPurgeTarget>();
    auto filter = std::make_shared<RecordingPurgeTarget>();

    EXPECT_FALSE(ctx, group.registerChild("unknown", decoder));
    EXPECT_FALSE(ctx, group.registerChild("decoder", nullptr));
    EXPECT_TRUE(ctx, group.registerChild("decoder", decoder));
    EXPECT_FALSE(ctx, group.registerChild("decoder", decoder));
    EXPECT_FALSE(ctx, group.seal());
    EXPECT_TRUE(ctx, group.registerChild("filter", filter));
    EXPECT_TRUE(ctx, group.seal());
    EXPECT_FALSE(ctx, group.registerChild("filter", filter));

    const auto acknowledgement = group.purgeAll({4, 5, 9});
    EXPECT_TRUE(ctx, acknowledgement);
    if (!acknowledgement) return;
    EXPECT_EQ(ctx, acknowledgement.value().participant,
              MediaAvGenerationParticipant::CanonicalLineage);
    EXPECT_EQ(ctx, acknowledgement.value().transitionSequence,
              static_cast<std::uint64_t>(9));
    EXPECT_TRUE(ctx, acknowledgement.value().status);
    EXPECT_EQ(ctx, decoder->calls, 1);
    EXPECT_EQ(ctx, filter->calls, 1);
}

void testParticipantGroupAcknowledgesOnlyCompleteSuccessfulPurge(
    TestContext& ctx)
{
    const MediaAvGenerationParticipantPlan plan{
        MediaAvGenerationParticipant::CanonicalLineage,
        {"decoder", "filter"}};
    auto created = MediaAvGenerationParticipantGroup::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto group = std::move(created).value();
    auto decoder = std::make_shared<RecordingPurgeTarget>();
    auto filter = std::make_shared<RecordingPurgeTarget>(
        ::media::ErrorInfo::internalError("planned purge failure"));
    EXPECT_TRUE(ctx, group.registerChild("decoder", decoder));
    EXPECT_FALSE(ctx, group.purgeAll({4, 5, 9}));
    EXPECT_TRUE(ctx, group.registerChild("filter", filter));
    EXPECT_TRUE(ctx, group.seal());
    EXPECT_FALSE(ctx, group.purgeAll({4, 5, 9}));
    EXPECT_EQ(ctx, decoder->calls, 1);
    EXPECT_EQ(ctx, filter->calls, 1);
}

void testCoordinatorRejectsIncompletePlans(TestContext& ctx)
{
    auto duplicate = transitionPlan();
    duplicate.participants.push_back(duplicate.participants.front());
    EXPECT_FALSE(ctx, MediaAvGenerationTransitionCoordinator::create(
                          std::move(duplicate)));
    auto missingChildren = transitionPlan();
    missingChildren.participants.front().requiredChildren.clear();
    EXPECT_FALSE(ctx, MediaAvGenerationTransitionCoordinator::create(
                          std::move(missingChildren)));
    auto zeroDrainWindow = transitionPlan();
    zeroDrainWindow.terminalDrainWindow = ms(0);
    EXPECT_FALSE(ctx, MediaAvGenerationTransitionCoordinator::create(
                          std::move(zeroDrainWindow)));
}

class ManualMasterClock final : public MediaMasterClock {
public:
    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(m_now);
    }

    void set(MediaRunningTime now) noexcept
    {
        m_now = now;
    }

private:
    MediaRunningTime m_now = ms(0);
};

MediaAvSyncPlan syncPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "generation-transition";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 64;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = ms(40);
    auto planned = MediaAvSyncPlanner::plan(request);
    auto plan = std::move(planned).value();
    plan.audioServo.commandLeadNs = ms(1'500);
    plan.audioServo.compensationWindowNs = ms(2'000);
    plan.audioServo.frequencyFilterTimeConstantNs = ms(5'000);
    return plan;
}

MediaAvGenerationTransitionPlan reacquisitionPlan()
{
    return MediaAvGenerationTransitionPlan{
        {{MediaAvGenerationParticipant::CanonicalLineage, {"video"}},
         {MediaAvGenerationParticipant::Scheduler, {"audio"}}},
        ms(500),
        ms(100)};
}

struct ActiveGroupFixture final {
    std::shared_ptr<ManualMasterClock> clock;
    std::shared_ptr<MediaAvEpochTransitionService> transition;
    std::shared_ptr<MediaAvSyncGroupRuntime> group;
};

ActiveGroupFixture makeActiveGroup(
    TestContext& ctx,
    const MediaAvGenerationTransitionPlan& plan)
{
    ActiveGroupFixture fixture;
    fixture.clock = std::make_shared<ManualMasterClock>();
    auto transition = MediaAvEpochTransitionService::create(plan);
    EXPECT_TRUE(ctx, transition);
    if (!transition) return fixture;
    fixture.transition = std::move(transition).value();

    auto sharedEpoch = MediaSharedNtpEpoch::create(
        ms(0), std::chrono::nanoseconds(0));
    EXPECT_TRUE(ctx, sharedEpoch);
    if (!sharedEpoch) return fixture;
    auto group = MediaAvSyncGroupRuntime::create(
        MediaAvSyncGroupKey("generation-transition"),
        syncPlan(),
        fixture.clock,
        std::make_shared<const MediaSharedNtpEpoch>(
            std::move(sharedEpoch).value()),
        fixture.transition);
    EXPECT_TRUE(ctx, group);
    if (!group) return fixture;
    fixture.group = std::move(group).value();
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::activateInitial(
            fixture.transition, 1));
    return fixture;
}

std::optional<MediaAvGenerationParticipantGroup> sealedParticipant(
    TestContext& ctx,
    const MediaAvGenerationParticipantPlan& plan,
    const std::shared_ptr<RecordingPurgeTarget>& target)
{
    auto created = MediaAvGenerationParticipantGroup::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return std::nullopt;
    auto participant = std::move(created).value();
    EXPECT_TRUE(ctx, participant.registerChild(
                         plan.requiredChildren.front(), target));
    EXPECT_TRUE(ctx, participant.seal());
    return participant;
}

std::vector<MediaAvGenerationParticipantGroup> sealedParticipants(
    TestContext& ctx,
    const MediaAvGenerationTransitionPlan& plan,
    const std::shared_ptr<RecordingPurgeTarget>& videoTarget,
    const std::shared_ptr<RecordingPurgeTarget>& audioTarget,
    bool includeAudio = true)
{
    std::vector<MediaAvGenerationParticipantGroup> participants;
    auto video = sealedParticipant(ctx, plan.participants[0], videoTarget);
    if (video) participants.push_back(std::move(*video));
    if (includeAudio) {
        auto audio = sealedParticipant(ctx, plan.participants[1], audioTarget);
        if (audio) participants.push_back(std::move(*audio));
    }
    return participants;
}

bool installCoordinator(
    TestContext& ctx,
    const ActiveGroupFixture& fixture,
    std::vector<MediaAvGenerationParticipantGroup> participants)
{
    auto coordinator = MediaAvReacquisitionCoordinator::create(
        fixture.transition, fixture.clock, std::move(participants));
    EXPECT_TRUE(ctx, coordinator);
    if (!coordinator || !fixture.group) return false;
    auto installed = fixture.group->installReacquisitionCoordinator(
        std::move(coordinator).value());
    EXPECT_TRUE(ctx, installed);
    return static_cast<bool>(installed);
}

void expectSameError(
    TestContext& ctx,
    const ::media::ErrorInfo& actual,
    const ::media::ErrorInfo& expected)
{
    EXPECT_EQ(ctx, actual.code, expected.code);
    EXPECT_EQ(ctx, actual.nativeCode, expected.nativeCode);
    EXPECT_EQ(ctx, actual.message, expected.message);
}

void testGroupOwnedReacquisitionPurgesAndClosesOutput(TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(
                ctx, plan, videoTarget, audioTarget))) {
        return;
    }

    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {1, MediaAvReacquisitionReason::HardDiscontinuity}));
    const auto acquiring = fixture.group->reacquisitionSnapshot();
    EXPECT_EQ(ctx, acquiring.phase, MediaAvReacquisitionPhase::Acquiring);
    EXPECT_TRUE(ctx, acquiring.transition.has_value());
    if (!acquiring.transition) return;
    EXPECT_EQ(ctx, acquiring.transition->oldGeneration,
              static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, acquiring.transition->nextGeneration,
              static_cast<std::uint64_t>(2));
    EXPECT_FALSE(ctx, fixture.group->epochTransitionSnapshot().outputPermitted);
    EXPECT_EQ(ctx, videoTarget->purges(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, audioTarget->purges(), static_cast<std::uint64_t>(1));

    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {1, MediaAvReacquisitionReason::HardDiscontinuity}));
    EXPECT_EQ(ctx, videoTarget->purges(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, audioTarget->purges(), static_cast<std::uint64_t>(1));

    EXPECT_TRUE(ctx, fixture.group->markReacquisitionReadyForActivation(
                         2, acquiring.transition->transitionSequence));
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::ReadyForActivation);
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::activateNext(
            fixture.transition,
            2,
            acquiring.transition->transitionSequence));
    EXPECT_TRUE(ctx, fixture.group->markReacquisitionActivated(
                         2, acquiring.transition->transitionSequence));
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Inactive);
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().outputPermitted);
}

void testFutureGenerationAndPreBeginSupersession(TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(
                ctx, plan, videoTarget, audioTarget))) {
        return;
    }

    EXPECT_EQ(ctx,
              fixture.group->observeGeneration(2).value(),
              MediaAvSyncGroupRuntime::GenerationDisposition::
                  ReacquisitionRequired);
    EXPECT_EQ(ctx,
              fixture.group->observeGeneration(4).value(),
              MediaAvSyncGroupRuntime::GenerationDisposition::
                  ReacquisitionRequired);
    EXPECT_EQ(ctx,
              fixture.group->observeGeneration(3).value(),
              MediaAvSyncGroupRuntime::GenerationDisposition::
                  ReacquisitionRequired);
    const auto request = fixture.group->reacquisitionRequest();
    EXPECT_TRUE(ctx, request.has_value());
    if (!request) return;
    EXPECT_EQ(ctx, request->observedGeneration,
              static_cast<std::uint64_t>(4));
    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(*request));
    const auto snapshot = fixture.group->reacquisitionSnapshot();
    EXPECT_TRUE(ctx, snapshot.transition.has_value());
    if (snapshot.transition) {
        EXPECT_EQ(ctx, snapshot.transition->oldGeneration,
                  static_cast<std::uint64_t>(1));
        EXPECT_EQ(ctx, snapshot.transition->nextGeneration,
                  static_cast<std::uint64_t>(4));
    }
}

void testMissingCoordinatorAndIncompatibleRequestAreRejected(
    TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    EXPECT_FALSE(ctx, fixture.group->requestReacquisition(
                          {1, MediaAvReacquisitionReason::Flush}));

    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(
                ctx, plan, videoTarget, audioTarget))) {
        return;
    }
    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {3, MediaAvReacquisitionReason::FutureGeneration}));
    const auto incompatible = fixture.group->requestReacquisition(
        {4, MediaAvReacquisitionReason::FutureGeneration});
    EXPECT_FALSE(ctx, incompatible);
    if (incompatible) return;
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Aborted);
    const auto poisoned = fixture.group->requestReacquisition(
        {3, MediaAvReacquisitionReason::FutureGeneration});
    EXPECT_FALSE(ctx, poisoned);
    if (!poisoned) {
        expectSameError(ctx, poisoned.error(), incompatible.error());
    }
}

void testPurgeFailurePoisonsWithFirstError(TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    const auto plannedFailure =
        ::media::ErrorInfo::internalError("first planned purge failure");
    auto videoTarget =
        std::make_shared<RecordingPurgeTarget>(plannedFailure);
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(
                ctx, plan, videoTarget, audioTarget))) {
        return;
    }

    const auto failed = fixture.group->requestReacquisition(
        {1, MediaAvReacquisitionReason::RecoveryBudgetExhausted});
    EXPECT_FALSE(ctx, failed);
    if (failed) return;
    expectSameError(ctx, failed.error(), plannedFailure);
    EXPECT_EQ(ctx, videoTarget->purges(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, audioTarget->purges(), static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Aborted);
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().poisoned);

    const auto repeated = fixture.group->pollEpochReacquisitionTimeout();
    EXPECT_FALSE(ctx, repeated);
    if (!repeated) {
        expectSameError(ctx, repeated.error(), plannedFailure);
    }
}

void testMissingAcknowledgementTimesOutWithFirstError(TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto unusedAudioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(
                ctx,
                plan,
                videoTarget,
                unusedAudioTarget,
                false))) {
        return;
    }

    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {1, MediaAvReacquisitionReason::Flush}));
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Purging);
    fixture.clock->set(ms(500));
    const auto timedOut = fixture.group->pollEpochReacquisitionTimeout();
    EXPECT_FALSE(ctx, timedOut);
    if (timedOut) return;
    EXPECT_EQ(ctx, timedOut.error().code, ::media::ErrorCode::Cancelled);
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Aborted);
    const auto repeated = fixture.group->pollEpochReacquisitionTimeout();
    EXPECT_FALSE(ctx, repeated);
    if (!repeated) {
        expectSameError(ctx, repeated.error(), timedOut.error());
    }
}

void testUnplannedAcknowledgementPoisonsWithFirstError(TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto target = std::make_shared<RecordingPurgeTarget>();
    const MediaAvGenerationParticipantPlan unplanned{
        MediaAvGenerationParticipant::AudioCorrection,
        {"unexpected"}};
    auto participant = sealedParticipant(ctx, unplanned, target);
    if (!participant) return;
    std::vector<MediaAvGenerationParticipantGroup> participants;
    participants.push_back(std::move(*participant));
    if (!installCoordinator(ctx, fixture, std::move(participants))) {
        return;
    }

    const auto failed = fixture.group->requestReacquisition(
        {1, MediaAvReacquisitionReason::HardDiscontinuity});
    EXPECT_FALSE(ctx, failed);
    if (failed) return;
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().poisoned);
    const auto repeated = fixture.group->pollEpochReacquisitionTimeout();
    EXPECT_FALSE(ctx, repeated);
    if (!repeated) {
        expectSameError(ctx, repeated.error(), failed.error());
    }
}

} // namespace

int main()
{
    TestContext ctx;
    testParticipantGroupRequiresExactSealedChildSet(ctx);
    testParticipantGroupAcknowledgesOnlyCompleteSuccessfulPurge(ctx);
    testCoordinatorRejectsIncompletePlans(ctx);
    testGroupOwnedReacquisitionPurgesAndClosesOutput(ctx);
    testFutureGenerationAndPreBeginSupersession(ctx);
    testMissingCoordinatorAndIncompatibleRequestAreRejected(ctx);
    testPurgeFailurePoisonsWithFirstError(ctx);
    testMissingAcknowledgementTimesOutWithFirstError(ctx);
    testUnplannedAcknowledgementPoisonsWithFirstError(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
