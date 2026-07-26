#include "internal/graph/nodes/audio/MediaAudioStartupTrimNode.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaDecodedAudioTrimInputBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#include <cassert>
#include <iostream>
#include <memory>

using namespace media::ffmpeg::graph;

namespace {

MediaRunningTime sampleTime(std::int64_t sample)
{
    return MediaRunningTime::fromNanoseconds(sample * 1'000'000'000LL / 48'000);
}

std::shared_ptr<const MediaCanonicalLineage> lineage(
    std::uint64_t sequence,
    std::uint64_t generation)
{
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        sampleTime(0), std::nullopt, sampleTime(480),
        MediaDecodeOrderMode::PresentationOrderNoReorder, "audio-source",
        MediaSourceAccessUnitSequence(sequence),
        MediaTimeMappingConfidence::Locked, generation});
}

MediaBufferRef frame(std::int64_t begin, int samples, std::uint64_t sequence,
                     std::uint64_t generation)
{
    auto raw = ::media::ffmpeg::makeFrame();
    assert(raw);
    raw->format = AV_SAMPLE_FMT_FLTP;
    raw->sample_rate = 48'000;
    raw->nb_samples = samples;
    av_channel_layout_default(&raw->ch_layout, 2);
    assert(av_frame_get_buffer(raw.get(), 0) == 0);
    auto wrapped = FFmpegBufferFactory::wrapFrame(std::move(raw), MediaStreamKind::Audio);
    assert(wrapped);
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        wrapped.value(), lineage(sequence, generation),
        {begin, begin + samples, 48'000});
    assert(canonical);
    return std::move(canonical).value();
}

void zeroAndPartialTrimProveEpochSourceStart()
{
    const MediaAudioPlaybackOrigin zeroOrigin{7, sampleTime(0), sampleTime(0), 300, 48'000};
    MediaAudioStartupTrimNode zero(MediaNodeId::fromValue(1), zeroOrigin,
                                   std::make_shared<MediaAudioStartupTrimLineageState>(
                                       MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    auto zeroResult = zero.apply(frame(0, 480, 1, 7), 0);
    assert(zeroResult && zeroResult.value());
    const auto* zeroAudio = dynamic_cast<const MediaCanonicalAudioSamplesBuffer*>(
        zeroResult.value().get());
    assert(zeroAudio && zeroAudio->interval().begin == 0);

    const MediaAudioPlaybackOrigin partialOrigin{7, sampleTime(240), sampleTime(0), 300, 48'000};
    MediaAudioStartupTrimNode partial(MediaNodeId::fromValue(2), partialOrigin,
                                      std::make_shared<MediaAudioStartupTrimLineageState>(
                                          MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    auto partialResult = partial.apply(frame(0, 480, 2, 7), 240);
    assert(partialResult && partialResult.value());
    const auto* partialAudio = dynamic_cast<const MediaCanonicalAudioSamplesBuffer*>(
        partialResult.value().get());
    assert(partialAudio && partialAudio->interval().begin == 240);
    assert(partialAudio->interval().end == 480);
    assert(partialAudio->lineage()->sourceSequence == MediaSourceAccessUnitSequence(2));
    assert(!partial.apply(frame(480, 480, 3, 7), 1)); // non-zero release trim cannot repeat
    assert(partial.apply(frame(480, 480, 3, 7), 0));
}

void nonAlignedEpochSelectsTheFirstAudioSampleAtOrAfterIt()
{
    const auto nonAlignedEpoch = MediaRunningTime::fromNanoseconds(5'000'001);
    const MediaAudioPlaybackOrigin origin{
        7, nonAlignedEpoch, sampleTime(0), 0, 48'000};
    MediaAudioStartupTrimNode trim(
        MediaNodeId::fromValue(45), origin,
        std::make_shared<MediaAudioStartupTrimLineageState>(
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));

    auto output = trim.apply(frame(0, 480, 45, 7), 241);
    assert(output && output.value());
    const auto* canonical =
        dynamic_cast<const MediaCanonicalAudioSamplesBuffer*>(
            output.value().get());
    assert(canonical && canonical->interval().begin == 241);

    MediaAudioStartupTrimNode beforeEpoch(
        MediaNodeId::fromValue(46), origin,
        std::make_shared<MediaAudioStartupTrimLineageState>(
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    assert(!beforeEpoch.apply(frame(0, 480, 46, 7), 240));
}

void typedTrimInputBecomesBoundOutputWithoutTrimMetadata()
{
    const MediaAudioPlaybackOrigin origin{7, sampleTime(240), sampleTime(0), 300, 48'000};
    MediaAudioStartupTrimNode trim(MediaNodeId::fromValue(7), origin,
                                   std::make_shared<MediaAudioStartupTrimLineageState>(
                                       MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    auto decoded = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 4, 7), origin, 240);
    assert(decoded);
    auto output = trim.applyDecoded(decoded.value());
    assert(output && output.value());
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
        output.value().get());
    assert(bound && bound->audioOrigin() == origin);
    assert(bound->media()->interval().begin == 240);
}

void fullFrameTrimRequiresTheNextSampleToBeTheEpochStart()
{
    const MediaAudioPlaybackOrigin origin{7, sampleTime(480), sampleTime(0), 0, 48'000};
    MediaAudioStartupTrimNode trim(MediaNodeId::fromValue(3), origin,
                                   std::make_shared<MediaAudioStartupTrimLineageState>(
                                       MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    auto discarded = trim.apply(frame(0, 480, 1, 7), 480);
    assert(discarded && !discarded.value());
    auto next = trim.apply(frame(480, 480, 2, 7), 0);
    assert(next && next.value());

    MediaAudioStartupTrimNode wrong(MediaNodeId::fromValue(4), origin,
                                    std::make_shared<MediaAudioStartupTrimLineageState>(
                                        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    assert(wrong.apply(frame(0, 480, 1, 7), 480));
    assert(!wrong.apply(frame(481, 480, 2, 7), 0));
}

void spanningTrimAndGenerationMismatchPreservePayloadOwnership()
{
    const MediaAudioPlaybackOrigin origin{7, sampleTime(240), sampleTime(0), 0, 48'000};
    MediaAudioStartupTrimNode trim(MediaNodeId::fromValue(5), origin,
                                   std::make_shared<MediaAudioStartupTrimLineageState>(
                                       MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    auto owned = frame(0, 480, 1, 7);
    const long before = owned.use_count();
    auto spanning = trim.apply(owned, 481);
    assert(spanning && !spanning.value());
    assert(owned.use_count() == before);

    const MediaAudioPlaybackOrigin wrongGeneration{8, sampleTime(0), sampleTime(0), 0, 48'000};
    MediaAudioStartupTrimNode mismatch(MediaNodeId::fromValue(6), wrongGeneration,
                                       std::make_shared<MediaAudioStartupTrimLineageState>(
                                           MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    assert(!mismatch.apply(frame(0, 480, 1, 7), 0));
}

void exactTargetPurgeRestartsTheNextGeneration()
{
    const auto mode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<MediaAudioStartupTrimLineageState>(mode, 1);
    MediaAudioStartupTrimNode trim(MediaNodeId::fromValue(8), state);
    auto firstTarget = trim.generationPurgeTarget();
    auto secondTarget = trim.generationPurgeTarget();
    assert(firstTarget && firstTarget == secondTarget);

    const MediaAudioPlaybackOrigin generation7{
        7, sampleTime(0), sampleTime(0), 0, 48'000};
    auto oldInput = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 10, 7), generation7, 0);
    assert(oldInput);
    auto oldOutput = trim.applyDecoded(oldInput.value());
    assert(oldOutput && oldOutput.value());

    assert(firstTarget->purge(MediaAvGenerationPurge{7, 8, 1}));
    assert(!state->isCurrent(7));
    assert(state->isCurrent(8));

    const MediaAudioPlaybackOrigin generation8{
        8, sampleTime(0), sampleTime(0), 0, 48'000};
    auto nextInput = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 11, 8), generation8, 0);
    assert(nextInput);
    auto nextOutput = trim.applyDecoded(nextInput.value());
    assert(nextOutput && nextOutput.value());
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
        nextOutput.value().get());
    assert(bound && bound->audioOrigin().generation == 8);
}

void retainedOldOutputIsDroppedBeforeNextGenerationRestart()
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "trim.source");
    const auto trimId = graph.addNode(MediaNodeKind::AudioStartupTrim, "trim.node");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "trim.sink");
    graph.addOutputPort(source, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(trimId, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(trimId, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(1);
    assert(graph.connect(source, "frame", trimId, "frame", "trim.input", policy));
    assert(graph.connect(trimId, "frame", sink, "frame", "trim.output", policy));

    MediaGraphExecutionContext execution;
    assert(execution.compile(graph));
    auto* input = execution.findInputChannel(trimId, "frame");
    auto* output = execution.findOutputChannel(trimId, "frame");
    assert(input && output);

    const auto mode = MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    auto state = std::make_shared<MediaAudioStartupTrimLineageState>(mode, 1);
    MediaAudioStartupTrimNode trim(trimId, state);
    assert(trim.start(execution));

    const MediaAudioPlaybackOrigin oldOrigin{7, sampleTime(0), sampleTime(0), 0, 48'000};
    auto old = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 20, 7), oldOrigin, 0);
    assert(old && input->push(old.value()));
    auto blocker = FFmpegBufferFactory::makeFlush(MediaStreamKind::Audio);
    assert(blocker && output->push(blocker.value()));
    auto blocked = trim.process(execution);
    assert(blocked && blocked.value().state == MediaNodeProcessState::Waiting);

    assert(trim.generationPurgeTarget()->purge({7, 8, 1}));
    MediaBufferRef popped;
    assert(output->tryPop(popped) && popped == blocker.value());
    assert(trim.process(execution));
    assert(!output->tryPop(popped));

    const MediaAudioPlaybackOrigin nextOrigin{8, sampleTime(0), sampleTime(0), 0, 48'000};
    auto next = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 21, 8), nextOrigin, 0);
    assert(next && input->push(next.value()));
    assert(trim.process(execution));
    assert(output->tryPop(popped));
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(popped.get());
    assert(bound && bound->audioOrigin().generation == 8);
    assert(!output->tryPop(popped));
    trim.abort(execution);
}

void releaseDirectiveSpansFramesAndPurgeClearsRemainingTrim()
{
    const MediaAudioPlaybackOrigin partialOrigin{
        7, sampleTime(1'200), sampleTime(0), 0, 48'000};
    auto partialState = std::make_shared<MediaAudioStartupTrimLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8);
    MediaAudioStartupTrimNode partial(
        MediaNodeId::fromValue(30), partialOrigin, partialState);
    auto first = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 30, 7), partialOrigin, 1'200);
    auto second = MediaDecodedAudioTrimInputBuffer::create(
        frame(480, 480, 31, 7), partialOrigin, 0);
    auto third = MediaDecodedAudioTrimInputBuffer::create(
        frame(960, 480, 32, 7), partialOrigin, 0);
    assert(first && second && third);
    auto firstResult = partial.applyDecoded(first.value());
    assert(firstResult && !firstResult.value());
    assert(partialState->remainingTrimSamples == 720);
    auto secondResult = partial.applyDecoded(second.value());
    assert(secondResult && !secondResult.value());
    assert(partialState->remainingTrimSamples == 240);
    auto thirdResult = partial.applyDecoded(third.value());
    assert(thirdResult && thirdResult.value());
    const auto* partialOutput = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
        thirdResult.value().get());
    assert(partialOutput && partialOutput->media()->interval().begin == 1'200);
    assert(partialState->remainingTrimSamples == 0);

    const MediaAudioPlaybackOrigin exactOrigin{
        7, sampleTime(960), sampleTime(0), 0, 48'000};
    auto exactState = std::make_shared<MediaAudioStartupTrimLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8);
    MediaAudioStartupTrimNode exact(
        MediaNodeId::fromValue(31), exactOrigin, exactState);
    auto exactFirst = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 33, 7), exactOrigin, 960);
    auto exactSecond = MediaDecodedAudioTrimInputBuffer::create(
        frame(480, 480, 34, 7), exactOrigin, 0);
    auto exactThird = MediaDecodedAudioTrimInputBuffer::create(
        frame(960, 480, 35, 7), exactOrigin, 0);
    assert(exactFirst && exactSecond && exactThird);
    auto exactFirstResult = exact.applyDecoded(exactFirst.value());
    auto exactSecondResult = exact.applyDecoded(exactSecond.value());
    auto exactThirdResult = exact.applyDecoded(exactThird.value());
    assert(exactFirstResult && !exactFirstResult.value());
    assert(exactSecondResult && !exactSecondResult.value());
    assert(exactThirdResult && exactThirdResult.value());

    auto repeatedState = std::make_shared<MediaAudioStartupTrimLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8);
    MediaAudioStartupTrimNode repeated(
        MediaNodeId::fromValue(32), exactOrigin, repeatedState);
    assert(repeated.applyDecoded(exactFirst.value()));
    auto repeatedDirective = MediaDecodedAudioTrimInputBuffer::create(
        frame(480, 480, 36, 7), exactOrigin, 1);
    assert(repeatedDirective && !repeated.applyDecoded(repeatedDirective.value()));

    auto purgeState = std::make_shared<MediaAudioStartupTrimLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8);
    MediaAudioStartupTrimNode purge(
        MediaNodeId::fromValue(33), exactOrigin, purgeState);
    assert(purge.applyDecoded(exactFirst.value()));
    assert(purgeState->remainingTrimSamples == 480);
    assert(purge.generationPurgeTarget()->purge({7, 8, 1}));
    assert(purgeState->remainingTrimSamples == 0);
    assert(!purgeState->releaseTrimConsumed);
    const MediaAudioPlaybackOrigin nextOrigin{
        8, sampleTime(0), sampleTime(0), 0, 48'000};
    auto next = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 37, 8), nextOrigin, 0);
    assert(next);
    auto nextResult = purge.applyDecoded(next.value());
    assert(nextResult && nextResult.value());
}

void terminalBeforeFirstRetainedSampleFailsClosed()
{
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "terminal.source");
    const auto trimId = graph.addNode(MediaNodeKind::AudioStartupTrim, "terminal.trim");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "terminal.sink");
    graph.addOutputPort(source, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(trimId, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(trimId, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(2);
    assert(graph.connect(source, "frame", trimId, "frame", "terminal.input", policy));
    assert(graph.connect(trimId, "frame", sink, "frame", "terminal.output", policy));

    MediaGraphExecutionContext execution;
    assert(execution.compile(graph));
    auto* input = execution.findInputChannel(trimId, "frame");
    assert(input);
    auto state = std::make_shared<MediaAudioStartupTrimLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 2);
    MediaAudioStartupTrimNode trim(trimId, state);
    assert(trim.start(execution));

    const MediaAudioPlaybackOrigin remainingOrigin{
        7, sampleTime(960), sampleTime(0), 0, 48'000};
    auto remaining = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 40, 7), remainingOrigin, 960);
    assert(remaining && input->push(remaining.value()));
    assert(trim.process(execution));
    assert(state->remainingTrimSamples == 480);
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
    assert(eof && input->push(eof.value()));
    assert(!trim.process(execution));

    assert(trim.generationPurgeTarget()->purge({7, 8, 1}));
    const MediaAudioPlaybackOrigin exactOrigin{
        8, sampleTime(480), sampleTime(0), 0, 48'000};
    auto exact = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 41, 8), exactOrigin, 480);
    assert(exact && input->push(exact.value()));
    assert(trim.process(execution));
    assert(state->remainingTrimSamples == 0);
    assert(state->waitingForFirstPostTrimSample);
    auto flush = FFmpegBufferFactory::makeFlush(MediaStreamKind::Audio);
    assert(flush && input->push(flush.value()));
    assert(!trim.process(execution));
    trim.abort(execution);
}

void stopStartClearsOwnedTrimAndGenerationState()
{
    MediaGraphExecutionContext execution;
    auto state = std::make_shared<MediaAudioStartupTrimLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 2);
    MediaAudioStartupTrimNode trim(MediaNodeId::fromValue(42), state);
    assert(trim.start(execution));

    const MediaAudioPlaybackOrigin oldOrigin{
        7, sampleTime(960), sampleTime(0), 0, 48'000};
    auto old = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 42, 7), oldOrigin, 960);
    assert(old);
    auto oldResult = trim.applyDecoded(old.value());
    assert(oldResult && !oldResult.value());
    assert(state->origin && state->origin->generation == 7);
    assert(state->releaseTrimConsumed);
    assert(state->remainingTrimSamples == 480);
    assert(state->waitingForFirstPostTrimSample);
    assert(state->expectedNextSample == 480);

    assert(trim.stop(execution));
    assert(!state->origin);
    assert(!state->releaseTrimConsumed);
    assert(state->remainingTrimSamples == 0);
    assert(!state->waitingForFirstPostTrimSample);
    assert(!state->expectedNextSample);
    assert(state->isCurrent(8));

    assert(trim.start(execution));
    const MediaAudioPlaybackOrigin nextOrigin{
        8, sampleTime(0), sampleTime(0), 0, 48'000};
    auto next = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 43, 8), nextOrigin, 0);
    assert(next);
    auto nextResult = trim.applyDecoded(next.value());
    assert(nextResult && nextResult.value());

    trim.abort(execution);
    assert(!state->origin);
    assert(!state->releaseTrimConsumed);
    assert(state->remainingTrimSamples == 0);
    assert(!state->waitingForFirstPostTrimSample);
    assert(!state->expectedNextSample);
    assert(state->isCurrent(9));

    assert(trim.start(execution));
    const MediaAudioPlaybackOrigin afterAbortOrigin{
        9, sampleTime(0), sampleTime(0), 0, 48'000};
    auto afterAbort = MediaDecodedAudioTrimInputBuffer::create(
        frame(0, 480, 44, 9), afterAbortOrigin, 0);
    assert(afterAbort);
    auto afterAbortResult = trim.applyDecoded(afterAbort.value());
    assert(afterAbortResult && afterAbortResult.value());
    trim.abort(execution);
}

} // namespace

int main()
{
    zeroAndPartialTrimProveEpochSourceStart();
    nonAlignedEpochSelectsTheFirstAudioSampleAtOrAfterIt();
    fullFrameTrimRequiresTheNextSampleToBeTheEpochStart();
    spanningTrimAndGenerationMismatchPreservePayloadOwnership();
    typedTrimInputBecomesBoundOutputWithoutTrimMetadata();
    exactTargetPurgeRestartsTheNextGeneration();
    retainedOldOutputIsDroppedBeforeNextGenerationRestart();
    releaseDirectiveSpansFramesAndPurgeClearsRemainingTrim();
    terminalBeforeFirstRetainedSampleFailsClosed();
    stopStartClearsOwnedTrimAndGenerationState();
    std::cout << "audio startup trim node tests passed\n";
    return 0;
}
