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

    const auto exactMinimum = MediaRunningTime::checkedFromTicks(
        std::numeric_limits<std::int64_t>::min(), 1, 1'000'000'000);
    EXPECT_TRUE(ctx, exactMinimum);
    if (exactMinimum) {
        EXPECT_EQ(ctx,
                  exactMinimum.value().nanoseconds(),
                  std::numeric_limits<std::int64_t>::min());
    }
    EXPECT_FALSE(ctx, MediaRunningTime::checkedFromTicks(
                          std::numeric_limits<std::int64_t>::min(),
                          2,
                          1'000'000'000));
}

void expectForwardWrap(TestContext& ctx,
                       MediaTimestampCounterKind counterKind,
                       std::uint64_t modulus,
                       int timeBaseNumerator,
                       int timeBaseDenominator)
{
    auto unwrapper = MediaTimestampUnwrapper::create(counterKind, 7);
    EXPECT_TRUE(ctx, unwrapper);
    if (!unwrapper) {
        return;
    }

    EXPECT_EQ(ctx, unwrapper.value().counterKind(), counterKind);
    EXPECT_EQ(ctx, unwrapper.value().modulus(), modulus);

    const auto beforeWrap = unwrapper.value().unwrap(
        MediaProtocolTimestamp(modulus - 3, timeBaseNumerator, timeBaseDenominator));
    EXPECT_EQ(ctx, beforeWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, beforeWrap.timestamp.ticks(), static_cast<std::int64_t>(modulus - 3));
    EXPECT_EQ(ctx, beforeWrap.timestamp.timeBaseNumerator(), timeBaseNumerator);
    EXPECT_EQ(ctx, beforeWrap.timestamp.timeBaseDenominator(), timeBaseDenominator);
    EXPECT_EQ(ctx, beforeWrap.generation, static_cast<std::uint64_t>(7));

    const auto afterWrap = unwrapper.value().unwrap(
        MediaProtocolTimestamp(3, timeBaseNumerator, timeBaseDenominator));
    EXPECT_EQ(ctx, afterWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, afterWrap.timestamp.ticks(), static_cast<std::int64_t>(modulus + 3));
    EXPECT_EQ(ctx, afterWrap.timestamp.timeBaseNumerator(), timeBaseNumerator);
    EXPECT_EQ(ctx, afterWrap.timestamp.timeBaseDenominator(), timeBaseDenominator);
    EXPECT_EQ(ctx, afterWrap.generation, static_cast<std::uint64_t>(7));
}

void testTimestampWraps(TestContext& ctx)
{
    constexpr std::uint64_t rtpModulus = std::uint64_t{1} << 32;
    constexpr std::uint64_t ptsModulus = std::uint64_t{1} << 33;
    constexpr std::uint64_t pcrModulus = (std::uint64_t{1} << 33) * 300;
    expectForwardWrap(ctx, MediaTimestampCounterKind::Rtp32, rtpModulus, 1, 90'000);
    expectForwardWrap(ctx, MediaTimestampCounterKind::MpegTsPtsDts33, ptsModulus, 1, 90'000);
    expectForwardWrap(ctx, MediaTimestampCounterKind::MpegTsPcr27Mhz, pcrModulus, 1, 27'000'000);
}

void testTimestampDiscontinuityAndReset(TestContext& ctx)
{
    auto unwrapper = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, 11);
    EXPECT_TRUE(ctx, unwrapper);
    if (!unwrapper) {
        return;
    }

    const auto timestamp = [](std::uint64_t ticks) {
        return MediaProtocolTimestamp(ticks, 1, 90'000);
    };

    EXPECT_EQ(ctx, unwrapper.value().unwrap(timestamp(1'000)).status, MediaTimestampUnwrapStatus::Value);
    const auto backward = unwrapper.value().unwrap(timestamp(999));
    EXPECT_EQ(ctx, backward.status, MediaTimestampUnwrapStatus::Discontinuity);
    EXPECT_EQ(ctx, backward.reason, MediaTimestampDiscontinuityReason::BackwardMovement);
    EXPECT_EQ(ctx, backward.generation, static_cast<std::uint64_t>(11));

    const auto continued = unwrapper.value().unwrap(timestamp(1'001));
    EXPECT_EQ(ctx, continued.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, continued.timestamp.ticks(), static_cast<std::int64_t>(1'001));

    unwrapper.value().reset(12);
    const auto resetValue = unwrapper.value().unwrap(timestamp(5));
    EXPECT_EQ(ctx, resetValue.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, resetValue.timestamp.ticks(), static_cast<std::int64_t>(5));
    EXPECT_EQ(ctx, resetValue.generation, static_cast<std::uint64_t>(12));
}

void testTimestampBoundaryClassificationAndStatePreservation(TestContext& ctx)
{
    constexpr std::uint64_t modulus = std::uint64_t{1} << 32;
    constexpr std::uint64_t halfRange = modulus / 2;
    const auto timestamp = [](std::uint64_t ticks) {
        return MediaProtocolTimestamp(ticks, 1, 90'000);
    };

    auto forward = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, 21);
    EXPECT_TRUE(ctx, forward);
    if (!forward) return;
    EXPECT_EQ(ctx, forward.value().unwrap(timestamp(10)).status, MediaTimestampUnwrapStatus::Value);
    const auto forwardHalf = forward.value().unwrap(timestamp(10 + halfRange));
    EXPECT_EQ(ctx, forwardHalf.reason, MediaTimestampDiscontinuityReason::AmbiguousMovement);
    const auto forwardContinued = forward.value().unwrap(timestamp(11));
    EXPECT_EQ(ctx, forwardContinued.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx, forwardContinued.timestamp.ticks(), static_cast<std::int64_t>(11));

    auto backward = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, 22);
    EXPECT_TRUE(ctx, backward);
    if (!backward) return;
    EXPECT_EQ(ctx, backward.value().unwrap(timestamp(halfRange + 10)).status, MediaTimestampUnwrapStatus::Value);
    const auto backwardHalf = backward.value().unwrap(timestamp(10));
    EXPECT_EQ(ctx, backwardHalf.reason, MediaTimestampDiscontinuityReason::AmbiguousMovement);
    const auto backwardContinued = backward.value().unwrap(timestamp(halfRange + 11));
    EXPECT_EQ(ctx, backwardContinued.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx,
              backwardContinued.timestamp.ticks(),
              static_cast<std::int64_t>(halfRange + 11));

    const auto outOfRange = backward.value().unwrap(timestamp(modulus));
    EXPECT_EQ(ctx, outOfRange.reason, MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    const auto negativeRaw = backward.value().unwrap(MediaProtocolTimestamp(-1, 1, 90'000));
    EXPECT_EQ(ctx, negativeRaw.reason, MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    const auto wrongTimeBase = backward.value().unwrap(MediaProtocolTimestamp(20, 1, 48'000));
    EXPECT_EQ(ctx, wrongTimeBase.reason, MediaTimestampDiscontinuityReason::TimeBaseMismatch);
    const auto afterOutOfRange = backward.value().unwrap(timestamp(halfRange + 12));
    EXPECT_EQ(ctx, afterOutOfRange.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx,
              afterOutOfRange.timestamp.ticks(),
              static_cast<std::int64_t>(halfRange + 12));
}

void testTimestampUnwrappedOverflowPreservesState(TestContext& ctx)
{
    constexpr std::uint64_t modulus = (std::uint64_t{1} << 33) * 300;
    constexpr std::uint64_t step = modulus / 2 - 1;
    auto unwrapper = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::MpegTsPcr27Mhz, 31);
    EXPECT_TRUE(ctx, unwrapper);
    if (!unwrapper) return;

    const auto timestamp = [](std::uint64_t ticks) {
        return MediaProtocolTimestamp(ticks, 1, 27'000'000);
    };
    std::uint64_t raw = 0;
    std::uint64_t lastAcceptedRaw = raw;
    EXPECT_EQ(ctx, unwrapper.value().unwrap(timestamp(raw)).status, MediaTimestampUnwrapStatus::Value);
    MediaTimestampUnwrapResult result;
    do {
        raw = (raw + step) % modulus;
        result = unwrapper.value().unwrap(timestamp(raw));
        if (result.status == MediaTimestampUnwrapStatus::Value) {
            lastAcceptedRaw = raw;
        }
    } while (result.status == MediaTimestampUnwrapStatus::Value);

    EXPECT_EQ(ctx, result.reason, MediaTimestampDiscontinuityReason::UnwrappedValueOverflow);
    const auto preserved = unwrapper.value().unwrap(timestamp((lastAcceptedRaw + 1) % modulus));
    EXPECT_EQ(ctx, preserved.status, MediaTimestampUnwrapStatus::Value);
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
    testTimestampBoundaryClassificationAndStatePreservation(ctx);
    testTimestampUnwrappedOverflowPreservesState(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
