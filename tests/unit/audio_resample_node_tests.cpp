#include "common/TestAssert.h"

#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/sync/MediaAudioCorrection.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <vector>
#include <optional>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

std::shared_ptr<AudioResampleLineageState> legacyResampleLineageState()
{
    return std::make_shared<AudioResampleLineageState>(
        MediaAudioLineageExecutionMode::LegacyPlainPacket, 0);
}

void testZeroCapacitySwrConversionDoesNotConsumeLiveInput(TestContext& ctx)
{
    auto state = legacyResampleLineageState();
    AudioResampleSwrSession session(state);
    auto input = ::media::ffmpeg::makeFrame();
    auto target = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, input && target);
    if (!input || !target) return;
    input->format = AV_SAMPLE_FMT_FLTP;
    input->sample_rate = 44'100;
    input->nb_samples = 441;
    av_channel_layout_default(&input->ch_layout, 2);
    EXPECT_TRUE(ctx, av_frame_get_buffer(input.get(), 0) == 0);
    target->sample_fmt = AV_SAMPLE_FMT_FLTP;
    target->sample_rate = 48'000;
    av_channel_layout_default(&target->ch_layout, 2);
    EXPECT_TRUE(ctx, session.ensureInitialized(*input, *target));
    const auto delayBefore = swr_get_delay(state->swr.get(), 44'100);
    EXPECT_EQ(ctx, delayBefore, static_cast<std::int64_t>(0));
    EXPECT_TRUE(ctx, swr_get_out_samples(state->swr.get(), 0) > 0);
    auto initialEvidence = session.inspectDrainEvidence(44'100, 48'000);
    EXPECT_TRUE(ctx, initialEvidence);
    EXPECT_TRUE(ctx, initialEvidence &&
        initialEvidence.value() == AudioSwrDrainEvidence::NoDelay);
    auto zeroCapacity = session.convertLive(
        const_cast<const uint8_t**>(input->extended_data),
        input->nb_samples, 0, *target);
    EXPECT_TRUE(ctx, zeroCapacity);
    EXPECT_TRUE(ctx, zeroCapacity && zeroCapacity.value().capacity == 0);
    EXPECT_TRUE(ctx, zeroCapacity && !zeroCapacity.value().output);
    EXPECT_EQ(ctx, swr_get_delay(state->swr.get(), 44'100), delayBefore);
    auto accepted = session.convertLive(
        const_cast<const uint8_t**>(input->extended_data),
        input->nb_samples, 1'024, *target);
    EXPECT_TRUE(ctx, accepted);
    EXPECT_TRUE(ctx, accepted && accepted.value().capacity > 0);
    EXPECT_TRUE(ctx, accepted && accepted.value().produced > 0);
}

void testZeroUpperBoundDrainStillObtainsExhaustionProof(TestContext& ctx)
{
    auto state = legacyResampleLineageState();
    AudioResampleSwrSession session(state);
    auto input = ::media::ffmpeg::makeFrame();
    auto target = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, input && target);
    if (!input || !target) return;
    input->format = AV_SAMPLE_FMT_FLTP;
    input->sample_rate = 48'000;
    input->nb_samples = 1;
    av_channel_layout_default(&input->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(input.get(), 0), 0);
    target->sample_fmt = AV_SAMPLE_FMT_FLTP;
    target->sample_rate = 48'000;
    av_channel_layout_default(&target->ch_layout, 2);
    EXPECT_TRUE(ctx, session.ensureInitialized(*input, *target));
    EXPECT_EQ(ctx, swr_get_out_samples(state->swr.get(), 0), 0);

    auto evidence = session.inspectDrainEvidence(48'000, 48'000);
    EXPECT_TRUE(ctx, evidence);
    EXPECT_TRUE(ctx, evidence &&
        evidence.value() == AudioSwrDrainEvidence::NoDelay);
    auto drained = session.drainQuantum(1'024, *target);
    EXPECT_TRUE(ctx, drained);
    EXPECT_EQ(ctx, drained ? drained.value().capacity : 0, 1);
    EXPECT_EQ(ctx, drained ? drained.value().produced : -1, 0);
    EXPECT_TRUE(ctx, drained && drained.value().exhausted.has_value());
}

void testCorrectionOptionsAreMandatoryAndModeSpecific(TestContext& ctx)
{
    MediaGraph unknownModeGraph;
    const MediaNodeId unknownModeId = unknownModeGraph.addNode(
        MediaNodeKind::AudioResample, "unknown_mode");
    EXPECT_TRUE(ctx, unknownModeGraph.setNodeOption(
        unknownModeId, MediaAudioCorrectionOptionKey::Mode, "automatic"));
    MediaGraphExecutionContext unknownModeExecution;
    EXPECT_TRUE(ctx, unknownModeExecution.compile(unknownModeGraph));
    AudioResampleNode unknownMode(unknownModeId, MediaAudioLineageExecutionMode::LegacyPlainPacket, legacyResampleLineageState());
    EXPECT_FALSE(ctx, unknownMode.start(unknownModeExecution));

    MediaGraph missingModeGraph;
    const MediaNodeId missingModeId = missingModeGraph.addNode(
        MediaNodeKind::AudioResample, "missing_mode");
    MediaGraphExecutionContext missingModeExecution;
    EXPECT_TRUE(ctx, missingModeExecution.compile(missingModeGraph));
    AudioResampleNode missingMode(missingModeId, MediaAudioLineageExecutionMode::LegacyPlainPacket, legacyResampleLineageState());
    EXPECT_FALSE(ctx, missingMode.start(missingModeExecution));

    MediaGraph disabledGraph;
    const MediaNodeId disabledId = disabledGraph.addNode(
        MediaNodeKind::AudioResample, "disabled");
    EXPECT_TRUE(ctx, disabledGraph.setNodeOption(
        disabledId, MediaAudioCorrectionOptionKey::Mode, "disabled"));
    MediaGraphExecutionContext disabledExecution;
    EXPECT_TRUE(ctx, disabledExecution.compile(disabledGraph));
    AudioResampleNode disabled(disabledId, MediaAudioLineageExecutionMode::LegacyPlainPacket, legacyResampleLineageState());
    EXPECT_TRUE(ctx, disabled.start(disabledExecution));
    EXPECT_TRUE(ctx, disabled.flush(disabledExecution));
    auto disabledFlush = disabled.process(disabledExecution);
    EXPECT_TRUE(ctx, disabledFlush);
    EXPECT_TRUE(ctx, disabledFlush &&
        disabledFlush.value().state == MediaNodeProcessState::Progress);
    EXPECT_TRUE(ctx, disabled.stop(disabledExecution));

    MediaGraph missingGenerationGraph;
    const MediaNodeId missingGenerationId = missingGenerationGraph.addNode(
        MediaNodeKind::AudioResample, "missing_generation");
    EXPECT_TRUE(ctx, missingGenerationGraph.setNodeOption(
        missingGenerationId,
        MediaAudioCorrectionOptionKey::Mode,
        "external_required"));
    MediaGraphExecutionContext missingGenerationExecution;
    EXPECT_TRUE(ctx, missingGenerationExecution.compile(missingGenerationGraph));
    AudioResampleNode missingGeneration(missingGenerationId, MediaAudioLineageExecutionMode::LegacyPlainPacket, legacyResampleLineageState());
    EXPECT_FALSE(ctx, missingGeneration.start(missingGenerationExecution));

    MediaGraph externalGraph;
    const MediaNodeId externalId = externalGraph.addNode(
        MediaNodeKind::AudioResample, "external_flush");
    EXPECT_TRUE(ctx, externalGraph.setNodeOption(
        externalId, MediaAudioCorrectionOptionKey::Mode, "external_required"));
    EXPECT_TRUE(ctx, externalGraph.setNodeOption(
        externalId, MediaAudioCorrectionOptionKey::Generation, "1"));
    EXPECT_TRUE(ctx, externalGraph.setNodeOption(
        externalId, MediaAudioCorrectionOptionKey::LookaheadWindows, "2"));
    MediaGraphExecutionContext externalExecution;
    EXPECT_TRUE(ctx, externalExecution.compile(externalGraph));
    AudioResampleNode external(externalId, MediaAudioLineageExecutionMode::LegacyPlainPacket, legacyResampleLineageState());
    EXPECT_TRUE(ctx, external.start(externalExecution));
    EXPECT_TRUE(ctx, external.flush(externalExecution));
    auto externalFlush = external.process(externalExecution);
    EXPECT_TRUE(ctx, externalFlush);
    EXPECT_TRUE(ctx, externalFlush &&
        externalFlush.value().state == MediaNodeProcessState::Progress);
    EXPECT_TRUE(ctx, external.stop(externalExecution));
}

void testEofDrainSettlesPendingCorrectionTailThroughNode(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(4);
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "codec_source");
    const MediaNodeId frameSource = graph.addNode(MediaNodeKind::DebugDump, "frame_source");
    const MediaNodeId correctionSource = graph.addNode(MediaNodeKind::DebugDump, "correction_source");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, "resample");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Mode, "external_required");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Generation, "3");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::LookaheadWindows, "2");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(correctionSource, "correction", MediaStreamKind::Audio,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(resample, "correction", MediaStreamKind::Audio,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.connect(codecSource, "codec", resample, "codec", "codec", policy);
    graph.connect(frameSource, "frame", resample, "frame", "frame", policy);
    graph.connect(correctionSource, "correction", resample, "correction", "correction", policy);
    graph.connect(resample, "frame", sink, "frame", "output", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    auto state = std::make_shared<AudioResampleLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8);
    AudioResampleNode node(
        resample, MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
        state);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* codecInput = execution.findInputChannel(resample, "codec");
    MediaChannel* frameInput = execution.findInputChannel(resample, "frame");
    MediaChannel* correctionInput = execution.findInputChannel(resample, "correction");
    MediaChannel* output = execution.findOutputChannel(resample, "frame");
    EXPECT_TRUE(ctx, codecInput && frameInput && correctionInput && output);
    if (!codecInput || !frameInput || !correctionInput || !output) {
        return;
    }

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->time_base = AVRational{1, 48'000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer);
    if (!codecBuffer) {
        return;
    }
    EXPECT_TRUE(ctx, codecInput->push(codecBuffer.value()));

    const auto makeCommand = [&](std::uint64_t sequence,
                                 std::int64_t effective) {
        auto quantizer = MediaAudioCorrectionQuantizer::create(
            MediaRunningTime::fromNanoseconds(1'024'000),
            MediaRunningTime::fromNanoseconds(512'000), 1'000'000);
        if (!quantizer) {
            return std::optional<MediaAudioCompensationCommand>{};
        }
        auto scheduled = std::move(quantizer).value().schedule(
            3, sequence, effective, 0,
            MediaAudioCorrectionTelemetry{
                MediaRunningTime::fromNanoseconds(0), 0, 0, false});
        if (!scheduled || !scheduled.value()) {
            return std::optional<MediaAudioCompensationCommand>{};
        }
        return std::optional<MediaAudioCompensationCommand>{
            std::move(*scheduled.value())};
    };
    auto active = makeCommand(1, 0);
    auto pendingTail = makeCommand(2, 1'024);
    EXPECT_TRUE(ctx, active && pendingTail);
    if (!active || !pendingTail) return;
    EXPECT_TRUE(ctx, correctionInput->push(
        makeMediaBufferRef<MediaAudioCorrectionBuffer>(*active)));
    EXPECT_TRUE(ctx, correctionInput->push(
        makeMediaBufferRef<MediaAudioCorrectionBuffer>(*pendingTail)));

    auto frame = ::media::ffmpeg::makeFrame();
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 48'000;
    frame->nb_samples = 1'024;
    frame->pts = 0;
    av_channel_layout_default(&frame->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(frame.get(), 0), 0);
    auto frameBuffer = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, frameBuffer);
    if (!frameBuffer) {
        return;
    }
    auto sourceLineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            MediaRunningTime::fromNanoseconds(0), std::nullopt,
            MediaRunningTime::fromNanoseconds(21'333'333),
            MediaDecodeOrderMode::PresentationOrderNoReorder,
            "resample-exact-delay", MediaSourceAccessUnitSequence(1),
            MediaTimeMappingConfidence::Locked, 3});
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        frameBuffer.value(), sourceLineage, {0, 1'024, 48'000});
    EXPECT_TRUE(ctx, canonical);
    if (!canonical) return;
    const MediaAudioPlaybackOrigin origin{
        3, MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0), 0, 48'000};
    auto bound = MediaBoundCanonicalAudioBuffer::create(canonical.value(), origin);
    EXPECT_TRUE(ctx, bound && frameInput->push(bound.value()));
    if (!bound) return;
    auto eof = FFmpegBufferFactory::makeEof();
    EXPECT_TRUE(ctx, eof && frameInput->push(eof.value()));
    if (!eof) return;

    bool observedRetainedFilterDelay = false;
    bool observedEof = false;
    for (int step = 0; step < 16 && !observedEof; ++step) {
        auto result = node.process(execution);
        EXPECT_TRUE(ctx, result);
        if (!result) break;
        if (!observedRetainedFilterDelay && state->swr &&
            state->outputSampleIndex == 1'024) {
            EXPECT_TRUE(ctx, swr_get_delay(state->swr.get(), 48'000) > 0);
            EXPECT_TRUE(ctx, swr_get_out_samples(state->swr.get(), 0) > 0);
            AudioResampleSwrSession observation(state);
            auto evidence = observation.inspectDrainEvidence(48'000, 48'000);
            EXPECT_TRUE(ctx, evidence);
            EXPECT_TRUE(ctx, evidence &&
                evidence.value() == AudioSwrDrainEvidence::MayProduce);
            observedRetainedFilterDelay = true;
        }
        MediaBufferRef emitted;
        while (output->tryPop(emitted)) {
            observedEof = observedEof || emitted->isEof();
        }
    }
    EXPECT_TRUE(ctx, observedRetainedFilterDelay);
    EXPECT_TRUE(ctx, observedEof);
    EXPECT_TRUE(ctx, state->correctionExecutor &&
                     state->correctionExecutor->settleTerminal());
    node.abort(execution);
}

void testExternalCorrectionFlushPreservesEpochAndEmitsEofOnce(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(8);
    const auto correctionPolicy = MediaBlockingEdgePolicyPlanner::planQueue(4);
    const auto outputPolicy = MediaBlockingEdgePolicyPlanner::planQueue(1);
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "window.codec_source");
    const MediaNodeId frameSource = graph.addNode(MediaNodeKind::DebugDump, "window.frame_source");
    const MediaNodeId correctionSource = graph.addNode(MediaNodeKind::DebugDump, "window.correction_source");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, "window.resample");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "window.sink");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Mode, "external_required");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Generation, "7");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::LookaheadWindows, "2");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(correctionSource, "correction", MediaStreamKind::Audio, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(resample, "correction", MediaStreamKind::Audio, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.connect(codecSource, "codec", resample, "codec", "window.codec", policy);
    graph.connect(frameSource, "frame", resample, "frame", "window.frame", policy);
    graph.connect(correctionSource, "correction", resample, "correction", "window.correction", correctionPolicy);
    graph.connect(resample, "frame", sink, "frame", "window.output", outputPolicy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    auto lineageState = std::make_shared<AudioResampleLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 16);
    AudioResampleNode node(
        resample, MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
        lineageState);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* codecInput = execution.findInputChannel(resample, "codec");
    MediaChannel* frameInput = execution.findInputChannel(resample, "frame");
    MediaChannel* correctionInput = execution.findInputChannel(resample, "correction");
    MediaChannel* output = execution.findOutputChannel(resample, "frame");
    EXPECT_TRUE(ctx, codecInput && frameInput && correctionInput && output);
    if (!codecInput || !frameInput || !correctionInput || !output) return;

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->time_base = AVRational{1, 48'000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer);
    if (!codecBuffer) return;
    EXPECT_TRUE(ctx, codecInput->push(codecBuffer.value()));

    const auto makeCommand = [&](std::uint64_t sequence,
                                 std::int64_t effective,
                                 int samples,
                                 int stretchPpm) -> std::optional<MediaAudioCompensationCommand> {
        auto quantizer = MediaAudioCorrectionQuantizer::create(
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(samples) * 1'000),
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(samples) * 500),
            1'000'000);
        EXPECT_TRUE(ctx, quantizer);
        if (!quantizer) return std::nullopt;
        auto command = std::move(quantizer).value().schedule(
            7, sequence, effective, stretchPpm,
            MediaAudioCorrectionTelemetry{
                MediaRunningTime::fromNanoseconds(0), 0, 0, false});
        EXPECT_TRUE(ctx, command && command.value());
        if (!command || !command.value()) return std::nullopt;
        return std::move(*command.value());
    };
    auto firstCommand = makeCommand(1, 0, 1'024, -977);
    auto secondCommand = makeCommand(2, 1'023, 1'024, 977);
    auto drainCommand = makeCommand(3, 2'048, 1'024, 0);
    auto tailCommand = makeCommand(4, 3'072, 1'024, 0);
    EXPECT_TRUE(ctx, firstCommand && secondCommand && drainCommand && tailCommand);
    if (!firstCommand || !secondCommand || !drainCommand || !tailCommand) return;
    EXPECT_TRUE(ctx, correctionInput->push(
        makeMediaBufferRef<MediaAudioCorrectionBuffer>(*firstCommand)));
    EXPECT_TRUE(ctx, correctionInput->push(
        makeMediaBufferRef<MediaAudioCorrectionBuffer>(*secondCommand)));

    auto frame = ::media::ffmpeg::makeFrame();
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 44'100;
    frame->nb_samples = 1'882;
    frame->pts = 0;
    av_channel_layout_default(&frame->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(frame.get(), 0), 0);
    auto frameBuffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, frameBuffer);
    if (!frameBuffer) return;
    const MediaAudioPlaybackOrigin origin{
        7, MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0), 0, 48'000};
    const auto makeBound = [&](MediaBufferRef media, std::int64_t begin,
                               std::int64_t end, std::uint64_t sequence) {
        auto sourceLineage = std::make_shared<const MediaCanonicalLineage>(
            MediaCanonicalLineage{
                MediaRunningTime::fromNanoseconds(0), std::nullopt,
                MediaRunningTime::fromNanoseconds(1),
                MediaDecodeOrderMode::PresentationOrderNoReorder,
                "resample-long", MediaSourceAccessUnitSequence(sequence),
                MediaTimeMappingConfidence::Locked, 7});
        auto canonical = MediaCanonicalAudioSamplesBuffer::create(
            std::move(media), std::move(sourceLineage),
            {begin, end, 44'100});
        return canonical
            ? MediaBoundCanonicalAudioBuffer::create(canonical.value(), origin)
            : ::media::Result<MediaBufferRef>::failure(canonical.error());
    };
    auto firstBound = makeBound(frameBuffer.value(), 0, 1'882, 1);
    EXPECT_TRUE(ctx, firstBound);
    if (!firstBound) return;
    EXPECT_TRUE(ctx, frameInput->push(firstBound.value()));

    auto flush = FFmpegBufferFactory::makeFlush();
    EXPECT_TRUE(ctx, flush);
    if (!flush) return;
    EXPECT_TRUE(ctx, frameInput->push(flush.value()));

    auto continuationFrame = ::media::ffmpeg::makeFrame();
    continuationFrame->format = AV_SAMPLE_FMT_FLTP;
    continuationFrame->sample_rate = 44'100;
    continuationFrame->nb_samples = 941;
    continuationFrame->pts = 1'882;
    av_channel_layout_default(&continuationFrame->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(continuationFrame.get(), 0), 0);
    auto continuationBuffer = FFmpegBufferFactory::wrapFrame(
        std::move(continuationFrame), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, continuationBuffer);
    if (!continuationBuffer) return;
    auto secondBound = makeBound(
        continuationBuffer.value(), 1'882, 2'823, 2);
    EXPECT_TRUE(ctx, secondBound);
    if (!secondBound) return;
    EXPECT_TRUE(ctx, frameInput->push(secondBound.value()));

    auto eof = FFmpegBufferFactory::makeEof();
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    EXPECT_TRUE(ctx, frameInput->push(eof.value()));

    auto blocker = ::media::ffmpeg::makeFrame();
    blocker->format = AV_SAMPLE_FMT_FLTP;
    blocker->sample_rate = 48'000;
    blocker->nb_samples = 1;
    blocker->pts = -1;
    av_channel_layout_default(&blocker->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(blocker.get(), 0), 0);
    auto blockerBuffer = FFmpegBufferFactory::wrapFrame(
        std::move(blocker), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, blockerBuffer);
    if (!blockerBuffer) return;
    EXPECT_TRUE(ctx, output->push(blockerBuffer.value()));

    auto consumeFirstCorrection = node.process(execution);
    auto produceFirstWindow = node.process(execution);
    auto blockedOutput = node.process(execution);
    EXPECT_TRUE(ctx, consumeFirstCorrection);
    EXPECT_TRUE(ctx, produceFirstWindow);
    EXPECT_TRUE(ctx, blockedOutput);
    EXPECT_TRUE(ctx, blockedOutput &&
        blockedOutput.value().state == MediaNodeProcessState::Waiting);
    MediaBufferRef removedBlocker;
    EXPECT_TRUE(ctx, output->tryPop(removedBlocker));
    EXPECT_TRUE(ctx, correctionInput->push(
        makeMediaBufferRef<MediaAudioCorrectionBuffer>(*drainCommand)));
    EXPECT_TRUE(ctx, correctionInput->push(
        makeMediaBufferRef<MediaAudioCorrectionBuffer>(*tailCommand)));

    std::vector<MediaBufferRef> frames;
    int eofCount = 0;
    int flushCount = 0;
    bool finished = false;
    bool emittedWhileCorrectionsQueued = false;
    std::size_t maximumCorrectionQueue = correctionInput->size();
    for (int step = 0; step < 32 && !finished; ++step) {
        maximumCorrectionQueue = std::max(
            maximumCorrectionQueue, correctionInput->size());
        auto result = node.process(execution);
        EXPECT_TRUE(ctx, result);
        if (!result) break;
        if (result.value().state == MediaNodeProcessState::Waiting) {
            continue;
        }
        MediaBufferRef emitted;
        while (output->tryPop(emitted)) {
            if (emitted->isEof()) {
                ++eofCount;
                finished = true;
            } else if (emitted->isFlush()) {
                ++flushCount;
                EXPECT_TRUE(ctx, lineageState->correctionExecutor.has_value());
                if (lineageState->correctionExecutor) {
                    EXPECT_FALSE(ctx,
                        lineageState->correctionExecutor->settleTerminal());
                }
            } else {
                const auto* bound =
                    dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(emitted.get());
                EXPECT_TRUE(ctx, bound != nullptr);
                const AVFrame* emittedFrame = bound
                    ? FFmpegFrameView::frame(bound->media()->media())
                    : nullptr;
                EXPECT_TRUE(ctx, emittedFrame != nullptr);
                EXPECT_TRUE(ctx, emittedFrame && emittedFrame->nb_samples > 0);
                frames.push_back(emitted);
                emittedWhileCorrectionsQueued = emittedWhileCorrectionsQueued ||
                    correctionInput->size() > 0;
            }
        }
    }
    EXPECT_TRUE(ctx, finished);
    EXPECT_EQ(ctx, eofCount, 1);
    EXPECT_EQ(ctx, flushCount, 1);
    EXPECT_TRUE(ctx, frames.size() >= static_cast<std::size_t>(2));
    if (frames.size() >= 2) {
        const auto* firstBound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
            frames[0].get());
        const auto* secondBound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
            frames[1].get());
        EXPECT_TRUE(ctx, firstBound && secondBound);
        const AVFrame* firstFrame = firstBound
            ? FFmpegFrameView::frame(firstBound->media()->media()) : nullptr;
        const AVFrame* secondFrame = secondBound
            ? FFmpegFrameView::frame(secondBound->media()->media()) : nullptr;
        EXPECT_TRUE(ctx, firstFrame && secondFrame);
        if (!firstFrame || !secondFrame) return;
        EXPECT_EQ(ctx, firstFrame->nb_samples, 1'023);
        EXPECT_EQ(ctx, firstFrame->pts, static_cast<std::int64_t>(0));
        EXPECT_EQ(ctx, secondFrame->pts, static_cast<std::int64_t>(1'023));
    }
    std::int64_t expectedPts = 0;
    std::int64_t expectedInterval = origin.epochOutputSampleIndex;
    for (const auto& emitted : frames) {
        const auto* bound =
            dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(emitted.get());
        EXPECT_TRUE(ctx, bound != nullptr);
        if (!bound) continue;
        const AVFrame* emittedFrame = FFmpegFrameView::frame(
            bound->media()->media());
        EXPECT_EQ(ctx, emittedFrame->pts, expectedPts);
        std::int64_t fragmentSamples = 0;
        for (const auto& fragment : bound->media()->fragments()) {
            EXPECT_EQ(ctx, fragment.interval.begin, expectedInterval);
            EXPECT_EQ(ctx, fragment.interval.sampleRate, 48'000);
            fragmentSamples += fragment.interval.end - fragment.interval.begin;
            expectedInterval = fragment.interval.end;
        }
        EXPECT_EQ(ctx, fragmentSamples,
                  static_cast<std::int64_t>(emittedFrame->nb_samples));
        expectedPts += emittedFrame->nb_samples;
    }
    constexpr std::int64_t InputSamples = 1'882 + 941;
    constexpr std::int64_t ExpectedResampledSamples =
        (InputSamples * 48'000 + 44'100 - 1) / 44'100;
    EXPECT_EQ(ctx, expectedPts, ExpectedResampledSamples);
    EXPECT_EQ(ctx, expectedInterval,
              origin.epochOutputSampleIndex + ExpectedResampledSamples);
    EXPECT_TRUE(ctx, lineageState->outputIntervals.finish());
    EXPECT_TRUE(ctx, emittedWhileCorrectionsQueued);
    EXPECT_TRUE(ctx, maximumCorrectionQueue <= static_cast<std::size_t>(3));
    EXPECT_TRUE(ctx, node.process(execution));
    MediaBufferRef duplicateEof;
    EXPECT_FALSE(ctx, output->tryPop(duplicateEof));
    node.abort(execution);
}

void testCodecMetadataCloseBeforeBindFails(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(2);
    const MediaNodeId source = graph.addNode(MediaNodeKind::DebugDump, "codec_close.source");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, "codec_close.resample");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Mode, "disabled");
    graph.addOutputPort(source, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.connect(source, "codec", resample, "codec", "codec", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    AudioResampleNode node(resample, MediaAudioLineageExecutionMode::LegacyPlainPacket, legacyResampleLineageState());
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* codecInput = execution.findInputChannel(resample, "codec");
    EXPECT_TRUE(ctx, codecInput != nullptr);
    if (!codecInput) return;
    codecInput->close();
    auto result = node.process(execution);
    EXPECT_FALSE(ctx, result);
    node.abort(execution);
}

void testClosedInputDrainsResamplerBeforeFinishing(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(8);
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "close.codec");
    const MediaNodeId frameSource = graph.addNode(MediaNodeKind::DebugDump, "close.frame");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, "close.resample");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "close.sink");
    EXPECT_TRUE(ctx, graph.setNodeOption(
        resample, MediaAudioCorrectionOptionKey::Mode, "disabled"));
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.connect(codecSource, "codec", resample, "codec", "close.codec", policy);
    graph.connect(frameSource, "frame", resample, "frame", "close.frame", policy);
    graph.connect(resample, "frame", sink, "frame", "close.output", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    AudioResampleNode node(resample, MediaAudioLineageExecutionMode::LegacyPlainPacket, legacyResampleLineageState());
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* codecInput = execution.findInputChannel(resample, "codec");
    MediaChannel* frameInput = execution.findInputChannel(resample, "frame");
    MediaChannel* output = execution.findOutputChannel(resample, "frame");
    EXPECT_TRUE(ctx, codecInput && frameInput && output);
    if (!codecInput || !frameInput || !output) return;

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->time_base = AVRational{1, 48'000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer);
    if (!codecBuffer) return;
    EXPECT_TRUE(ctx, codecInput->push(codecBuffer.value()));

    auto frame = ::media::ffmpeg::makeFrame();
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 44'100;
    frame->nb_samples = 4'410;
    frame->pts = 0;
    av_channel_layout_default(&frame->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(frame.get(), 0), 0);
    auto frameBuffer = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, frameBuffer);
    if (!frameBuffer) return;
    EXPECT_TRUE(ctx, frameInput->push(frameBuffer.value()));
    frameInput->close();

    bool finished = false;
    int outputSamples = 0;
    for (int step = 0; step < 16 && !finished; ++step) {
        auto result = node.process(execution);
        EXPECT_TRUE(ctx, result);
        if (!result) break;
        finished = result.value().state == MediaNodeProcessState::Finished;
        MediaBufferRef emitted;
        while (output->tryPop(emitted)) {
            const AVFrame* outputFrame = FFmpegFrameView::frame(emitted);
            EXPECT_TRUE(ctx, outputFrame != nullptr);
            if (outputFrame) outputSamples += outputFrame->nb_samples;
        }
    }
    EXPECT_TRUE(ctx, finished);
    EXPECT_TRUE(ctx, outputSamples > 0);
    node.abort(execution);
}

void testSynchronizedOriginOwnsFirstOutputSampleIndex(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(4);
    const auto codecSource = graph.addNode(MediaNodeKind::DebugDump, "sync.codec");
    const auto frameSource = graph.addNode(MediaNodeKind::DebugDump, "sync.frame");
    const auto correctionSource = graph.addNode(MediaNodeKind::DebugDump, "sync.correction");
    const auto resample = graph.addNode(MediaNodeKind::AudioResample, "sync.resample");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "sync.sink");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Mode, "external_required");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Generation, "7");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::LookaheadWindows, "2");
    graph.setNodeOption(resample, std::string(MediaAudioLineageModeOptionKey),
                        "synchronized_released_audio");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(correctionSource, "correction", MediaStreamKind::Audio,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(resample, "correction", MediaStreamKind::Audio,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.connect(codecSource, "codec", resample, "codec", "sync.codec", policy);
    graph.connect(frameSource, "frame", resample, "frame", "sync.frame", policy);
    graph.connect(correctionSource, "correction", resample, "correction",
                  "sync.correction", policy);
    graph.connect(resample, "frame", sink, "frame", "sync.output", policy);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    AudioResampleNode node(
        resample, MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
        std::make_shared<AudioResampleLineageState>(
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 8));
    EXPECT_TRUE(ctx, node.start(execution));

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->time_base = AVRational{1, 48'000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer);
    if (!codecBuffer) return;
    EXPECT_TRUE(ctx, execution.findInputChannel(resample, "codec")->push(codecBuffer.value()));

    auto rawFrame = ::media::ffmpeg::makeFrame();
    rawFrame->format = AV_SAMPLE_FMT_FLTP;
    rawFrame->sample_rate = 48'000;
    rawFrame->nb_samples = 480;
    rawFrame->pts = 0;
    av_channel_layout_default(&rawFrame->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(rawFrame.get(), 0), 0);
    auto raw = FFmpegBufferFactory::wrapFrame(std::move(rawFrame), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, raw);
    if (!raw) return;
    auto lineage = std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        MediaRunningTime::fromNanoseconds(0), std::nullopt,
        MediaRunningTime::fromNanoseconds(10'000'000),
        MediaDecodeOrderMode::PresentationOrderNoReorder, "audio",
        MediaSourceAccessUnitSequence(1), MediaTimeMappingConfidence::Locked, 7});
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        raw.value(), lineage, {0, 480, 48'000});
    EXPECT_TRUE(ctx, canonical);
    if (!canonical) return;
    const MediaAudioPlaybackOrigin origin{
        7, MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0), 12'345, 48'000};
    auto bound = MediaBoundCanonicalAudioBuffer::create(canonical.value(), origin);
    EXPECT_TRUE(ctx, bound);
    if (!bound) return;
    auto quantizer = MediaAudioCorrectionQuantizer::create(
        MediaRunningTime::fromNanoseconds(10'000'000),
        MediaRunningTime::fromNanoseconds(5'000'000), 48'000);
    EXPECT_TRUE(ctx, quantizer);
    if (!quantizer) return;
    auto scheduled = std::move(quantizer).value().schedule(
        7, 1, origin.epochOutputSampleIndex, 2'084,
        MediaAudioCorrectionTelemetry{
            MediaRunningTime::fromNanoseconds(0), 0, 0, false});
    EXPECT_TRUE(ctx, scheduled && scheduled.value());
    if (!scheduled || !scheduled.value()) return;
    EXPECT_EQ(ctx, scheduled.value()->sampleDelta(), 1);
    EXPECT_TRUE(ctx, execution.findInputChannel(resample, "correction")->push(
        makeMediaBufferRef<MediaAudioCorrectionBuffer>(*scheduled.value())));
    EXPECT_TRUE(ctx, execution.findInputChannel(resample, "frame")->push(bound.value()));
    for (int step = 0; step < 6; ++step) EXPECT_TRUE(ctx, node.process(execution));
    MediaBufferRef output;
    EXPECT_TRUE(ctx, execution.findOutputChannel(resample, "frame")->tryPop(output));
    const auto* typed = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(output.get());
    EXPECT_TRUE(ctx, typed != nullptr);
    if (typed) {
        const AVFrame* outputFrame = FFmpegFrameView::frame(typed->media()->media());
        EXPECT_TRUE(ctx, outputFrame != nullptr);
        EXPECT_EQ(ctx, typed->media()->interval().begin, static_cast<std::int64_t>(12'345));
        if (outputFrame) {
            EXPECT_EQ(ctx, outputFrame->nb_samples, 464);
            EXPECT_EQ(ctx, typed->media()->interval().end,
                      typed->media()->interval().begin + outputFrame->nb_samples);
        }
    }
    node.abort(execution);
}

void testPurgeDropsRetainedResampleOutputAndRestartsGeneration(TestContext& ctx)
{
    MediaGraph graph;
    const auto inputPolicy = MediaBlockingEdgePolicyPlanner::planQueue(4);
    const auto outputPolicy = MediaBlockingEdgePolicyPlanner::planQueue(1);
    const auto codecSource = graph.addNode(MediaNodeKind::DebugDump, "purge.codec");
    const auto frameSource = graph.addNode(MediaNodeKind::DebugDump, "purge.frame");
    const auto resample = graph.addNode(MediaNodeKind::AudioResample, "purge.resample");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "purge.sink");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Mode, "disabled");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    EXPECT_TRUE(ctx, graph.connect(codecSource, "codec", resample, "codec",
                                   "purge.codec", inputPolicy));
    EXPECT_TRUE(ctx, graph.connect(frameSource, "frame", resample, "frame",
                                   "purge.frame", inputPolicy));
    EXPECT_TRUE(ctx, graph.connect(resample, "frame", sink, "frame",
                                   "purge.output", outputPolicy));

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    auto state = std::make_shared<AudioResampleLineageState>(
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 1);
    AudioResampleNode node(
        resample, MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
        state);
    EXPECT_TRUE(ctx, node.start(execution));
    auto* codecInput = execution.findInputChannel(resample, "codec");
    auto* frameInput = execution.findInputChannel(resample, "frame");
    auto* output = execution.findOutputChannel(resample, "frame");
    EXPECT_TRUE(ctx, codecInput && frameInput && output);
    if (!codecInput || !frameInput || !output) return;

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->time_base = AVRational{1, 48'000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer && codecInput->push(codecBuffer.value()));
    if (!codecBuffer) return;
    EXPECT_TRUE(ctx, node.process(execution));

    const auto makeBound = [&](std::uint64_t generation,
                               std::uint64_t sequence) -> MediaBufferRef {
        auto frame = ::media::ffmpeg::makeFrame();
        frame->format = AV_SAMPLE_FMT_FLTP;
        frame->sample_rate = 48'000;
        frame->nb_samples = 480;
        frame->pts = 0;
        av_channel_layout_default(&frame->ch_layout, 2);
        if (av_frame_get_buffer(frame.get(), 0) < 0) return {};
        auto raw = FFmpegBufferFactory::wrapFrame(
            std::move(frame), MediaStreamKind::Audio);
        if (!raw) return {};
        auto lineage = std::make_shared<const MediaCanonicalLineage>(
            MediaCanonicalLineage{
                MediaRunningTime::fromNanoseconds(0), std::nullopt,
                MediaRunningTime::fromNanoseconds(10'000'000),
                MediaDecodeOrderMode::PresentationOrderNoReorder, "audio",
                MediaSourceAccessUnitSequence(sequence),
                MediaTimeMappingConfidence::Locked, generation});
        auto canonical = MediaCanonicalAudioSamplesBuffer::create(
            raw.value(), lineage, {0, 480, 48'000});
        if (!canonical) return {};
        auto bound = MediaBoundCanonicalAudioBuffer::create(
            canonical.value(),
            {generation, MediaRunningTime::fromNanoseconds(0),
             MediaRunningTime::fromNanoseconds(0), 0, 48'000});
        return bound ? std::move(bound).value() : MediaBufferRef{};
    };

    auto blocker = FFmpegBufferFactory::makeFlush(MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, blocker && output->push(blocker.value()));
    auto old = makeBound(7, 1);
    EXPECT_TRUE(ctx, old && frameInput->push(old));
    auto blocked = node.process(execution);
    EXPECT_TRUE(ctx, blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    EXPECT_TRUE(ctx, node.generationPurgeTarget()->purge({7, 8, 1}));
    EXPECT_EQ(ctx, state->outputIntervals.queuedSamples(), static_cast<std::int64_t>(0));
    EXPECT_FALSE(ctx, state->correctionExecutor.has_value() &&
                          state->correctionExecutor->generation() == 7);

    MediaBufferRef popped;
    EXPECT_TRUE(ctx, output->tryPop(popped) && popped == blocker.value());
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_FALSE(ctx, output->tryPop(popped));
    auto next = makeBound(8, 2);
    EXPECT_TRUE(ctx, next && frameInput->push(next));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, output->tryPop(popped));
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
        popped.get());
    EXPECT_TRUE(ctx, bound && bound->audioOrigin().generation == 8);
    EXPECT_FALSE(ctx, output->tryPop(popped));
    node.abort(execution);
}

void testSynchronizedFrameRejectsFragmentSampleMismatch(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(2);
    const auto codecSource = graph.addNode(MediaNodeKind::DebugDump, "mismatch.codec");
    const auto frameSource = graph.addNode(MediaNodeKind::DebugDump, "mismatch.frame");
    const auto resample = graph.addNode(MediaNodeKind::AudioResample, "mismatch.resample");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "mismatch.sink");
    graph.setNodeOption(resample, MediaAudioCorrectionOptionKey::Mode, "disabled");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio,
                        MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio,
                       MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    EXPECT_TRUE(ctx, graph.connect(codecSource, "codec", resample, "codec",
                                   "mismatch.codec", policy));
    EXPECT_TRUE(ctx, graph.connect(frameSource, "frame", resample, "frame",
                                   "mismatch.frame", policy));
    EXPECT_TRUE(ctx, graph.connect(resample, "frame", sink, "frame",
                                   "mismatch.output", policy));

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    AudioResampleNode node(
        resample, MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
        std::make_shared<AudioResampleLineageState>(
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio, 2));
    EXPECT_TRUE(ctx, node.start(execution));

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48'000;
    codec->time_base = AVRational{1, 48'000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer);
    if (!codecBuffer) return;
    EXPECT_TRUE(ctx, execution.findInputChannel(resample, "codec")->push(
                         codecBuffer.value()));
    EXPECT_TRUE(ctx, node.process(execution));

    auto rawFrame = ::media::ffmpeg::makeFrame();
    rawFrame->format = AV_SAMPLE_FMT_FLTP;
    rawFrame->sample_rate = 48'000;
    rawFrame->nb_samples = 480;
    av_channel_layout_default(&rawFrame->ch_layout, 2);
    EXPECT_EQ(ctx, av_frame_get_buffer(rawFrame.get(), 0), 0);
    auto raw = FFmpegBufferFactory::wrapFrame(
        std::move(rawFrame), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, raw);
    if (!raw) return;
    auto exactLineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{
            MediaRunningTime::fromNanoseconds(0), std::nullopt,
            MediaRunningTime::fromNanoseconds(9'979'166),
            MediaDecodeOrderMode::PresentationOrderNoReorder, "audio",
            MediaSourceAccessUnitSequence(1),
            MediaTimeMappingConfidence::Locked, 7});
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        raw.value(), exactLineage, {0, 479, 48'000});
    EXPECT_TRUE(ctx, canonical);
    if (!canonical) return;
    auto bound = MediaBoundCanonicalAudioBuffer::create(
        canonical.value(),
        {7, MediaRunningTime::fromNanoseconds(0),
         MediaRunningTime::fromNanoseconds(0), 0, 48'000});
    EXPECT_TRUE(ctx, bound);
    if (!bound) return;
    EXPECT_TRUE(ctx, execution.findInputChannel(resample, "frame")->push(
                         bound.value()));
    EXPECT_FALSE(ctx, node.process(execution));
    MediaBufferRef unexpected;
    EXPECT_FALSE(ctx, execution.findOutputChannel(resample, "frame")->tryPop(
                          unexpected));
    node.abort(execution);
}

} // namespace

void runAudioResampleNodeTests(TestContext& ctx)
{
    testZeroCapacitySwrConversionDoesNotConsumeLiveInput(ctx);
    testZeroUpperBoundDrainStillObtainsExhaustionProof(ctx);
    testCorrectionOptionsAreMandatoryAndModeSpecific(ctx);
    testEofDrainSettlesPendingCorrectionTailThroughNode(ctx);
    testCodecMetadataCloseBeforeBindFails(ctx);
    testExternalCorrectionFlushPreservesEpochAndEmitsEofOnce(ctx);
    testClosedInputDrainsResamplerBeforeFinishing(ctx);
    testSynchronizedOriginOwnsFirstOutputSampleIndex(ctx);
    testPurgeDropsRetainedResampleOutputAndRestartsGeneration(ctx);
    testSynchronizedFrameRejectsFragmentSampleMismatch(ctx);
}
