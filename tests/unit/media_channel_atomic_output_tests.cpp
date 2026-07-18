#include "common/GraphRuntimeTestSupport.h"
#include "common/TestAssert.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace {

using media_transcode::test::TestContext;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

template <typename Predicate>
bool waitUntil(Predicate&& predicate)
{
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(5);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

struct ChannelFixture final {
    MediaGraph graph;
    MediaNodeId source;
    MediaNodeId videoSink;
    MediaNodeId audioSink;
    MediaGraphExecutionContext execution;
};

std::unique_ptr<ChannelFixture> makeFixture(
    TestContext& ctx,
    MediaEdgePolicy videoPolicy,
    MediaEdgePolicy audioPolicy)
{
    auto fixture = std::make_unique<ChannelFixture>();
    fixture->source = fixture->graph.addNode(MediaNodeKind::DebugDump, "source");
    fixture->videoSink = fixture->graph.addNode(
        MediaNodeKind::DebugDump, "video-sink");
    fixture->audioSink = fixture->graph.addNode(
        MediaNodeKind::DebugDump, "audio-sink");
    fixture->graph.addOutputPort(
        fixture->source, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture->graph.addOutputPort(
        fixture->source, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture->graph.addInputPort(
        fixture->videoSink, "in", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture->graph.addInputPort(
        fixture->audioSink, "in", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    fixture->graph.connect(
        fixture->source, "video", fixture->videoSink, "in", "video",
        videoPolicy);
    fixture->graph.connect(
        fixture->source, "audio", fixture->audioSink, "in", "audio",
        audioPolicy);
    EXPECT_TRUE(ctx, fixture->execution.compile(fixture->graph));
    return fixture;
}

MediaChannel* video(ChannelFixture& fixture)
{
    return fixture.execution.findInputChannel(fixture.videoSink, "in");
}

MediaChannel* audio(ChannelFixture& fixture)
{
    return fixture.execution.findInputChannel(fixture.audioSink, "in");
}

void testTransactionOwnsReferencesAndSerializesLifecycle(TestContext& ctx)
{
    const auto verify = [&](bool abortOutput) {
        auto fixture = makeFixture(
            ctx, MediaGraphBuildSupport::blockingQueuePolicy(2),
            MediaGraphBuildSupport::blockingQueuePolicy(2));
        auto terminal = makeMediaBufferRef<MediaControlBuffer>(
            MediaControlBufferKind::Eof);
        MediaAtomicOutputTransaction::AcquireResult acquired = [&] {
            std::array<MediaBufferRef, 1> videoValues{terminal};
            std::array<MediaBufferRef, 1> audioValues{terminal};
            const std::array<MediaAtomicOutputBatch, 2> batches{
                MediaAtomicOutputBatch{video(*fixture), videoValues},
                MediaAtomicOutputBatch{audio(*fixture), audioValues}};
            return MediaAtomicOutputTransaction::acquire(
                "Atomic output lifecycle race", batches);
        }();
        EXPECT_TRUE(ctx, acquired && acquired.value().has_value());
        if (!acquired || !acquired.value()) return;
        terminal.reset();

        std::barrier boundary(2);
        std::atomic<bool> attempting{false};
        std::thread lifecycle([&] {
            boundary.arrive_and_wait();
            attempting.store(true, std::memory_order_release);
            if (abortOutput) audio(*fixture)->abort();
            else audio(*fixture)->close();
        });
        boundary.arrive_and_wait();
        EXPECT_TRUE(ctx, waitUntil([&] {
            return attempting.load(std::memory_order_acquire);
        }));
        EXPECT_TRUE(ctx, acquired.value()->commit());
        acquired.value().reset();
        lifecycle.join();
        EXPECT_EQ(ctx, video(*fixture)->size(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, audio(*fixture)->size(), static_cast<std::size_t>(1));
        MediaBufferRef retainedVideo;
        MediaBufferRef retainedAudio;
        EXPECT_TRUE(ctx, video(*fixture)->tryPop(retainedVideo));
        EXPECT_TRUE(ctx, audio(*fixture)->tryPop(retainedAudio));
        EXPECT_TRUE(ctx, retainedVideo && retainedVideo == retainedAudio);

        auto before = makeFixture(
            ctx, MediaGraphBuildSupport::blockingQueuePolicy(2),
            MediaGraphBuildSupport::blockingQueuePolicy(2));
        if (abortOutput) audio(*before)->abort();
        else audio(*before)->close();
        auto beforeTerminal = makeMediaBufferRef<MediaControlBuffer>(
            MediaControlBufferKind::Eof);
        std::array<MediaBufferRef, 1> beforeVideo{beforeTerminal};
        std::array<MediaBufferRef, 1> beforeAudio{beforeTerminal};
        const std::array<MediaAtomicOutputBatch, 2> beforeBatches{
            MediaAtomicOutputBatch{video(*before), beforeVideo},
            MediaAtomicOutputBatch{audio(*before), beforeAudio}};
        EXPECT_FALSE(ctx, MediaAtomicOutputTransaction::acquire(
                              "Lifecycle before atomic output", beforeBatches));
        EXPECT_EQ(ctx, video(*before)->size(), static_cast<std::size_t>(0));
        EXPECT_EQ(ctx, audio(*before)->size(), static_cast<std::size_t>(0));
    };
    verify(false);
    verify(true);
}

void testConsumersCannotObservePartialAtomicCommit(TestContext& ctx)
{
    auto fixture = makeFixture(
        ctx, MediaGraphBuildSupport::blockingQueuePolicy(2),
        MediaGraphBuildSupport::blockingQueuePolicy(2));
    auto terminal = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Eof);
    std::array<MediaBufferRef, 1> videoValues{terminal};
    std::array<MediaBufferRef, 1> audioValues{terminal};
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{video(*fixture), videoValues},
        MediaAtomicOutputBatch{audio(*fixture), audioValues}};
    auto acquired = MediaAtomicOutputTransaction::acquire(
        "Atomic consumer visibility", batches);
    EXPECT_TRUE(ctx, acquired && acquired.value().has_value());
    if (!acquired || !acquired.value()) return;

    std::barrier boundary(3);
    std::atomic<int> attempting{0};
    bool videoVisible = false;
    bool audioVisible = false;
    MediaBufferRef consumedVideo;
    MediaBufferRef consumedAudio;
    std::thread videoConsumer([&] {
        boundary.arrive_and_wait();
        attempting.fetch_add(1, std::memory_order_release);
        videoVisible = video(*fixture)->tryPop(consumedVideo);
    });
    std::thread audioConsumer([&] {
        boundary.arrive_and_wait();
        attempting.fetch_add(1, std::memory_order_release);
        audioVisible = audio(*fixture)->tryPop(consumedAudio);
    });
    boundary.arrive_and_wait();
    EXPECT_TRUE(ctx, waitUntil([&] {
        return attempting.load(std::memory_order_acquire) == 2;
    }));
    EXPECT_TRUE(ctx, acquired.value()->commit());
    acquired.value().reset();
    videoConsumer.join();
    audioConsumer.join();
    EXPECT_TRUE(ctx, videoVisible && audioVisible);
    EXPECT_TRUE(ctx, consumedVideo == terminal && consumedAudio == terminal);
    EXPECT_EQ(ctx, video(*fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, audio(*fixture)->size(), static_cast<std::size_t>(0));
}

void testBlockingConsumersReturnOnlyAfterAtomicPublish(TestContext& ctx)
{
    auto fixture = makeFixture(
        ctx, MediaGraphBuildSupport::blockingQueuePolicy(2),
        MediaGraphBuildSupport::blockingQueuePolicy(2));
    auto terminal = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Eof);
    ::media::Status videoStatus = ::media::Status::failure(
        ::media::ErrorInfo::internalError("video pop did not run"));
    ::media::Status audioStatus = ::media::Status::failure(
        ::media::ErrorInfo::internalError("audio pop did not run"));
    MediaBufferRef consumedVideo;
    MediaBufferRef consumedAudio;
    std::thread videoConsumer([&] {
        videoStatus = video(*fixture)->pop(consumedVideo);
    });
    std::thread audioConsumer([&] {
        audioStatus = audio(*fixture)->pop(consumedAudio);
    });
    EXPECT_TRUE(ctx, waitUntil([&] {
        return video(*fixture)->metrics().queue.blockedConsumers.load() == 1 &&
            audio(*fixture)->metrics().queue.blockedConsumers.load() == 1;
    }));
    std::array<MediaBufferRef, 1> videoValues{terminal};
    std::array<MediaBufferRef, 1> audioValues{terminal};
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{video(*fixture), videoValues},
        MediaAtomicOutputBatch{audio(*fixture), audioValues}};
    auto acquired = MediaAtomicOutputTransaction::acquire(
        "Blocking atomic consumer visibility", batches);
    EXPECT_TRUE(ctx, acquired && acquired.value().has_value());
    if (!acquired || !acquired.value()) {
        video(*fixture)->abort();
        audio(*fixture)->abort();
        videoConsumer.join();
        audioConsumer.join();
        return;
    }
    EXPECT_TRUE(ctx, acquired.value()->commit());
    acquired.value().reset();
    videoConsumer.join();
    audioConsumer.join();
    EXPECT_TRUE(ctx, videoStatus && audioStatus);
    EXPECT_TRUE(ctx, consumedVideo == terminal && consumedAudio == terminal);
}

void testChannelPushPreservesNullDropAndLifecycleContracts(TestContext& ctx)
{
    auto dropPolicy = MediaGraphBuildSupport::blockingQueuePolicy(1);
    dropPolicy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::DropNewest;
    auto dropFixture = makeFixture(ctx, dropPolicy, dropPolicy);
    MediaChannel* channel = video(*dropFixture);
    auto nullStatus = channel->push({});
    EXPECT_FALSE(ctx, nullStatus);
    if (!nullStatus) {
        EXPECT_EQ(ctx, nullStatus.error().code,
                  ::media::ErrorCode::InvalidArgument);
    }
    auto first = makePacketBuffer(true, 1, MediaStreamKind::Video);
    auto second = makePacketBuffer(true, 2, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, first && second);
    if (!first || !second) return;
    EXPECT_TRUE(ctx, channel->push(first.value()));
    EXPECT_TRUE(ctx, channel->push(second.value()));
    EXPECT_EQ(ctx, channel->metrics().pushed.load(), static_cast<std::uint64_t>(2));
    EXPECT_EQ(ctx, channel->metrics().queue.dropped.load(),
              static_cast<std::uint64_t>(1));

    const auto verifyRelease = [&](bool abortChannel) {
        auto fixture = makeFixture(
            ctx, MediaGraphBuildSupport::blockingQueuePolicy(1),
            MediaGraphBuildSupport::blockingQueuePolicy(1));
        MediaChannel* blockedChannel = video(*fixture);
        EXPECT_TRUE(ctx, blockedChannel->push(first.value()));
        std::barrier boundary(2);
        std::atomic<bool> attempting{false};
        ::media::Status result = ::media::Status::success();
        std::thread producer([&] {
            boundary.arrive_and_wait();
            attempting.store(true, std::memory_order_release);
            result = blockedChannel->push(second.value());
        });
        boundary.arrive_and_wait();
        EXPECT_TRUE(ctx, waitUntil([&] {
            return attempting.load(std::memory_order_acquire);
        }));
        const bool enteredWait = waitUntil([&] {
            return blockedChannel->metrics().queue.blockedProducers.load() == 1;
        });
        EXPECT_TRUE(ctx, enteredWait);
        const auto blockedAttempts =
            blockedChannel->metrics().queue.blockedPushes.load();
        EXPECT_EQ(ctx,
                  blockedChannel->metrics().queue.blockedProducers.load(),
                  static_cast<std::size_t>(1));
        EXPECT_TRUE(ctx,
                    blockedChannel->metrics().queue.blockedPushes.load() >= 1);
        for (int yield = 0; yield < 1'000; ++yield) {
            std::this_thread::yield();
        }
        EXPECT_EQ(ctx,
                  blockedChannel->metrics().queue.blockedPushes.load(),
                  blockedAttempts);
        if (abortChannel) blockedChannel->abort();
        else blockedChannel->close();
        producer.join();
        EXPECT_EQ(ctx,
                  blockedChannel->metrics().queue.blockedProducers.load(),
                  static_cast<std::size_t>(0));
        EXPECT_FALSE(ctx, result);
        if (!result) {
            EXPECT_EQ(ctx, result.error().code,
                      abortChannel ? ::media::ErrorCode::InternalError
                                   : ::media::ErrorCode::Cancelled);
        }
    };
    verifyRelease(false);
    verifyRelease(true);

    auto consumerFixture = makeFixture(
        ctx, MediaGraphBuildSupport::blockingQueuePolicy(1),
        MediaGraphBuildSupport::blockingQueuePolicy(1));
    MediaChannel* emptyChannel = video(*consumerFixture);
    std::atomic<bool> consumerAttempting{false};
    MediaBufferRef consumed;
    ::media::Status consumerStatus = ::media::Status::failure(
        ::media::ErrorInfo::internalError("consumer pop did not run"));
    std::thread consumer([&] {
        consumerAttempting.store(true, std::memory_order_release);
        consumerStatus = emptyChannel->pop(consumed);
    });
    EXPECT_TRUE(ctx, waitUntil([&] {
        return consumerAttempting.load(std::memory_order_acquire) &&
            emptyChannel->metrics().queue.blockedConsumers.load() == 1;
    }));
    EXPECT_EQ(ctx, emptyChannel->metrics().queue.blockedConsumers.load(),
              static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, emptyChannel->push(first.value()));
    consumer.join();
    EXPECT_TRUE(ctx, consumerStatus && consumed == first.value());
    EXPECT_EQ(ctx, emptyChannel->metrics().queue.blockedConsumers.load(),
              static_cast<std::size_t>(0));
}

} // namespace

int main()
{
    TestContext ctx;
    testTransactionOwnsReferencesAndSerializesLifecycle(ctx);
    testConsumersCannotObservePartialAtomicCommit(ctx);
    testBlockingConsumersReturnOnlyAfterAtomicPublish(ctx);
    testChannelPushPreservesNullDropAndLifecycleContracts(ctx);
    if (ctx.failures != 0) return 1;
    std::cout << "Media channel atomic output tests passed\n";
    return 0;
}
