#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/sync/MediaVideoSyncController.h"

#include <limits>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaRealtimeRtpTranscodeRequest avSyncRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "video-sync-controller";
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
    return request;
}

MediaAvSyncPlan completePlan(bool allowRepeat = true)
{
    auto planned = MediaAvSyncPlanner::plan(avSyncRequest());
    if (!planned) return {};
    MediaAvSyncPlan plan = std::move(planned).value();
    plan.audioServo.commandLeadNs = ms(1'500);
    plan.audioServo.compensationWindowNs = ms(2'000);
    plan.audioServo.frequencyFilterTimeConstantNs = ms(5'000);
    plan.video.allowRecoveryRepeat = allowRepeat;
    plan.video.maximumConsecutiveRecoveryActions = 2;
    return plan;
}

MediaVideoFrameMeasurement measurement(
    std::int64_t presentationMs,
    std::int64_t phaseErrorMs,
    std::uint64_t sequence,
    bool keyFrame = false,
    std::uint64_t generation = 1) noexcept
{
    return MediaVideoFrameMeasurement{
        ms(presentationMs),
        ms(presentationMs),
        ms(presentationMs - phaseErrorMs),
        generation,
        sequence,
        keyFrame};
}

MediaVideoRepeatRequest repeatRequest(
    std::int64_t repeatPresentationMs,
    std::int64_t lastEmittedPresentationMs,
    std::int64_t masterNowMs,
    std::uint64_t sequence,
    std::uint64_t generation = 1) noexcept
{
    return MediaVideoRepeatRequest{
        ms(repeatPresentationMs),
        ms(repeatPresentationMs),
        ms(lastEmittedPresentationMs),
        ms(masterNowMs),
        generation,
        sequence};
}

MediaAvSyncResult<MediaVideoSyncController> makeController(
    TestContext& ctx,
    bool allowRepeat = true)
{
    auto controller = MediaVideoSyncController::create(completePlan(allowRepeat), 1);
    EXPECT_TRUE(ctx, controller);
    return controller;
}

void testEarlyHoldAndDisplayWindowsKeepPresentationTime(TestContext& ctx)
{
    auto created = makeController(ctx);
    if (!created) return;
    auto controller = std::move(created).value();

    const auto earlyMeasurement = measurement(1'000, 21, 1);
    auto early = controller.update(earlyMeasurement);
    EXPECT_TRUE(ctx, early);
    if (!early) return;
    EXPECT_EQ(ctx, early.value().kind(), MediaVideoSyncDecisionKind::Hold);
    EXPECT_EQ(ctx, early.value().presentationOnMaster(),
              earlyMeasurement.targetPresentationOnMaster);
    EXPECT_EQ(ctx, early.value().phaseError(), ms(21));

    const auto exactEarly = measurement(1'000, 20, 1);
    auto displayEarly = controller.update(exactEarly);
    EXPECT_TRUE(ctx, displayEarly);
    if (!displayEarly) return;
    EXPECT_EQ(ctx, displayEarly.value().kind(), MediaVideoSyncDecisionKind::Display);

    const auto exactLate = measurement(1'080, -40, 2);
    auto displayLateBoundary = controller.update(exactLate);
    EXPECT_TRUE(ctx, displayLateBoundary);
    if (!displayLateBoundary) return;
    EXPECT_EQ(ctx, displayLateBoundary.value().kind(), MediaVideoSyncDecisionKind::Display);

    const auto lateMeasurement = measurement(1'120, -41, 3);
    auto late = controller.update(lateMeasurement);
    EXPECT_TRUE(ctx, late);
    if (!late) return;
    EXPECT_EQ(ctx, late.value().kind(), MediaVideoSyncDecisionKind::DisplayLate);
    EXPECT_EQ(ctx, late.value().presentationOnMaster(),
              lateMeasurement.targetPresentationOnMaster);

    auto lastLate = controller.update(measurement(1'160, -79, 5));
    EXPECT_TRUE(ctx, lastLate);
    if (!lastLate) return;
    EXPECT_EQ(ctx, lastLate.value().kind(), MediaVideoSyncDecisionKind::DisplayLate);
}

void testRecoverableLateNonKeyFrameDropsAtBoundary(TestContext& ctx)
{
    auto created = makeController(ctx);
    if (!created) return;
    auto controller = std::move(created).value();
    const auto lateMeasurement = measurement(2'000, -80, 1);
    auto decision = controller.update(lateMeasurement);
    EXPECT_TRUE(ctx, decision);
    if (!decision) return;
    EXPECT_EQ(ctx, decision.value().kind(), MediaVideoSyncDecisionKind::Drop);
    EXPECT_EQ(ctx, decision.value().presentationOnMaster(),
              lateMeasurement.targetPresentationOnMaster);
    EXPECT_EQ(ctx, decision.value().consecutiveRecoveryActions(), 1);
}

void testRepeatOnlyAuthorizesExplicitCadenceRecovery(TestContext& ctx)
{
    auto created = makeController(ctx, true);
    if (!created) return;
    auto controller = std::move(created).value();

    auto late = controller.update(measurement(3'000, -100, 1));
    EXPECT_TRUE(ctx, late);
    if (!late) return;
    EXPECT_EQ(ctx, late.value().kind(), MediaVideoSyncDecisionKind::Drop);

    auto repeat = controller.update(repeatRequest(3'020, 3'000, 3'020, 2));
    EXPECT_TRUE(ctx, repeat);
    if (!repeat) return;
    EXPECT_EQ(ctx, repeat.value().kind(),
              MediaVideoSyncDecisionKind::RepeatPreviousFrame);
    EXPECT_EQ(ctx, repeat.value().presentationOnMaster(), ms(3'020));

    auto unavailableCreated = makeController(ctx, true);
    if (!unavailableCreated) return;
    auto unavailableController = std::move(unavailableCreated).value();
    EXPECT_FALSE(ctx, unavailableController.update(
        repeatRequest(3'000, 3'000, 3'020, 1)));
    auto decodeLeadRepeat = unavailableController.update(
        repeatRequest(3'040, 3'000, 3'020, 1));
    EXPECT_TRUE(ctx, decodeLeadRepeat);
    if (decodeLeadRepeat) {
        EXPECT_EQ(ctx, decodeLeadRepeat.value().kind(),
                  MediaVideoSyncDecisionKind::RepeatPreviousFrame);
    }

    auto heldRepeatCreated = makeController(ctx, true);
    if (!heldRepeatCreated) return;
    auto heldRepeatController = std::move(heldRepeatCreated).value();
    auto heldRepeat = heldRepeatController.update(
        repeatRequest(3'041, 3'000, 3'020, 1));
    EXPECT_TRUE(ctx, heldRepeat);
    if (heldRepeat) {
        EXPECT_EQ(ctx, heldRepeat.value().kind(),
                  MediaVideoSyncDecisionKind::Hold);
    }

    auto disabledCreated = makeController(ctx, false);
    if (!disabledCreated) return;
    auto disabledController = std::move(disabledCreated).value();
    auto disabledDrop = disabledController.update(measurement(3'000, -80, 1));
    auto disabled = disabledController.update(
        repeatRequest(3'020, 3'000, 3'020, 2));
    auto afterNoAction = disabledController.update(measurement(3'040, -80, 3));
    EXPECT_TRUE(ctx, disabledDrop && disabled && afterNoAction);
    if (!disabledDrop || !disabled || !afterNoAction) return;
    EXPECT_EQ(ctx, disabled.value().kind(), MediaVideoSyncDecisionKind::NoAction);
    EXPECT_EQ(ctx, disabled.value().consecutiveRecoveryActions(), 0);
    EXPECT_EQ(ctx, afterNoAction.value().kind(), MediaVideoSyncDecisionKind::Drop);
    EXPECT_EQ(ctx, afterNoAction.value().consecutiveRecoveryActions(), 1);

    auto hardCreated = makeController(ctx, true);
    if (!hardCreated) return;
    auto hardController = std::move(hardCreated).value();
    auto hard = hardController.update(repeatRequest(2'750, 2'700, 3'000, 1));
    EXPECT_TRUE(ctx, hard);
    if (!hard) return;
    EXPECT_EQ(ctx, hard.value().kind(), MediaVideoSyncDecisionKind::Reacquire);

    auto limitedCreated = makeController(ctx, true);
    if (!limitedCreated) return;
    auto limitedController = std::move(limitedCreated).value();
    auto first = limitedController.update(repeatRequest(4'020, 4'000, 4'020, 1));
    auto second = limitedController.update(repeatRequest(4'040, 4'020, 4'040, 2));
    auto third = limitedController.update(repeatRequest(4'060, 4'040, 4'060, 3));
    EXPECT_TRUE(ctx, first && second && third);
    if (!first || !second || !third) return;
    EXPECT_EQ(ctx, first.value().kind(),
              MediaVideoSyncDecisionKind::RepeatPreviousFrame);
    EXPECT_EQ(ctx, second.value().kind(),
              MediaVideoSyncDecisionKind::RepeatPreviousFrame);
    EXPECT_EQ(ctx, third.value().kind(), MediaVideoSyncDecisionKind::Reacquire);
}

void testKeyFramesAreProtectedBelowHardDiscontinuity(TestContext& ctx)
{
    auto created = makeController(ctx);
    if (!created) return;
    auto controller = std::move(created).value();
    const auto keyMeasurement = measurement(4'000, -249, 1, true);
    auto key = controller.update(keyMeasurement);
    EXPECT_TRUE(ctx, key);
    if (!key) return;
    EXPECT_EQ(ctx, key.value().kind(),
              MediaVideoSyncDecisionKind::DisplayPreservedKeyFrame);
    EXPECT_EQ(ctx, key.value().consecutiveRecoveryActions(), 0);
    EXPECT_EQ(ctx, key.value().presentationOnMaster(),
              keyMeasurement.targetPresentationOnMaster);
}

void testRecoveryCountResetRulesAndLimit(TestContext& ctx)
{
    auto created = makeController(ctx);
    if (!created) return;
    auto controller = std::move(created).value();
    auto first = controller.update(measurement(5'000, -80, 1));
    auto hold = controller.update(measurement(5'040, 21, 2));
    auto release = controller.update(measurement(5'040, 20, 2));
    auto displayLate = controller.update(measurement(5'080, -41, 3));
    auto key = controller.update(measurement(5'120, -100, 4, true));
    auto second = controller.update(measurement(5'160, -80, 5));
    auto third = controller.update(measurement(5'200, -80, 6));
    auto fourth = controller.update(measurement(5'240, -80, 7));
    EXPECT_TRUE(ctx, first && hold && release && displayLate && key && second && third && fourth);
    if (!first || !hold || !release || !displayLate || !key || !second || !third || !fourth) return;
    EXPECT_EQ(ctx, first.value().consecutiveRecoveryActions(), 1);
    EXPECT_EQ(ctx, hold.value().consecutiveRecoveryActions(), 1);
    EXPECT_EQ(ctx, release.value().kind(), MediaVideoSyncDecisionKind::Display);
    EXPECT_EQ(ctx, displayLate.value().consecutiveRecoveryActions(), 0);
    EXPECT_EQ(ctx, key.value().consecutiveRecoveryActions(), 0);
    EXPECT_EQ(ctx, second.value().kind(), MediaVideoSyncDecisionKind::Drop);
    EXPECT_EQ(ctx, second.value().consecutiveRecoveryActions(), 1);
    EXPECT_EQ(ctx, third.value().kind(), MediaVideoSyncDecisionKind::Drop);
    EXPECT_EQ(ctx, third.value().consecutiveRecoveryActions(), 2);
    EXPECT_EQ(ctx, fourth.value().kind(), MediaVideoSyncDecisionKind::Reacquire);
    EXPECT_EQ(ctx, fourth.value().consecutiveRecoveryActions(), 0);

    auto afterLimit = controller.update(measurement(5'280, -80, 8));
    EXPECT_TRUE(ctx, afterLimit);
    if (!afterLimit) return;
    EXPECT_EQ(ctx, afterLimit.value().kind(), MediaVideoSyncDecisionKind::Drop);
    EXPECT_EQ(ctx, afterLimit.value().consecutiveRecoveryActions(), 1);

    EXPECT_TRUE(ctx, controller.reset(2));
    EXPECT_TRUE(ctx, controller.update(measurement(6'000, -80, 1, false, 2)));
    auto normal = controller.update(measurement(6'040, 0, 2, false, 2));
    EXPECT_TRUE(ctx, normal);
    if (!normal) return;
    EXPECT_EQ(ctx, normal.value().kind(), MediaVideoSyncDecisionKind::Display);
    EXPECT_EQ(ctx, normal.value().consecutiveRecoveryActions(), 0);
}

void testHardDiscontinuityAndGenerationIsolation(TestContext& ctx)
{
    auto created = makeController(ctx);
    if (!created) return;
    auto controller = std::move(created).value();
    auto hardLate = controller.update(measurement(6'000, -250, 1, true));
    EXPECT_TRUE(ctx, hardLate);
    if (!hardLate) return;
    EXPECT_EQ(ctx, hardLate.value().kind(), MediaVideoSyncDecisionKind::Reacquire);
    auto afterHard = controller.update(measurement(6'040, -80, 2));
    EXPECT_TRUE(ctx, afterHard);
    if (!afterHard) return;
    EXPECT_EQ(ctx, afterHard.value().kind(), MediaVideoSyncDecisionKind::Drop);
    EXPECT_EQ(ctx, afterHard.value().consecutiveRecoveryActions(), 1);

    EXPECT_TRUE(ctx, controller.reset(2));
    auto hardEarly = controller.update(measurement(6'000, 250, 1, true, 2));
    EXPECT_TRUE(ctx, hardEarly);
    if (!hardEarly) return;
    EXPECT_EQ(ctx, hardEarly.value().kind(), MediaVideoSyncDecisionKind::Hold);

    auto decodeLeadCreated = makeController(ctx);
    if (!decodeLeadCreated) return;
    auto decodeLeadController = std::move(decodeLeadCreated).value();
    auto decodeLead = decodeLeadController.update(MediaVideoFrameMeasurement{
        ms(6'000), ms(6'100), ms(6'000), 1, 1, true});
    EXPECT_TRUE(ctx, decodeLead);
    if (decodeLead) {
        EXPECT_EQ(ctx, decodeLead.value().kind(),
                  MediaVideoSyncDecisionKind::Display);
    }

    EXPECT_TRUE(ctx, controller.reset(3));
    EXPECT_TRUE(ctx, controller.update(measurement(7'000, -80, 1, false, 3)));
    auto old = controller.update(measurement(7'040, 0, 2, false, 2));
    auto future = controller.update(measurement(7'040, 0, 2, false, 4));
    EXPECT_TRUE(ctx, old && future);
    if (!old || !future) return;
    EXPECT_EQ(ctx, old.value().kind(), MediaVideoSyncDecisionKind::DropOldGeneration);
    EXPECT_EQ(ctx, old.value().consecutiveRecoveryActions(), 1);
    EXPECT_EQ(ctx, future.value().kind(), MediaVideoSyncDecisionKind::Reacquire);
    EXPECT_EQ(ctx, future.value().consecutiveRecoveryActions(), 1);

    MediaVideoFrameMeasurement staleOverflow{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max()),
        MediaRunningTime::fromNanoseconds(-1),
        2,
        2,
        false};
    MediaVideoRepeatRequest futureOverflow{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max()),
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(-1),
        4,
        2};
    auto staleOverflowDecision = controller.update(staleOverflow);
    auto futureOverflowDecision = controller.update(futureOverflow);
    EXPECT_TRUE(ctx, staleOverflowDecision && futureOverflowDecision);
    if (!staleOverflowDecision || !futureOverflowDecision) return;
    EXPECT_EQ(ctx,
              staleOverflowDecision.value().kind(),
              MediaVideoSyncDecisionKind::DropOldGeneration);
    EXPECT_EQ(ctx,
              futureOverflowDecision.value().kind(),
              MediaVideoSyncDecisionKind::Reacquire);

    auto staleZeroSequence = measurement(7'040, 0, 0, false, 2);
    auto futureZeroSequence = repeatRequest(7'020, 7'000, 7'040, 0, 4);
    auto staleZeroDecision = controller.update(staleZeroSequence);
    auto futureZeroDecision = controller.update(futureZeroSequence);
    EXPECT_TRUE(ctx, staleZeroDecision && futureZeroDecision);
    if (!staleZeroDecision || !futureZeroDecision) return;
    EXPECT_EQ(ctx,
              staleZeroDecision.value().kind(),
              MediaVideoSyncDecisionKind::DropOldGeneration);
    EXPECT_EQ(ctx,
              futureZeroDecision.value().kind(),
              MediaVideoSyncDecisionKind::Reacquire);

    auto current = controller.update(measurement(7'040, -80, 2, false, 3));
    EXPECT_TRUE(ctx, current);
    if (!current) return;
    EXPECT_EQ(ctx, current.value().kind(), MediaVideoSyncDecisionKind::Drop);
    EXPECT_EQ(ctx, current.value().consecutiveRecoveryActions(), 2);
}

void testMeasurementPolicyAndResetContracts(TestContext& ctx)
{
    auto created = makeController(ctx);
    if (!created) return;
    auto controller = std::move(created).value();
    EXPECT_TRUE(ctx, controller.update(measurement(8'000, 0, 1)));
    EXPECT_FALSE(ctx, controller.update(measurement(8'040, 0, 1)));
    EXPECT_FALSE(ctx, controller.update(measurement(8'040, 0, 0)));

    MediaVideoSyncMeasurement overflow = MediaVideoFrameMeasurement{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max()),
        MediaRunningTime::fromNanoseconds(-1),
        1, 2, false};
    auto overflowDecision = controller.update(overflow);
    EXPECT_FALSE(ctx, overflowDecision);
    if (!overflowDecision) {
        EXPECT_EQ(ctx, overflowDecision.error().code(), MediaAvSyncErrorCode::TimeOverflow);
    }
    EXPECT_TRUE(ctx, controller.update(measurement(8'040, 0, 2)));

    MediaVideoSyncMeasurement repeatOverflow = MediaVideoRepeatRequest{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max()),
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(-1),
        1,
        3};
    auto repeatOverflowDecision = controller.update(repeatOverflow);
    EXPECT_FALSE(ctx, repeatOverflowDecision);
    if (!repeatOverflowDecision) {
        EXPECT_EQ(ctx,
                  repeatOverflowDecision.error().code(),
                  MediaAvSyncErrorCode::TimeOverflow);
    }
    EXPECT_FALSE(ctx, controller.update(repeatRequest(8'060, 8'040, 8'060, 0)));

    auto incomplete = completePlan();
    incomplete.video.dropThresholdNs.reset();
    EXPECT_FALSE(ctx, MediaVideoSyncController::create(incomplete, 1));

    auto missingTopology = completePlan();
    missingTopology.topology.reset();
    EXPECT_FALSE(ctx, MediaVideoSyncController::create(missingTopology, 1));

    auto unknownTopology = completePlan();
    unknownTopology.topology = MediaAvSyncTopology::Unknown;
    EXPECT_FALSE(ctx, MediaVideoSyncController::create(unknownTopology, 1));
    EXPECT_FALSE(ctx, MediaVideoSyncController::create(completePlan(), 0));

    auto unorderedRecovery = completePlan(false);
    unorderedRecovery.recovery.suspectThresholdNs = ms(80);
    EXPECT_FALSE(ctx, MediaVideoSyncController::create(unorderedRecovery, 1));

    EXPECT_FALSE(ctx, controller.reset(0));
    EXPECT_FALSE(ctx, controller.reset(1));
    EXPECT_TRUE(ctx, controller.reset(2));
    EXPECT_FALSE(ctx, controller.reset(2));
    EXPECT_FALSE(ctx, controller.reset(1));
    auto newGeneration = measurement(9'000, 0, 1);
    newGeneration.generation = 2;
    EXPECT_TRUE(ctx, controller.update(newGeneration));
}

void testHoldDoesNotConsumeFrameBeforeDeadlineReevaluation(TestContext& ctx)
{
    auto created = makeController(ctx);
    if (!created) return;
    auto controller = std::move(created).value();
    const auto heldFrame = measurement(10'100, 100, 1, true);
    auto hold = controller.update(heldFrame);
    EXPECT_TRUE(ctx, hold);
    if (!hold) return;
    EXPECT_EQ(ctx, hold.value().kind(), MediaVideoSyncDecisionKind::Hold);
    EXPECT_TRUE(ctx, hold.value().recheckAtMasterTime().has_value());
    if (hold.value().recheckAtMasterTime()) {
        EXPECT_EQ(ctx, *hold.value().recheckAtMasterTime(), ms(10'080));
    }

    auto beforeDeadline = heldFrame;
    beforeDeadline.masterNow = MediaRunningTime::fromNanoseconds(
        ms(10'080).nanoseconds() - 1);
    auto stillHeld = controller.update(beforeDeadline);
    EXPECT_TRUE(ctx, stillHeld);
    if (stillHeld) {
        EXPECT_EQ(ctx, stillHeld.value().kind(), MediaVideoSyncDecisionKind::Hold);
    }
    auto overtakingFrame = measurement(10'120, 40, 2, true);
    EXPECT_FALSE(ctx, controller.update(overtakingFrame));
    auto changedTarget = heldFrame;
    changedTarget.targetPresentationOnMaster = ms(10'101);
    EXPECT_FALSE(ctx, controller.update(changedTarget));
    auto changedKeyFrame = heldFrame;
    changedKeyFrame.keyFrame = false;
    EXPECT_FALSE(ctx, controller.update(changedKeyFrame));

    auto dueFrame = heldFrame;
    dueFrame.masterNow = ms(10'080);
    auto display = controller.update(dueFrame);
    EXPECT_TRUE(ctx, display);
    if (display) {
        EXPECT_EQ(ctx, display.value().kind(), MediaVideoSyncDecisionKind::Display);
        EXPECT_EQ(ctx, display.value().sequence(), static_cast<std::uint64_t>(1));
    }
}

} // namespace

void runVideoSyncControllerTests(TestContext& ctx)
{
    testEarlyHoldAndDisplayWindowsKeepPresentationTime(ctx);
    testRecoverableLateNonKeyFrameDropsAtBoundary(ctx);
    testRepeatOnlyAuthorizesExplicitCadenceRecovery(ctx);
    testKeyFramesAreProtectedBelowHardDiscontinuity(ctx);
    testRecoveryCountResetRulesAndLimit(ctx);
    testHardDiscontinuityAndGenerationIsolation(ctx);
    testMeasurementPolicyAndResetContracts(ctx);
    testHoldDoesNotConsumeFrameBeforeDeadlineReevaluation(ctx);
}
