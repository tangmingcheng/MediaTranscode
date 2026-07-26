#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaAvGenerationTransitionCoordinator.h"
#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
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
        std::uint64_t epochGeneration,
        std::uint64_t audioGeneration,
        std::uint64_t transitionSequence)
    {
        return service->activateNextAfter(
            transitionSequence,
            MediaPlaybackEpoch{MediaRunningTime::fromNanoseconds(30'000'000),
                               MediaRunningTime::fromNanoseconds(40'000'000),
                               epochGeneration},
            MediaAudioPlaybackOrigin{
                audioGeneration,
                MediaRunningTime::fromNanoseconds(30'000'000),
                MediaRunningTime::fromNanoseconds(40'000'000),
                0,
                48'000});
    }
};

struct MediaAvReacquisitionCoordinatorTestAccess final {
    static bool waitForActivationArbitrationWaiter(
        const MediaAvSyncGroupRuntime& group)
    {
        const auto coordinator = group.m_reacquisitionCoordinator;
        if (!coordinator) return false;
        std::unique_lock<std::mutex> lock(coordinator->m_mutex);
        return coordinator->m_activationWaitChanged.wait_for(
            lock,
            std::chrono::seconds(1),
            [&coordinator] {
                return coordinator->m_activationWaiters != 0;
            });
    }
};

} // namespace media::ffmpeg::graph

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

template <typename T>
concept HasDirectEpochReacquisitionBegin = requires(T& group) {
    group.beginEpochReacquisition(std::uint64_t{1}, std::uint64_t{2});
};

template <typename T>
concept HasDirectEpochReacquisitionAcknowledgement = requires(
    T& group,
    MediaAvGenerationAcknowledgement acknowledgement) {
    group.acknowledgeEpochReacquisition(std::move(acknowledgement));
};

static_assert(
    !HasDirectEpochReacquisitionBegin<MediaAvSyncGroupRuntime>);
static_assert(
    !HasDirectEpochReacquisitionAcknowledgement<MediaAvSyncGroupRuntime>);

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

class BlockingPurgeTarget final : public MediaAvGenerationPurgeTarget {
public:
    explicit BlockingPurgeTarget(
        std::optional<::media::ErrorInfo> failure = std::nullopt)
        : m_failure(std::move(failure))
        , m_entered(m_enteredPromise.get_future().share())
    {
    }

    ::media::Status purge(const MediaAvGenerationPurge&) override
    {
        m_enteredPromise.set_value();
        std::unique_lock<std::mutex> lock(m_mutex);
        m_releasedCondition.wait(lock, [this] { return m_released; });
        return m_failure
            ? ::media::Status::failure(*m_failure)
            : ::media::Status::success();
    }

    bool waitUntilEntered() const
    {
        return m_entered.wait_for(std::chrono::seconds(1)) ==
            std::future_status::ready;
    }

    void release()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_released = true;
        }
        m_releasedCondition.notify_all();
    }

private:
    std::optional<::media::ErrorInfo> m_failure;
    std::promise<void> m_enteredPromise;
    std::shared_future<void> m_entered;
    std::mutex m_mutex;
    std::condition_variable m_releasedCondition;
    bool m_released = false;
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
    std::shared_ptr<MediaAvGenerationPurgeTarget> target)
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

bool installCoordinator(
    TestContext& ctx,
    const ActiveGroupFixture& fixture,
    std::vector<MediaAvGenerationParticipantGroup> participants);
void expectSameError(
    TestContext& ctx,
    const ::media::ErrorInfo& actual,
    const ::media::ErrorInfo& expected);

void testCoordinatorDoesNotHoldLockAcrossPurgeCallbacks(TestContext& ctx)
{
    using namespace std::chrono_literals;
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto blockingTarget = std::make_shared<BlockingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    std::vector<MediaAvGenerationParticipantGroup> participants;
    auto video = sealedParticipant(
        ctx, plan.participants[0], blockingTarget);
    auto audio = sealedParticipant(
        ctx, plan.participants[1], audioTarget);
    if (!video || !audio) return;
    participants.push_back(std::move(*video));
    participants.push_back(std::move(*audio));
    if (!installCoordinator(ctx, fixture, std::move(participants))) return;

    auto request = std::async(std::launch::async, [&fixture] {
        return fixture.group->requestReacquisition(
            {1, MediaAvReacquisitionReason::HardDiscontinuity});
    });
    const bool entered = blockingTarget->waitUntilEntered();
    EXPECT_TRUE(ctx, entered);
    if (!entered) {
        blockingTarget->release();
        (void)request.get();
        return;
    }
    auto snapshot = std::async(std::launch::async, [&fixture] {
        return fixture.group->reacquisitionSnapshot();
    });
    const auto snapshotReadiness = snapshot.wait_for(1s);
    EXPECT_EQ(ctx, snapshotReadiness, std::future_status::ready);
    blockingTarget->release();

    const auto duringPurge = snapshot.get();
    EXPECT_EQ(ctx, duringPurge.phase, MediaAvReacquisitionPhase::Purging);
    EXPECT_TRUE(ctx, duringPurge.transition.has_value());
    EXPECT_TRUE(ctx, request.get());
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Acquiring);
}

void testConcurrentIncompatibleRequestPreservesFirstError(
    TestContext& ctx)
{
    using namespace std::chrono_literals;
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    const auto laterPurgeFailure =
        ::media::ErrorInfo::internalError("later purge callback failure");
    auto blockingTarget =
        std::make_shared<BlockingPurgeTarget>(laterPurgeFailure);
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    std::vector<MediaAvGenerationParticipantGroup> participants;
    auto video = sealedParticipant(
        ctx, plan.participants[0], blockingTarget);
    auto audio = sealedParticipant(
        ctx, plan.participants[1], audioTarget);
    if (!video || !audio) return;
    participants.push_back(std::move(*video));
    participants.push_back(std::move(*audio));
    if (!installCoordinator(ctx, fixture, std::move(participants))) return;

    auto first = std::async(std::launch::async, [&fixture] {
        return fixture.group->requestReacquisition(
            {1, MediaAvReacquisitionReason::HardDiscontinuity});
    });
    const bool entered = blockingTarget->waitUntilEntered();
    EXPECT_TRUE(ctx, entered);
    if (!entered) {
        blockingTarget->release();
        (void)first.get();
        return;
    }
    auto incompatible = std::async(std::launch::async, [&fixture] {
        return fixture.group->requestReacquisition(
            {1, MediaAvReacquisitionReason::Flush});
    });
    const auto incompatibleReadiness = incompatible.wait_for(1s);
    EXPECT_EQ(ctx, incompatibleReadiness, std::future_status::ready);
    blockingTarget->release();

    const auto rejected = incompatible.get();
    EXPECT_FALSE(ctx, rejected);
    const auto initial = first.get();
    EXPECT_FALSE(ctx, initial);
    if (!rejected && !initial) {
        const auto& expected = rejected.error();
        EXPECT_EQ(
            ctx,
            expected.message,
            std::string(
                "A/V reacquisition rejects an incompatible request after transition begin"));
        expectSameError(ctx, initial.error(), expected);
        EXPECT_TRUE(ctx, expected.message != laterPurgeFailure.message);
        const auto repeated = fixture.group->requestReacquisition(
            {1, MediaAvReacquisitionReason::HardDiscontinuity});
        EXPECT_FALSE(ctx, repeated);
        if (!repeated) {
            expectSameError(ctx, repeated.error(), expected);
        }
    }
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Aborted);
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
    auto reserved = fixture.group->reserveReacquisitionActivation(
        2, acquiring.transition->transitionSequence);
    EXPECT_TRUE(ctx, reserved);
    if (!reserved) return;
    auto activation = std::move(reserved).value();
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::activateNext(
            fixture.transition,
            2,
            2,
            acquiring.transition->transitionSequence));
    EXPECT_TRUE(ctx, activation.authorizePublication());
    EXPECT_TRUE(ctx, activation.finalizePublication());
    activation.completePublished();
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Inactive);
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().outputPermitted);
}

void testAbortBeforeActivationAuthorizationRejectsPublication(TestContext& ctx)
{
    using namespace std::chrono_literals;
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(ctx, plan, videoTarget, audioTarget))) {
        return;
    }

    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {1, MediaAvReacquisitionReason::HardDiscontinuity}));
    const auto acquiring = fixture.group->reacquisitionSnapshot();
    EXPECT_TRUE(ctx, acquiring.transition.has_value());
    if (!acquiring.transition) return;
    EXPECT_TRUE(ctx, fixture.group->markReacquisitionReadyForActivation(
                         2, acquiring.transition->transitionSequence));

    auto reserved = fixture.group->reserveReacquisitionActivation(
        2, acquiring.transition->transitionSequence);
    EXPECT_TRUE(ctx, reserved);
    if (!reserved) return;
    auto activation = std::move(reserved).value();
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::activateNext(
            fixture.transition,
            2,
            2,
            acquiring.transition->transitionSequence));

    std::promise<void> workerStarted;
    auto aborted = std::async(std::launch::async, [&fixture, &workerStarted] {
        workerStarted.set_value();
        fixture.group->markAborted();
    });
    workerStarted.get_future().wait();
    EXPECT_TRUE(
        ctx,
        MediaAvReacquisitionCoordinatorTestAccess::
            waitForActivationArbitrationWaiter(*fixture.group));
    EXPECT_EQ(ctx, fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Activating);
    activation.abandon();
    EXPECT_EQ(ctx, aborted.wait_for(1s), std::future_status::ready);
    EXPECT_FALSE(ctx, activation.authorizePublication());
    EXPECT_EQ(ctx, fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Aborted);
    EXPECT_FALSE(ctx, fixture.group->epochTransitionSnapshot().outputPermitted);
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().poisoned);
}

void testIncompatibleEvidenceBeforeActivationAuthorizationRejectsPublication(
    TestContext& ctx)
{
    using namespace std::chrono_literals;
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(ctx, plan, videoTarget, audioTarget))) {
        return;
    }

    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {1, MediaAvReacquisitionReason::HardDiscontinuity}));
    const auto acquiring = fixture.group->reacquisitionSnapshot();
    EXPECT_TRUE(ctx, acquiring.transition.has_value());
    if (!acquiring.transition) return;
    EXPECT_TRUE(ctx, fixture.group->markReacquisitionReadyForActivation(
                         2, acquiring.transition->transitionSequence));
    auto reserved = fixture.group->reserveReacquisitionActivation(
        2, acquiring.transition->transitionSequence);
    EXPECT_TRUE(ctx, reserved);
    if (!reserved) return;
    auto activation = std::move(reserved).value();
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::activateNext(
            fixture.transition,
            2,
            2,
            acquiring.transition->transitionSequence));

    std::promise<void> workerStarted;
    auto incompatible = std::async(std::launch::async, [&fixture, &workerStarted] {
        workerStarted.set_value();
        return fixture.group->observeGeneration(3);
    });
    workerStarted.get_future().wait();
    EXPECT_TRUE(
        ctx,
        MediaAvReacquisitionCoordinatorTestAccess::
            waitForActivationArbitrationWaiter(*fixture.group));
    EXPECT_EQ(ctx, fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Activating);
    activation.abandon();
    EXPECT_EQ(ctx, incompatible.wait_for(1s), std::future_status::ready);
    const auto incompatibleResult = incompatible.get();
    EXPECT_FALSE(ctx, incompatibleResult);
    EXPECT_EQ(ctx, fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Aborted);
    EXPECT_FALSE(ctx, activation.authorizePublication());
    const auto repeated = fixture.group->reserveReacquisitionActivation(
        2, acquiring.transition->transitionSequence);
    EXPECT_FALSE(ctx, repeated);
    if (!incompatibleResult && !repeated) {
        expectSameError(ctx, repeated.error(), incompatibleResult.error());
    }
    EXPECT_FALSE(ctx, fixture.group->epochTransitionSnapshot().outputPermitted);
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().poisoned);
}

void testPublicationReservationLinearizesReacquisitionBegin(
    TestContext& ctx)
{
    using namespace std::chrono_literals;
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(ctx, plan, videoTarget, audioTarget))) {
        return;
    }

    auto publication =
        fixture.group->reserveStartupReleasePublication(
            MediaAvStartupReleaseKind::ActiveEpochPassThrough,
            1,
            std::nullopt);
    EXPECT_TRUE(ctx, publication);
    if (!publication) return;
    EXPECT_EQ(ctx,
              publication.value().disposition(),
              MediaAvStartupReleaseDisposition::Publish);
    std::promise<void> workerStarted;
    auto reacquisition = std::async(std::launch::async, [&fixture, &workerStarted] {
        workerStarted.set_value();
        return fixture.group->requestReacquisition(
            {1, MediaAvReacquisitionReason::HardDiscontinuity});
    });
    workerStarted.get_future().wait();
    EXPECT_TRUE(
        ctx,
        MediaAvReacquisitionCoordinatorTestAccess::
            waitForActivationArbitrationWaiter(*fixture.group));
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().outputPermitted);

    publication.value().completePublished();
    EXPECT_EQ(ctx,
              reacquisition.wait_for(1s),
              std::future_status::ready);
    EXPECT_TRUE(ctx, reacquisition.get());
    EXPECT_FALSE(ctx, fixture.group->epochTransitionSnapshot().outputPermitted);
    EXPECT_EQ(ctx,
              fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Acquiring);
}

void testReadinessMutationCannotCrossPublishing(TestContext& ctx)
{
    using namespace std::chrono_literals;
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(ctx, plan, videoTarget, audioTarget))) {
        return;
    }

    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {1, MediaAvReacquisitionReason::HardDiscontinuity}));
    const auto acquiring = fixture.group->reacquisitionSnapshot();
    EXPECT_TRUE(ctx, acquiring.transition.has_value());
    if (!acquiring.transition) return;
    EXPECT_TRUE(ctx, fixture.group->markReacquisitionReadyForActivation(
                         2, acquiring.transition->transitionSequence));
    auto reserved = fixture.group->reserveReacquisitionActivation(
        2, acquiring.transition->transitionSequence);
    EXPECT_TRUE(ctx, reserved);
    if (!reserved) return;
    auto activation = std::move(reserved).value();
    EXPECT_TRUE(
        ctx,
        MediaAvEpochTransitionServiceTestAccess::activateNext(
            fixture.transition,
            2,
            2,
            acquiring.transition->transitionSequence));
    EXPECT_TRUE(ctx, activation.authorizePublication());
    EXPECT_TRUE(ctx, activation.finalizePublication());
    EXPECT_EQ(ctx, fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Inactive);

    std::promise<void> workerStarted;
    auto readiness = std::async(
        std::launch::async,
        [&fixture, &workerStarted, sequence =
             acquiring.transition->transitionSequence] {
            workerStarted.set_value();
            return fixture.group->markReacquisitionReadyForActivation(
                2, sequence);
        });
    workerStarted.get_future().wait();
    EXPECT_TRUE(
        ctx,
        MediaAvReacquisitionCoordinatorTestAccess::
            waitForActivationArbitrationWaiter(*fixture.group));
    EXPECT_EQ(ctx, fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Inactive);

    activation.completePublished();
    EXPECT_EQ(ctx, readiness.wait_for(1s), std::future_status::ready);
    const auto failed = readiness.get();
    EXPECT_FALSE(ctx, failed);
    EXPECT_EQ(ctx, fixture.group->reacquisitionSnapshot().phase,
              MediaAvReacquisitionPhase::Aborted);
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

    const auto firstObservation = fixture.group->observeGeneration(2);
    EXPECT_TRUE(ctx, firstObservation);
    if (!firstObservation) return;
    const auto staleRequest = fixture.group->reacquisitionRequest();
    EXPECT_TRUE(ctx, staleRequest.has_value());
    if (!staleRequest) return;
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
    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(*staleRequest));
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
    EXPECT_FALSE(ctx, fixture.group->pollEpochReacquisitionTimeout());

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
    const auto incompatible = fixture.group->observeGeneration(4);
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

std::optional<MediaAvGenerationPurge> startAcquiringReacquisition(
    TestContext& ctx,
    const ActiveGroupFixture& fixture,
    const MediaAvGenerationTransitionPlan& plan)
{
    auto videoTarget = std::make_shared<RecordingPurgeTarget>();
    auto audioTarget = std::make_shared<RecordingPurgeTarget>();
    if (!installCoordinator(
            ctx,
            fixture,
            sealedParticipants(
                ctx, plan, videoTarget, audioTarget))) {
        return std::nullopt;
    }
    EXPECT_TRUE(ctx, fixture.group->requestReacquisition(
                         {1, MediaAvReacquisitionReason::HardDiscontinuity}));
    const auto snapshot = fixture.group->reacquisitionSnapshot();
    EXPECT_EQ(ctx, snapshot.phase, MediaAvReacquisitionPhase::Acquiring);
    EXPECT_TRUE(ctx, snapshot.transition.has_value());
    return snapshot.transition;
}

void testActivationEpochPairMismatchPoisonsFirstError(TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    const auto purge = startAcquiringReacquisition(ctx, fixture, plan);
    if (!purge) return;
    EXPECT_TRUE(ctx, fixture.group->markReacquisitionReadyForActivation(
                         2, purge->transitionSequence));
    auto reserved = fixture.group->reserveReacquisitionActivation(
        2, purge->transitionSequence);
    EXPECT_TRUE(ctx, reserved);
    if (!reserved) return;
    auto activation = std::move(reserved).value();

    const auto failed =
        MediaAvEpochTransitionServiceTestAccess::activateNext(
            fixture.transition, 2, 3, purge->transitionSequence);
    EXPECT_FALSE(ctx, failed);
    if (failed) return;
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().poisoned);
    activation.abandon();
    const auto repeated = fixture.group->reserveReacquisitionActivation(
        2, purge->transitionSequence);
    EXPECT_FALSE(ctx, repeated);
    if (!repeated) {
        expectSameError(ctx, repeated.error(), failed.error());
    }
}

void testCompletedTransitionSequenceMismatchPoisonsFirstError(
    TestContext& ctx)
{
    const auto plan = reacquisitionPlan();
    auto fixture = makeActiveGroup(ctx, plan);
    if (!fixture.group) return;
    const auto purge = startAcquiringReacquisition(ctx, fixture, plan);
    if (!purge) return;
    EXPECT_TRUE(ctx, fixture.group->markReacquisitionReadyForActivation(
                         2, purge->transitionSequence));
    auto reserved = fixture.group->reserveReacquisitionActivation(
        2, purge->transitionSequence);
    EXPECT_TRUE(ctx, reserved);
    if (!reserved) return;
    auto activation = std::move(reserved).value();

    const auto failed =
        MediaAvEpochTransitionServiceTestAccess::activateNext(
            fixture.transition,
            2,
            2,
            purge->transitionSequence + 1);
    EXPECT_FALSE(ctx, failed);
    if (failed) return;
    EXPECT_TRUE(ctx, fixture.group->epochTransitionSnapshot().poisoned);
    activation.abandon();
    const auto repeated = fixture.group->reserveReacquisitionActivation(
        2, purge->transitionSequence);
    EXPECT_FALSE(ctx, repeated);
    if (!repeated) {
        expectSameError(ctx, repeated.error(), failed.error());
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
    testAbortBeforeActivationAuthorizationRejectsPublication(ctx);
    testIncompatibleEvidenceBeforeActivationAuthorizationRejectsPublication(
        ctx);
    testPublicationReservationLinearizesReacquisitionBegin(ctx);
    testReadinessMutationCannotCrossPublishing(ctx);
    testFutureGenerationAndPreBeginSupersession(ctx);
    testMissingCoordinatorAndIncompatibleRequestAreRejected(ctx);
    testActivationEpochPairMismatchPoisonsFirstError(ctx);
    testCompletedTransitionSequenceMismatchPoisonsFirstError(ctx);
    testPurgeFailurePoisonsWithFirstError(ctx);
    testMissingAcknowledgementTimesOutWithFirstError(ctx);
    testUnplannedAcknowledgementPoisonsWithFirstError(ctx);
    testCoordinatorDoesNotHoldLockAcrossPurgeCallbacks(ctx);
    testConcurrentIncompatibleRequestPreservesFirstError(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
