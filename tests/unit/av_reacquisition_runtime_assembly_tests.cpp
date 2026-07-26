#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"

#include <memory>
#include <string>

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class AcceptingPurgeTarget final : public MediaAvGenerationPurgeTarget {
public:
    ::media::Status purge(const MediaAvGenerationPurge&) override
    {
        return ::media::Status::success();
    }
};

MediaAvGenerationTransitionPlan separateRtpPlan()
{
    return MediaAvGenerationTransitionPlanner::plan(
        MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp,
        ms(500),
        ms(100));
}

void registerCompletePlan(
    TestContext& ctx,
    MediaAvGenerationParticipantAssembler& assembler,
    const MediaAvGenerationTransitionPlan& plan)
{
    for (const auto& participant : plan.participants) {
        for (const auto& identity : participant.requiredChildren) {
            EXPECT_TRUE(
                ctx,
                assembler.registerTarget(
                    participant.participant,
                    MediaAvGenerationPurgeRegistration{
                        identity, std::make_shared<AcceptingPurgeTarget>()}));
        }
    }
}

void exactPlannerProductSeals(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    registerCompletePlan(ctx, assembler, plan);

    auto sealed = assembler.seal();
    EXPECT_TRUE(ctx, sealed);
    if (sealed) {
        EXPECT_EQ(ctx, sealed.value().size(), plan.participants.size());
    }
    EXPECT_FALSE(ctx, assembler.seal());
}

void missingRegistrationIsRejected(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    bool skipped = false;
    for (const auto& participant : plan.participants) {
        for (const auto& identity : participant.requiredChildren) {
            if (!skipped) {
                skipped = true;
                continue;
            }
            EXPECT_TRUE(
                ctx,
                assembler.registerTarget(
                    participant.participant,
                    {identity, std::make_shared<AcceptingPurgeTarget>()}));
        }
    }
    EXPECT_FALSE(ctx, assembler.seal());
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            plan.participants.front().participant,
            {plan.participants.front().requiredChildren.front(),
             std::make_shared<AcceptingPurgeTarget>()}));
    EXPECT_FALSE(ctx, assembler.seal());
}

void duplicateRegistrationIsRejected(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    const auto& participant = plan.participants.front();
    const auto& identity = participant.requiredChildren.front();
    EXPECT_TRUE(
        ctx,
        assembler.registerTarget(
            participant.participant,
            {identity, std::make_shared<AcceptingPurgeTarget>()}));
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            participant.participant,
            {identity, std::make_shared<AcceptingPurgeTarget>()}));
}

void unexpectedRegistrationIsRejected(TestContext& ctx)
{
    const auto plan = separateRtpPlan();
    auto created = MediaAvGenerationParticipantAssembler::create(plan);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            MediaAvGenerationParticipant::Scheduler,
            {"not_planned", std::make_shared<AcceptingPurgeTarget>()}));
}

void emptyRegistrationIsRejected(TestContext& ctx)
{
    auto created =
        MediaAvGenerationParticipantAssembler::create(separateRtpPlan());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            MediaAvGenerationParticipant::Scheduler,
            {"", std::make_shared<AcceptingPurgeTarget>()}));
}

void nullRegistrationIsRejected(TestContext& ctx)
{
    auto created =
        MediaAvGenerationParticipantAssembler::create(separateRtpPlan());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto assembler = std::move(created).value();
    EXPECT_FALSE(
        ctx,
        assembler.registerTarget(
            MediaAvGenerationParticipant::Scheduler,
            {"scheduler_generation_state", nullptr}));
}

} // namespace

int main()
{
    TestContext ctx;
    exactPlannerProductSeals(ctx);
    missingRegistrationIsRejected(ctx);
    duplicateRegistrationIsRejected(ctx);
    unexpectedRegistrationIsRejected(ctx);
    emptyRegistrationIsRejected(ctx);
    nullRegistrationIsRejected(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
