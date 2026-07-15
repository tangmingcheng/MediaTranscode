#include "common/TestAssert.h"

#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

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
        {
            {MediaAvGenerationParticipant::CanonicalLineage,
             {"decoder", "filter"}},
            {MediaAvGenerationParticipant::Scheduler, {"scheduler"}}
        },
        ms(500),
        ms(100)};
}

MediaPlaybackEpoch epoch(std::uint64_t generation)
{
    return MediaPlaybackEpoch{ms(10), ms(20), generation};
}

MediaAudioPlaybackOrigin audioOrigin(std::uint64_t generation)
{
    return MediaAudioPlaybackOrigin{generation, ms(10), ms(20), 0, 48'000};
}

class RecordingPurgeTarget final : public MediaAvGenerationPurgeTarget {
public:
    explicit RecordingPurgeTarget(bool succeeds = true)
        : m_succeeds(succeeds)
    {
    }

    ::media::Status purge(const MediaAvGenerationPurge& purge) override
    {
        ++calls;
        lastPurge = purge;
        if (!m_succeeds) {
            return ::media::Status::failure(
                ::media::ErrorInfo::internalError("planned purge failure"));
        }
        return ::media::Status::success();
    }

    int calls = 0;
    std::optional<MediaAvGenerationPurge> lastPurge;

private:
    bool m_succeeds;
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
    EXPECT_FALSE(ctx, group.seal());

    const MediaAvGenerationPurge purge{4, 5, 9};
    const auto acknowledgement = group.purgeAll(purge);
    EXPECT_TRUE(ctx, acknowledgement);
    if (!acknowledgement) return;
    EXPECT_EQ(ctx, acknowledgement.value().participant,
              MediaAvGenerationParticipant::CanonicalLineage);
    EXPECT_EQ(ctx, acknowledgement.value().transitionSequence,
              static_cast<std::uint64_t>(9));
    EXPECT_TRUE(ctx, acknowledgement.value().status);
    EXPECT_EQ(ctx, decoder->calls, 1);
    EXPECT_EQ(ctx, filter->calls, 1);
    EXPECT_EQ(ctx, decoder->lastPurge.value().nextGeneration,
              static_cast<std::uint64_t>(5));
}

void testParticipantGroupAcknowledgesOnlyCompleteSuccessfulPurge(TestContext& ctx)
{
    const MediaAvGenerationParticipantPlan plan{
        MediaAvGenerationParticipant::CanonicalLineage,
        {"decoder", "filter"}};
    auto created = MediaAvGenerationParticipantGroup::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto group = std::move(created).value();
    auto decoder = std::make_shared<RecordingPurgeTarget>();
    auto filter = std::make_shared<RecordingPurgeTarget>(false);
    EXPECT_TRUE(ctx, group.registerChild("decoder", decoder));
    EXPECT_FALSE(ctx, group.purgeAll(MediaAvGenerationPurge{4, 5, 9}));
    EXPECT_TRUE(ctx, group.registerChild("filter", filter));
    EXPECT_TRUE(ctx, group.seal());
    EXPECT_FALSE(ctx, group.purgeAll(MediaAvGenerationPurge{4, 5, 9}));
    EXPECT_EQ(ctx, decoder->calls, 1);
    EXPECT_EQ(ctx, filter->calls, 1);
}

void testCoordinatorRevokesAndRequiresExactAcknowledgements(TestContext& ctx)
{
    auto created = MediaAvEpochTransitionService::create(transitionPlan());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto service = created.value();
    EXPECT_TRUE(ctx, service->activateInitial(epoch(4), audioOrigin(4)));
    EXPECT_TRUE(ctx, service->snapshot().outputPermitted);

    auto purge = service->beginReacquisition(4, 5);
    EXPECT_TRUE(ctx, purge);
    if (!purge) return;
    EXPECT_FALSE(ctx, service->snapshot().outputPermitted);
    const auto sequence = purge.value().transitionSequence;
    EXPECT_FALSE(ctx, service->acknowledge({
        MediaAvGenerationParticipant::RtpAudioOutput,
        sequence,
        ::media::Status::success()}));
    EXPECT_FALSE(ctx, service->acknowledge({
        MediaAvGenerationParticipant::Scheduler,
        sequence + 1,
        ::media::Status::success()}));

    auto first = service->acknowledge({
        MediaAvGenerationParticipant::Scheduler,
        sequence,
        ::media::Status::success()});
    EXPECT_TRUE(ctx, first);
    EXPECT_FALSE(ctx, first.value());
    EXPECT_FALSE(ctx, service->acknowledge({
        MediaAvGenerationParticipant::Scheduler,
        sequence,
        ::media::Status::success()}));
    auto second = service->acknowledge({
        MediaAvGenerationParticipant::CanonicalLineage,
        sequence,
        ::media::Status::success()});
    EXPECT_TRUE(ctx, second);
    EXPECT_TRUE(ctx, second.value());
    EXPECT_FALSE(ctx, service->snapshot().outputPermitted);
    EXPECT_FALSE(ctx, service->activateNextAfter(
                          sequence, epoch(6), audioOrigin(6)));
    EXPECT_EQ(ctx, service->snapshot().readiness,
              MediaAvGenerationReadiness::Acquiring);
    EXPECT_EQ(ctx, service->snapshot().playbackEpoch,
              std::optional<MediaPlaybackEpoch>(epoch(4)));
    EXPECT_FALSE(ctx, service->snapshot().outputPermitted);
    EXPECT_TRUE(ctx, service->activateNextAfter(
                         sequence, epoch(5), audioOrigin(5)));
    EXPECT_TRUE(ctx, service->snapshot().outputPermitted);
    EXPECT_FALSE(ctx, service->activateNextAfter(
                          sequence, epoch(5), audioOrigin(5)));
}

void testCoordinatorRejectsInvalidPlansTimeoutFailureAndAbort(TestContext& ctx)
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

    auto timed = MediaAvEpochTransitionService::create(transitionPlan());
    EXPECT_TRUE(ctx, timed);
    if (!timed) return;
    EXPECT_TRUE(ctx, timed.value()->activateInitial(epoch(1), audioOrigin(1)));
    EXPECT_TRUE(ctx, timed.value()->beginReacquisition(1, 2));
    EXPECT_TRUE(ctx, timed.value()->pollTransitionTimeout(ms(499)));
    EXPECT_FALSE(ctx, timed.value()->pollTransitionTimeout(ms(500)));
    EXPECT_TRUE(ctx, timed.value()->snapshot().poisoned);
    EXPECT_FALSE(ctx, timed.value()->snapshot().outputPermitted);
    EXPECT_FALSE(ctx, timed.value()->beginReacquisition(1, 2));

    auto failed = MediaAvEpochTransitionService::create(transitionPlan());
    EXPECT_TRUE(ctx, failed);
    if (!failed) return;
    EXPECT_TRUE(ctx, failed.value()->activateInitial(epoch(1), audioOrigin(1)));
    auto purge = failed.value()->beginReacquisition(1, 2);
    EXPECT_TRUE(ctx, purge);
    if (!purge) return;
    EXPECT_FALSE(ctx, failed.value()->acknowledge({
        MediaAvGenerationParticipant::Scheduler,
        purge.value().transitionSequence,
        ::media::Status::failure(
            ::media::ErrorInfo::internalError("purge failed"))}));
    EXPECT_TRUE(ctx, failed.value()->snapshot().poisoned);

    auto aborted = MediaAvEpochTransitionService::create(transitionPlan());
    EXPECT_TRUE(ctx, aborted);
    if (!aborted) return;
    EXPECT_TRUE(ctx, aborted.value()->activateInitial(epoch(1), audioOrigin(1)));
    aborted.value()->abort();
    EXPECT_TRUE(ctx, aborted.value()->snapshot().poisoned);
    EXPECT_FALSE(ctx, aborted.value()->snapshot().outputPermitted);
    EXPECT_FALSE(ctx, aborted.value()->beginReacquisition(1, 2));
}

void testServicePublishesReadinessAndGenerationStateAtomically(TestContext& ctx)
{
    auto created = MediaAvEpochTransitionService::create(transitionPlan());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto service = created.value();
    auto initial = service->snapshot();
    EXPECT_EQ(ctx, initial.readiness, MediaAvGenerationReadiness::Acquiring);
    EXPECT_FALSE(ctx, initial.playbackEpoch);
    EXPECT_FALSE(ctx, initial.audioOrigin);
    EXPECT_FALSE(ctx, initial.outputPermitted);

    EXPECT_TRUE(ctx, service->activateInitial(epoch(4), audioOrigin(4)));
    auto locked = service->snapshot();
    EXPECT_EQ(ctx, locked.readiness, MediaAvGenerationReadiness::Locked);
    EXPECT_EQ(ctx, locked.playbackEpoch, std::optional<MediaPlaybackEpoch>(epoch(4)));
    EXPECT_EQ(ctx, locked.audioOrigin->generation, static_cast<std::uint64_t>(4));
    EXPECT_TRUE(ctx, locked.outputPermitted);
    EXPECT_FALSE(ctx, service->activateInitial(epoch(4), audioOrigin(4)));

    auto purge = service->beginReacquisition(4, 5);
    EXPECT_TRUE(ctx, purge);
    if (!purge) return;
    auto reacquire = service->snapshot();
    EXPECT_EQ(ctx, reacquire.readiness, MediaAvGenerationReadiness::Reacquire);
    EXPECT_FALSE(ctx, reacquire.outputPermitted);
    EXPECT_FALSE(ctx, service->activateNextAfter(
                          purge.value().transitionSequence,
                          epoch(5), audioOrigin(5)));

    auto first = service->acknowledge({
        MediaAvGenerationParticipant::CanonicalLineage,
        purge.value().transitionSequence,
        ::media::Status::success()});
    EXPECT_TRUE(ctx, first);
    EXPECT_FALSE(ctx, first.value());
    EXPECT_EQ(ctx, service->snapshot().readiness,
              MediaAvGenerationReadiness::Reacquire);
    auto complete = service->acknowledge({
        MediaAvGenerationParticipant::Scheduler,
        purge.value().transitionSequence,
        ::media::Status::success()});
    EXPECT_TRUE(ctx, complete);
    EXPECT_TRUE(ctx, complete.value());
    EXPECT_EQ(ctx, service->snapshot().readiness,
              MediaAvGenerationReadiness::Acquiring);
    EXPECT_FALSE(ctx, service->snapshot().outputPermitted);

    EXPECT_FALSE(ctx, service->activateNextAfter(
                          purge.value().transitionSequence + 1,
                          epoch(5), audioOrigin(5)));
    auto beforePublish = service->snapshot();
    EXPECT_FALSE(ctx, beforePublish.outputPermitted);
    EXPECT_TRUE(ctx, service->activateNextAfter(
                         purge.value().transitionSequence,
                         epoch(5), audioOrigin(5)));
    auto next = service->snapshot();
    EXPECT_EQ(ctx, next.readiness, MediaAvGenerationReadiness::Locked);
    EXPECT_EQ(ctx, next.playbackEpoch, std::optional<MediaPlaybackEpoch>(epoch(5)));
    EXPECT_EQ(ctx, next.audioOrigin->generation, static_cast<std::uint64_t>(5));
    EXPECT_TRUE(ctx, next.outputPermitted);
}

void testServiceRejectsConflictingEpochAndPoisonsOnTimeout(TestContext& ctx)
{
    auto created = MediaAvEpochTransitionService::create(transitionPlan());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto service = created.value();
    auto conflicting = audioOrigin(4);
    conflicting.masterRelease = ms(21);
    EXPECT_FALSE(ctx, service->activateInitial(epoch(4), conflicting));
    EXPECT_TRUE(ctx, service->activateInitial(epoch(4), audioOrigin(4)));
    EXPECT_TRUE(ctx, service->beginReacquisition(4, 5));
    EXPECT_FALSE(ctx, service->pollTransitionTimeout(ms(500)));
    const auto poisoned = service->snapshot();
    EXPECT_EQ(ctx, poisoned.readiness, MediaAvGenerationReadiness::Reacquire);
    EXPECT_FALSE(ctx, poisoned.outputPermitted);
    EXPECT_TRUE(ctx, poisoned.poisoned);
    EXPECT_FALSE(ctx, service->activateNextAfter(1, epoch(5), audioOrigin(5)));
}

} // namespace

int main()
{
    TestContext ctx;
    testParticipantGroupRequiresExactSealedChildSet(ctx);
    testParticipantGroupAcknowledgesOnlyCompleteSuccessfulPurge(ctx);
    testCoordinatorRevokesAndRequiresExactAcknowledgements(ctx);
    testCoordinatorRejectsInvalidPlansTimeoutFailureAndAbort(ctx);
    testServicePublishesReadinessAndGenerationStateAtomically(ctx);
    testServiceRejectsConflictingEpochAndPoisonsOnTimeout(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
