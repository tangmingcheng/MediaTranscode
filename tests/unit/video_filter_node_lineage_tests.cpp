#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace media::ffmpeg::graph;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed: " #condition << " at " << __LINE__ << '\n'; \
    std::exit(1); } } while (false)

namespace {

struct FilterGraphFixture {
    MediaGraph graph;
    MediaNodeId node;
    MediaNodeId sink;
};

FilterGraphFixture makeFilterGraph(
    const std::string& pipeline = "fps=50",
    std::size_t outputCapacity = 16)
{
    FilterGraphFixture fixture;
    const auto frameSource = fixture.graph.addNode(
        MediaNodeKind::ControlSignal, "filter.frame.source");
    const auto codecSource = fixture.graph.addNode(
        MediaNodeKind::ControlSignal, "filter.codec.source");
    fixture.node = fixture.graph.addNode(MediaNodeKind::VideoFilter, "video.filter");
    fixture.sink = fixture.graph.addNode(MediaNodeKind::ControlSignal, "filter.sink");
    CHECK(fixture.graph.setNodeOption(fixture.node, "filter.pipeline.filter", pipeline));

    fixture.graph.addOutputPort(frameSource, "frame", MediaStreamKind::Video,
                                MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    fixture.graph.addInputPort(fixture.node, "frame", MediaStreamKind::Video,
                               MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    fixture.graph.addOutputPort(fixture.node, "frame", MediaStreamKind::Video,
                                MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    fixture.graph.addInputPort(fixture.sink, "frame", MediaStreamKind::Video,
                               MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    fixture.graph.addOutputPort(codecSource, "codec", MediaStreamKind::Metadata,
                                MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
    fixture.graph.addInputPort(fixture.node, "codec", MediaStreamKind::Metadata,
                               MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
    fixture.graph.connect(frameSource, "frame", fixture.node, "frame", "filter input",
                          MediaGraphBuildSupport::blockingQueuePolicy(8));
    fixture.graph.connect(codecSource, "codec", fixture.node, "codec", "filter codec",
                          MediaGraphBuildSupport::blockingQueuePolicy(1));
    fixture.graph.connect(fixture.node, "frame", fixture.sink, "frame", "filter output",
                          MediaGraphBuildSupport::blockingQueuePolicy(outputCapacity));
    return fixture;
}

std::shared_ptr<const MediaCanonicalLineage> makeLineage(
    std::uint64_t generation, std::uint64_t sequence)
{
    const auto time = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(sequence) * 40'000'000);
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        time, time, MediaRunningTime::fromNanoseconds(40'000'000),
        MediaDecodeOrderMode::PresentationOrderNoReorder, "filter-node-test",
        MediaSourceAccessUnitSequence(sequence), MediaTimeMappingConfidence::Locked,
        generation});
}

MediaBufferRef makeCanonicalFrame(std::uint64_t generation,
                                  std::uint64_t sequence,
                                  std::int64_t pts)
{
    auto frame = ::media::ffmpeg::makeFrame();
    CHECK(frame);
    frame->pts = pts;
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = 16;
    frame->height = 16;
    frame->duration = 1;
    frame->sample_aspect_ratio = {1, 1};
    CHECK(av_frame_get_buffer(frame.get(), 32) == 0);

    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Video);
    CHECK(wrapped);
    MediaTimeDescriptor time;
    time.timeBase = {1, 25};
    time.frameRate = {25, 1};
    wrapped.value()->setTimeDescriptor(time);
    auto canonical = MediaCanonicalVideoFrameBuffer::create(
        wrapped.value(), makeLineage(generation, sequence));
    CHECK(canonical);
    canonical.value()->setTimeDescriptor(time);
    return std::move(canonical).value();
}

MediaBufferRef makeEncoderContext()
{
    auto context = ::media::ffmpeg::makeCodecContext(nullptr);
    CHECK(context);
    context->time_base = {1, 50};
    context->framerate = {50, 1};
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    context->width = 16;
    context->height = 16;
    auto wrapped = FFmpegBufferFactory::wrapCodecContext(std::move(context));
    CHECK(wrapped);
    wrapped.value()->setStreamKind(MediaStreamKind::Metadata);
    return wrapped.value();
}

MediaBufferRef makeFlush()
{
    return makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Flush);
}

void realFpsFilterPreservesOneToManyLineageAcrossFlushReuse()
{
    auto created = MediaCodecLineageRegistry::create(8);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(
        std::move(created).value());
    auto fixture = makeFilterGraph();
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(fixture.graph));
    auto* codec = execution.findInputChannel(fixture.node, "codec");
    auto* input = execution.findInputChannel(fixture.node, "frame");
    auto* output = execution.findInputChannel(fixture.sink, "frame");
    CHECK(codec && input && output);
    CHECK(codec->push(makeEncoderContext()));

    VideoFilterNode node(fixture.node, registry);
    CHECK(node.start(execution));
    CHECK(node.process(execution));

    const auto runBatch = [&](std::uint64_t generation,
                              std::uint64_t firstSequence) {
        std::map<std::uint64_t, std::size_t> sequenceCounts;
        std::vector<std::int64_t> observedPts;
        std::int64_t previousPts = AV_NOPTS_VALUE;
        const auto drain = [&] {
            MediaBufferRef observed;
            bool sawFlush = false;
            while (output->tryPop(observed)) {
                if (observed->isFlush()) {
                    sawFlush = true;
                    continue;
                }
                const auto lineage = FFmpegFrameView::canonicalLineage(observed);
                CHECK(lineage && lineage->generation == generation);
                ++sequenceCounts[lineage->sourceSequence.value()];
                const AVFrame* frame = FFmpegFrameView::frame(observed);
                CHECK(frame && frame->pts != AV_NOPTS_VALUE);
                CHECK(previousPts == AV_NOPTS_VALUE || frame->pts > previousPts);
                previousPts = frame->pts;
                observedPts.push_back(frame->pts);
            }
            return sawFlush;
        };

        CHECK(input->push(makeCanonicalFrame(generation, firstSequence, 0)));
        CHECK(node.process(execution));
        (void)drain();
        CHECK(input->push(makeCanonicalFrame(generation, firstSequence + 1, 1)));
        CHECK(node.process(execution));
        (void)drain();
        CHECK(input->push(makeFlush()));
        CHECK(node.process(execution));
        CHECK(drain());

        CHECK(sequenceCounts.size() == 2);
        CHECK(sequenceCounts.at(firstSequence) == 2);
        CHECK(sequenceCounts.at(firstSequence + 1) == 2);
        CHECK((observedPts == std::vector<std::int64_t>{0, 1, 2, 3}));
        CHECK(registry->finishGeneration(generation));
    };

    runBatch(81, 1);
    runBatch(82, 3);
    CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
}

void realFilterDropsPurgedRetainedOutputBeforeNextGeneration()
{
    auto created = MediaCodecLineageRegistry::create(16);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(
        std::move(created).value());
    auto fixture = makeFilterGraph("null", 1);
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(fixture.graph));
    auto* codec = execution.findInputChannel(fixture.node, "codec");
    auto* input = execution.findInputChannel(fixture.node, "frame");
    auto* output = execution.findInputChannel(fixture.sink, "frame");
    CHECK(codec && input && output);
    CHECK(codec->push(makeEncoderContext()));
    CHECK(output->push(makeCanonicalFrame(90, 900, 900)));

    VideoFilterNode node(fixture.node, registry);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(input->push(makeCanonicalFrame(95, 1, 0)));
    auto blocked = node.process(execution);
    CHECK(blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    CHECK(registry->purge({95, 96, 1}));

    MediaBufferRef observed;
    CHECK(output->tryPop(observed));
    CHECK(node.process(execution));
    CHECK(!output->tryPop(observed));
    for (std::uint64_t sequence : {2u, 3u}) {
        CHECK(input->push(makeCanonicalFrame(
            96, sequence, static_cast<std::int64_t>(sequence))));
        bool produced = false;
        for (int attempt = 0; attempt < 6 && !produced; ++attempt) {
            CHECK(node.process(execution));
            produced = output->tryPop(observed);
        }
        CHECK(produced);
        const auto lineage = FFmpegFrameView::canonicalLineage(observed);
        CHECK(lineage && lineage->generation == 96 &&
              lineage->sourceSequence.value() == sequence);
        observed.reset();
        CHECK(!output->tryPop(observed));
    }
    CHECK(registry->finishGeneration(95));
    CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    CHECK(registry->finishGeneration(96));
}

} // namespace

int main()
{
    realFpsFilterPreservesOneToManyLineageAcrossFlushReuse();
    realFilterDropsPurgedRetainedOutputBeforeNextGeneration();
    std::cout << "video filter node lineage tests passed\n";
    return 0;
}
