#include "common/TestAssert.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/nodes/mux/FFmpegFileMuxSession.h"
#include "internal/graph/nodes/mux/MediaMuxSessionFactory.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

struct SessionTrace final {
    std::vector<std::string> calls;
    std::optional<::media::ErrorInfo> writeFailure;
    MediaMuxSessionPollResult pollResult{false, std::nullopt};
};

class TraceMuxSession final : public MediaMuxSession {
public:
    explicit TraceMuxSession(std::shared_ptr<SessionTrace> trace)
        : m_trace(std::move(trace))
    {
    }

    ::media::Status bindResource(MediaGraphExecutionContext&, const MediaBufferRef&) override
    {
        m_trace->calls.emplace_back("resource");
        m_resourceBound = true;
        return ::media::Status::success();
    }

    ::media::Status bindStreamConfig(MediaGraphExecutionContext&, const MediaBufferRef&) override
    {
        m_trace->calls.emplace_back("config");
        m_configBound = true;
        return ::media::Status::success();
    }

    bool bindingsReady() const noexcept override
    {
        return m_resourceBound && m_configBound;
    }

    ::media::Status write(MediaGraphExecutionContext&, const MediaBufferRef&) override
    {
        m_trace->calls.emplace_back("write");
        return m_trace->writeFailure
            ? ::media::Status::failure(*m_trace->writeFailure)
            : ::media::Status::success();
    }

    ::media::Result<MediaMuxSessionPollResult> poll(MediaGraphExecutionContext&) override
    {
        m_trace->calls.emplace_back("poll");
        return ::media::Result<MediaMuxSessionPollResult>::success(m_trace->pollResult);
    }

    ::media::Status flush(MediaGraphExecutionContext&) override
    {
        m_trace->calls.emplace_back("flush");
        return ::media::Status::success();
    }

    ::media::Status finish(MediaGraphExecutionContext&) override
    {
        m_trace->calls.emplace_back("finish");
        return ::media::Status::success();
    }

    void abort() noexcept override
    {
        m_trace->calls.emplace_back("abort");
    }

private:
    std::shared_ptr<SessionTrace> m_trace;
    bool m_resourceBound = false;
    bool m_configBound = false;
};

class TraceMuxSessionFactory final : public MediaMuxSessionFactory {
public:
    explicit TraceMuxSessionFactory(std::shared_ptr<SessionTrace> trace)
        : m_trace(std::move(trace))
    {
    }

    ::media::Result<std::unique_ptr<MediaMuxSession>> create(
        const MediaNodeOptions&) const override
    {
        m_trace->calls.emplace_back("create");
        return ::media::Result<std::unique_ptr<MediaMuxSession>>::success(
            std::make_unique<TraceMuxSession>(m_trace));
    }

private:
    std::shared_ptr<SessionTrace> m_trace;
};

class TestMuxBuffer final : public MediaBuffer {
public:
    MediaBufferType type() const noexcept override { return MediaBufferType::Unknown; }
};

MediaBufferRef testBuffer(MediaStreamKind stream)
{
    auto buffer = makeMediaBufferRef<TestMuxBuffer>();
    buffer->setStreamKind(stream);
    return buffer;
}

struct FileMuxHarness final {
    MediaGraph graph;
    MediaNodeId mux;
    MediaGraphExecutionContext execution;
    std::unique_ptr<FileMuxNode> runtime;
    std::shared_ptr<SessionTrace> trace = std::make_shared<SessionTrace>();

    bool initialize(TestContext& ctx)
    {
        const auto resourceSource = graph.addNode(MediaNodeKind::DebugDump, "resource");
        const auto configSource = graph.addNode(MediaNodeKind::DebugDump, "config");
        const auto packetSource = graph.addNode(MediaNodeKind::DebugDump, "packet");
        mux = graph.addNode(MediaNodeKind::FileMux, "mux");
        graph.setNodeOption(mux, MediaTranscodeOptionKey::MuxSessionKind, "ffmpeg_file");
        graph.setNodeOption(mux, MediaTranscodeOptionKey::MuxExpectVideo, "1");
        graph.setNodeOption(mux, MediaTranscodeOptionKey::MuxExpectAudio, "0");
        graph.addOutputPort(resourceSource, "out", MediaStreamKind::Metadata,
                            MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
        graph.addOutputPort(configSource, "out", MediaStreamKind::Video,
                            MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
        graph.addOutputPort(packetSource, "out", MediaStreamKind::Video,
                            MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
        graph.addInputPort(mux, "resource", MediaStreamKind::Metadata,
                           MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
        graph.addInputPort(mux, "codec", MediaStreamKind::Any,
                           MediaEdgeKind::Metadata, MediaPayloadKind::Unknown, true, true);
        graph.addInputPort(mux, "packet", MediaStreamKind::Any,
                           MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown, true, true);
        const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
        graph.connect(resourceSource, "out", mux, "resource", "resource", policy);
        graph.connect(configSource, "out", mux, "codec", "config", policy);
        graph.connect(packetSource, "out", mux, "packet", "packet", policy);
        EXPECT_TRUE(ctx, execution.compile(graph));
        runtime = std::make_unique<FileMuxNode>(
            mux, std::make_unique<TraceMuxSessionFactory>(trace));
        EXPECT_TRUE(ctx, runtime->start(execution));
        return execution.compiled();
    }

    ::media::Status push(const char* port, MediaBufferRef buffer)
    {
        return execution.findInputChannel(mux, port)->push(std::move(buffer));
    }

    void close(const char* port)
    {
        execution.findInputChannel(mux, port)->close();
    }
};

bool bindRequiredInputs(FileMuxHarness& harness, TestContext& ctx)
{
    EXPECT_TRUE(ctx, harness.push(
        "resource", testBuffer(MediaStreamKind::Metadata)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.push(
        "codec", testBuffer(MediaStreamKind::Video)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    return harness.trace->calls.size() >= 3;
}

void testFactoryFailsClosed(TestContext& ctx)
{
    ExplicitMediaMuxSessionFactory factory;
    MediaNodeOptions options;
    EXPECT_FALSE(ctx, factory.create(options));
    options.set(MediaTranscodeOptionKey::MuxSessionKind, "unknown");
    EXPECT_FALSE(ctx, factory.create(options));
    options.set(MediaTranscodeOptionKey::MuxSessionKind, "project_mpegts");
    EXPECT_FALSE(ctx, factory.create(options));

    options.set(MediaTranscodeOptionKey::MuxSessionKind, "ffmpeg_file");
    EXPECT_FALSE(ctx, factory.create(options));
    options.set(MediaTranscodeOptionKey::MuxExpectVideo, "0");
    options.set(MediaTranscodeOptionKey::MuxExpectAudio, "0");
    EXPECT_FALSE(ctx, factory.create(options));
    options.set(MediaTranscodeOptionKey::MuxExpectVideo, "1");
    options.set(MediaTranscodeOptionKey::MuxExpectAudio, "0");
    EXPECT_TRUE(ctx, factory.create(options));
}

void expectDefaultNodeRejectsSessionKind(
    TestContext& ctx,
    const std::optional<std::string>& sessionKind)
{
    MediaGraph graph;
    const auto mux = graph.addNode(MediaNodeKind::FileMux, "mux.invalid.session.kind");
    if (sessionKind) {
        graph.setNodeOption(mux, MediaTranscodeOptionKey::MuxSessionKind, *sessionKind);
    }
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    FileMuxNode runtime(mux);
    EXPECT_TRUE(ctx, runtime.start(execution));
    EXPECT_FALSE(ctx, runtime.process(execution));
}

void testDefaultNodeFailsClosedForSessionKind(TestContext& ctx)
{
    expectDefaultNodeRejectsSessionKind(ctx, std::nullopt);
    expectDefaultNodeRejectsSessionKind(ctx, std::string("unknown"));
}

MediaBufferRef videoCodecParameters(MediaStreamKind stream = MediaStreamKind::Video)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->codec_id = AV_CODEC_ID_H264;
    parameters->width = 16;
    parameters->height = 16;
    auto buffer = makeMediaBufferRef<FFmpegCodecParametersBuffer>(std::move(parameters));
    buffer->setStreamKind(stream);
    MediaTimeDescriptor time;
    time.timeBase = {1, 90'000};
    buffer->setTimeDescriptor(time);
    return buffer;
}

MediaBufferRef audioCodecParameters()
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    parameters->codec_type = AVMEDIA_TYPE_AUDIO;
    parameters->codec_id = AV_CODEC_ID_AAC;
    parameters->sample_rate = 48'000;
    parameters->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    auto buffer = makeMediaBufferRef<FFmpegCodecParametersBuffer>(std::move(parameters));
    buffer->setStreamKind(MediaStreamKind::Audio);
    MediaTimeDescriptor time;
    time.timeBase = {1, 48'000};
    buffer->setTimeDescriptor(time);
    return buffer;
}

void testFfmpegSessionMovesExistingLifecycle(TestContext& ctx)
{
    AVFormatContext* raw = nullptr;
    EXPECT_EQ(ctx, avformat_alloc_output_context2(&raw, nullptr, "null", nullptr), 0);
    EXPECT_TRUE(ctx, raw != nullptr);
    if (!raw) return;
    ::media::ffmpeg::OutputFormatContextPtr owner(raw);
    auto resource = FFmpegBufferFactory::wrapOutputFormatContext(std::move(owner));
    EXPECT_TRUE(ctx, resource);
    if (!resource) return;

    FFmpegFileMuxSession session(true, false);
    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, session.bindStreamConfig(execution, videoCodecParameters()));
    EXPECT_TRUE(ctx, session.bindResource(execution, resource.value()));

    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    EXPECT_EQ(ctx, av_new_packet(packet.get(), 4), 0);
    packet->pts = 0;
    packet->dts = 0;
    packet->duration = 3'000;
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(packet), MediaStreamKind::Video, std::nullopt);
    EXPECT_TRUE(ctx, wrapped);
    if (!wrapped) return;
    MediaTimeDescriptor packetTime;
    packetTime.timeBase = {1, 90'000};
    wrapped.value()->setTimeDescriptor(packetTime);
    EXPECT_TRUE(ctx, session.write(execution, wrapped.value()));
    EXPECT_TRUE(ctx, session.flush(execution));
    EXPECT_TRUE(ctx, session.finish(execution));
    EXPECT_TRUE(ctx, session.finish(execution));
}

void testFfmpegSessionRejectsInvalidKindBeforeContextMutation(TestContext& ctx)
{
    AVFormatContext* raw = nullptr;
    EXPECT_EQ(ctx, avformat_alloc_output_context2(&raw, nullptr, "null", nullptr), 0);
    EXPECT_TRUE(ctx, raw != nullptr);
    if (!raw) return;
    FFmpegFileMuxSession session(true, false);
    MediaGraphExecutionContext execution;
    auto borrowed = FFmpegBufferFactory::borrowFormatContext(raw);
    EXPECT_TRUE(ctx, borrowed);
    if (!borrowed) {
        avformat_free_context(raw);
        return;
    }
    EXPECT_TRUE(ctx, session.bindResource(execution, borrowed.value()));
    auto invalid = session.bindStreamConfig(
        execution, videoCodecParameters(MediaStreamKind::Metadata));
    EXPECT_FALSE(ctx, invalid);
    EXPECT_EQ(ctx, raw->nb_streams, 0u);
    session.abort();
    avformat_free_context(raw);
}

void testFfmpegSessionRejectsUnplannedStreamBeforeContextMutation(TestContext& ctx)
{
    AVFormatContext* raw = nullptr;
    EXPECT_EQ(ctx, avformat_alloc_output_context2(&raw, nullptr, "null", nullptr), 0);
    EXPECT_TRUE(ctx, raw != nullptr);
    if (!raw) return;
    FFmpegFileMuxSession session(true, false);
    MediaGraphExecutionContext execution;
    auto borrowed = FFmpegBufferFactory::borrowFormatContext(raw);
    EXPECT_TRUE(ctx, borrowed);
    if (!borrowed) {
        avformat_free_context(raw);
        return;
    }
    EXPECT_TRUE(ctx, session.bindResource(execution, borrowed.value()));
    EXPECT_FALSE(ctx, session.bindStreamConfig(execution, audioCodecParameters()));
    EXPECT_EQ(ctx, raw->nb_streams, 0u);
    session.abort();
    avformat_free_context(raw);
}

void testNodeDelegatesLifecycleInOrder(TestContext& ctx)
{
    FileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push("resource", testBuffer(MediaStreamKind::Metadata)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.push("codec", testBuffer(MediaStreamKind::Video)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    EXPECT_TRUE(ctx, harness.push("packet", testBuffer(MediaStreamKind::Video)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    auto flush = FFmpegBufferFactory::makeFlush(MediaStreamKind::Control);
    EXPECT_TRUE(ctx, flush);
    if (flush) EXPECT_TRUE(ctx, harness.push("packet", std::move(flush).value()));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    harness.close("resource");
    harness.close("codec");
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    EXPECT_TRUE(ctx, eof);
    if (eof) EXPECT_TRUE(ctx, harness.push("packet", std::move(eof).value()));
    auto finished = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, finished);
    if (finished) EXPECT_EQ(ctx, finished.value().state, MediaNodeProcessState::Finished);
    const std::vector<std::string> expected{
        "create", "resource", "config", "write", "flush", "finish"};
    EXPECT_EQ(ctx, harness.trace->calls, expected);
}

void testNodeMapsPollResult(TestContext& ctx)
{
    FileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    if (!bindRequiredInputs(harness, ctx)) return;
    const MediaAvSyncGroupKey group("mux-group");
    const MediaRunningTime deadline = MediaRunningTime::fromNanoseconds(1234);
    harness.trace->pollResult = {
        true, MediaNodeProcessResult::DeadlineWait{group, deadline}};
    auto progress = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, progress);
    if (progress) EXPECT_EQ(ctx, progress.value().state, MediaNodeProcessState::Progress);

    harness.trace->pollResult = {false, MediaNodeProcessResult::DeadlineWait{group, deadline}};
    auto waiting = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, waiting);
    if (waiting) {
        EXPECT_EQ(ctx, waiting.value().state, MediaNodeProcessState::Waiting);
        EXPECT_TRUE(ctx, waiting.value().deadlineWait.has_value());
        if (waiting.value().deadlineWait) {
            EXPECT_EQ(ctx, waiting.value().deadlineWait->syncGroup, group);
            EXPECT_EQ(ctx, waiting.value().deadlineWait->masterDeadline, deadline);
        }
    }
}

void testNodeFinishesOnPacketEofWhileBindingsRemainOpen(TestContext& ctx)
{
    FileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    if (!bindRequiredInputs(harness, ctx)) return;
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    EXPECT_TRUE(ctx, harness.push("packet", std::move(eof).value()));
    auto finished = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, finished);
    if (finished) {
        EXPECT_EQ(ctx, finished.value().state, MediaNodeProcessState::Finished);
    }
    EXPECT_FALSE(ctx, harness.execution.findInputChannel(
                          harness.mux, "resource")->closed());
    EXPECT_FALSE(ctx, harness.execution.findInputChannel(
                          harness.mux, "codec")->closed());
    EXPECT_EQ(ctx, harness.trace->calls.back(), std::string("finish"));
}

void testNodePreservesFirstFailureAndAbortsOnce(TestContext& ctx)
{
    FileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    if (!bindRequiredInputs(harness, ctx)) return;
    harness.trace->writeFailure = ::media::ErrorInfo::ioFailure("first write failure");
    EXPECT_TRUE(ctx, harness.push("packet", testBuffer(MediaStreamKind::Video)));
    auto first = harness.runtime->process(harness.execution);
    EXPECT_FALSE(ctx, first);
    EXPECT_TRUE(ctx, harness.push("packet", testBuffer(MediaStreamKind::Video)));
    auto repeated = harness.runtime->process(harness.execution);
    EXPECT_FALSE(ctx, repeated);
    if (!repeated) EXPECT_EQ(ctx, repeated.error().message, std::string("first write failure"));
    harness.runtime->abort(harness.execution);
    harness.runtime->abort(harness.execution);
    std::size_t aborts = 0;
    for (const auto& call : harness.trace->calls) if (call == "abort") ++aborts;
    EXPECT_EQ(ctx, aborts, static_cast<std::size_t>(1));
}

void testNodeRejectsBindingEofBeforePayload(TestContext& ctx)
{
    FileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    EXPECT_TRUE(ctx, harness.push("resource", std::move(eof).value()));
    auto result = harness.runtime->process(harness.execution);
    EXPECT_FALSE(ctx, result);
    if (!result) {
        EXPECT_TRUE(ctx, result.error().message.find(
                             "resource binding channel reached EOF") !=
                             std::string::npos);
    }
}

void testNodeRejectsClosedBindingBeforePayload(TestContext& ctx)
{
    FileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    harness.close("codec");
    auto result = harness.runtime->process(harness.execution);
    EXPECT_FALSE(ctx, result);
    if (!result) {
        EXPECT_TRUE(ctx, result.error().message.find(
                             "codec binding channel closed") !=
                             std::string::npos);
    }
}

void testNodeAcceptsBindingCloseAfterSuccessfulPayload(TestContext& ctx)
{
    FileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push(
        "resource", testBuffer(MediaStreamKind::Metadata)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    harness.close("resource");
    auto waiting = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, waiting);
    if (waiting) {
        EXPECT_EQ(ctx, waiting.value().state, MediaNodeProcessState::Waiting);
    }
    EXPECT_TRUE(ctx, harness.push(
        "codec", testBuffer(MediaStreamKind::Video)));
    EXPECT_TRUE(ctx, harness.runtime->process(harness.execution));
    const std::vector<std::string> expected{"create", "resource", "config"};
    EXPECT_EQ(ctx, harness.trace->calls, expected);
}

} // namespace

void runFileMuxSessionTests(TestContext& ctx)
{
    testFactoryFailsClosed(ctx);
    testDefaultNodeFailsClosedForSessionKind(ctx);
    testFfmpegSessionMovesExistingLifecycle(ctx);
    testFfmpegSessionRejectsInvalidKindBeforeContextMutation(ctx);
    testFfmpegSessionRejectsUnplannedStreamBeforeContextMutation(ctx);
    testNodeDelegatesLifecycleInOrder(ctx);
    testNodeMapsPollResult(ctx);
    testNodeFinishesOnPacketEofWhileBindingsRemainOpen(ctx);
    testNodePreservesFirstFailureAndAbortsOnce(ctx);
    testNodeRejectsBindingEofBeforePayload(ctx);
    testNodeRejectsClosedBindingBeforePayload(ctx);
    testNodeAcceptsBindingCloseAfterSuccessfulPayload(ctx);
}
