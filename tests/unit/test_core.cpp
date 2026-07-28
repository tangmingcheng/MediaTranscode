#include "common/TestAssert.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAvSyncError.h"
#include "internal/graph/sync/MediaVideoSyncController.h"
#include "internal/graph/time/MediaCanonicalTimeMapper.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/time/MediaTimestampUnwrapper.h"

#include <cstdint>
#include <limits>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

void runAudioDriftServoTests(TestContext& ctx);
void runVideoSyncControllerTests(TestContext& ctx);

namespace {

MediaProtocolTimestamp protocolTimestamp(TestContext& ctx,
                                         std::int64_t ticks,
                                         int timeBaseNumerator,
                                         int timeBaseDenominator)
{
    auto timestamp = MediaProtocolTimestamp::create(
        ticks, timeBaseNumerator, timeBaseDenominator);
    EXPECT_TRUE(ctx, timestamp);
    return timestamp.value();
}

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

void testDefaultResultErrorContract(TestContext& ctx)
{
    const auto valueSuccess = ::media::Result<int>::success(7);
    EXPECT_TRUE(ctx, valueSuccess);
    EXPECT_EQ(ctx, valueSuccess.value(), 7);
    EXPECT_TRUE(ctx, valueSuccess.error().ok());
    EXPECT_EQ(ctx, valueSuccess.error().code, ::media::ErrorCode::None);

    const auto valueNormalized = ::media::Result<int>::failure(::media::ErrorInfo{});
    EXPECT_FALSE(ctx, valueNormalized);
    EXPECT_EQ(ctx, valueNormalized.error().code, ::media::ErrorCode::InternalError);
    EXPECT_EQ(ctx, valueNormalized.error().message, std::string("unknown error"));

    const auto voidSuccess = ::media::Result<void>::success();
    EXPECT_TRUE(ctx, voidSuccess);
    EXPECT_TRUE(ctx, voidSuccess.error().ok());
    EXPECT_EQ(ctx, voidSuccess.error().code, ::media::ErrorCode::None);

    const auto voidNormalized = ::media::Result<void>::failure(::media::ErrorInfo{});
    EXPECT_FALSE(ctx, voidNormalized);
    EXPECT_EQ(ctx, voidNormalized.error().code, ::media::ErrorCode::InternalError);
    EXPECT_EQ(ctx, voidNormalized.error().message, std::string("unknown error"));
}

void testMixedAtomicFanoutRejected(TestContext& ctx)
{
    MediaGraph graph;
    const MediaNodeId source = graph.addNode(
        MediaNodeKind::DebugDump, "mixed-fanout-source");
    const MediaNodeId firstSink = graph.addNode(
        MediaNodeKind::DebugDump, "mixed-fanout-first");
    const MediaNodeId secondSink = graph.addNode(
        MediaNodeKind::DebugDump, "mixed-fanout-second");
    graph.addOutputPort(
        source, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(
        firstSink, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(
        secondSink, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    EXPECT_TRUE(ctx, graph.connect(
        source, "packet", firstSink, "packet", "atomic",
        MediaBlockingEdgePolicyPlanner::planAtomicOutput(2)));
    EXPECT_TRUE(ctx, graph.connect(
        source, "packet", secondSink, "packet", "non-atomic",
        MediaBlockingEdgePolicyPlanner::planQueue(2)));

    const auto validation = MediaGraphValidation::validate(graph);
    EXPECT_FALSE(ctx, validation.ok());
    bool foundMixedPolicy = false;
    for (const auto& issue : validation.issues) {
        foundMixedPolicy = foundMixedPolicy ||
            (issue.code == MediaGraphErrorCode::InvalidPolicy &&
             issue.portId.isValid() &&
             issue.message ==
                 "Output fan-out cannot mix atomic and non-atomic policies");
    }
    EXPECT_TRUE(ctx, foundMixedPolicy);
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
        protocolTimestamp(ctx,
                          static_cast<std::int64_t>(modulus - 3),
                          timeBaseNumerator,
                          timeBaseDenominator));
    EXPECT_EQ(ctx, beforeWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_TRUE(ctx, beforeWrap.timestamp);
    EXPECT_EQ(ctx, beforeWrap.timestamp->ticks(), static_cast<std::int64_t>(modulus - 3));
    EXPECT_EQ(ctx, beforeWrap.timestamp->timeBaseNumerator(), timeBaseNumerator);
    EXPECT_EQ(ctx, beforeWrap.timestamp->timeBaseDenominator(), timeBaseDenominator);
    EXPECT_EQ(ctx, beforeWrap.generation, static_cast<std::uint64_t>(7));

    const auto afterWrap = unwrapper.value().unwrap(
        protocolTimestamp(ctx, 3, timeBaseNumerator, timeBaseDenominator));
    EXPECT_EQ(ctx, afterWrap.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_TRUE(ctx, afterWrap.timestamp);
    EXPECT_EQ(ctx, afterWrap.timestamp->ticks(), static_cast<std::int64_t>(modulus + 3));
    EXPECT_EQ(ctx, afterWrap.timestamp->timeBaseNumerator(), timeBaseNumerator);
    EXPECT_EQ(ctx, afterWrap.timestamp->timeBaseDenominator(), timeBaseDenominator);
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

    const auto timestamp = [&ctx](std::uint64_t ticks) {
        return protocolTimestamp(ctx, static_cast<std::int64_t>(ticks), 1, 90'000);
    };

    EXPECT_EQ(ctx, unwrapper.value().unwrap(timestamp(1'000)).status, MediaTimestampUnwrapStatus::Value);
    const auto backward = unwrapper.value().unwrap(timestamp(999));
    EXPECT_EQ(ctx, backward.status, MediaTimestampUnwrapStatus::Discontinuity);
    EXPECT_EQ(ctx, backward.reason, MediaTimestampDiscontinuityReason::BackwardMovement);
    EXPECT_EQ(ctx, backward.generation, static_cast<std::uint64_t>(11));

    const auto continued = unwrapper.value().unwrap(timestamp(1'001));
    EXPECT_EQ(ctx, continued.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_TRUE(ctx, continued.timestamp);
    EXPECT_EQ(ctx, continued.timestamp->ticks(), static_cast<std::int64_t>(1'001));

    unwrapper.value().reset(12);
    const auto resetValue = unwrapper.value().unwrap(timestamp(5));
    EXPECT_EQ(ctx, resetValue.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_TRUE(ctx, resetValue.timestamp);
    EXPECT_EQ(ctx, resetValue.timestamp->ticks(), static_cast<std::int64_t>(5));
    EXPECT_EQ(ctx, resetValue.generation, static_cast<std::uint64_t>(12));
}

void testTimestampBoundaryClassificationAndStatePreservation(TestContext& ctx)
{
    constexpr std::uint64_t modulus = std::uint64_t{1} << 32;
    constexpr std::uint64_t halfRange = modulus / 2;
    const auto timestamp = [&ctx](std::uint64_t ticks) {
        return protocolTimestamp(ctx, static_cast<std::int64_t>(ticks), 1, 90'000);
    };

    auto forward = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, 21);
    EXPECT_TRUE(ctx, forward);
    if (!forward) return;
    EXPECT_EQ(ctx, forward.value().unwrap(timestamp(10)).status, MediaTimestampUnwrapStatus::Value);
    const auto forwardHalf = forward.value().unwrap(timestamp(10 + halfRange));
    EXPECT_EQ(ctx, forwardHalf.reason, MediaTimestampDiscontinuityReason::AmbiguousMovement);
    const auto forwardContinued = forward.value().unwrap(timestamp(11));
    EXPECT_EQ(ctx, forwardContinued.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_TRUE(ctx, forwardContinued.timestamp);
    EXPECT_EQ(ctx, forwardContinued.timestamp->ticks(), static_cast<std::int64_t>(11));

    auto backward = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, 22);
    EXPECT_TRUE(ctx, backward);
    if (!backward) return;
    EXPECT_EQ(ctx, backward.value().unwrap(timestamp(halfRange + 10)).status, MediaTimestampUnwrapStatus::Value);
    const auto backwardHalf = backward.value().unwrap(timestamp(10));
    EXPECT_EQ(ctx, backwardHalf.reason, MediaTimestampDiscontinuityReason::AmbiguousMovement);
    const auto backwardContinued = backward.value().unwrap(timestamp(halfRange + 11));
    EXPECT_EQ(ctx, backwardContinued.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx,
              backwardContinued.timestamp->ticks(),
              static_cast<std::int64_t>(halfRange + 11));

    const auto outOfRange = backward.value().unwrap(timestamp(modulus));
    EXPECT_EQ(ctx, outOfRange.reason, MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    const auto negativeRaw = backward.value().unwrap(protocolTimestamp(ctx, -1, 1, 90'000));
    EXPECT_EQ(ctx, negativeRaw.reason, MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    const auto wrongTimeBase = backward.value().unwrap(protocolTimestamp(ctx, 20, 1, 48'000));
    EXPECT_EQ(ctx, wrongTimeBase.reason, MediaTimestampDiscontinuityReason::TimeBaseMismatch);
    const auto afterOutOfRange = backward.value().unwrap(timestamp(halfRange + 12));
    EXPECT_EQ(ctx, afterOutOfRange.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_EQ(ctx,
              afterOutOfRange.timestamp->ticks(),
              static_cast<std::int64_t>(halfRange + 12));
}

void testTimestampUnwrappedOverflowPreservesState(TestContext& ctx)
{
    constexpr std::uint64_t modulus = (std::uint64_t{1} << 33) * 300;
    constexpr std::uint64_t step = modulus / 2 - 1;
    auto unwrapper = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::MpegTsPcr27Mhz, 31);
    EXPECT_TRUE(ctx, unwrapper);
    if (!unwrapper) return;

    const auto timestamp = [&ctx](std::uint64_t ticks) {
        return protocolTimestamp(ctx, static_cast<std::int64_t>(ticks), 1, 27'000'000);
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

void testProtocolTimestampValidityAndEmptyInitialState(TestContext& ctx)
{
    EXPECT_FALSE(ctx, MediaProtocolTimestamp::create(0, 0, 90'000));
    EXPECT_FALSE(ctx, MediaProtocolTimestamp::create(0, 1, 0));
    EXPECT_FALSE(ctx, MediaProtocolTimestamp::create(0, -1, 90'000));

    const auto valid = MediaProtocolTimestamp::create(42, 1, 90'000);
    EXPECT_TRUE(ctx, valid);
    if (valid) {
        EXPECT_EQ(ctx, valid.value().ticks(), static_cast<std::int64_t>(42));
    }

    auto unwrapper = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, 40);
    EXPECT_TRUE(ctx, unwrapper);
    if (!unwrapper) return;

    const auto firstNegative = unwrapper.value().unwrap(protocolTimestamp(ctx, -1, 1, 90'000));
    EXPECT_EQ(ctx, firstNegative.reason, MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    EXPECT_FALSE(ctx, firstNegative.timestamp.has_value());

    const auto firstUpperBound = unwrapper.value().unwrap(
        protocolTimestamp(ctx, static_cast<std::int64_t>(std::uint64_t{1} << 32), 1, 90'000));
    EXPECT_EQ(ctx, firstUpperBound.reason, MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    EXPECT_FALSE(ctx, firstUpperBound.timestamp.has_value());

    const auto accepted = unwrapper.value().unwrap(protocolTimestamp(ctx, 7, 1, 90'000));
    EXPECT_EQ(ctx, accepted.status, MediaTimestampUnwrapStatus::Value);
    EXPECT_TRUE(ctx, accepted.timestamp.has_value());
}

MediaCanonicalSourceTimestamp sourceTimestamp(
    std::optional<MediaRunningTime> presentationTime,
    std::optional<MediaRunningTime> decodeTime,
    std::optional<MediaRunningTime> duration,
    std::uint64_t generation,
    const char* sourceIdentity,
    MediaTimeMappingConfidence confidence)
{
    return MediaCanonicalSourceTimestamp(
        presentationTime,
        decodeTime,
        duration,
        generation,
        sourceIdentity,
        confidence);
}

void testCanonicalMappingAcrossProtocolTimeBases(TestContext& ctx)
{
    const auto rtpSource = MediaRunningTime::checkedFromTicks(180'000, 1, 90'000);
    const auto tsSource = MediaRunningTime::checkedFromTicks(54'000'000, 1, 27'000'000);
    EXPECT_TRUE(ctx, rtpSource);
    EXPECT_TRUE(ctx, tsSource);
    if (!rtpSource || !tsSource) return;

    const auto mapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(250'000'000),
        MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "clock-group-a",
        7});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;

    const auto rtp = mapper.value().map(sourceTimestamp(
        rtpSource.value(),
        rtpSource.value(),
        MediaRunningTime::fromNanoseconds(20'000'000),
        7,
        "clock-group-a",
        MediaTimeMappingConfidence::Locked));
    const auto ts = mapper.value().map(sourceTimestamp(
        tsSource.value(),
        tsSource.value(),
        MediaRunningTime::fromNanoseconds(20'000'000),
        7,
        "clock-group-a",
        MediaTimeMappingConfidence::Locked));
    EXPECT_TRUE(ctx, rtp);
    EXPECT_TRUE(ctx, ts);
    if (!rtp || !ts) return;
    EXPECT_EQ(ctx, rtp.value().presentationTime().nanoseconds(), 1'250'000'000);
    EXPECT_EQ(ctx, ts.value().presentationTime(), rtp.value().presentationTime());
    EXPECT_EQ(ctx, rtp.value().generation(), static_cast<std::uint64_t>(7));
    EXPECT_EQ(ctx, ts.value().sourceIdentity(), std::string("clock-group-a"));
}

void testCanonicalMappingKeepsPresentationAndDecodeSeparate(TestContext& ctx)
{
    const auto mapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(10'000'000'000),
        MediaRunningTime::fromNanoseconds(0),
        MediaAvSyncTopology::MpegTsToMpegTs,
        "video",
        3});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;

    const auto mapped = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(10'080'000'000),
        MediaRunningTime::fromNanoseconds(10'040'000'000),
        MediaRunningTime::fromNanoseconds(20'000'000),
        3,
        "video",
        MediaTimeMappingConfidence::Degraded));
    EXPECT_TRUE(ctx, mapped);
    if (!mapped) return;
    EXPECT_EQ(ctx, mapped.value().presentationTime().nanoseconds(), 80'000'000);
    EXPECT_TRUE(ctx, mapped.value().decodeTime());
    EXPECT_EQ(ctx, mapped.value().decodeTime()->nanoseconds(), 40'000'000);
    EXPECT_TRUE(ctx, mapped.value().duration());
    EXPECT_EQ(ctx, mapped.value().duration()->nanoseconds(), 20'000'000);
    EXPECT_EQ(ctx, mapped.value().confidence(), MediaTimeMappingConfidence::Degraded);
}

void testCanonicalMappingLargeValuesAvoidIntermediateOverflow(TestContext& ctx)
{
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto mapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(maximum - 1'000),
        MediaRunningTime::fromNanoseconds(500),
        MediaAvSyncTopology::MpegTsToMpegTs,
        "large",
        4});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;

    const auto mapped = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(maximum - 1),
        std::nullopt,
        std::nullopt,
        4,
        "large",
        MediaTimeMappingConfidence::Locked));
    EXPECT_TRUE(ctx, mapped);
    if (mapped) {
        EXPECT_EQ(ctx, mapped.value().presentationTime().nanoseconds(), 1'499);
        EXPECT_FALSE(ctx, mapped.value().decodeTime());
    }
}

void testCanonicalMappingRejectsUnmappableEvidence(TestContext& ctx)
{
    const auto unbound = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0),
        MediaAvSyncTopology::MpegTsToMpegTs,
        "",
        9});
    EXPECT_FALSE(ctx, unbound);
    if (!unbound) {
        EXPECT_EQ(ctx, unbound.error().code(), MediaAvSyncErrorCode::EmptySourceIdentity);
        EXPECT_EQ(ctx, unbound.error().topology(), MediaAvSyncTopology::MpegTsToMpegTs);
    }

    const auto mapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0),
        MediaAvSyncTopology::MpegTsToMpegTs,
        "audio",
        9});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;

    const auto missingPresentation = mapper.value().map(sourceTimestamp(
        std::nullopt,
        MediaRunningTime::fromNanoseconds(123),
        std::nullopt,
        9,
        "audio",
        MediaTimeMappingConfidence::Locked));
    EXPECT_FALSE(ctx, missingPresentation);
    if (!missingPresentation) {
        EXPECT_EQ(ctx, missingPresentation.error().code(), MediaAvSyncErrorCode::MissingSourceEvidence);
        EXPECT_EQ(ctx, missingPresentation.error().state(), MediaAvSyncErrorState::Mapping);
        EXPECT_EQ(ctx, missingPresentation.error().observedStreamIdentity(), std::string("audio"));
        EXPECT_EQ(ctx, missingPresentation.error().observedGeneration(), std::optional<std::uint64_t>(9));
    }

    const auto wrongIdentity = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(123),
        std::nullopt,
        std::nullopt,
        9,
        "video",
        MediaTimeMappingConfidence::Locked));
    EXPECT_FALSE(ctx, wrongIdentity);
    if (!wrongIdentity) {
        EXPECT_EQ(ctx, wrongIdentity.error().code(), MediaAvSyncErrorCode::SourceIdentityMismatch);
        EXPECT_EQ(ctx, wrongIdentity.error().expectedStreamIdentity(), std::string("audio"));
        EXPECT_EQ(ctx, wrongIdentity.error().observedStreamIdentity(), std::string("video"));
    }
}

void testCanonicalMappingResetRejectsOldGeneration(TestContext& ctx)
{
    auto mapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(1'000),
        MediaRunningTime::fromNanoseconds(10),
        MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "video",
        20});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;

    const auto oldEvidence = sourceTimestamp(
        MediaRunningTime::fromNanoseconds(1'100),
        std::nullopt,
        std::nullopt,
        20,
        "video",
        MediaTimeMappingConfidence::Locked);
    EXPECT_TRUE(ctx, mapper.value().map(oldEvidence));

    EXPECT_TRUE(ctx, mapper.value().reset(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(5'000),
        MediaRunningTime::fromNanoseconds(0),
        MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "video",
        21}));
    EXPECT_EQ(ctx, mapper.value().generation(), static_cast<std::uint64_t>(21));

    const auto rejected = mapper.value().map(oldEvidence);
    EXPECT_FALSE(ctx, rejected);
    if (!rejected) {
        EXPECT_EQ(ctx, rejected.error().code(), MediaAvSyncErrorCode::GenerationMismatch);
        EXPECT_EQ(ctx, rejected.error().expectedGeneration(), std::optional<std::uint64_t>(21));
        EXPECT_EQ(ctx, rejected.error().observedGeneration(), std::optional<std::uint64_t>(20));
    }

    const auto staleReset = mapper.value().reset(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0),
        MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "video",
        20});
    EXPECT_FALSE(ctx, staleReset);
    EXPECT_EQ(ctx, mapper.value().generation(), static_cast<std::uint64_t>(21));

    const auto emptyIdentityReset = mapper.value().reset(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(-100),
        MediaRunningTime::fromNanoseconds(-200),
        MediaAvSyncTopology::MpegTsToMpegTs,
        "",
        22});
    EXPECT_FALSE(ctx, emptyIdentityReset);
    if (!emptyIdentityReset) {
        EXPECT_EQ(ctx,
                  emptyIdentityReset.error().code(),
                  MediaAvSyncErrorCode::EmptySourceIdentity);
    }
    const auto preserved = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(5'100),
        std::nullopt,
        std::nullopt,
        21,
        "video",
        MediaTimeMappingConfidence::Locked));
    EXPECT_TRUE(ctx, preserved);
    if (preserved) {
        EXPECT_EQ(ctx, preserved.value().presentationTime().nanoseconds(), 100);
    }
}

void testCanonicalMappingRejectsFutureGenerationAndOverflow(TestContext& ctx)
{
    auto mapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(-10),
        MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max() - 5),
        MediaAvSyncTopology::MpegTsToMpegTs,
        "audio",
        30});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;

    const auto future = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(-10),
        std::nullopt,
        std::nullopt,
        31,
        "audio",
        MediaTimeMappingConfidence::Locked));
    EXPECT_FALSE(ctx, future);

    const auto overflow = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(0),
        std::nullopt,
        std::nullopt,
        30,
        "audio",
        MediaTimeMappingConfidence::Locked));
    EXPECT_FALSE(ctx, overflow);
    if (!overflow) {
        EXPECT_EQ(ctx, overflow.error().code(), MediaAvSyncErrorCode::TimeOverflow);
        EXPECT_TRUE(ctx, overflow.error().observedSourceTime());
        EXPECT_EQ(ctx, overflow.error().sourceEpoch().nanoseconds(), -10);
        EXPECT_EQ(ctx,
                  overflow.error().runningTimeEpoch().nanoseconds(),
                  std::numeric_limits<std::int64_t>::max() - 5);
    }

    const auto negativeDuration = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(-10),
        std::nullopt,
        MediaRunningTime::fromNanoseconds(-1),
        30,
        "audio",
        MediaTimeMappingConfidence::Locked));
    EXPECT_FALSE(ctx, negativeDuration);
}

void testCanonicalMappingAvoidsRepresentableAffineIntermediateOverflow(TestContext& ctx)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

    const auto positiveMapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(-1),
        MediaRunningTime::fromNanoseconds(-1),
        MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "video",
        41});
    EXPECT_TRUE(ctx, positiveMapper);
    if (positiveMapper) {
        const auto mapped = positiveMapper.value().map(sourceTimestamp(
            MediaRunningTime::fromNanoseconds(maximum),
            MediaRunningTime::fromNanoseconds(maximum),
            std::nullopt,
            41,
            "video",
            MediaTimeMappingConfidence::Locked));
        EXPECT_TRUE(ctx, mapped);
        if (mapped) {
            EXPECT_EQ(ctx, mapped.value().presentationTime().nanoseconds(), maximum);
            EXPECT_EQ(ctx, mapped.value().decodeTime()->nanoseconds(), maximum);
        }
    }

    auto negativeMapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(1),
        MediaAvSyncTopology::MpegTsToMpegTs,
        "audio",
        42});
    EXPECT_TRUE(ctx, negativeMapper);
    if (negativeMapper) {
        const auto cancellationMapped = negativeMapper.value().map(sourceTimestamp(
            MediaRunningTime::fromNanoseconds(minimum),
            MediaRunningTime::fromNanoseconds(minimum),
            std::nullopt,
            42,
            "audio",
            MediaTimeMappingConfidence::Locked));
        EXPECT_TRUE(ctx, cancellationMapped);
        if (cancellationMapped) {
            EXPECT_EQ(ctx,
                      cancellationMapped.value().presentationTime().nanoseconds(),
                      minimum);
            EXPECT_EQ(ctx,
                      cancellationMapped.value().decodeTime()->nanoseconds(),
                      minimum);
        }

        const auto threeTermCancellation = negativeMapper.value().reset(
            MediaCanonicalTimeMapperConfig{
                MediaRunningTime::fromNanoseconds(minimum),
                MediaRunningTime::fromNanoseconds(minimum),
                MediaAvSyncTopology::MpegTsToMpegTs,
                "audio",
                43});
        EXPECT_TRUE(ctx, threeTermCancellation);
        const auto mapped = negativeMapper.value().map(sourceTimestamp(
            MediaRunningTime::fromNanoseconds(minimum),
            std::nullopt,
            std::nullopt,
            43,
            "audio",
            MediaTimeMappingConfidence::Locked));
        EXPECT_TRUE(ctx, mapped);
        if (mapped) {
            EXPECT_EQ(ctx, mapped.value().presentationTime().nanoseconds(), minimum);
        }
    }
}

void testCanonicalMappingRejectsFinalValueBelowMinimum(TestContext& ctx)
{
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    const auto mapper = MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig{
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(minimum),
        MediaAvSyncTopology::SeparateRtpToSeparateRtp,
        "audio",
        44});
    EXPECT_TRUE(ctx, mapper);
    if (!mapper) return;

    const auto belowMinimum = mapper.value().map(sourceTimestamp(
        MediaRunningTime::fromNanoseconds(0),
        std::nullopt,
        std::nullopt,
        44,
        "audio",
        MediaTimeMappingConfidence::Locked));
    EXPECT_FALSE(ctx, belowMinimum);
    if (!belowMinimum) {
        EXPECT_EQ(ctx, belowMinimum.error().code(), MediaAvSyncErrorCode::TimeOverflow);
        EXPECT_EQ(ctx, belowMinimum.error().state(), MediaAvSyncErrorState::Mapping);
        EXPECT_EQ(ctx,
                  belowMinimum.error().topology(),
                  MediaAvSyncTopology::SeparateRtpToSeparateRtp);
        EXPECT_EQ(ctx,
                  belowMinimum.error().observedSourceTime(),
                  std::optional<MediaRunningTime>(MediaRunningTime::fromNanoseconds(0)));
        EXPECT_EQ(ctx, belowMinimum.error().sourceEpoch().nanoseconds(), 1);
        EXPECT_EQ(ctx, belowMinimum.error().runningTimeEpoch().nanoseconds(), minimum);
        EXPECT_EQ(ctx, belowMinimum.error().minimumRunningTimeNs(), minimum);
        EXPECT_EQ(ctx,
                  belowMinimum.error().maximumRunningTimeNs(),
                  std::numeric_limits<std::int64_t>::max());
    }
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
    EXPECT_TRUE(ctx, graph.connect(
        source, "packet", sink, "packet", "core",
        MediaBlockingEdgePolicyPlanner::planQueue(2)));
    EXPECT_EQ(ctx, graph.nodeCount(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, graph.edgeCount(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, MediaGraphValidation::validate(graph).ok());
    testMixedAtomicFanoutRejected(ctx);
    testRunningTimeArithmetic(ctx);
    testDefaultResultErrorContract(ctx);
    testRunningTimeRationalRescale(ctx);
    testTimestampWraps(ctx);
    testTimestampDiscontinuityAndReset(ctx);
    testTimestampBoundaryClassificationAndStatePreservation(ctx);
    testTimestampUnwrappedOverflowPreservesState(ctx);
    testProtocolTimestampValidityAndEmptyInitialState(ctx);
    testCanonicalMappingAcrossProtocolTimeBases(ctx);
    testCanonicalMappingKeepsPresentationAndDecodeSeparate(ctx);
    testCanonicalMappingLargeValuesAvoidIntermediateOverflow(ctx);
    testCanonicalMappingRejectsUnmappableEvidence(ctx);
    testCanonicalMappingResetRejectsOldGeneration(ctx);
    testCanonicalMappingRejectsFutureGenerationAndOverflow(ctx);
    testCanonicalMappingAvoidsRepresentableAffineIntermediateOverflow(ctx);
    testCanonicalMappingRejectsFinalValueBelowMinimum(ctx);
    runAudioDriftServoTests(ctx);
    runVideoSyncControllerTests(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
