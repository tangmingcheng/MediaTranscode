#include "unit/fixtures/ScheduledRtpOutputNodeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace media_transcode::test::scheduled_rtp_output {

using namespace media::ffmpeg::graph;

namespace {

struct AtomicReplaceState final {
    int beginCalls = 0;
    int writeCalls = 0;
    int flushCalls = 0;
    int replaceCalls = 0;
    bool failWrite = false;
    bool failFlush = false;
    bool failReplace = false;
    std::string target;
    std::string content;
    std::string stagedContent;
};

class FakeAtomicReplaceTransaction final
    : public MediaAtomicFileReplaceTransaction {
public:
    explicit FakeAtomicReplaceTransaction(
        std::shared_ptr<AtomicReplaceState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Status writeAll(std::span<const std::uint8_t> bytes) override
    {
        ++m_state->writeCalls;
        if (m_state->failWrite) {
            return ::media::Status::failure(
                ::media::ErrorInfo::ioFailure(
                    "scripted atomic write", -1));
        }
        m_state->stagedContent.assign(
            reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return ::media::Status::success();
    }

    ::media::Status flushAndClose() override
    {
        ++m_state->flushCalls;
        if (m_state->failFlush) {
            return ::media::Status::failure(
                ::media::ErrorInfo::ioFailure(
                    "scripted atomic flush", -2));
        }
        return ::media::Status::success();
    }

    ::media::Status replaceTarget() override
    {
        ++m_state->replaceCalls;
        if (m_state->failReplace) {
            return ::media::Status::failure(
                ::media::ErrorInfo::ioFailure(
                    "scripted atomic replace", -3));
        }
        m_state->content = m_state->stagedContent;
        return ::media::Status::success();
    }

private:
    std::shared_ptr<AtomicReplaceState> m_state;
};

class FakeAtomicReplacePort final : public MediaAtomicFileReplacePort {
public:
    explicit FakeAtomicReplacePort(std::shared_ptr<AtomicReplaceState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>> begin(
        std::string_view targetPathUtf8) override
    {
        ++m_state->beginCalls;
        m_state->target = targetPathUtf8;
        m_state->stagedContent.clear();
        std::unique_ptr<MediaAtomicFileReplaceTransaction> transaction =
            std::make_unique<FakeAtomicReplaceTransaction>(m_state);
        return ::media::Result<
            std::unique_ptr<MediaAtomicFileReplaceTransaction>>::success(
            std::move(transaction));
    }

private:
    std::shared_ptr<AtomicReplaceState> m_state;
};

struct PublisherFixture final {
    MediaGraph graph;
    MediaNodeId videoSource;
    MediaNodeId audioSource;
    MediaNodeId publisher;
    MediaGraphExecutionContext execution;
    std::shared_ptr<AtomicReplaceState> state;
    std::unique_ptr<MediaDualMediaSdpPublisherNode> node;
};

std::unique_ptr<PublisherFixture> publisherFixture(
    TestContext& ctx,
    std::shared_ptr<AtomicReplaceState> state,
    std::size_t capacity)
{
    auto fixture = std::make_unique<PublisherFixture>();
    fixture->videoSource = fixture->graph.addNode(
        MediaNodeKind::DebugDump, "video");
    fixture->audioSource = fixture->graph.addNode(
        MediaNodeKind::DebugDump, "audio");
    fixture->publisher = fixture->graph.addNode(
        MediaNodeKind::DualMediaSdpPublisher, "publisher");
    fixture->graph.addOutputPort(
        fixture->videoSource, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    fixture->graph.addOutputPort(
        fixture->audioSource, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    fixture->graph.addInputPort(
        fixture->publisher, "video", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    fixture->graph.addInputPort(
        fixture->publisher, "audio", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(capacity);
    fixture->graph.connect(
        fixture->videoSource, "description", fixture->publisher,
        "video", "video", policy);
    fixture->graph.connect(
        fixture->audioSource, "description", fixture->publisher,
        "audio", "audio", policy);
    EXPECT_TRUE(ctx, fixture->execution.compile(fixture->graph));
    fixture->state = std::move(state);
    auto created = MediaDualMediaSdpPublisherNode::create(
        fixture->publisher, "task8.sdp",
        std::make_unique<FakeAtomicReplacePort>(fixture->state));
    EXPECT_TRUE(ctx, created);
    if (!created) return {};
    fixture->node = std::move(created).value();
    EXPECT_TRUE(ctx, fixture->node->start(fixture->execution));
    return fixture;
}

bool pushDescriptions(
    TestContext& ctx,
    PublisherFixture& fixture,
    std::uint64_t generation = 1,
    std::uint64_t sessionVersion = 1)
{
    auto video = description(
        MediaScheduledStream::Video, generation, sessionVersion);
    auto audio = description(
        MediaScheduledStream::Audio, generation, sessionVersion);
    EXPECT_TRUE(ctx, video && audio);
    if (!video || !audio) return false;
    EXPECT_TRUE(ctx, fixture.execution.findInputChannel(
                         fixture.publisher, "video")
                         ->push(std::move(video).value()));
    EXPECT_TRUE(ctx, fixture.execution.findInputChannel(
                         fixture.publisher, "audio")
                         ->push(std::move(audio).value()));
    return true;
}

void testPublisherRejectsQueuedDuplicateBeforeAtomicReplace(TestContext& ctx)
{
    auto state = std::make_shared<AtomicReplaceState>();
    auto fixture = publisherFixture(ctx, state, 4);
    if (!fixture) return;
    auto video = description(MediaScheduledStream::Video);
    auto duplicate = description(MediaScheduledStream::Video);
    auto audio = description(MediaScheduledStream::Audio);
    EXPECT_TRUE(ctx, video && duplicate && audio);
    if (!video || !duplicate || !audio) return;
    auto* videoInput = fixture->execution.findInputChannel(
        fixture->publisher, "video");
    auto* audioInput = fixture->execution.findInputChannel(
        fixture->publisher, "audio");
    EXPECT_TRUE(ctx, videoInput->push(std::move(video).value()));
    EXPECT_TRUE(ctx, videoInput->push(std::move(duplicate).value()));
    EXPECT_TRUE(ctx, audioInput->push(std::move(audio).value()));
    EXPECT_FALSE(ctx, fixture->node->process(fixture->execution));
    EXPECT_EQ(ctx, state->beginCalls, 0);
    EXPECT_EQ(ctx, state->replaceCalls, 0);
    fixture->node->abort(fixture->execution);
}

void testPublisherCommitsCompleteSdpAndRepublishesNextGeneration(
    TestContext& ctx)
{
    auto state = std::make_shared<AtomicReplaceState>();
    auto fixture = publisherFixture(ctx, state, 2);
    if (!fixture || !pushDescriptions(ctx, *fixture)) return;
    auto published = fixture->node->process(fixture->execution);
    EXPECT_TRUE(ctx, published &&
                         published.value().state ==
                             MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, state->beginCalls, 1);
    EXPECT_EQ(ctx, state->writeCalls, 1);
    EXPECT_EQ(ctx, state->flushCalls, 1);
    EXPECT_EQ(ctx, state->replaceCalls, 1);
    EXPECT_EQ(ctx, state->target, std::string("task8.sdp"));
    EXPECT_TRUE(ctx, state->content.find(
                         "m=video 6000 RTP/AVP 96\r\n") !=
                         std::string::npos);
    EXPECT_TRUE(ctx, state->content.find(
                         "m=audio 6002 RTP/AVP 97\r\n") !=
                         std::string::npos);
    EXPECT_TRUE(ctx, state->content.find(
                         "cname:task8@example\r\n") !=
                         std::string::npos);
    EXPECT_TRUE(ctx, fixture->node->stop(fixture->execution));
    EXPECT_TRUE(ctx, fixture->node->start(fixture->execution));
    if (!pushDescriptions(ctx, *fixture, 2, 2)) return;
    auto republished = fixture->node->process(fixture->execution);
    EXPECT_TRUE(ctx, republished &&
                         republished.value().state ==
                             MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, state->beginCalls, 2);
    EXPECT_EQ(ctx, state->replaceCalls, 2);
    EXPECT_TRUE(ctx, state->content.find(
                         " 2 IN IP4 127.0.0.1\r\n") !=
                         std::string::npos);
    EXPECT_TRUE(ctx, fixture->node->stop(fixture->execution));
}

void testPublisherPreservesPreviousSdpAcrossAtomicStageFailures(
    TestContext& ctx)
{
    for (int failureStage = 0; failureStage != 3; ++failureStage) {
        auto state = std::make_shared<AtomicReplaceState>();
        state->content = "previous-generation-sdp";
        state->failWrite = failureStage == 0;
        state->failFlush = failureStage == 1;
        state->failReplace = failureStage == 2;
        auto fixture = publisherFixture(ctx, state, 2);
        if (!fixture || !pushDescriptions(ctx, *fixture)) continue;
        EXPECT_FALSE(ctx, fixture->node->process(fixture->execution));
        EXPECT_EQ(ctx, state->content,
                  std::string("previous-generation-sdp"));
        EXPECT_EQ(ctx, state->writeCalls, 1);
        EXPECT_EQ(ctx, state->flushCalls, failureStage == 0 ? 0 : 1);
        EXPECT_EQ(ctx, state->replaceCalls, failureStage == 2 ? 1 : 0);
        fixture->node->abort(fixture->execution);
    }
}

void testPublisherFlushDiscardsPartialGeneration(TestContext& ctx)
{
    auto state = std::make_shared<AtomicReplaceState>();
    auto fixture = publisherFixture(ctx, state, 2);
    if (!fixture) return;
    auto partial = description(MediaScheduledStream::Video, 1, 1);
    EXPECT_TRUE(ctx, partial);
    if (!partial) return;
    EXPECT_TRUE(ctx, fixture->execution.findInputChannel(
                         fixture->publisher, "video")
                         ->push(std::move(partial).value()));
    auto retained = fixture->node->process(fixture->execution);
    EXPECT_TRUE(ctx, retained &&
                         retained.value().state ==
                             MediaNodeProcessState::Progress);
    EXPECT_EQ(ctx, state->beginCalls, 0);

    EXPECT_TRUE(ctx, fixture->node->flush(fixture->execution));
    if (!pushDescriptions(ctx, *fixture, 2, 2)) return;
    auto published = fixture->node->process(fixture->execution);
    EXPECT_TRUE(ctx, published &&
                         published.value().state ==
                             MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, state->beginCalls, 1);
    EXPECT_TRUE(ctx, state->content.find(
                         " 2 IN IP4 127.0.0.1\r\n") !=
                         std::string::npos);
    EXPECT_TRUE(ctx, fixture->node->stop(fixture->execution));
}

void testPublisherFlushStartsGenerationAfterFinished(TestContext& ctx)
{
    auto state = std::make_shared<AtomicReplaceState>();
    auto fixture = publisherFixture(ctx, state, 2);
    if (!fixture || !pushDescriptions(ctx, *fixture, 1, 1)) return;
    auto first = fixture->node->process(fixture->execution);
    EXPECT_TRUE(ctx, first &&
                         first.value().state ==
                             MediaNodeProcessState::Finished);

    EXPECT_TRUE(ctx, fixture->node->flush(fixture->execution));
    if (!pushDescriptions(ctx, *fixture, 2, 2)) return;
    auto second = fixture->node->process(fixture->execution);
    EXPECT_TRUE(ctx, second &&
                         second.value().state ==
                             MediaNodeProcessState::Finished);
    EXPECT_EQ(ctx, state->beginCalls, 2);
    EXPECT_EQ(ctx, state->replaceCalls, 2);
    EXPECT_TRUE(ctx, state->content.find(
                         " 2 IN IP4 127.0.0.1\r\n") !=
                         std::string::npos);
    EXPECT_TRUE(ctx, fixture->node->stop(fixture->execution));
}

} // namespace

void runDualMediaSdpPublisherTests(TestContext& ctx)
{
    testPublisherRejectsQueuedDuplicateBeforeAtomicReplace(ctx);
    testPublisherCommitsCompleteSdpAndRepublishesNextGeneration(ctx);
    testPublisherPreservesPreviousSdpAcrossAtomicStageFailures(ctx);
    testPublisherFlushDiscardsPartialGeneration(ctx);
    testPublisherFlushStartsGenerationAfterFinished(ctx);
}

} // namespace media_transcode::test::scheduled_rtp_output
