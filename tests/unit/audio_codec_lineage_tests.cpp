#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "internal/graph/sync/lineage/MediaAudioSampleProjection.h"
#include "internal/graph/nodes/audio/AudioEncoderFrameQueue.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

extern "C" {
#include <libavutil/channel_layout.h>
}

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <limits>

using namespace media::ffmpeg::graph;

namespace {

MediaRunningTime ns(std::int64_t value)
{
    return MediaRunningTime::fromNanoseconds(value);
}

std::shared_ptr<const MediaCanonicalLineage> lineage(
    std::uint64_t generation,
    std::uint64_t sequence)
{
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        ns(static_cast<std::int64_t>(sequence) * 20'000'000),
        std::nullopt,
        ns(20'000'000),
        MediaDecodeOrderMode::PresentationOrderNoReorder,
        "audio-source",
        MediaSourceAccessUnitSequence(sequence),
        MediaTimeMappingConfidence::Locked,
        generation});
}

void aggregationAndSplitPreserveExactImmutableLineage()
{
    MediaAudioIntervalAccumulator accumulator;
    auto first = lineage(7, 1);
    auto second = lineage(7, 2);
    assert(accumulator.push({first, {0, 480, 48'000}}));
    assert(accumulator.push({second, {480, 960, 48'000}}));

    auto head = accumulator.take(600);
    assert(head && head.value().size() == 2);
    assert(head.value()[0].lineage == first);
    assert(head.value()[0].interval.begin == 0);
    assert(head.value()[0].interval.end == 480);
    assert(head.value()[1].lineage == second);
    assert(head.value()[1].interval.begin == 480);
    assert(head.value()[1].interval.end == 600);

    auto tail = accumulator.take(360);
    assert(tail && tail.value().size() == 1);
    assert(tail.value()[0].lineage == second);
    assert(tail.value()[0].interval.begin == 600);
    assert(tail.value()[0].interval.end == 960);
    assert(accumulator.finish());
}

void invalidContinuityGenerationAndResidueAreTerminal()
{
    MediaAudioIntervalAccumulator gap;
    assert(gap.push({lineage(9, 1), {0, 480, 48'000}}));
    assert(!gap.push({lineage(9, 2), {481, 960, 48'000}}));

    MediaAudioIntervalAccumulator overlap;
    assert(overlap.push({lineage(9, 1), {0, 480, 48'000}}));
    assert(!overlap.push({lineage(9, 2), {479, 960, 48'000}}));

    MediaAudioIntervalAccumulator generation;
    assert(generation.push({lineage(9, 1), {0, 480, 48'000}}));
    assert(!generation.push({lineage(10, 2), {480, 960, 48'000}}));

    MediaAudioIntervalAccumulator residue;
    assert(residue.push({lineage(9, 1), {0, 480, 48'000}}));
    assert(!residue.finish());
    assert(!residue.take(481));
}

void drainedAccumulatorRetainsContinuityAuthorityUntilReset()
{
    const auto rejectAfterDrain = [](MediaAudioIntervalFragment next) {
        MediaAudioIntervalAccumulator accumulator;
        assert(accumulator.push({lineage(9, 1), {0, 10, 48'000}}));
        assert(accumulator.take(10));
        assert(!accumulator.push(std::move(next)));
    };
    rejectAfterDrain({lineage(9, 2), {11, 21, 48'000}});
    rejectAfterDrain({lineage(9, 2), {9, 19, 48'000}});
    rejectAfterDrain({lineage(10, 2), {10, 20, 48'000}});
    rejectAfterDrain({lineage(9, 2), {10, 20, 44'100}});
}

void droppedResidueRequiresExactAppliedCorrectionAuthorization()
{
    MediaAudioIntervalAccumulator exact;
    assert(exact.push({lineage(9, 1), {0, 3, 48'000}}));
    assert(exact.settleDroppedSamples(3));
    assert(exact.finish());

    MediaAudioIntervalAccumulator underAuthorized;
    assert(underAuthorized.push({lineage(9, 1), {0, 3, 48'000}}));
    assert(!underAuthorized.settleDroppedSamples(2));

    MediaAudioIntervalAccumulator overAuthorized;
    assert(overAuthorized.push({lineage(9, 1), {0, 3, 48'000}}));
    assert(!overAuthorized.settleDroppedSamples(4));
}

void sampleCountBoundaryNeverWraps()
{
    MediaAudioIntervalAccumulator accumulator;
    const auto midpoint = std::numeric_limits<std::int64_t>::max() / 2;
    assert(accumulator.push(
        {lineage(12, 1), {0, midpoint, 48'000}}));
    assert(accumulator.push(
        {lineage(12, 2),
         {midpoint, std::numeric_limits<std::int64_t>::max(), 48'000}}));
    assert(accumulator.queuedSamples() ==
           std::numeric_limits<std::int64_t>::max());
    assert(accumulator.queuedSamples() > 0);
}

void encoderFifoSplitsExactIntervalsWithPayload()
{
    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    assert(codec);
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->frame_size = 480;
    av_channel_layout_default(&codec->ch_layout, 2);
    AudioEncoderFrameQueue queue(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8);
    assert(queue.configure(*codec));
    auto frame = ::media::ffmpeg::makeFrame();
    assert(frame);
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 48'000;
    frame->nb_samples = 960;
    frame->pts = 1'000;
    av_channel_layout_default(&frame->ch_layout, 2);
    assert(av_frame_get_buffer(frame.get(), 0) == 0);
    auto first = lineage(7, 10);
    auto second = lineage(7, 11);
    assert(queue.push(*frame, {
        {first, {1'000, 1'480, 48'000}},
        {second, {1'480, 1'960, 48'000}}}));
    auto head = queue.popFullFrame();
    assert(head && head.value().media->nb_samples == 480);
    assert(head.value().fragments.size() == 1);
    assert(head.value().fragments.front().lineage == first);
    auto tail = queue.popFullFrame();
    assert(tail && tail.value().media->nb_samples == 480);
    assert(tail.value().fragments.size() == 1);
    assert(tail.value().fragments.front().lineage == second);
    assert(queue.finishLineage());
}

void uniqueLineageCapacityDeduplicatesSplitIntervals()
{
    const auto shared = lineage(7, 20);
    const auto distinct = lineage(7, 21);
    MediaAudioLineageCapacity capacity(1);
    assert(capacity.observe(std::vector<MediaAudioIntervalFragment>{
        {shared, {0, 240, 48'000}},
        {shared, {240, 480, 48'000}}}));
    assert(capacity.leaseCount() == 1);
    assert(!capacity.observe(distinct));
}

void encoderFifoCapacityTracksUniqueLineageLeases()
{
    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    assert(codec);
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->frame_size = 480;
    av_channel_layout_default(&codec->ch_layout, 2);
    AudioEncoderFrameQueue queue(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 1);
    assert(queue.configure(*codec));

    const auto makeFrame = [](int samples, std::int64_t pts) {
        auto frame = ::media::ffmpeg::makeFrame();
        assert(frame);
        frame->format = AV_SAMPLE_FMT_FLTP;
        frame->sample_rate = 48'000;
        frame->nb_samples = samples;
        frame->pts = pts;
        av_channel_layout_default(&frame->ch_layout, 2);
        assert(av_frame_get_buffer(frame.get(), 0) == 0);
        return frame;
    };

    const auto shared = lineage(7, 30);
    const auto distinct = lineage(7, 31);
    auto first = makeFrame(240, 0);
    assert(queue.push(*first, {{shared, {0, 240, 48'000}}}));
    auto rejected = makeFrame(240, 240);
    assert(!queue.push(*rejected, {{distinct, {240, 480, 48'000}}}));
    assert(queue.queuedSamples() == 240);
    auto continuation = makeFrame(240, 240);
    assert(queue.push(
        *continuation, {{shared, {240, 480, 48'000}}}));
    auto output = queue.popFullFrame();
    assert(output && output.value().fragments.size() == 2);
    assert(output.value().fragments[0].lineage == shared);
    assert(output.value().fragments[1].lineage == shared);
    assert(queue.finishLineage());
}

void sampleProjectionIsCumulativeAndFailsClosedOnEveryOverflow()
{
    auto projection = MediaAudioSampleProjection::create(1'000, 44'100, 48'000);
    assert(projection);
    std::int64_t expectedBegin = 1'000;
    std::int64_t totalSource = 0;
    for (int index = 0; index < 10'000; ++index) {
        const std::int64_t samples = index % 3 == 0 ? 941 : 1'024;
        totalSource += samples;
        auto interval = projection.value().append(samples);
        assert(interval);
        assert(interval.value().begin == expectedBegin);
        expectedBegin = interval.value().end;
    }
    assert(expectedBegin == 1'000 + av_rescale_q_rnd(
        totalSource, AVRational{1, 44'100}, AVRational{1, 48'000},
        AV_ROUND_NEAR_INF));

    auto cumulativeOverflow = MediaAudioSampleProjection::create(
        0, 48'000, 48'000);
    assert(cumulativeOverflow);
    assert(cumulativeOverflow.value().append(
        std::numeric_limits<std::int64_t>::max()));
    assert(!cumulativeOverflow.value().append(1));

    auto rescaleOverflow = MediaAudioSampleProjection::create(
        0, 1, std::numeric_limits<int>::max());
    assert(rescaleOverflow);
    assert(!rescaleOverflow.value().append(
        std::numeric_limits<std::int64_t>::max()));

    auto startOverflow = MediaAudioSampleProjection::create(
        std::numeric_limits<std::int64_t>::max(), 48'000, 48'000);
    assert(startOverflow);
    assert(!startOverflow.value().append(1));

    auto extensionOverflow = MediaAudioSampleProjection::create(
        std::numeric_limits<std::int64_t>::max() - 1, 48'000, 48'000);
    assert(extensionOverflow);
    assert(extensionOverflow.value().append(1));
    assert(!extensionOverflow.value().extend(1));
}

} // namespace

int main()
{
    aggregationAndSplitPreserveExactImmutableLineage();
    invalidContinuityGenerationAndResidueAreTerminal();
    drainedAccumulatorRetainsContinuityAuthorityUntilReset();
    droppedResidueRequiresExactAppliedCorrectionAuthorization();
    sampleCountBoundaryNeverWraps();
    encoderFifoSplitsExactIntervalsWithPayload();
    uniqueLineageCapacityDeduplicatesSplitIntervals();
    encoderFifoCapacityTracksUniqueLineageLeases();
    sampleProjectionIsCumulativeAndFailsClosedOnEveryOverflow();
    std::cout << "audio codec lineage tests passed\n";
    return 0;
}
