#include "common/GraphRuntimeTestSupport.h"
#include "common/TestAssert.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/channel/MediaReservedOutputTransaction.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/queue/MediaBlockingQueueStorage.h"

#include <array>
#include <atomic>
#include <barrier>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>

namespace {
thread_local bool g_failCurrentThreadAllocations = false;
}

void* operator new(std::size_t size)
{
    if (g_failCurrentThreadAllocations) throw std::bad_alloc();
    if (void* allocation = std::malloc(size)) return allocation;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* allocation) noexcept
{
    std::free(allocation);
}

void operator delete[](void* allocation) noexcept
{
    std::free(allocation);
}

void operator delete(void* allocation, std::size_t) noexcept
{
    std::free(allocation);
}

void operator delete[](void* allocation, std::size_t) noexcept
{
    std::free(allocation);
}

namespace media::ffmpeg::graph {

struct MediaChannelAtomicOutputTestAccess final {
    static bool waitForBlockedProducer(MediaChannel& channel)
    {
        while (channel.m_externalBlockedProducers.load(
                   std::memory_order_acquire) == 0) {
            channel.m_externalBlockedProducers.wait(
                0, std::memory_order_acquire);
        }
        return true;
    }

    static bool waitForBlockedConsumer(MediaChannel& channel)
    {
        while (channel.m_externalBlockedConsumers.load(
                   std::memory_order_acquire) == 0) {
            channel.m_externalBlockedConsumers.wait(
                0, std::memory_order_acquire);
        }
        return true;
    }

    static bool waitForBlockedLifecycleMutation(MediaChannel& channel)
    {
        while (channel.m_externalLifecycleMutations.load(
                   std::memory_order_acquire) == 0) {
            channel.m_externalLifecycleMutations.wait(
                0, std::memory_order_acquire);
        }
        return true;
    }

    static std::uint64_t mutationSequence(const MediaChannel& channel)
    {
        return channel.m_mutationSequence.load(std::memory_order_acquire);
    }
};

} // namespace media::ffmpeg::graph

namespace {

using media_transcode::test::TestContext;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

class CurrentThreadAllocationFailureScope final {
public:
    CurrentThreadAllocationFailureScope() noexcept
    {
        g_failCurrentThreadAllocations = true;
    }

    ~CurrentThreadAllocationFailureScope()
    {
        g_failCurrentThreadAllocations = false;
    }
};

void failPreparationAllocation(
    std::vector<MediaBufferRef>&,
    std::size_t)
{
    throw std::bad_alloc();
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

MediaEdgePolicy atomicPolicy(std::size_t capacity)
{
    auto policy = MediaBlockingEdgePolicyPlanner::planQueue(capacity);
    policy.queuePolicy.storageMode = MediaQueueStorageMode::AtomicPrepared;
    return policy;
}

void testTransactionOwnsReferencesAndSerializesLifecycle(TestContext& ctx)
{
    const auto verify = [&](bool abortOutput) {
        auto fixture = makeFixture(
            ctx, atomicPolicy(2), atomicPolicy(2));
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
        std::thread lifecycle([&] {
            boundary.arrive_and_wait();
            if (abortOutput) audio(*fixture)->abort();
            else audio(*fixture)->close();
        });
        boundary.arrive_and_wait();
        EXPECT_TRUE(
            ctx,
            MediaChannelAtomicOutputTestAccess::
                waitForBlockedLifecycleMutation(*audio(*fixture)));
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
            ctx, atomicPolicy(2), atomicPolicy(2));
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
        ctx, atomicPolicy(2), atomicPolicy(2));
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
    std::promise<void> videoStarted;
    std::promise<void> audioStarted;
    bool videoVisible = false;
    bool audioVisible = false;
    MediaBufferRef consumedVideo;
    MediaBufferRef consumedAudio;
    std::thread videoConsumer([&] {
        boundary.arrive_and_wait();
        videoStarted.set_value();
        videoVisible = video(*fixture)->tryPop(consumedVideo);
    });
    std::thread audioConsumer([&] {
        boundary.arrive_and_wait();
        audioStarted.set_value();
        audioVisible = audio(*fixture)->tryPop(consumedAudio);
    });
    boundary.arrive_and_wait();
    videoStarted.get_future().wait();
    audioStarted.get_future().wait();
    EXPECT_TRUE(ctx, acquired.value()->commit());
    acquired.value().reset();
    videoConsumer.join();
    audioConsumer.join();
    EXPECT_TRUE(ctx, videoVisible && audioVisible);
    EXPECT_TRUE(ctx, consumedVideo == terminal && consumedAudio == terminal);
    EXPECT_EQ(ctx, video(*fixture)->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, audio(*fixture)->size(), static_cast<std::size_t>(0));
}

void testEmptyTransactionalBatchStillRequiresCompletePolicy(TestContext& ctx)
{
    auto invalidPolicy = atomicPolicy(1);
    invalidPolicy.queuePolicy.orderingPolicy =
        MediaQueueOrderingPolicy::Timestamp;
    auto fixture = makeFixture(
        ctx, atomicPolicy(1), invalidPolicy);
    auto terminal = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Flush);
    const std::array<MediaBufferRef, 1> videoValues{terminal};
    const std::span<const MediaBufferRef> noAudio;
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{video(*fixture), videoValues},
        MediaAtomicOutputBatch{audio(*fixture), noAudio}};
    auto acquired = MediaAtomicOutputTransaction::acquire(
        "Atomic optional output contract", batches);
    EXPECT_FALSE(ctx, acquired);
    EXPECT_EQ(ctx, video(*fixture)->size(), std::size_t{0});
    EXPECT_EQ(ctx, audio(*fixture)->size(), std::size_t{0});
}

void testBlockingConsumersReturnOnlyAfterAtomicPublish(TestContext& ctx)
{
    auto fixture = makeFixture(
        ctx, atomicPolicy(2), atomicPolicy(2));
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
    EXPECT_TRUE(ctx, MediaChannelAtomicOutputTestAccess::waitForBlockedConsumer(
                         *video(*fixture)));
    EXPECT_TRUE(ctx, MediaChannelAtomicOutputTestAccess::waitForBlockedConsumer(
                         *audio(*fixture)));
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
    auto dropPolicy = MediaBlockingEdgePolicyPlanner::planQueue(1);
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
            ctx, MediaBlockingEdgePolicyPlanner::planQueue(1),
            MediaBlockingEdgePolicyPlanner::planQueue(1));
        MediaChannel* blockedChannel = video(*fixture);
        EXPECT_TRUE(ctx, blockedChannel->push(first.value()));
        std::barrier boundary(2);
        std::promise<void> workerStarted;
        ::media::Status result = ::media::Status::success();
        std::thread producer([&] {
            boundary.arrive_and_wait();
            workerStarted.set_value();
            result = blockedChannel->push(second.value());
        });
        boundary.arrive_and_wait();
        workerStarted.get_future().wait();
        const bool enteredWait =
            MediaChannelAtomicOutputTestAccess::waitForBlockedProducer(
                *blockedChannel);
        EXPECT_TRUE(ctx, enteredWait);
        EXPECT_EQ(ctx,
                  blockedChannel->metrics().queue.blockedProducers.load(),
                  static_cast<std::size_t>(1));
        EXPECT_TRUE(ctx,
                    blockedChannel->metrics().queue.blockedPushes.load() >= 1);
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
        ctx, MediaBlockingEdgePolicyPlanner::planQueue(1),
        MediaBlockingEdgePolicyPlanner::planQueue(1));
    MediaChannel* emptyChannel = video(*consumerFixture);
    std::promise<void> consumerStarted;
    MediaBufferRef consumed;
    ::media::Status consumerStatus = ::media::Status::failure(
        ::media::ErrorInfo::internalError("consumer pop did not run"));
    std::thread consumer([&] {
        consumerStarted.set_value();
        consumerStatus = emptyChannel->pop(consumed);
    });
    consumerStarted.get_future().wait();
    EXPECT_TRUE(ctx, MediaChannelAtomicOutputTestAccess::waitForBlockedConsumer(
                         *emptyChannel));
    EXPECT_EQ(ctx, emptyChannel->metrics().queue.blockedConsumers.load(),
              static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, emptyChannel->push(first.value()));
    consumer.join();
    EXPECT_TRUE(ctx, consumerStatus && consumed == first.value());
    EXPECT_EQ(ctx, emptyChannel->metrics().queue.blockedConsumers.load(),
              static_cast<std::size_t>(0));
}

void testCapacityReservationPreventsOrdinaryPushAndCancelReleasesCapacity(
    TestContext& ctx)
{
    auto fixture = makeFixture(
        ctx, atomicPolicy(1), atomicPolicy(1));
    auto reserved = makePacketBuffer(true, 1, MediaStreamKind::Video);
    auto competing = makePacketBuffer(true, 2, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, reserved && competing);
    if (!reserved || !competing) return;

    const std::array<MediaBufferRef, 1> values{reserved.value()};
    const std::array<MediaAtomicOutputBatch, 1> batches{
        MediaAtomicOutputBatch{video(*fixture), values}};
    auto reservation = MediaReservedOutputTransaction::reserve(
        "Reserved capacity", batches);
    EXPECT_TRUE(ctx, reservation && reservation.value().has_value());
    if (!reservation || !reservation.value()) return;
    EXPECT_EQ(ctx, video(*fixture)->pushOutcome(competing.value()),
              MediaQueuePushOutcome::WouldBlock);

    reservation.value().reset();
    EXPECT_EQ(ctx, video(*fixture)->pushOutcome(competing.value()),
              MediaQueuePushOutcome::Accepted);
}

void testAuthorizedReservationSurvivesCloseButAbortTerminatesIt(
    TestContext& ctx)
{
    const auto verify = [&](bool abortOutput) {
        auto fixture = makeFixture(
            ctx, atomicPolicy(1), atomicPolicy(1));
        auto reserved = makePacketBuffer(true, 1, MediaStreamKind::Video);
        EXPECT_TRUE(ctx, reserved);
        if (!reserved) return;
        const std::array<MediaBufferRef, 1> values{reserved.value()};
        const std::array<MediaAtomicOutputBatch, 1> batches{
            MediaAtomicOutputBatch{video(*fixture), values}};
        auto reservation = MediaReservedOutputTransaction::reserve(
            "Authorized lifecycle", batches);
        EXPECT_TRUE(ctx, reservation && reservation.value().has_value());
        if (!reservation || !reservation.value()) return;
        const std::array<MediaOutputCapacityReservationHandle, 1> handles{
            reservation.value()->handle()};
        EXPECT_TRUE(ctx, MediaReservedOutputTransaction::authorize(
                             handles, [] { return ::media::Status::success(); }));

        if (abortOutput) video(*fixture)->abort();
        else video(*fixture)->close();
        const auto committed = reservation.value()->commit();
        EXPECT_EQ(ctx, static_cast<bool>(committed), !abortOutput);
        EXPECT_EQ(ctx, video(*fixture)->size(),
                  abortOutput ? std::size_t{0} : std::size_t{1});
    };
    verify(false);
    verify(true);
}

void testReservedPublishPerformsNoAllocation(TestContext& ctx)
{
    auto fixture = makeFixture(
        ctx, atomicPolicy(2), atomicPolicy(2));
    auto terminal = makeMediaBufferRef<MediaControlBuffer>(
        MediaControlBufferKind::Eof);
    std::array<MediaBufferRef, 1> videoValues{terminal};
    std::array<MediaBufferRef, 1> audioValues{terminal};
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{video(*fixture), videoValues},
        MediaAtomicOutputBatch{audio(*fixture), audioValues}};
    auto acquired = MediaAtomicOutputTransaction::acquire(
        "Allocation-free atomic publish", batches);
    EXPECT_TRUE(ctx, acquired && acquired.value().has_value());
    if (!acquired || !acquired.value()) return;

    {
        CurrentThreadAllocationFailureScope allocationFailure;
        acquired.value()->commitReserved();
    }

    EXPECT_EQ(ctx, video(*fixture)->size(), std::size_t{1});
    EXPECT_EQ(ctx, audio(*fixture)->size(), std::size_t{1});
}

void testThrowingFinalAuthorizationReturnsErrorAndReleasesReservation(
    TestContext& ctx)
{
    auto fixture = makeFixture(
        ctx, atomicPolicy(1), atomicPolicy(1));
    auto reserved = makePacketBuffer(true, 1, MediaStreamKind::Video);
    auto competing = makePacketBuffer(true, 2, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, reserved && competing);
    if (!reserved || !competing) return;

    const std::array<MediaBufferRef, 1> values{reserved.value()};
    const std::array<MediaAtomicOutputBatch, 1> batches{
        MediaAtomicOutputBatch{video(*fixture), values}};
    auto reservation = MediaReservedOutputTransaction::reserve(
        "Throwing final authorization", batches);
    EXPECT_TRUE(ctx, reservation && reservation.value().has_value());
    if (!reservation || !reservation.value()) return;
    const std::array<MediaOutputCapacityReservationHandle, 1> handles{
        reservation.value()->handle()};
    EXPECT_TRUE(ctx, MediaReservedOutputTransaction::authorize(
                         handles, [] { return ::media::Status::success(); }));

    const auto committed = reservation.value()->commit([]() -> ::media::Status {
        throw std::runtime_error("injected final authorization failure");
    });
    EXPECT_FALSE(ctx, committed);
    if (!committed) {
        EXPECT_EQ(ctx, committed.error().code,
                  ::media::ErrorCode::InternalError);
    }
    EXPECT_EQ(ctx, video(*fixture)->size(), std::size_t{0});
    reservation.value().reset();
    EXPECT_EQ(ctx, video(*fixture)->pushOutcome(competing.value()),
              MediaQueuePushOutcome::Accepted);
}

void testPreparationAllocationFailuresAreResultsAndPublishNothing(
    TestContext& ctx)
{
    auto value = makePacketBuffer(true, 1, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, value);
    if (!value) return;
    const std::array<MediaBufferRef, 1> values{value.value()};
    MediaBlockingQueueStorage storage(
        atomicPolicy(2).queuePolicy,
        failPreparationAllocation);
    const auto prepared = storage.prepare(values);
    EXPECT_FALSE(ctx, prepared);
    if (!prepared) {
        EXPECT_EQ(ctx, prepared.error().code,
                  ::media::ErrorCode::InternalError);
    }
    EXPECT_TRUE(ctx, storage.empty());
}

void testDuplicateChannelReservationAggregatesAndPublishesOnce(
    TestContext& ctx)
{
    auto fixture = makeFixture(
        ctx, atomicPolicy(2), atomicPolicy(2));
    auto first = makePacketBuffer(true, 1, MediaStreamKind::Video);
    auto second = makePacketBuffer(true, 2, MediaStreamKind::Video);
    auto competing = makePacketBuffer(true, 3, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, first && second && competing);
    if (!first || !second || !competing) return;

    const std::array<MediaBufferRef, 1> firstValues{first.value()};
    const std::array<MediaBufferRef, 1> secondValues{second.value()};
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{video(*fixture), firstValues},
        MediaAtomicOutputBatch{video(*fixture), secondValues}};
    auto reservation = MediaReservedOutputTransaction::reserve(
        "Duplicate channel aggregation", batches);
    EXPECT_TRUE(ctx, reservation && reservation.value().has_value());
    if (!reservation || !reservation.value()) return;
    EXPECT_EQ(ctx, video(*fixture)->pushOutcome(competing.value()),
              MediaQueuePushOutcome::WouldBlock);

    const std::array<MediaOutputCapacityReservationHandle, 1> handles{
        reservation.value()->handle()};
    EXPECT_TRUE(ctx, MediaReservedOutputTransaction::authorize(
                         handles, [] { return ::media::Status::success(); }));
    const std::uint64_t before =
        MediaChannelAtomicOutputTestAccess::mutationSequence(*video(*fixture));
    EXPECT_TRUE(ctx, reservation.value()->commit());
    const std::uint64_t after =
        MediaChannelAtomicOutputTestAccess::mutationSequence(*video(*fixture));
    EXPECT_EQ(ctx, after, before + 1);
    EXPECT_EQ(ctx, video(*fixture)->size(), std::size_t{2});

    MediaBufferRef firstPopped;
    MediaBufferRef secondPopped;
    EXPECT_TRUE(ctx, video(*fixture)->tryPop(firstPopped));
    EXPECT_TRUE(ctx, video(*fixture)->tryPop(secondPopped));
    EXPECT_TRUE(ctx, firstPopped == first.value());
    EXPECT_TRUE(ctx, secondPopped == second.value());
}

} // namespace

int main()
{
    TestContext ctx;
    testTransactionOwnsReferencesAndSerializesLifecycle(ctx);
    testConsumersCannotObservePartialAtomicCommit(ctx);
    testEmptyTransactionalBatchStillRequiresCompletePolicy(ctx);
    testBlockingConsumersReturnOnlyAfterAtomicPublish(ctx);
    testReservedPublishPerformsNoAllocation(ctx);
    testChannelPushPreservesNullDropAndLifecycleContracts(ctx);
    testCapacityReservationPreventsOrdinaryPushAndCancelReleasesCapacity(ctx);
    testAuthorizedReservationSurvivesCloseButAbortTerminatesIt(ctx);
    testThrowingFinalAuthorizationReturnsErrorAndReleasesReservation(ctx);
    testPreparationAllocationFailuresAreResultsAndPublishNothing(ctx);
    testDuplicateChannelReservationAggregatesAndPublishesOnce(ctx);
    if (ctx.failures != 0) return 1;
    std::cout << "Media channel atomic output tests passed\n";
    return 0;
}
