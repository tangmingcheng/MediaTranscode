#include "common/TestAssert.h"

#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaAvGenerationTransitionCoordinator.h"

#include <memory>
#include <optional>

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
    explicit RecordingPurgeTarget(bool succeeds = true)
        : m_succeeds(succeeds)
    {
    }

    ::media::Status purge(const MediaAvGenerationPurge& purge) override
    {
        ++calls;
        lastPurge = purge;
        return m_succeeds
            ? ::media::Status::success()
            : ::media::Status::failure(
                  ::media::ErrorInfo::internalError("planned purge failure"));
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
    auto filter = std::make_shared<RecordingPurgeTarget>(false);
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

} // namespace

int main()
{
    TestContext ctx;
    testParticipantGroupRequiresExactSealedChildSet(ctx);
    testParticipantGroupAcknowledgesOnlyCompleteSuccessfulPurge(ctx);
    testCoordinatorRejectsIncompletePlans(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
