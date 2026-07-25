#include "common/TestAssert.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAudioCorrection.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"
#include "internal/graph/sync/MediaAudioDriftServo.h"

#include <limits>
#include <optional>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

constexpr MediaRunningTime ms(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

constexpr MediaRunningTime seconds(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000'000);
}

MediaAvSyncAudioServoPolicy policy()
{
    MediaAvSyncAudioServoPolicy value;
    value.deadbandNs = ms(1);
    value.phaseFilterTimeConstantNs = ms(100);
    value.frequencyFilterTimeConstantNs = seconds(2);
    value.proportionalGainPpmPerSecond = 20'000;
    value.integralGainPpmPerSecondSquared = 1'000;
    value.integratorLimitPpm = 2'000;
    value.frequencyFeedForwardNumerator = 1;
    value.frequencyFeedForwardDenominator = 1;
    value.frequencyDeadbandPpm = 10;
    value.maximumMeasuredFrequencyPpm = 1'000'000;
    value.recoveryExitFrequencyPpm = 500;
    value.antiWindupMode = MediaAudioServoAntiWindupMode::ConditionalIntegration;
    value.minimumUpdateIntervalNs = ms(10);
    value.maximumMeasurementGapNs = ms(500);
    value.maximumSlewPpmPerSecond = 1'000;
    value.normalCorrectionLimitPpm = 1'000;
    value.recoveryCorrectionLimitPpm = 5'000;
    value.recoveryEnterThresholdNs = ms(100);
    value.recoveryExitThresholdNs = ms(50);
    value.recoveryExitHoldNs = ms(200);
    value.compensationWindowNs = seconds(1);
    value.commandLeadNs = ms(750);
    value.outputSampleRate = 48'000;
    value.correctionLookaheadWindows = 2;
    return value;
}

void allowOneSecondMeasurements(MediaAvSyncAudioServoPolicy& value)
{
    value.maximumMeasurementGapNs = ms(1'000);
    value.commandLeadNs = ms(1'200);
    value.compensationWindowNs = ms(1'500);
    value.frequencyFilterTimeConstantNs = seconds(3);
}

MediaAudioDriftMeasurement measurement(std::int64_t errorMs,
                                       std::int64_t observedMs,
                                       std::uint64_t sequence = 1) noexcept
{
    return MediaAudioDriftMeasurement{
        ms(errorMs), ms(observedMs), 1, sequence, observedMs * 48, 48'000};
}

MediaAvSyncResult<MediaAudioDriftServo> makeServo(
    TestContext& ctx,
    MediaAvSyncAudioServoPolicy value = policy())
{
    auto servo = MediaAudioDriftServo::create(
        MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        value,
        ms(250),
        1);
    EXPECT_TRUE(ctx, servo);
    return servo;
}

void testDeadbandAndPhaseFiltering(TestContext& ctx)
{
    auto created = makeServo(ctx);
    if (!created) return;
    auto servo = std::move(created).value();
    auto initial = servo.update(measurement(1, 0, 1));
    EXPECT_TRUE(ctx, initial);
    if (!initial) return;
    EXPECT_EQ(ctx, initial.value().kind(), MediaAudioServoDecisionKind::Apply);
    EXPECT_TRUE(ctx, initial.value().command());
    if (!initial.value().command()) return;
    EXPECT_EQ(ctx, initial.value().command()->sampleDelta(), 0);
    EXPECT_EQ(ctx, initial.value().command()->effectiveOutputSampleIndex(), 0);

    auto belowDeadband = servo.update(measurement(1, 100, 2));
    EXPECT_TRUE(ctx, belowDeadband);
    if (!belowDeadband) return;
    EXPECT_EQ(ctx, belowDeadband.value().kind(), MediaAudioServoDecisionKind::None);

    auto filtered = servo.update(measurement(100, 200, 3));
    EXPECT_TRUE(ctx, filtered);
    if (!filtered) return;
    EXPECT_TRUE(ctx, filtered.value().filteredPhaseError().nanoseconds() > ms(1).nanoseconds());
    EXPECT_TRUE(ctx, filtered.value().filteredPhaseError().nanoseconds() < ms(100).nanoseconds());
    EXPECT_EQ(ctx, filtered.value().kind(), MediaAudioServoDecisionKind::None);
}

void testFrequencyFiltering(TestContext& ctx)
{
    auto value = policy();
    value.phaseFilterTimeConstantNs = ms(20);
    allowOneSecondMeasurements(value);
    auto created = makeServo(ctx, value);
    if (!created) return;
    auto servo = std::move(created).value();
    EXPECT_TRUE(ctx, servo.update(measurement(0, 0, 1)));
    auto correction = servo.update(measurement(1, 1000, 2));
    EXPECT_TRUE(ctx, correction);
    if (!correction) return;
    EXPECT_EQ(ctx, correction.value().filteredFrequencyPpm(), 250);
}

void testConditionalIntegrationPreventsWindup(TestContext& ctx)
{
    auto value = policy();
    value.phaseFilterTimeConstantNs = ms(20);
    value.proportionalGainPpmPerSecond = 100'000;
    value.integralGainPpmPerSecondSquared = 100'000;
    value.frequencyFeedForwardNumerator = 0;
    value.integratorLimitPpm = 5'000;
    auto created = makeServo(ctx, value);
    if (!created) return;
    auto servo = std::move(created).value();
    EXPECT_TRUE(ctx, servo.update(measurement(0, 0, 1)));
    std::optional<MediaAudioCompensationCommand> last;
    for (std::int64_t time = 500; time <= 5'000; time += 500) {
        auto saturated = servo.update(measurement(200, time, static_cast<std::uint64_t>(time / 100 + 1)));
        EXPECT_TRUE(ctx, saturated);
        if (!saturated) return;
        EXPECT_EQ(ctx, saturated.value().integralPpm(), 0);
        if (saturated.value().kind() == MediaAudioServoDecisionKind::Apply) {
            last = saturated.value().command();
        }
    }
    EXPECT_TRUE(ctx, last);
    if (!last) return;
    EXPECT_TRUE(ctx, last->stretchPpm() > 0);
    EXPECT_TRUE(ctx, last->stretchPpm() <= 5'000);
}

void testSlewAndNormalClamp(TestContext& ctx)
{
    auto value = policy();
    value.maximumSlewPpmPerSecond = 100;
    value.recoveryEnterThresholdNs = ms(200);
    value.recoveryExitThresholdNs = ms(100);
    allowOneSecondMeasurements(value);
    auto created = makeServo(ctx, value);
    if (!created) return;
    auto servo = std::move(created).value();
    EXPECT_TRUE(ctx, servo.update(measurement(0, 0, 1)));
    auto slewLimited = servo.update(measurement(90, 1000, 2));
    EXPECT_TRUE(ctx, slewLimited);
    if (!slewLimited) return;
    EXPECT_TRUE(ctx, slewLimited.value().command());
    if (!slewLimited.value().command()) return;
    EXPECT_EQ(ctx, slewLimited.value().command()->stretchPpm(), 100);

    allowOneSecondMeasurements(value);
    value.maximumSlewPpmPerSecond = 1'000;
    auto clampedCreated = makeServo(ctx, value);
    if (!clampedCreated) return;
    auto clampedServo = std::move(clampedCreated).value();
    EXPECT_TRUE(ctx, clampedServo.update(measurement(0, 0, 1)));
    auto clamped = clampedServo.update(measurement(90, 1000, 2));
    EXPECT_TRUE(ctx, clamped);
    if (!clamped) return;
    EXPECT_TRUE(ctx, clamped.value().command());
    if (!clamped.value().command()) return;
    EXPECT_EQ(ctx, clamped.value().command()->stretchPpm(), 1'000);
    EXPECT_FALSE(ctx, clamped.value().recovering());
}

void testRecoveryClampAndExitHold(TestContext& ctx)
{
    auto value = policy();
    value.recoveryExitFrequencyPpm = 500'000;
    value.recoveryExitHoldNs = seconds(1);
    auto created = makeServo(ctx, value);
    if (!created) return;
    auto servo = std::move(created).value();
    EXPECT_TRUE(ctx, servo.update(measurement(0, 0, 1)));
    std::optional<MediaAudioCompensationCommand> enter;
    for (std::int64_t time = 500; time <= 5'000; time += 500) {
        auto next = servo.update(measurement(
            150, time, static_cast<std::uint64_t>(time / 500 + 1)));
        EXPECT_TRUE(ctx, next);
        if (!next) return;
        if (next.value().kind() == MediaAudioServoDecisionKind::Apply) {
            enter = next.value().command();
        }
    }
    EXPECT_TRUE(ctx, enter);
    if (!enter) return;
    EXPECT_TRUE(ctx, enter->stretchPpm() > 1'000);
    EXPECT_TRUE(ctx, enter->stretchPpm() <= 5'000);
    EXPECT_TRUE(ctx, enter->recovering());

    auto aboveExit = servo.update(measurement(70, 5500, 12));
    EXPECT_TRUE(ctx, aboveExit);
    if (!aboveExit) return;
    EXPECT_TRUE(ctx, aboveExit.value().recovering());
    auto firstBelowExit = servo.update(measurement(20, 6000, 13));
    EXPECT_TRUE(ctx, firstBelowExit);
    if (!firstBelowExit) return;
    EXPECT_TRUE(ctx, firstBelowExit.value().recovering());
    MediaAudioServoDecision settled = firstBelowExit.value();
    for (std::int64_t time = 6500; time <= 12'000; time += 500) {
        auto next = servo.update(measurement(
            20, time, static_cast<std::uint64_t>(time / 500 + 1)));
        EXPECT_TRUE(ctx, next);
        if (!next) return;
        settled = next.value();
    }
    EXPECT_FALSE(ctx, settled.recovering());
    if (settled.command()) {
        EXPECT_TRUE(ctx, settled.command()->stretchPpm() <= 1'000);
    }
}

void testHardDiscontinuityAndReset(TestContext& ctx)
{
    auto created = makeServo(ctx);
    if (!created) return;
    auto servo = std::move(created).value();
    EXPECT_TRUE(ctx, servo.update(measurement(0, 0, 1)));
    auto discontinuity = servo.update(measurement(250, 100, 2));
    EXPECT_TRUE(ctx, discontinuity);
    if (!discontinuity) return;
    EXPECT_EQ(ctx, discontinuity.value().kind(), MediaAudioServoDecisionKind::Reacquire);
    EXPECT_FALSE(ctx, discontinuity.value().command());

    auto afterDiscontinuity = servo.update(measurement(0, 200, 3));
    EXPECT_TRUE(ctx, afterDiscontinuity);
    if (!afterDiscontinuity) return;
    EXPECT_EQ(ctx, afterDiscontinuity.value().kind(), MediaAudioServoDecisionKind::Apply);

    EXPECT_TRUE(ctx, servo.update(measurement(50, 300, 4)));
    EXPECT_TRUE(ctx, servo.reset(1, 0));
    EXPECT_FALSE(ctx, servo.reset(0, 0));
    auto afterReset = servo.update(measurement(0, 0, 1));
    EXPECT_TRUE(ctx, afterReset);
    if (!afterReset) return;
    EXPECT_EQ(ctx, afterReset.value().filteredFrequencyPpm(), 0);
    EXPECT_EQ(ctx, afterReset.value().integralPpm(), 0);
}

void testCompensationConversion(TestContext& ctx)
{
    const auto quantize = [&](std::uint64_t sequence,
                              std::int64_t index,
                              int ppm,
                              MediaRunningTime window,
                        int rate) -> std::optional<MediaAudioCompensationCommand> {
        auto created = MediaAudioCorrectionQuantizer::create(
            window, MediaRunningTime::fromNanoseconds(window.nanoseconds() / 2), rate);
        EXPECT_TRUE(ctx, created);
        if (!created) return std::nullopt;
        auto result = std::move(created).value().schedule(
            1, sequence, index, ppm,
            MediaAudioCorrectionTelemetry{ms(ppm >= 0 ? 5 : -5), 0, 0, false});
        EXPECT_TRUE(ctx, result);
        if (!result) return std::nullopt;
        EXPECT_TRUE(ctx, result.value().has_value());
        if (!result.value()) return std::nullopt;
        return std::move(*result.value());
    };
    auto positive = quantize(7, 96'000, 1'000, seconds(1), 48'000);
    EXPECT_TRUE(ctx, positive);
    if (!positive) return;
    EXPECT_EQ(ctx, positive->sampleDelta(), 48);
    EXPECT_EQ(ctx, positive->compensationDistance(), 48'048);

    auto negative = quantize(8, 144'000, -5'000, seconds(1), 48'000);
    EXPECT_TRUE(ctx, negative);
    if (!negative) return;
    EXPECT_EQ(ctx, negative->sampleDelta(), -240);
    EXPECT_EQ(ctx, negative->compensationDistance(), 47'760);

    auto halfAway = quantize(9, 192'000, 1, seconds(1), 500'000);
    EXPECT_TRUE(ctx, halfAway);
    if (!halfAway) return;
    EXPECT_EQ(ctx, halfAway->sampleDelta(), 1);
    EXPECT_EQ(ctx, halfAway->compensationDistance(), 500'001);

    auto zero = quantize(10, 692'001, 0, seconds(1), 48'000);
    EXPECT_TRUE(ctx, zero);
    if (!zero) return;
    EXPECT_EQ(ctx, zero->sampleDelta(), 0);
    EXPECT_EQ(ctx, zero->compensationDistance(), 48'000);
}

void testMeasurementContractsAndGenerationDecisions(TestContext& ctx)
{
    auto created = makeServo(ctx);
    if (!created) return;
    auto servo = std::move(created).value();
    EXPECT_TRUE(ctx, servo.update(measurement(0, 100, 1)));

    auto rollback = measurement(0, 90, 2);
    EXPECT_FALSE(ctx, servo.update(rollback));

    auto wrongRate = measurement(0, 200, 2);
    wrongRate.outputSampleRate = 44'100;
    EXPECT_FALSE(ctx, servo.update(wrongRate));

    auto oldGeneration = measurement(0, 200, 2);
    oldGeneration.generation = 0;
    auto dropped = servo.update(oldGeneration);
    EXPECT_TRUE(ctx, dropped);
    if (!dropped) return;
    EXPECT_EQ(ctx, dropped.value().kind(), MediaAudioServoDecisionKind::DropOldGeneration);

    auto futureGeneration = measurement(0, 200, 2);
    futureGeneration.generation = 2;
    auto reacquire = servo.update(futureGeneration);
    EXPECT_TRUE(ctx, reacquire);
    if (!reacquire) return;
    EXPECT_EQ(ctx, reacquire.value().kind(), MediaAudioServoDecisionKind::Reacquire);

    auto gap = measurement(30, 1'000, 2);
    auto reset = servo.update(gap);
    EXPECT_TRUE(ctx, reset);
    if (!reset) return;
    EXPECT_EQ(ctx, reset.value().kind(), MediaAudioServoDecisionKind::Reacquire);
    EXPECT_EQ(ctx, reset.value().filteredFrequencyPpm(), 0);
    EXPECT_EQ(ctx, reset.value().integralPpm(), 0);
}

void runLongTermQuantizationCase(TestContext& ctx, int targetPpm)
{
    auto value = policy();
    value.proportionalGainPpmPerSecond = 10'000;
    value.integralGainPpmPerSecondSquared = 1;
    value.frequencyFeedForwardNumerator = 0;
    allowOneSecondMeasurements(value);
    auto created = makeServo(ctx, value);
    if (!created) return;
    auto servo = std::move(created).value();
    const std::int64_t phaseNs = static_cast<std::int64_t>(targetPpm) * 100'000;
    MediaAudioDriftMeasurement initial{
        MediaRunningTime::fromNanoseconds(phaseNs), ms(0), 1, 1, 0, 48'000};
    auto initialCorrection = servo.update(initial);
    EXPECT_TRUE(ctx, initialCorrection);
    if (!initialCorrection) return;
    EXPECT_EQ(ctx, initialCorrection.value().kind(), MediaAudioServoDecisionKind::Apply);

    std::int64_t observedNs = 0;
    std::int64_t totalDelta = 0;
    std::int64_t correctionCount = 0;
    for (std::uint64_t sequence = 2; sequence <= 201; ++sequence) {
        observedNs += (sequence % 2 == 0 ? 990 : 1'000) * 1'000'000;
        MediaAudioDriftMeasurement sample{
            MediaRunningTime::fromNanoseconds(phaseNs),
            MediaRunningTime::fromNanoseconds(observedNs),
            1,
            sequence,
            observedNs * 48'000 / 1'000'000'000,
            48'000};
        auto correction = servo.update(sample);
        EXPECT_TRUE(ctx, correction);
        if (!correction) return;
        if (correction.value().kind() == MediaAudioServoDecisionKind::Apply) {
            EXPECT_TRUE(ctx, correction.value().command());
            if (!correction.value().command()) return;
            EXPECT_EQ(ctx, correction.value().command()->stretchPpm(), targetPpm);
            totalDelta += correction.value().command()->sampleDelta();
            ++correctionCount;
        }
    }
    const std::int64_t numerator =
        static_cast<std::int64_t>(72'000) * targetPpm * correctionCount;
    const std::int64_t quotient = numerator / 1'000'000;
    const std::int64_t remainder = numerator % 1'000'000;
    const std::int64_t expectedDelta = quotient +
        (remainder >= 500'000 ? 1 : (remainder <= -500'000 ? -1 : 0));
    EXPECT_EQ(ctx, totalDelta, expectedDelta);
}

void testLongTermSmallCorrectionsWithMeasurementJitter(TestContext& ctx)
{
    for (const int ppm : {-500, -100, -50, 50, 100, 500}) {
        runLongTermQuantizationCase(ctx, ppm);
    }
}

void testWindowRemainderAndExplicitEpochReset(TestContext& ctx)
{
    auto created = MediaAudioCorrectionQuantizer::create(
        MediaRunningTime::fromNanoseconds(333'333'333),
        MediaRunningTime::fromNanoseconds(200'000'000),
        44'100);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto quantizer = std::move(created).value();
    std::int64_t index = 0;
    std::int64_t nominalTotal = 0;
    for (std::uint64_t sequence = 1; sequence <= 10; ++sequence) {
        auto scheduled = quantizer.schedule(
            1, sequence, index, 100,
            MediaAudioCorrectionTelemetry{ms(0), 0, 0, false});
        EXPECT_TRUE(ctx, scheduled && scheduled.value());
        if (!scheduled || !scheduled.value()) return;
        if (!scheduled || !scheduled.value()) return;
        const auto& command = *scheduled.value();
        EXPECT_EQ(ctx, command.effectiveOutputSampleIndex(), index);
        nominalTotal += command.compensationDistance() - command.sampleDelta();
        index += command.compensationDistance();
    }
    EXPECT_EQ(ctx, nominalTotal, static_cast<std::int64_t>(147'000));

    EXPECT_TRUE(ctx, quantizer.resetEpoch(1'000));
    auto resetWindow = quantizer.schedule(
        2, 1, 1'000, 100,
        MediaAudioCorrectionTelemetry{ms(0), 0, 0, false});
    EXPECT_TRUE(ctx, resetWindow && resetWindow.value());
    if (!resetWindow || !resetWindow.value()) return;
    EXPECT_EQ(ctx, resetWindow.value()->effectiveOutputSampleIndex(),
              static_cast<std::int64_t>(1'000));
    EXPECT_EQ(ctx,
              resetWindow.value()->compensationDistance() -
                  resetWindow.value()->sampleDelta(),
              14'700);
}

void testQuantizerRejectsUnrepresentableProductsAndIndexes(TestContext& ctx)
{
    EXPECT_FALSE(ctx, MediaAudioCorrectionQuantizer::create(
        MediaRunningTime::fromNanoseconds(
            std::numeric_limits<std::int64_t>::max()),
        ms(1),
        384'000));

    auto created = MediaAudioCorrectionQuantizer::create(seconds(1), ms(750), 48'000);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto quantizer = std::move(created).value();
    auto overflow = quantizer.schedule(
        1,
        1,
        std::numeric_limits<std::int64_t>::max() - 47'999,
        0,
        MediaAudioCorrectionTelemetry{ms(0), 0, 0, false});
    EXPECT_FALSE(ctx, overflow);
}

} // namespace

void runAudioDriftServoTests(TestContext& ctx)
{
    testDeadbandAndPhaseFiltering(ctx);
    testFrequencyFiltering(ctx);
    testConditionalIntegrationPreventsWindup(ctx);
    testSlewAndNormalClamp(ctx);
    testRecoveryClampAndExitHold(ctx);
    testHardDiscontinuityAndReset(ctx);
    testCompensationConversion(ctx);
    testMeasurementContractsAndGenerationDecisions(ctx);
    testLongTermSmallCorrectionsWithMeasurementJitter(ctx);
    testWindowRemainderAndExplicitEpochReset(ctx);
    testQuantizerRejectsUnrepresentableProductsAndIndexes(ctx);
}
