#include "common/TestAssert.h"

#include "internal/graph/runtime/queue/MediaSpscRingQueue.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <chrono>
#include <iostream>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

int main()
{
    TestContext ctx;
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::SpscRing;
    policy.capacity = 1024;
    policy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    MediaSpscRingQueue queue(policy);
    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return 1;
    auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), MediaStreamKind::Video);
    EXPECT_TRUE(ctx, buffer);
    if (!buffer) return 1;

    constexpr std::size_t iterations = 100000;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        EXPECT_TRUE(ctx, queue.push(buffer.value()));
        MediaBufferRef popped;
        EXPECT_TRUE(ctx, queue.tryPop(popped));
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started);
    std::cout << "PERFORMANCE_BASELINE queue=spsc iterations=" << iterations
              << " elapsed_us=" << elapsed.count()
              << " high_water=" << queue.metrics().peakSize.load() << '\n';
    return ctx.failures == 0 ? 0 : 1;
}
