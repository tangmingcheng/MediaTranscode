#include "common/TestAssert.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/time/MediaTimestampUnwrapper.h"

#include <cstdint>
#include <limits>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

void testRunningTimeArithmetic(TestContext& ctx)
{
    const auto first = MediaRunningTime::fromNanoseconds(1'500'000'000);
    const auto second = MediaRunningTime::fromNanoseconds(500'000'000);

    const auto sum = first.checkedAdd(second);
    EXPECT_TRUE(ctx, sum);
    EXPECT_EQ(ctx, sum.value().nanoseconds(), 2'000'000'000);

    const auto difference = first.checkedSubtract(second);
    EXPECT_TRUE(ctx, difference);
    EXPECT_EQ(ctx, difference.value().nanoseconds(), 1'000'000'000);

    const auto addOverflow = MediaRunningTime::fromNanoseconds(
                                 std::numeric_limits<std::int64_t>::max())
                                 .checkedAdd(MediaRunningTime::fromNanoseconds(1));
    EXPECT_FALSE(ctx, addOverflow);

    const auto subtractOverflow = MediaRunningTime::fromNanoseconds(
                                      std::numeric_limits<std::int64_t>::min())
                                      .checkedSubtract(MediaRunningTime::fromNanoseconds(1));
    EXPECT_FALSE(ctx, subtractOverflow);
}

void testRunningTimeRationalRescale(TestContext& ctx)
{
    const auto oneSecond = MediaRunningTime::checkedFromTicks(90'000, 1, 90'000);
    EXPECT_TRUE(ctx, oneSecond);
    EXPECT_EQ(ctx, oneSecond.value().nanoseconds(), 1'000'000'000);

    const auto negativeFrame = MediaRunningTime::checkedFromTicks(-480, 1, 48'000);
    EXPECT_TRUE(ctx, negativeFrame);
    EXPECT_EQ(ctx, negativeFrame.value().nanoseconds(), -10'000'000);

    EXPECT_FALSE(ctx, MediaRunningTime::checkedFromTicks(1, 1, 0));
    EXPECT_FALSE(ctx, MediaRunningTime::checkedFromTicks(
                          std::numeric_limits<std::int64_t>::max(), 1, 1));
}

void expectForwardWrap(TestContext& ctx, std::uint8_t bitWidth)
{
    auto unwrapper = MediaTimestampUnwrapper::create(bitWidth, 7);
    EXPECT_TRUE(ctx, unwrapper);
    if (!unwrapper) {
        return;
    }

    const std::uint64_t modulus = std::uint64_t{1} << bitWidth;
    const auto beforeWrap = unwrapper.value().unwrap(modulus - 3);
    EXPECT_EQ(ctx, beforeWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, beforeWrap.timestamp, static_cast<std::int64_t>(modulus - 3));
    EXPECT_EQ(ctx, beforeWrap.generation, static_cast<std::uint64_t>(7));

    const auto afterWrap = unwrapper.value().unwrap(3);
    EXPECT_EQ(ctx, afterWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, afterWrap.timestamp, static_cast<std::int64_t>(modulus + 3));
    EXPECT_EQ(ctx, afterWrap.generation, static_cast<std::uint64_t>(7));
}

void testTimestampWraps(TestContext& ctx)
{
    expectForwardWrap(ctx, 32); // RTP timestamp.
    expectForwardWrap(ctx, 33); // MPEG-TS PTS/DTS.

    auto pcrUnwrapper = MediaTimestampUnwrapper::createPcr(9);
    EXPECT_TRUE(ctx, pcrUnwrapper);
    if (!pcrUnwrapper) {
        return;
    }

    constexpr std::uint64_t pcrModulus = (std::uint64_t{1} << 33) * 300;
    const auto beforePcrWrap = pcrUnwrapper.value().unwrap(pcrModulus - 3);
    EXPECT_EQ(ctx, beforePcrWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, beforePcrWrap.timestamp, static_cast<std::int64_t>(pcrModulus - 3));

    const auto afterPcrWrap = pcrUnwrapper.value().unwrap(3);
    EXPECT_EQ(ctx, afterPcrWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, afterPcrWrap.timestamp, static_cast<std::int64_t>(pcrModulus + 3));
    EXPECT_EQ(ctx, afterPcrWrap.generation, static_cast<std::uint64_t>(9));
}

void testTimestampDiscontinuityAndReset(TestContext& ctx)
{
    auto unwrapper = MediaTimestampUnwrapper::create(32, 11);
    EXPECT_TRUE(ctx, unwrapper);
    if (!unwrapper) {
        return;
    }

    EXPECT_EQ(ctx, unwrapper.value().unwrap(1'000).status, MediaTimestampUnwrapStatus::Value);
    const auto backward = unwrapper.value().unwrap(999);
    EXPECT_EQ(ctx, backward.status, MediaTimestampUnwrapStatus::Discontinuity);
    EXPECT_EQ(ctx, backward.reason, MediaTimestampDiscontinuityReason::BackwardMovement);
    EXPECT_EQ(ctx, backward.generation, static_cast<std::uint64_t>(11));

    const auto continued = unwrapper.value().unwrap(1'001);
    EXPECT_EQ(ctx, continued.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, continued.timestamp, static_cast<std::int64_t>(1'001));

    unwrapper.value().reset(12);
    const auto resetValue = unwrapper.value().unwrap(5);
    EXPECT_EQ(ctx, resetValue.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, resetValue.timestamp, static_cast<std::int64_t>(5));
    EXPECT_EQ(ctx, resetValue.generation, static_cast<std::uint64_t>(12));

    EXPECT_FALSE(ctx, MediaTimestampUnwrapper::create(31, 0));
    EXPECT_FALSE(ctx, MediaTimestampUnwrapper::create(63, 0));
}

} // namespace

int main()
{
    TestContext ctx;
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "core.source");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "core.sink");
    graph.addOutputPort(source, "packet", MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(sink, "packet", MediaStreamKind::Video,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    EXPECT_TRUE(ctx, graph.connect(source, "packet", sink, "packet"));
    EXPECT_EQ(ctx, graph.nodeCount(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, graph.edgeCount(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, MediaGraphValidation::validate(graph).ok());
    testRunningTimeArithmetic(ctx);
    testRunningTimeRationalRescale(ctx);
    testTimestampWraps(ctx);
    testTimestampDiscontinuityAndReset(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
