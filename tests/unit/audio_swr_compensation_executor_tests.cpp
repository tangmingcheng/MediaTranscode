#include "common/TestAssert.h"

#include "internal/graph/nodes/audio/AudioSwrCompensationExecutor.h"
#include "internal/graph/nodes/audio/AudioResampleLineageState.h"
#include "internal/graph/nodes/audio/AudioResampleSwrSession.h"
#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"
#include "internal/graph/sync/MediaAudioDriftServo.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaAudioCompensationCommand command(std::uint64_t generation,
                             std::uint64_t sequence,
                             std::int64_t effective,
                             int delta,
                             int distance,
                             TestContext& ctx)
{
    const int nominal = distance - delta;
    const int ppm = nominal > 0
        ? static_cast<int>(static_cast<std::int64_t>(delta) * 1'000'000 / nominal)
        : 0;
    auto quantizer = MediaAudioCorrectionQuantizer::create(
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(nominal) * 1'000),
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(nominal) * 500),
        1'000'000);
    if (!quantizer) {
        EXPECT_TRUE(ctx, quantizer);
        std::terminate();
    }
    auto value = std::move(quantizer).value().schedule(
        generation, sequence, effective, ppm,
        MediaAudioCorrectionTelemetry{
            MediaRunningTime::fromNanoseconds(0), 0, 0, false});
    if (!value || !value.value()) {
        EXPECT_TRUE(ctx, value && value.value());
        std::terminate();
    }
    return std::move(*value.value());
}

void testBufferKeepsImmutableCommand(TestContext& ctx)
{
    MediaAudioCorrectionBuffer buffer(command(3, 7, 1024, 48, 48048, ctx));
    EXPECT_EQ(ctx, buffer.type(), MediaBufferType::Control);
    EXPECT_EQ(ctx, buffer.command().generation(), static_cast<std::uint64_t>(3));
    EXPECT_EQ(ctx, buffer.command().sequence(), static_cast<std::uint64_t>(7));
    EXPECT_EQ(ctx, buffer.command().effectiveOutputSampleIndex(), 1024);
    EXPECT_EQ(ctx, buffer.command().sampleDelta(), 48);
    EXPECT_EQ(ctx, buffer.command().compensationDistance(), 48048);
}

void testExecutorRejectsGenerationSequenceOverlapAndGaps(TestContext& ctx)
{
    EXPECT_FALSE(ctx, AudioSwrCompensationExecutor::create(
        static_cast<MediaAudioCorrectionExecutionMode>(255), 0, 0));
    EXPECT_FALSE(ctx, AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::Disabled, 1, 0));
    EXPECT_FALSE(ctx, AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 0, 2));
    auto executorResult = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 3, 2);
    EXPECT_TRUE(ctx, executorResult);
    if (!executorResult) return;
    auto executor = std::move(executorResult).value();

    EXPECT_TRUE(ctx, executor.enqueue(command(3, 1, 0, 1, 1001, ctx)));
    EXPECT_FALSE(ctx, executor.enqueue(command(2, 2, 1001, 1, 1001, ctx)));
    EXPECT_FALSE(ctx, executor.enqueue(command(3, 1, 1001, 1, 1001, ctx)));
    EXPECT_FALSE(ctx, executor.enqueue(command(3, 2, 1000, 1, 1001, ctx)));
    EXPECT_TRUE(ctx, executor.enqueue(command(3, 2, 1001, 1, 1001, ctx)));
    EXPECT_FALSE(ctx, executor.enqueue(command(3, 3, 2003, 1, 1001, ctx)));

    auto first = executor.prepare(nullptr, 0);
    EXPECT_FALSE(ctx, first);

    auto disabled = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::Disabled, 0, 0);
    EXPECT_TRUE(ctx, disabled);
    if (!disabled) return;
    auto disabledWindow = disabled.value().prepare(nullptr, 0);
    EXPECT_TRUE(ctx, disabledWindow);
    if (!disabledWindow) return;
    EXPECT_FALSE(ctx, disabledWindow.value().correctionRequired);
}

void testRequiredModeFailsWithoutContiguousCommand(TestContext& ctx)
{
    auto executorResult = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 9, 2);
    EXPECT_TRUE(ctx, executorResult);
    if (!executorResult) return;
    auto executor = std::move(executorResult).value();
    EXPECT_FALSE(ctx, executor.prepare(nullptr, 0));
    EXPECT_TRUE(ctx, executor.enqueue(command(9, 1, 100, 0, 1000, ctx)));
    EXPECT_FALSE(ctx, executor.prepare(nullptr, 0));
}

void testZeroPpmCommandIsExecutedAsARealWindow(TestContext& ctx)
{
    auto executorResult = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 11, 2);
    EXPECT_TRUE(ctx, executorResult);
    if (!executorResult) {
        return;
    }
    auto executor = std::move(executorResult).value();
    EXPECT_TRUE(ctx, executor.enqueue(command(11, 1, 0, 0, 48'000, ctx)));

    SwrContext* swr = nullptr;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    const int allocateResult = swr_alloc_set_opts2(
        &swr,
        &stereo,
        AV_SAMPLE_FMT_FLTP,
        48'000,
        &stereo,
        AV_SAMPLE_FMT_FLTP,
        48'000,
        0,
        nullptr);
    EXPECT_EQ(ctx, allocateResult, 0);
    EXPECT_TRUE(ctx, swr != nullptr);
    if (!swr) {
        return;
    }
    EXPECT_EQ(ctx, swr_init(swr), 0);
    auto window = executor.prepare(swr, 0);
    EXPECT_TRUE(ctx, window);
    if (window) {
        EXPECT_TRUE(ctx, window.value().correctionRequired);
        EXPECT_EQ(ctx, window.value().maximumOutputSamples, 48'000);
    }
    EXPECT_TRUE(ctx, executor.advance(48'000));
    EXPECT_FALSE(ctx, executor.prepare(swr, 48'000));
    swr_free(&swr);
}

void runNonZeroBoundaryCase(TestContext& ctx,
                            int inputRate,
                            int inputSamples,
                            int delta,
                            int distance)
{
    auto executorResult = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 13, 2);
    EXPECT_TRUE(ctx, executorResult);
    if (!executorResult) return;
    auto executor = std::move(executorResult).value();
    EXPECT_TRUE(ctx, executor.enqueue(command(13, 1, 0, delta, distance, ctx)));
    EXPECT_TRUE(ctx, executor.enqueue(command(13, 2, distance, 0, 48'000, ctx)));

    SwrContext* swr = nullptr;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    EXPECT_EQ(ctx, swr_alloc_set_opts2(&swr, &stereo, AV_SAMPLE_FMT_FLTP, 48'000,
                                       &stereo, AV_SAMPLE_FMT_FLTP, inputRate,
                                       0, nullptr), 0);
    EXPECT_TRUE(ctx, swr != nullptr);
    if (!swr) return;
    EXPECT_EQ(ctx, swr_init(swr), 0);

    AVFrame* input = av_frame_alloc();
    EXPECT_TRUE(ctx, input != nullptr);
    if (!input) {
        swr_free(&swr);
        return;
    }
    input->format = AV_SAMPLE_FMT_FLTP;
    input->sample_rate = inputRate;
    input->nb_samples = inputSamples;
    EXPECT_EQ(ctx, av_channel_layout_copy(&input->ch_layout, &stereo), 0);
    EXPECT_EQ(ctx, av_frame_get_buffer(input, 0), 0);

    std::int64_t outputIndex = 0;
    bool first = true;
    for (int iteration = 0; iteration < 8; ++iteration) {
        auto window = executor.prepare(swr, outputIndex);
        EXPECT_TRUE(ctx, window);
        if (!window) break;
        const int count = first ? inputSamples : 0;
        const int bound = swr_get_out_samples(swr, count);
        const int capacity = std::min(bound, window.value().maximumOutputSamples);
        AVFrame* output = av_frame_alloc();
        output->format = AV_SAMPLE_FMT_FLTP;
        output->sample_rate = 48'000;
        output->nb_samples = capacity;
        av_channel_layout_copy(&output->ch_layout, &stereo);
        av_frame_get_buffer(output, 0);
        const int produced = swr_convert(
            swr, output->data, capacity,
            const_cast<const uint8_t**>(input->extended_data), count);
        first = false;
        EXPECT_TRUE(ctx, produced >= 0);
        if (produced <= 0) {
            av_frame_free(&output);
            break;
        }
        EXPECT_TRUE(ctx, executor.advance(produced));
        outputIndex += produced;
        av_frame_free(&output);
        if (produced < capacity) break;
    }
    EXPECT_TRUE(ctx, outputIndex > 0);
    EXPECT_TRUE(ctx, outputIndex <= distance);
    av_frame_free(&input);
    swr_free(&swr);
}

void testNonZeroAndVariableRateBoundaries(TestContext& ctx)
{
    runNonZeroBoundaryCase(ctx, 48'000, 48'000, 240, 48'240);
    runNonZeroBoundaryCase(ctx, 48'000, 48'000, -240, 47'760);
    runNonZeroBoundaryCase(ctx, 48'000, 48'000, 0, 48'000);
    runNonZeroBoundaryCase(ctx, 44'100, 44'100, 0, 48'000);
}

void testServoPublishesNextWindowBeforeExecutorBoundary(TestContext& ctx)
{
    MediaAvSyncAudioServoPolicy policy;
    policy.deadbandNs = MediaRunningTime::fromNanoseconds(1'000'000);
    policy.phaseFilterTimeConstantNs = MediaRunningTime::fromNanoseconds(50'000'000);
    policy.frequencyFilterTimeConstantNs = MediaRunningTime::fromNanoseconds(1'000'000'000);
    policy.proportionalGainPpmPerSecond = 1'000;
    policy.integralGainPpmPerSecondSquared = 100;
    policy.integratorLimitPpm = 500;
    policy.frequencyFeedForwardNumerator = 0;
    policy.frequencyFeedForwardDenominator = 1;
    policy.frequencyDeadbandPpm = 10;
    policy.maximumMeasuredFrequencyPpm = 10'000;
    policy.recoveryExitFrequencyPpm = 500;
    policy.antiWindupMode = MediaAudioServoAntiWindupMode::ConditionalIntegration;
    policy.minimumUpdateIntervalNs = MediaRunningTime::fromNanoseconds(10'000'000);
    policy.maximumMeasurementGapNs = MediaRunningTime::fromNanoseconds(100'000'000);
    policy.maximumSlewPpmPerSecond = 100;
    policy.normalCorrectionLimitPpm = 1'000;
    policy.recoveryCorrectionLimitPpm = 5'000;
    policy.recoveryEnterThresholdNs = MediaRunningTime::fromNanoseconds(100'000'000);
    policy.recoveryExitThresholdNs = MediaRunningTime::fromNanoseconds(50'000'000);
    policy.recoveryExitHoldNs = MediaRunningTime::fromNanoseconds(100'000'000);
    policy.compensationWindowNs = MediaRunningTime::fromNanoseconds(400'000'000);
    policy.commandLeadNs = MediaRunningTime::fromNanoseconds(200'000'000);
    policy.outputSampleRate = 1'000;
    policy.correctionLookaheadWindows = 2;

    auto servoResult = MediaAudioDriftServo::create(
        MediaAvSyncTopology::SeparateRtpToSeparateRtp, policy,
        MediaRunningTime::fromNanoseconds(250'000'000), 21);
    auto executorResult = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 21, 2);
    EXPECT_TRUE(ctx, servoResult && executorResult);
    if (!servoResult || !executorResult) return;
    auto servo = std::move(servoResult).value();
    auto executor = std::move(executorResult).value();
    EXPECT_FALSE(ctx, executor.reset(0));

    SwrContext* swr = nullptr;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    EXPECT_EQ(ctx, swr_alloc_set_opts2(
        &swr, &stereo, AV_SAMPLE_FMT_FLTP, 1'000,
        &stereo, AV_SAMPLE_FMT_FLTP, 1'000, 0, nullptr), 0);
    EXPECT_TRUE(ctx, swr != nullptr);
    if (!swr) return;
    EXPECT_EQ(ctx, swr_init(swr), 0);

    std::int64_t outputIndex = 0;
    for (std::uint64_t sequence = 1; sequence <= 9; ++sequence) {
        MediaAudioDriftMeasurement measurement{
            MediaRunningTime::fromNanoseconds(0),
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(sequence - 1) * 50'000'000),
            21, sequence, outputIndex, 1'000};
        auto decision = servo.update(measurement);
        EXPECT_TRUE(ctx, decision);
        if (!decision) break;
        if (decision.value().kind() == MediaAudioServoDecisionKind::Apply) {
            EXPECT_TRUE(ctx, decision.value().command());
            if (!decision.value().command()) break;
            MediaAudioCorrectionBuffer buffer(*decision.value().command());
            EXPECT_TRUE(ctx, executor.enqueue(buffer.command()));
        }
        auto window = executor.prepare(swr, outputIndex);
        EXPECT_TRUE(ctx, window);
        if (!window) break;
        EXPECT_TRUE(ctx, executor.advance(50));
        outputIndex += 50;
    }
    EXPECT_EQ(ctx, outputIndex, static_cast<std::int64_t>(450));
    auto nextWindow = executor.prepare(swr, 450);
    EXPECT_TRUE(ctx, nextWindow);
    swr_free(&swr);
}

void testFilterTailRequiresARegularContiguousLookahead(TestContext& ctx)
{
    auto executorResult = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 33, 2);
    EXPECT_TRUE(ctx, executorResult);
    if (!executorResult) return;
    auto executor = std::move(executorResult).value();
    EXPECT_TRUE(ctx, executor.enqueue(command(33, 1, 0, 0, 1'000, ctx)));

    SwrContext* swr = nullptr;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    EXPECT_EQ(ctx, swr_alloc_set_opts2(
        &swr, &stereo, AV_SAMPLE_FMT_FLTP, 48'000,
        &stereo, AV_SAMPLE_FMT_FLTP, 44'100, 0, nullptr), 0);
    EXPECT_TRUE(ctx, swr != nullptr);
    if (!swr) return;
    EXPECT_EQ(ctx, swr_init(swr), 0);

    AVFrame* input = av_frame_alloc();
    AVFrame* output = av_frame_alloc();
    EXPECT_TRUE(ctx, input && output);
    if (!input || !output) {
        av_frame_free(&input);
        av_frame_free(&output);
        swr_free(&swr);
        return;
    }
    input->format = AV_SAMPLE_FMT_FLTP;
    input->sample_rate = 44'100;
    input->nb_samples = 44'100;
    EXPECT_EQ(ctx, av_channel_layout_copy(&input->ch_layout, &stereo), 0);
    EXPECT_EQ(ctx, av_frame_get_buffer(input, 0), 0);
    output->format = AV_SAMPLE_FMT_FLTP;
    output->sample_rate = 48'000;
    output->nb_samples = 1'000;
    EXPECT_EQ(ctx, av_channel_layout_copy(&output->ch_layout, &stereo), 0);
    EXPECT_EQ(ctx, av_frame_get_buffer(output, 0), 0);

    auto active = executor.prepare(swr, 0);
    EXPECT_TRUE(ctx, active);
    if (!active) {
        av_frame_free(&input);
        av_frame_free(&output);
        swr_free(&swr);
        return;
    }
    const int produced = swr_convert(
        swr, output->data, 1'000,
        const_cast<const uint8_t**>(input->extended_data), input->nb_samples);
    EXPECT_EQ(ctx, produced, 1'000);
    EXPECT_TRUE(ctx, executor.advance(produced));
    EXPECT_TRUE(ctx, swr_get_out_samples(swr, 0) > 0);
    EXPECT_FALSE(ctx, executor.prepare(swr, 1'000));
    EXPECT_TRUE(ctx, executor.enqueue(command(33, 2, 1'000, 0, 1'000, ctx)));
    auto tailWindow = executor.prepare(swr, 1'000);
    EXPECT_TRUE(ctx, tailWindow);
    if (tailWindow) {
        // EOF has been observed: nullptr/zero is now the explicit FFmpeg tail
        // drain contract, separate from the non-null live-input test above.
        const int tailCapacity = std::min(
            swr_get_out_samples(swr, 0), tailWindow.value().maximumOutputSamples);
        const int tailProduced = swr_convert(
            swr, output->data, tailCapacity, nullptr, 0);
        EXPECT_TRUE(ctx, tailProduced > 0);
        if (tailProduced > 0) {
            EXPECT_TRUE(ctx, executor.advance(tailProduced));
        }
    }

    av_frame_free(&input);
    av_frame_free(&output);
    swr_free(&swr);
}

void testOutstandingDropAuthorizationTracksAppliedNetDelta(TestContext& ctx)
{
    const auto run = [&](std::initializer_list<int> deltas,
                         std::int64_t expectedOutstanding) {
        auto executorResult = AudioSwrCompensationExecutor::create(
            MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired,
            41, 2);
        EXPECT_TRUE(ctx, executorResult);
        if (!executorResult) return;
        auto executor = std::move(executorResult).value();

        SwrContext* swr = nullptr;
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        EXPECT_EQ(ctx, swr_alloc_set_opts2(
            &swr, &stereo, AV_SAMPLE_FMT_FLTP, 48'000,
            &stereo, AV_SAMPLE_FMT_FLTP, 48'000, 0, nullptr), 0);
        EXPECT_TRUE(ctx, swr != nullptr);
        if (!swr) return;
        EXPECT_EQ(ctx, swr_init(swr), 0);

        std::uint64_t sequence = 1;
        std::int64_t outputIndex = 0;
        for (const int delta : deltas) {
            constexpr int distance = 1'000;
            auto correction = command(
                41, sequence++, outputIndex, delta, distance, ctx);
            EXPECT_EQ(ctx, correction.sampleDelta(), delta);
            EXPECT_TRUE(ctx, executor.enqueue(correction));
            auto window = executor.prepare(swr, outputIndex);
            EXPECT_TRUE(ctx, window);
            if (!window) break;
            EXPECT_TRUE(ctx, executor.advance(distance));
            outputIndex += distance;
        }
        EXPECT_EQ(ctx, executor.outstandingAuthorizedDroppedSamples(),
                  expectedOutstanding);
        swr_free(&swr);
    };

    run({-1}, 1);
    run({1}, 0);
    run({1, -1}, 0);
    run({-1, 1}, 0);
}

void testOverflowAndIncompleteTerminalCorrectionFailClosed(TestContext& ctx)
{
    auto overflowQuantizer = MediaAudioCorrectionQuantizer::create(
        MediaRunningTime::fromNanoseconds(1'000'000),
        MediaRunningTime::fromNanoseconds(500'000), 1'000'000);
    EXPECT_TRUE(ctx, overflowQuantizer);
    if (!overflowQuantizer) return;
    auto overflow = std::move(overflowQuantizer).value().schedule(
        51, 1, std::numeric_limits<std::int64_t>::max() - 999, 0,
        MediaAudioCorrectionTelemetry{
            MediaRunningTime::fromNanoseconds(0), 0, 0, false});
    EXPECT_FALSE(ctx, overflow && overflow.value());

    const auto makeSwr = [&]() {
        SwrContext* swr = nullptr;
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        EXPECT_EQ(ctx, swr_alloc_set_opts2(
            &swr, &stereo, AV_SAMPLE_FMT_FLTP, 48'000,
            &stereo, AV_SAMPLE_FMT_FLTP, 48'000, 0, nullptr), 0);
        EXPECT_TRUE(ctx, swr != nullptr);
        if (swr) EXPECT_EQ(ctx, swr_init(swr), 0);
        return swr;
    };

    auto pending = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 52, 2);
    EXPECT_TRUE(ctx, pending);
    if (pending) {
        EXPECT_TRUE(ctx, pending.value().enqueue(command(52, 1, 0, 0, 1'000, ctx)));
        EXPECT_FALSE(ctx, pending.value().settleTerminal());
    }

    auto incomplete = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 53, 2);
    EXPECT_TRUE(ctx, incomplete);
    SwrContext* incompleteSwr = makeSwr();
    if (incomplete && incompleteSwr) {
        EXPECT_TRUE(ctx, incomplete.value().enqueue(command(53, 1, 0, 0, 1'000, ctx)));
        EXPECT_TRUE(ctx, incomplete.value().prepare(incompleteSwr, 0));
        EXPECT_TRUE(ctx, incomplete.value().advance(500));
        EXPECT_FALSE(ctx, incomplete.value().settleTerminal());
    }
    swr_free(&incompleteSwr);

    auto complete = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 54, 2);
    EXPECT_TRUE(ctx, complete);
    SwrContext* completeSwr = makeSwr();
    if (complete && completeSwr) {
        EXPECT_TRUE(ctx, complete.value().enqueue(command(54, 1, 0, 0, 1'000, ctx)));
        EXPECT_TRUE(ctx, complete.value().prepare(completeSwr, 0));
        EXPECT_TRUE(ctx, complete.value().advance(1'000));
        EXPECT_TRUE(ctx, complete.value().settleTerminal());
    }
    swr_free(&completeSwr);
}

std::optional<bool> settlePartiallyExecutedWindowAtExhaustion(
    int sampleDelta,
    TestContext& ctx)
{
    auto state = std::make_shared<AudioResampleLineageState>(
        MediaAudioLineageExecutionMode::LegacyPlainPacket, 0);
    AudioResampleSwrSession session(state);
    auto input = ::media::ffmpeg::makeFrame();
    auto target = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, input && target);
    if (!input || !target) return std::nullopt;
    input->format = AV_SAMPLE_FMT_FLTP;
    input->sample_rate = 48'000;
    input->nb_samples = 1'024;
    av_channel_layout_default(&input->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(input.get(), 0), 0);
    target->sample_fmt = AV_SAMPLE_FMT_FLTP;
    target->sample_rate = 48'000;
    av_channel_layout_default(&target->ch_layout, 2);
    EXPECT_TRUE(ctx, session.ensureInitialized(*input, *target));

    auto executorResult = AudioSwrCompensationExecutor::create(
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired, 61, 2);
    EXPECT_TRUE(ctx, executorResult);
    if (!executorResult) return std::nullopt;
    auto executor = std::move(executorResult).value();
    auto correction = command(61, 1, 0, sampleDelta, 4'096, ctx);
    EXPECT_EQ(ctx, correction.sampleDelta(), sampleDelta);
    EXPECT_TRUE(ctx, executor.enqueue(correction));
    auto window = executor.prepare(state->swr.get(), 0);
    EXPECT_TRUE(ctx, window);
    if (!window) return std::nullopt;

    auto live = session.convertLive(
        const_cast<const uint8_t**>(input->extended_data), input->nb_samples,
        window.value().maximumOutputSamples, *target);
    EXPECT_TRUE(ctx, live && live.value().produced > 0);
    if (!live || live.value().produced <= 0) return std::nullopt;
    EXPECT_TRUE(ctx, executor.advance(live.value().produced));

    bool observedTailOutput = false;
    bool observedExhaustion = false;
    std::optional<bool> settlementSucceeded;
    for (int step = 0; step < 8 && !observedExhaustion; ++step) {
        auto activeWindow = executor.prepare(
            state->swr.get(), live.value().produced);
        EXPECT_TRUE(ctx, activeWindow);
        if (!activeWindow) break;
        auto drain = session.drainQuantum(
            activeWindow.value().maximumOutputSamples, *target);
        EXPECT_TRUE(ctx, drain);
        if (!drain) break;
        if (drain.value().produced > 0) {
            observedTailOutput = true;
            EXPECT_TRUE(ctx, executor.advance(drain.value().produced));
        }
        if (drain.value().exhausted) {
            settlementSucceeded = static_cast<bool>(
                executor.settleTerminal(*drain.value().exhausted));
            observedExhaustion = true;
        }
    }
    EXPECT_TRUE(ctx, observedTailOutput);
    EXPECT_TRUE(ctx, observedExhaustion);
    EXPECT_TRUE(ctx, settlementSucceeded.has_value());
    if (!settlementSucceeded) return std::nullopt;
    if (*settlementSucceeded) {
        EXPECT_TRUE(ctx, executor.settleTerminal());
    } else {
        EXPECT_FALSE(ctx, executor.settleTerminal());
    }
    return settlementSucceeded;
}

void testExhaustionSettlesPartiallyExecutedZeroDeltaWindow(TestContext& ctx)
{
    auto settled = settlePartiallyExecutedWindowAtExhaustion(0, ctx);
    EXPECT_TRUE(ctx, settled && *settled);
}

void testExhaustionRejectsUnprovenPartialNonZeroWindow(TestContext& ctx)
{
    auto settled = settlePartiallyExecutedWindowAtExhaustion(-1, ctx);
    EXPECT_TRUE(ctx, settled.has_value());
    EXPECT_FALSE(ctx, settled && *settled);
}

} // namespace

void runAudioSwrCompensationExecutorTests(TestContext& ctx)
{
    testBufferKeepsImmutableCommand(ctx);
    testExecutorRejectsGenerationSequenceOverlapAndGaps(ctx);
    testRequiredModeFailsWithoutContiguousCommand(ctx);
    testZeroPpmCommandIsExecutedAsARealWindow(ctx);
    testNonZeroAndVariableRateBoundaries(ctx);
    testServoPublishesNextWindowBeforeExecutorBoundary(ctx);
    testFilterTailRequiresARegularContiguousLookahead(ctx);
    testOutstandingDropAuthorizationTracksAppliedNetDelta(ctx);
    testOverflowAndIncompleteTerminalCorrectionFailClosed(ctx);
    testExhaustionSettlesPartiallyExecutedZeroDeltaWindow(ctx);
    testExhaustionRejectsUnprovenPartialNonZeroWindow(ctx);
}
