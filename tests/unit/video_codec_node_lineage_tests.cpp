#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoEncodeNode.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoDecoderCodecApi.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoEncoderCodecApi.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>

using namespace media::ffmpeg::graph;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed: " #condition << " at " << __LINE__ << '\n'; \
    std::exit(1); } } while (false)

namespace {

std::shared_ptr<const MediaCanonicalLineage> makeLineage(
    std::uint64_t generation, std::uint64_t sequence)
{
    const auto time = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(sequence) * 40'000'000);
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        time, time, MediaRunningTime::fromNanoseconds(40'000'000),
        MediaDecodeOrderMode::PresentationOrderNoReorder, "codec-node-test",
        MediaSourceAccessUnitSequence(sequence), MediaTimeMappingConfidence::Locked,
        generation});
}

AVBufferRef* makeOpaque(MediaCodecLineageRegistry& registry,
                        const std::shared_ptr<const MediaCanonicalLineage>& lineage)
{
    auto token = registry.submit(lineage);
    CHECK(token);
    auto opaque = makeMediaFfmpegLineageOpaque(std::move(token).value());
    CHECK(opaque);
    return opaque.value();
}

MediaBufferRef makeCanonicalPacket(std::uint64_t generation, std::uint64_t sequence)
{
    auto packet = ::media::ffmpeg::makePacket();
    CHECK(packet && av_new_packet(packet.get(), 8) == 0);
    packet->pts = static_cast<std::int64_t>(sequence);
    packet->dts = packet->pts;
    packet->duration = 1;
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(packet), MediaStreamKind::Video, std::nullopt);
    CHECK(wrapped);
    MediaTimeDescriptor time;
    time.timeBase = {1, 25};
    wrapped.value()->setTimeDescriptor(time);
    auto canonical = MediaCanonicalAccessUnitBuffer::create(
        wrapped.value(), makeLineage(generation, sequence));
    CHECK(canonical);
    return std::move(canonical).value();
}

MediaBufferRef makeCanonicalFrame(std::uint64_t generation, std::uint64_t sequence)
{
    auto frame = ::media::ffmpeg::makeFrame();
    CHECK(frame);
    frame->pts = static_cast<std::int64_t>(sequence);
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = 16;
    frame->height = 16;
    CHECK(av_frame_get_buffer(frame.get(), 32) == 0);
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Video);
    CHECK(wrapped);
    MediaTimeDescriptor time;
    time.timeBase = {1, 25};
    wrapped.value()->setTimeDescriptor(time);
    auto canonical = MediaCanonicalVideoFrameBuffer::create(
        wrapped.value(), makeLineage(generation, sequence));
    CHECK(canonical);
    return std::move(canonical).value();
}

class ScriptedDecoderApi final : public MediaVideoDecoderCodecApi {
public:
    explicit ScriptedDecoderApi(AVBufferRef* delayed) : output(delayed) {}
    ~ScriptedDecoderApi() override { av_buffer_unref(&output); }

    int sendPacket(AVCodecContext*, const AVPacket* packet) override
    {
        if (!packet) { draining = true; return 0; }
        ++sendCalls;
        if (injectEagain) {
            injectEagain = false;
            awaitingRetry = true;
            firstIdentity = packet;
            return AVERROR(EAGAIN);
        }
        if (awaitingRetry) {
            CHECK(packet == firstIdentity);
            awaitingRetry = false;
        }
        CHECK(!output);
        output = av_buffer_ref(packet->opaque_ref);
        return output ? 0 : AVERROR(ENOMEM);
    }

    int receiveFrame(AVCodecContext*, AVFrame* frame) override
    {
        if (output) {
            frame->opaque_ref = output;
            output = nullptr;
            frame->format = AV_PIX_FMT_YUV420P;
            frame->width = 16;
            frame->height = 16;
            frame->pts = ++outputPts;
            frame->duration = 1;
            return 0;
        }
        return draining ? AVERROR_EOF : AVERROR(EAGAIN);
    }

    void flushBuffers(AVCodecContext*) override
    {
        ++flushCalls;
        if (awaitingRetry) ++canceledRetries;
        draining = false;
        awaitingRetry = false;
        firstIdentity = nullptr;
    }

    int sendCalls = 0;
    int flushCalls = 0;
    int canceledRetries = 0;
    const AVPacket* firstIdentity = nullptr;
    AVBufferRef* output = nullptr;
    bool draining = false;
    bool injectEagain = true;
    bool awaitingRetry = false;
    std::int64_t outputPts = 0;
};

class ScriptedEncoderApi final : public MediaVideoEncoderCodecApi {
public:
    explicit ScriptedEncoderApi(AVBufferRef* delayed) : output(delayed) {}
    ~ScriptedEncoderApi() override { av_buffer_unref(&output); }

    int sendFrame(AVCodecContext*, const AVFrame* frame) override
    {
        if (!frame) { draining = true; return 0; }
        ++sendCalls;
        if (injectEagain) {
            injectEagain = false;
            awaitingRetry = true;
            firstIdentity = frame;
            return AVERROR(EAGAIN);
        }
        if (awaitingRetry) {
            CHECK(frame == firstIdentity);
            awaitingRetry = false;
        }
        CHECK(!output);
        output = av_buffer_ref(frame->opaque_ref);
        return output ? 0 : AVERROR(ENOMEM);
    }

    int receivePacket(AVCodecContext*, AVPacket* packet) override
    {
        if (output) {
            CHECK(av_new_packet(packet, 8) == 0);
            packet->opaque_ref = output;
            output = nullptr;
            packet->pts = ++outputPts;
            packet->dts = packet->pts;
            packet->duration = 1;
            return 0;
        }
        return draining ? AVERROR_EOF : AVERROR(EAGAIN);
    }

    void flushBuffers(AVCodecContext*) override
    {
        ++flushCalls;
        if (awaitingRetry) ++canceledRetries;
        draining = false;
        awaitingRetry = false;
        firstIdentity = nullptr;
    }

    int sendCalls = 0;
    int flushCalls = 0;
    int canceledRetries = 0;
    const AVFrame* firstIdentity = nullptr;
    AVBufferRef* output = nullptr;
    bool draining = false;
    bool injectEagain = true;
    bool awaitingRetry = false;
    std::int64_t outputPts = 0;
};

struct CodecGraphFixture {
    MediaGraph graph;
    MediaNodeId node;
    MediaNodeId sink;
};

CodecGraphFixture makeCodecGraph(bool decode, std::size_t outputCapacity = 4)
{
    CodecGraphFixture fixture;
    const auto source = fixture.graph.addNode(MediaNodeKind::ControlSignal, "codec.data.source");
    const auto codecSource = fixture.graph.addNode(MediaNodeKind::ControlSignal, "codec.context.source");
    fixture.node = fixture.graph.addNode(
        decode ? MediaNodeKind::VideoDecode : MediaNodeKind::VideoEncode,
        decode ? "video.decode" : "video.encode");
    fixture.sink = fixture.graph.addNode(MediaNodeKind::ControlSignal, "codec.sink");
    const char* inputName = decode ? "packet" : "frame";
    const char* outputName = decode ? "frame" : "packet";
    const auto inputEdge = decode ? MediaEdgeKind::EncodedPacket : MediaEdgeKind::RawFrame;
    const auto outputEdge = decode ? MediaEdgeKind::RawFrame : MediaEdgeKind::EncodedPacket;
    const auto inputPayload = decode ? MediaPayloadKind::Packet : MediaPayloadKind::Frame;
    const auto outputPayload = decode ? MediaPayloadKind::Frame : MediaPayloadKind::Packet;
    fixture.graph.addOutputPort(source, inputName, MediaStreamKind::Video,
                                inputEdge, inputPayload, true, true);
    fixture.graph.addInputPort(fixture.node, inputName, MediaStreamKind::Video,
                               inputEdge, inputPayload, true, true);
    fixture.graph.addOutputPort(fixture.node, outputName, MediaStreamKind::Video,
                                outputEdge, outputPayload, true, true);
    fixture.graph.addInputPort(fixture.sink, outputName, MediaStreamKind::Video,
                               outputEdge, outputPayload, true, true);
    fixture.graph.addOutputPort(codecSource, "codec", MediaStreamKind::Metadata,
                                MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
    fixture.graph.addInputPort(fixture.node, "codec", MediaStreamKind::Metadata,
                               MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
    fixture.graph.connect(source, inputName, fixture.node, inputName, "codec input",
                          MediaGraphBuildSupport::blockingQueuePolicy(4));
    fixture.graph.connect(codecSource, "codec", fixture.node, "codec", "codec context",
                          MediaGraphBuildSupport::blockingQueuePolicy(1));
    fixture.graph.connect(fixture.node, outputName, fixture.sink, outputName, "codec output",
                          MediaGraphBuildSupport::blockingQueuePolicy(outputCapacity));
    return fixture;
}

MediaBufferRef makeCodecContext()
{
    auto context = ::media::ffmpeg::makeCodecContext(nullptr);
    CHECK(context);
    context->time_base = {1, 25};
    context->framerate = {25, 1};
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    context->width = 16;
    context->height = 16;
    auto wrapped = FFmpegBufferFactory::wrapCodecContext(std::move(context));
    CHECK(wrapped);
    wrapped.value()->setStreamKind(MediaStreamKind::Metadata);
    return wrapped.value();
}

MediaBufferRef makeRealDelayedEncoderContext()
{
    const AVCodec* codec = nullptr;
    for (const char* name : {"libx264", "libx265", "h264", "hevc"}) {
        const AVCodec* candidate = avcodec_find_encoder_by_name(name);
        if (candidate) {
            codec = candidate;
            break;
        }
    }
    if (!codec) {
        std::cerr << "UNSUPPORTED: no delayed software video encoder is available\n";
        std::exit(77);
    }
    auto context = ::media::ffmpeg::makeCodecContext(codec);
    CHECK(context);
    context->bit_rate = 400'000;
    context->width = 16;
    context->height = 16;
    context->time_base = {1, 25};
    context->framerate = {25, 1};
    context->gop_size = 12;
    context->max_b_frames = 1;
    context->pix_fmt = AV_PIX_FMT_YUV420P;
    context->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
    if (avcodec_open2(context.get(), codec, nullptr) < 0) {
        std::cerr << "UNSUPPORTED: selected delayed software encoder cannot be opened\n";
        std::exit(77);
    }
    auto wrapped = FFmpegBufferFactory::wrapCodecContext(std::move(context));
    CHECK(wrapped);
    wrapped.value()->setStreamKind(MediaStreamKind::Metadata);
    return wrapped.value();
}

MediaBufferRef makeFlush()
{
    return makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Flush);
}

MediaBufferRef makeEof()
{
    return makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Eof);
}

MediaBufferRef makeTerminal(bool eof, bool videoScoped)
{
    auto terminal = eof ? makeEof() : makeFlush();
    if (videoScoped) terminal->setStreamKind(MediaStreamKind::Video);
    return terminal;
}

template <typename View>
void checkSequence(const MediaBufferRef& output, std::uint64_t sequence, View view)
{
    auto lineage = view(output);
    CHECK(lineage && lineage->sourceSequence.value() == sequence);
}

void scriptedDecoderRetainsOneSubmissionAcrossEagain()
{
    auto created = MediaCodecLineageRegistry::create(2);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(std::move(created).value());
    auto api = std::make_shared<ScriptedDecoderApi>(makeOpaque(*registry, makeLineage(71, 1)));
    auto fixture = makeCodecGraph(true);
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(fixture.graph));
    auto* codec = execution.findInputChannel(fixture.node, "codec");
    auto* input = execution.findInputChannel(fixture.node, "packet");
    auto* output = execution.findInputChannel(fixture.sink, "frame");
    CHECK(codec && input && output);
    CHECK(codec->push(makeCodecContext()));
    CHECK(input->push(makeCanonicalPacket(71, 2)));
    VideoDecodeNode node(fixture.node, registry, api);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(node.process(execution));
    MediaBufferRef observed;
    CHECK(output->tryPop(observed));
    checkSequence(observed, 1, FFmpegFrameView::canonicalLineage);
    CHECK(node.process(execution));
    CHECK(output->tryPop(observed));
    checkSequence(observed, 2, FFmpegFrameView::canonicalLineage);
    CHECK(api->sendCalls == 2 && api->firstIdentity);
    CHECK(input->push(makeFlush()));
    CHECK(node.process(execution));
    CHECK(output->tryPop(observed) && observed->isFlush());
    CHECK(api->flushCalls == 1);
    CHECK(input->push(makeCanonicalPacket(71, 3)));
    CHECK(node.process(execution));
    CHECK(output->tryPop(observed));
    checkSequence(observed, 3, FFmpegFrameView::canonicalLineage);
    CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    CHECK(registry->finishGeneration(71));
}

void scriptedEncoderRetainsOneSubmissionAcrossEagain()
{
    auto created = MediaCodecLineageRegistry::create(2);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(std::move(created).value());
    auto api = std::make_shared<ScriptedEncoderApi>(makeOpaque(*registry, makeLineage(72, 1)));
    auto fixture = makeCodecGraph(false);
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(fixture.graph));
    auto* codec = execution.findInputChannel(fixture.node, "codec");
    auto* input = execution.findInputChannel(fixture.node, "frame");
    auto* output = execution.findInputChannel(fixture.sink, "packet");
    CHECK(codec && input && output);
    CHECK(codec->push(makeCodecContext()));
    VideoEncodeNode node(fixture.node, registry, api);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(input->push(makeCanonicalFrame(72, 2)));
    CHECK(node.process(execution));
    MediaBufferRef observed;
    CHECK(output->tryPop(observed));
    checkSequence(observed, 1, FFmpegPacketView::canonicalLineage);
    CHECK(node.process(execution));
    CHECK(output->tryPop(observed));
    checkSequence(observed, 2, FFmpegPacketView::canonicalLineage);
    CHECK(api->sendCalls == 2 && api->firstIdentity);
    CHECK(input->push(makeFlush()));
    CHECK(node.process(execution));
    CHECK(output->tryPop(observed) && observed->isFlush());
    CHECK(api->flushCalls == 1);
    CHECK(input->push(makeCanonicalFrame(72, 3)));
    CHECK(node.process(execution));
    CHECK(output->tryPop(observed));
    checkSequence(observed, 3, FFmpegPacketView::canonicalLineage);
    CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    CHECK(registry->finishGeneration(72));
}

void decoderDropsPurgedRetainedOutputBeforeNextGeneration()
{
    auto created = MediaCodecLineageRegistry::create(16);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(std::move(created).value());
    auto api = std::make_shared<ScriptedDecoderApi>(makeOpaque(*registry, makeLineage(91, 1)));
    AVBufferRef* lateOldLease = makeOpaque(*registry, makeLineage(91, 99));
    auto fixture = makeCodecGraph(true, 1);
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(fixture.graph));
    auto* codec = execution.findInputChannel(fixture.node, "codec");
    auto* input = execution.findInputChannel(fixture.node, "packet");
    auto* output = execution.findInputChannel(fixture.sink, "frame");
    CHECK(codec && input && output);
    CHECK(codec->push(makeCodecContext()));
    CHECK(output->push(makeCanonicalFrame(90, 900)));
    VideoDecodeNode node(fixture.node, registry, api);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(input->push(makeCanonicalPacket(91, 2)));
    auto blocked = node.process(execution);
    CHECK(blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    CHECK(node.generationPurgeTarget()->purge({91, 92, 1}));
    CHECK(api->flushCalls == 1);
    CHECK(api->canceledRetries == 1);

    MediaBufferRef observed;
    CHECK(output->tryPop(observed));
    CHECK(node.process(execution));
    CHECK(!output->tryPop(observed));
    for (std::uint64_t sequence : {3u, 4u}) {
        CHECK(input->push(makeCanonicalPacket(92, sequence)));
        bool produced = false;
        for (int attempt = 0; attempt < 6 && !produced; ++attempt) {
            CHECK(node.process(execution));
            produced = output->tryPop(observed);
        }
        CHECK(produced);
        auto lineage = FFmpegFrameView::canonicalLineage(observed);
        CHECK(lineage && lineage->generation == 92 &&
              lineage->sourceSequence.value() == sequence);
        observed.reset();
        CHECK(!output->tryPop(observed));
    }
    av_buffer_unref(&lateOldLease);
    CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    CHECK(registry->finishGeneration(92));
}

void encoderDropsPurgedRetainedOutputBeforeNextGeneration()
{
    auto created = MediaCodecLineageRegistry::create(16);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(std::move(created).value());
    auto api = std::make_shared<ScriptedEncoderApi>(makeOpaque(*registry, makeLineage(93, 1)));
    AVBufferRef* lateOldLease = makeOpaque(*registry, makeLineage(93, 99));
    auto fixture = makeCodecGraph(false, 1);
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(fixture.graph));
    auto* codec = execution.findInputChannel(fixture.node, "codec");
    auto* input = execution.findInputChannel(fixture.node, "frame");
    auto* output = execution.findInputChannel(fixture.sink, "packet");
    CHECK(codec && input && output);
    CHECK(codec->push(makeCodecContext()));
    CHECK(output->push(makeCanonicalPacket(90, 900)));
    VideoEncodeNode node(fixture.node, registry, api);
    CHECK(node.start(execution));
    CHECK(node.process(execution));
    CHECK(input->push(makeCanonicalFrame(93, 2)));
    auto blocked = node.process(execution);
    CHECK(blocked && blocked.value().state == MediaNodeProcessState::Waiting);
    CHECK(node.generationPurgeTarget()->purge({93, 94, 1}));
    CHECK(api->flushCalls == 1);
    CHECK(api->canceledRetries == 1);

    MediaBufferRef observed;
    CHECK(output->tryPop(observed));
    CHECK(node.process(execution));
    CHECK(!output->tryPop(observed));
    for (std::uint64_t sequence : {3u, 4u}) {
        CHECK(input->push(makeCanonicalFrame(94, sequence)));
        bool produced = false;
        for (int attempt = 0; attempt < 6 && !produced; ++attempt) {
            CHECK(node.process(execution));
            produced = output->tryPop(observed);
        }
        CHECK(produced);
        auto lineage = FFmpegPacketView::canonicalLineage(observed);
        CHECK(lineage && lineage->generation == 94 &&
              lineage->sourceSequence.value() == sequence);
        observed.reset();
        CHECK(!output->tryPop(observed));
    }
    av_buffer_unref(&lateOldLease);
    CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    CHECK(registry->finishGeneration(94));
}

void decoderPurgeCancelsRetainedTerminalAndCodecRetry()
{
    for (const bool eof : {false, true}) {
      for (const bool videoScoped : {false, true}) {
        auto created = MediaCodecLineageRegistry::create(4);
        CHECK(created);
        auto registry = std::make_shared<MediaCodecLineageRegistry>(
            std::move(created).value());
        auto api = std::make_shared<ScriptedDecoderApi>(
            makeOpaque(*registry, makeLineage(101, 1)));
        auto fixture = makeCodecGraph(true, 1);
        MediaGraphExecutionContext execution;
        CHECK(execution.compile(fixture.graph));
        auto* codec = execution.findInputChannel(fixture.node, "codec");
        auto* input = execution.findInputChannel(fixture.node, "packet");
        auto* output = execution.findInputChannel(fixture.sink, "frame");
        CHECK(codec && input && output);
        CHECK(codec->push(makeCodecContext()));
        VideoDecodeNode node(fixture.node, registry, api);
        CHECK(node.start(execution));
        CHECK(node.process(execution));

        CHECK(input->push(makeCanonicalPacket(101, 2)));
        CHECK(node.process(execution));
        MediaBufferRef observed;
        CHECK(output->tryPop(observed));
        observed.reset();
        CHECK(node.process(execution));
        CHECK(output->tryPop(observed));
        observed.reset();

        const auto blocker = makeCanonicalFrame(100, 900);
        CHECK(output->push(blocker));
        CHECK(input->push(makeTerminal(eof, videoScoped)));
        const auto retained = node.process(execution);
        CHECK(retained && retained.value().state != MediaNodeProcessState::Finished);
        const int flushCallsBeforePurge = api->flushCalls;
        CHECK(node.generationPurgeTarget()->purge({101, 102, 1}));
        CHECK(api->flushCalls == flushCallsBeforePurge + 1);
        CHECK(output->tryPop(observed) && observed == blocker);
        observed.reset();
        CHECK(node.process(execution));
        CHECK(!output->tryPop(observed));

        CHECK(input->push(makeCanonicalPacket(102, 3)));
        bool produced = false;
        for (int attempt = 0; attempt < 6 && !produced; ++attempt) {
            CHECK(node.process(execution));
            produced = output->tryPop(observed);
        }
        CHECK(produced);
        const auto lineage = FFmpegFrameView::canonicalLineage(observed);
        CHECK(lineage && lineage->generation == 102);
        CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
      }
    }
}

void encoderPurgeCancelsRetainedTerminalAndCodecRetry()
{
    for (const bool eof : {false, true}) {
      for (const bool videoScoped : {false, true}) {
        auto created = MediaCodecLineageRegistry::create(4);
        CHECK(created);
        auto registry = std::make_shared<MediaCodecLineageRegistry>(
            std::move(created).value());
        auto api = std::make_shared<ScriptedEncoderApi>(
            makeOpaque(*registry, makeLineage(103, 1)));
        auto fixture = makeCodecGraph(false, 1);
        MediaGraphExecutionContext execution;
        CHECK(execution.compile(fixture.graph));
        auto* codec = execution.findInputChannel(fixture.node, "codec");
        auto* input = execution.findInputChannel(fixture.node, "frame");
        auto* output = execution.findInputChannel(fixture.sink, "packet");
        CHECK(codec && input && output);
        CHECK(codec->push(makeCodecContext()));
        VideoEncodeNode node(fixture.node, registry, api);
        CHECK(node.start(execution));
        CHECK(node.process(execution));

        CHECK(input->push(makeCanonicalFrame(103, 2)));
        CHECK(node.process(execution));
        MediaBufferRef observed;
        CHECK(output->tryPop(observed));
        observed.reset();
        CHECK(node.process(execution));
        CHECK(output->tryPop(observed));
        observed.reset();

        const auto blocker = makeCanonicalPacket(100, 900);
        CHECK(output->push(blocker));
        CHECK(input->push(makeTerminal(eof, videoScoped)));
        const auto retained = node.process(execution);
        CHECK(retained && retained.value().state != MediaNodeProcessState::Finished);
        const int flushCallsBeforePurge = api->flushCalls;
        CHECK(node.generationPurgeTarget()->purge({103, 104, 1}));
        CHECK(api->flushCalls == flushCallsBeforePurge + 1);
        CHECK(output->tryPop(observed) && observed == blocker);
        observed.reset();
        CHECK(node.process(execution));
        CHECK(!output->tryPop(observed));

        CHECK(input->push(makeCanonicalFrame(104, 3)));
        bool produced = false;
        for (int attempt = 0; attempt < 6 && !produced; ++attempt) {
            CHECK(node.process(execution));
            produced = output->tryPop(observed);
        }
        CHECK(produced);
        const auto lineage = FFmpegPacketView::canonicalLineage(observed);
        CHECK(lineage && lineage->generation == 104);
        CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
      }
    }
}

void videoTerminalFreshnessRejectsAnotherMediaScope()
{
    auto created = MediaCodecLineageRegistry::create(1);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(
        std::move(created).value());
    auto api = std::make_shared<ScriptedDecoderApi>(nullptr);
    VideoDecodeLineageState state(registry, api);
    CHECK(state.observe(121));
    auto audioTerminal = makeEof();
    audioTerminal->setStreamKind(MediaStreamKind::Audio);
    CHECK(!state.authorizeRetainedControl(audioTerminal));
}

void codecPurgeReleasesPendingLeaseCapacityImmediately()
{
    {
        auto created = MediaCodecLineageRegistry::create(1);
        CHECK(created);
        auto registry = std::make_shared<MediaCodecLineageRegistry>(
            std::move(created).value());
        auto api = std::make_shared<ScriptedDecoderApi>(nullptr);
        auto fixture = makeCodecGraph(true);
        MediaGraphExecutionContext execution;
        CHECK(execution.compile(fixture.graph));
        auto* codec = execution.findInputChannel(fixture.node, "codec");
        auto* input = execution.findInputChannel(fixture.node, "packet");
        auto* output = execution.findInputChannel(fixture.sink, "frame");
        CHECK(codec && input && output);
        CHECK(codec->push(makeCodecContext()));
        VideoDecodeNode node(fixture.node, registry, api);
        CHECK(node.start(execution));
        CHECK(node.process(execution));
        CHECK(input->push(makeCanonicalPacket(131, 1)));
        CHECK(node.process(execution));
        CHECK(!registry->submit(makeLineage(132, 99)));
        CHECK(node.generationPurgeTarget()->purge({131, 132, 1}));
        CHECK(api->canceledRetries == 1);
        {
            auto released = registry->submit(makeLineage(132, 99));
            CHECK(released);
        }
        CHECK(input->push(makeCanonicalPacket(132, 2)));
        CHECK(node.process(execution));
        MediaBufferRef observed;
        CHECK(output->tryPop(observed));
        const auto lineage = FFmpegFrameView::canonicalLineage(observed);
        CHECK(lineage && lineage->generation == 132);
        CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    }
    {
        auto created = MediaCodecLineageRegistry::create(1);
        CHECK(created);
        auto registry = std::make_shared<MediaCodecLineageRegistry>(
            std::move(created).value());
        auto api = std::make_shared<ScriptedEncoderApi>(nullptr);
        auto fixture = makeCodecGraph(false);
        MediaGraphExecutionContext execution;
        CHECK(execution.compile(fixture.graph));
        auto* codec = execution.findInputChannel(fixture.node, "codec");
        auto* input = execution.findInputChannel(fixture.node, "frame");
        auto* output = execution.findInputChannel(fixture.sink, "packet");
        CHECK(codec && input && output);
        CHECK(codec->push(makeCodecContext()));
        VideoEncodeNode node(fixture.node, registry, api);
        CHECK(node.start(execution));
        CHECK(node.process(execution));
        CHECK(input->push(makeCanonicalFrame(133, 1)));
        CHECK(node.process(execution));
        CHECK(!registry->submit(makeLineage(134, 99)));
        CHECK(node.generationPurgeTarget()->purge({133, 134, 1}));
        CHECK(api->canceledRetries == 1);
        {
            auto released = registry->submit(makeLineage(134, 99));
            CHECK(released);
        }
        CHECK(input->push(makeCanonicalFrame(134, 2)));
        CHECK(node.process(execution));
        MediaBufferRef observed;
        CHECK(output->tryPop(observed));
        const auto lineage = FFmpegPacketView::canonicalLineage(observed);
        CHECK(lineage && lineage->generation == 134);
        CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    }
}

void realEncoderPreservesDelayedLineageThroughEofDrain()
{
    auto created = MediaCodecLineageRegistry::create(16);
    CHECK(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(std::move(created).value());
    auto fixture = makeCodecGraph(false);
    MediaGraphExecutionContext execution;
    CHECK(execution.compile(fixture.graph));
    auto* codec = execution.findInputChannel(fixture.node, "codec");
    auto* input = execution.findInputChannel(fixture.node, "frame");
    auto* output = execution.findInputChannel(fixture.sink, "packet");
    CHECK(codec && input && output);
    CHECK(codec->push(makeRealDelayedEncoderContext()));
    VideoEncodeNode node(fixture.node, registry);
    CHECK(node.start(execution));
    CHECK(node.process(execution));

    std::set<std::uint64_t> observedSequences;
    const auto drainOutput = [&] {
        MediaBufferRef observed;
        bool sawEof = false;
        while (output->tryPop(observed)) {
            if (observed->isEof()) {
                sawEof = true;
                continue;
            }
            auto lineage = FFmpegPacketView::canonicalLineage(observed);
            CHECK(lineage);
            observedSequences.insert(lineage->sourceSequence.value());
        }
        return sawEof;
    };

    for (std::uint64_t sequence = 1; sequence <= 3; ++sequence) {
        CHECK(input->push(makeCanonicalFrame(73, sequence)));
        CHECK(node.process(execution));
        (void)drainOutput();
    }
    CHECK(input->push(makeEof()));
    CHECK(node.process(execution));
    CHECK(drainOutput());
    CHECK(observedSequences.size() == 3);
    for (std::uint64_t sequence = 1; sequence <= 3; ++sequence) {
        CHECK(observedSequences.contains(sequence));
    }

    CHECK(static_cast<MediaRuntimeNode&>(node).stop(execution));
    CHECK(registry->finishGeneration(73));
}

} // namespace

int main()
{
    scriptedDecoderRetainsOneSubmissionAcrossEagain();
    scriptedEncoderRetainsOneSubmissionAcrossEagain();
    decoderDropsPurgedRetainedOutputBeforeNextGeneration();
    encoderDropsPurgedRetainedOutputBeforeNextGeneration();
    decoderPurgeCancelsRetainedTerminalAndCodecRetry();
    encoderPurgeCancelsRetainedTerminalAndCodecRetry();
    videoTerminalFreshnessRejectsAnotherMediaScope();
    codecPurgeReleasesPendingLeaseCapacityImmediately();
    realEncoderPreservesDelayedLineageThroughEofDrain();
    std::cout << "video codec node lineage tests passed\n";
    return 0;
}
