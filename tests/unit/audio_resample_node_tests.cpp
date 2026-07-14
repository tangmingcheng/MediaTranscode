#include "common/TestAssert.h"

#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/sync/MediaAudioCorrection.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"

extern "C" {
#include <libavutil/channel_layout.h>
}

#include <vector>
#include <optional>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

void testCorrectionOptionsAreMandatoryAndModeSpecific(TestContext& ctx)
{
    MediaGraph unknownModeGraph;
    const MediaNodeId unknownModeId = unknownModeGraph.addNode(
        MediaNodeKind::AudioResample, "unknown_mode");
    EXPECT_TRUE(ctx, unknownModeGraph.setNodeOption(
        unknownModeId, MediaAudioCorrectionOptionKey::Mode, "automatic"));
    MediaGraphExecutionContext unknownModeExecution;
    EXPECT_TRUE(ctx, unknownModeExecution.compile(unknownModeGraph));
    AudioResampleNode unknownMode(unknownModeId);
    EXPECT_FALSE(ctx, unknownMode.start(unknownModeExecution));

    MediaGraph missingModeGraph;
    const MediaNodeId missingModeId = missingModeGraph.addNode(
        MediaNodeKind::AudioResample, "missing_mode");
    MediaGraphExecutionContext missingModeExecution;
    EXPECT_TRUE(ctx, missingModeExecution.compile(missingModeGraph));
    AudioResampleNode missingMode(missingModeId);
    EXPECT_FALSE(ctx, missingMode.start(missingModeExecution));

    MediaGraph disabledGraph;
    const MediaNodeId disabledId = disabledGraph.addNode(
        MediaNodeKind::AudioResample, "disabled");
    EXPECT_TRUE(ctx, disabledGraph.setNodeOption(
        disabledId, MediaAudioCorrectionOptionKey::Mode, "disabled"));
    MediaGraphExecutionContext disabledExecution;
    EXPECT_TRUE(ctx, disabledExecution.compile(disabledGraph));
    AudioResampleNode disabled(disabledId);
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
    AudioResampleNode missingGeneration(missingGenerationId);
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
    AudioResampleNode external(externalId);
    EXPECT_TRUE(ctx, external.start(externalExecution));
    EXPECT_TRUE(ctx, external.flush(externalExecution));
    auto externalFlush = external.process(externalExecution);
    EXPECT_TRUE(ctx, externalFlush);
    EXPECT_TRUE(ctx, externalFlush &&
        externalFlush.value().state == MediaNodeProcessState::Progress);
    EXPECT_TRUE(ctx, external.stop(externalExecution));
}

void testExternalCorrectionRejectsSameFormatFrameWithoutCommand(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(4);
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
    AudioResampleNode node(resample);
    EXPECT_TRUE(ctx, node.start(execution));
    MediaChannel* codecInput = execution.findInputChannel(resample, "codec");
    MediaChannel* frameInput = execution.findInputChannel(resample, "frame");
    EXPECT_TRUE(ctx, codecInput != nullptr);
    EXPECT_TRUE(ctx, frameInput != nullptr);
    if (!codecInput || !frameInput) {
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
    EXPECT_TRUE(ctx, frameInput->push(frameBuffer.value()));
    EXPECT_FALSE(ctx, node.process(execution));
    node.abort(execution);
}

void testExternalCorrectionFlushPreservesEpochAndEmitsEofOnce(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
    const auto correctionPolicy = MediaGraphBuildSupport::blockingQueuePolicy(2);
    const auto outputPolicy = MediaGraphBuildSupport::blockingQueuePolicy(1);
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
    AudioResampleNode node(resample);
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
    auto regularLookaheadCommand = makeCommand(4, 3'072, 1'024, 0);
    EXPECT_TRUE(ctx, firstCommand && secondCommand && drainCommand &&
                         regularLookaheadCommand);
    if (!firstCommand || !secondCommand || !drainCommand ||
        !regularLookaheadCommand) return;
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
    EXPECT_TRUE(ctx, frameInput->push(frameBuffer.value()));

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
    EXPECT_TRUE(ctx, frameInput->push(continuationBuffer.value()));

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

    std::vector<MediaBufferRef> frames;
    int eofCount = 0;
    int flushCount = 0;
    bool finished = false;
    bool emittedWhileCorrectionsQueued = false;
    bool regularLookaheadQueued = false;
    std::size_t maximumCorrectionQueue = correctionInput->size();
    for (int step = 0; step < 32 && !finished; ++step) {
        if (!regularLookaheadQueued && correctionInput->size() < 2) {
            EXPECT_TRUE(ctx, correctionInput->push(
                makeMediaBufferRef<MediaAudioCorrectionBuffer>(
                    *regularLookaheadCommand)));
            regularLookaheadQueued = true;
        }
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
            } else {
                const AVFrame* emittedFrame = FFmpegFrameView::frame(emitted);
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
        const AVFrame* firstFrame = FFmpegFrameView::frame(frames[0]);
        const AVFrame* secondFrame = FFmpegFrameView::frame(frames[1]);
        EXPECT_EQ(ctx, firstFrame->nb_samples, 1'023);
        EXPECT_EQ(ctx, firstFrame->pts, static_cast<std::int64_t>(0));
        EXPECT_EQ(ctx, secondFrame->pts, static_cast<std::int64_t>(1'023));
    }
    std::int64_t expectedPts = 0;
    for (const auto& emitted : frames) {
        const AVFrame* emittedFrame = FFmpegFrameView::frame(emitted);
        EXPECT_EQ(ctx, emittedFrame->pts, expectedPts);
        expectedPts += emittedFrame->nb_samples;
    }
    constexpr std::int64_t InputSamples = 1'882 + 941;
    constexpr std::int64_t ExpectedResampledSamples =
        (InputSamples * 48'000 + 44'100 - 1) / 44'100;
    EXPECT_EQ(ctx, expectedPts, ExpectedResampledSamples);
    EXPECT_TRUE(ctx, emittedWhileCorrectionsQueued);
    EXPECT_TRUE(ctx, regularLookaheadQueued);
    EXPECT_TRUE(ctx, maximumCorrectionQueue <= static_cast<std::size_t>(2));
    EXPECT_TRUE(ctx, node.process(execution));
    MediaBufferRef duplicateEof;
    EXPECT_FALSE(ctx, output->tryPop(duplicateEof));
    node.abort(execution);
}

void testCodecMetadataCloseBeforeBindFails(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(2);
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
    AudioResampleNode node(resample);
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
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
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
    AudioResampleNode node(resample);
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

} // namespace

void runAudioResampleNodeTests(TestContext& ctx)
{
    testCorrectionOptionsAreMandatoryAndModeSpecific(ctx);
    testExternalCorrectionRejectsSameFormatFrameWithoutCommand(ctx);
    testCodecMetadataCloseBeforeBindFails(ctx);
    testExternalCorrectionFlushPreservesEpochAndEmitsEofOnce(ctx);
    testClosedInputDrainsResamplerBeforeFinishing(ctx);
}
