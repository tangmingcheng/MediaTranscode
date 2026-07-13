#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"
#include "internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h"

#include <cstdint>
#include <limits>
#include <optional>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaTsEvidenceCheckpoint checkpoint(std::uint64_t offset, std::uint64_t generation = 1)
{
    MediaTsEvidenceCheckpoint value;
    value.byteOffset = offset;
    value.generation = generation;
    return value;
}

MediaTsProgramClockPolicy clockPolicy()
{
    return MediaTsProgramClockPolicy{
        .programNumber = 1,
        .pmtPid = 0x100,
        .pcrPid = 0x101,
        .videoPid = 0x201,
        .audioPid = 0x202,
        .pcrInterval27Mhz = 2'700'000,
        .maximumJitter27Mhz = 2'700,
        .maximumGap27Mhz = 8'100'000};
}

MediaTsPcrObservation pcr(std::uint64_t raw,
                          std::uint64_t offset,
                          bool discontinuity = false)
{
    return MediaTsPcrObservation{
        .byteOffset = offset,
        .programNumber = 1,
        .pmtPid = 0x100,
        .pcrPid = 0x101,
        .videoPid = 0x201,
        .audioPid = 0x202,
        .pcr27Mhz = raw,
        .discontinuity = discontinuity};
}

void testEvidenceTimeline(TestContext& ctx)
{
    auto created = MediaTsEvidenceTimeline::create(3, 200);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto timeline = std::move(created.value());

    EXPECT_TRUE(ctx, timeline.append(checkpoint(100, 1)));
    EXPECT_TRUE(ctx, timeline.append(checkpoint(200, 2)));
    EXPECT_TRUE(ctx, timeline.append(checkpoint(300, 3)));
    auto exact = timeline.atOrBefore(200);
    auto earlier = timeline.atOrBefore(250);
    auto later = timeline.atOrBefore(400);
    EXPECT_TRUE(ctx, exact);
    EXPECT_TRUE(ctx, earlier);
    EXPECT_TRUE(ctx, later);
    if (exact) EXPECT_EQ(ctx, exact.value().generation, std::uint64_t{2});
    if (earlier) EXPECT_EQ(ctx, earlier.value().generation, std::uint64_t{2});
    if (later) EXPECT_EQ(ctx, later.value().generation, std::uint64_t{3});

    EXPECT_FALSE(ctx, timeline.append(checkpoint(300, 4)));
    EXPECT_FALSE(ctx, timeline.append(checkpoint(299, 4)));
    EXPECT_TRUE(ctx, timeline.observePacketPosition(300));
    EXPECT_TRUE(ctx, timeline.observePacketPosition(150));
    EXPECT_FALSE(ctx, timeline.observePacketPosition(99));

    EXPECT_FALSE(ctx, timeline.append(checkpoint(400, 4)));
    EXPECT_TRUE(ctx, timeline.observePacketPosition(500));
    EXPECT_TRUE(ctx, timeline.append(checkpoint(400, 4)));
    EXPECT_FALSE(ctx, timeline.atOrBefore(150));
    auto retained = timeline.atOrBefore(250);
    EXPECT_FALSE(ctx, retained);
}

void testEvidenceTimelineBoundaries(TestContext& ctx)
{
    auto one = MediaTsEvidenceTimeline::create(1, 100);
    EXPECT_TRUE(ctx, one);
    EXPECT_TRUE(ctx, one.value().append(checkpoint(100, 4)));
    EXPECT_TRUE(ctx, one.value().observePacketPosition(200));
    EXPECT_FALSE(ctx, one.value().append(checkpoint(300, 5)));
    auto exactBound = one.value().atOrBefore(100);
    EXPECT_TRUE(ctx, exactBound);
    EXPECT_FALSE(ctx, one.value().atOrBefore(99));

    auto predecessor = MediaTsEvidenceTimeline::create(3, 100);
    EXPECT_TRUE(ctx, predecessor);
    EXPECT_TRUE(ctx, predecessor.value().append(checkpoint(50, 1)));
    EXPECT_TRUE(ctx, predecessor.value().append(checkpoint(150, 2)));
    EXPECT_TRUE(ctx, predecessor.value().append(checkpoint(250, 3)));
    EXPECT_TRUE(ctx, predecessor.value().observePacketPosition(250));
    auto earliestLegal = predecessor.value().atOrBefore(150);
    EXPECT_TRUE(ctx, earliestLegal);
    if (earliestLegal) EXPECT_EQ(ctx, earliestLegal.value().generation, std::uint64_t{2});
}

void testProgramClockTracker(TestContext& ctx)
{
    auto created = MediaTsProgramClockTracker::create(clockPolicy(), 7);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto tracker = std::move(created.value());
    constexpr std::uint64_t pcrModulus = (std::uint64_t{1} << 33) * 300;
    EXPECT_TRUE(ctx, tracker.observe(pcr(pcrModulus - 1'000'000, 0)));
    EXPECT_TRUE(ctx, tracker.observe(pcr(1'700'100, 188)));
    EXPECT_TRUE(ctx, tracker.ready());
    EXPECT_EQ(ctx, tracker.generation(), std::uint64_t{7});
    auto calibration = tracker.calibration();
    EXPECT_TRUE(ctx, calibration);
    if (calibration) EXPECT_EQ(ctx, calibration.value().pcr27Mhz, static_cast<std::int64_t>(pcrModulus + 1'700'100));

    EXPECT_TRUE(ctx, tracker.observe(pcr(4'401'000, 376)));
    EXPECT_TRUE(ctx, tracker.observe(pcr(9'801'000, 564)));
    EXPECT_FALSE(ctx, tracker.observe(pcr(20'601'000, 752)));

    auto identity = MediaTsProgramClockTracker::create(clockPolicy(), 1);
    EXPECT_TRUE(ctx, identity);
    if (!identity) return;
    auto wrongProgram = pcr(1, 0);
    wrongProgram.programNumber = 2;
    EXPECT_FALSE(ctx, identity.value().observe(wrongProgram));
    auto wrongPid = pcr(1, 0);
    wrongPid.pcrPid = 0x102;
    EXPECT_FALSE(ctx, identity.value().observe(wrongPid));
    EXPECT_TRUE(ctx, identity.value().observe(pcr(1'000'000, 0)));
    EXPECT_FALSE(ctx, identity.value().observe(pcr(900'000, 188)));
    EXPECT_TRUE(ctx, identity.value().observe(pcr(3'700'000, 376)));
    EXPECT_FALSE(ctx, identity.value().observe(pcr(6'400'000, 376)));

    auto wrongElementary = pcr(1, 0);
    wrongElementary.videoPid = 0x211;
    EXPECT_FALSE(ctx, identity.value().observe(wrongElementary));
    EXPECT_TRUE(ctx, identity.value().observePcrContinuityLoss(0x777));
    EXPECT_TRUE(ctx, identity.value().ready());
    EXPECT_TRUE(ctx, identity.value().observeElementaryContinuityLoss(0x201));
    EXPECT_FALSE(ctx, identity.value().ready());

    auto events = MediaTsProgramClockTracker::create(clockPolicy(), 3);
    EXPECT_TRUE(ctx, events);
    if (!events) return;
    EXPECT_TRUE(ctx, events.value().observe(pcr(1'000'000, 0)));
    EXPECT_TRUE(ctx, events.value().observePcrContinuityLoss(0x101));
    EXPECT_FALSE(ctx, events.value().ready());
    EXPECT_EQ(ctx, events.value().generation(), std::uint64_t{4});
    EXPECT_TRUE(ctx, events.value().observe(pcr(2'000'000, 188, true)));
    EXPECT_EQ(ctx, events.value().generation(), std::uint64_t{5});
    EXPECT_FALSE(ctx, events.value().ready());
    EXPECT_TRUE(ctx, events.value().observe(pcr(4'700'000, 376)));
    EXPECT_TRUE(ctx, events.value().ready());
    EXPECT_FALSE(ctx, events.value().observeProgramIdentity(2, 0x100, 0x201, 0x202, 0x101));
    EXPECT_FALSE(ctx, events.value().observeProgramIdentity(1, 0x100, 0x201, 0x202, 0x102));

    auto atomic = MediaTsProgramClockTracker::create(clockPolicy(), 11);
    EXPECT_TRUE(ctx, atomic);
    EXPECT_TRUE(ctx, atomic.value().observe(pcr(10'000'000, 0)));
    EXPECT_TRUE(ctx, atomic.value().observe(pcr(12'700'000, 188)));
    auto beforeInvalid = atomic.value().calibration();
    auto invalidDiscontinuity = pcr((std::uint64_t{1} << 33) * 300, 376, true);
    EXPECT_FALSE(ctx, atomic.value().observe(invalidDiscontinuity));
    EXPECT_EQ(ctx, atomic.value().generation(), std::uint64_t{11});
    EXPECT_TRUE(ctx, atomic.value().ready());
    auto afterInvalid = atomic.value().calibration();
    EXPECT_TRUE(ctx, afterInvalid);
    if (beforeInvalid && afterInvalid)
        EXPECT_EQ(ctx, afterInvalid.value().pcr27Mhz, beforeInvalid.value().pcr27Mhz);

    auto continuous = MediaTsProgramClockTracker::create(clockPolicy(), 20);
    EXPECT_TRUE(ctx, continuous);
    EXPECT_TRUE(ctx, continuous.value().observe(pcr(100'000'000, 0)));
    EXPECT_TRUE(ctx, continuous.value().observe(pcr(102'700'000, 188)));
    auto oldTime = continuous.value().calibration().value().sourceTime;
    EXPECT_TRUE(ctx, continuous.value().observe(pcr(0, 376, true)));
    EXPECT_TRUE(ctx, continuous.value().observe(pcr(2'700'000, 564)));
    auto resetTime = continuous.value().calibration();
    EXPECT_TRUE(ctx, resetTime);
    if (resetTime) EXPECT_TRUE(ctx, resetTime.value().sourceTime >= oldTime);
    EXPECT_TRUE(ctx, continuous.value().observe(pcr(2'000'000'000, 752, true)));
    EXPECT_TRUE(ctx, continuous.value().observe(pcr(2'002'700'000, 940)));
    auto secondGeneration = continuous.value().calibration();
    EXPECT_TRUE(ctx, secondGeneration);
    if (resetTime && secondGeneration)
        EXPECT_TRUE(ctx, secondGeneration.value().sourceTime >= resetTime.value().sourceTime);

    auto exhausted = MediaTsProgramClockTracker::create(
        clockPolicy(), std::numeric_limits<std::uint64_t>::max());
    EXPECT_TRUE(ctx, exhausted);
    EXPECT_TRUE(ctx, exhausted.value().observe(pcr(0, 0)));
    EXPECT_FALSE(ctx, exhausted.value().observePcrContinuityLoss(0x101));
    EXPECT_EQ(ctx, exhausted.value().generation(), std::numeric_limits<std::uint64_t>::max());
}

void testSourceClockMapper(TestContext& ctx)
{
    MediaTsPcrCalibration anchor{
        .generation = 9,
        .pcr27Mhz = static_cast<std::int64_t>(((std::uint64_t{1} << 33) * 300) + 27'000'000),
        .sourceTime = MediaRunningTime::fromNanoseconds(-500'000'000)};
    auto created = MediaTsSourceClockMapper::create(anchor);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto mapper = std::move(created.value());

    auto independent = mapper.map((std::uint64_t{1} << 33) - 45'000,
                                  (std::uint64_t{1} << 33) - 90'000);
    EXPECT_TRUE(ctx, independent);
    if (independent) {
        EXPECT_TRUE(ctx, independent.value().presentationTime.has_value());
        EXPECT_TRUE(ctx, independent.value().decodeTime.has_value());
        if (independent.value().presentationTime)
            EXPECT_EQ(ctx, independent.value().presentationTime->nanoseconds(), std::int64_t{-2'000'000'000});
        if (independent.value().decodeTime)
            EXPECT_EQ(ctx, independent.value().decodeTime->nanoseconds(), std::int64_t{-2'500'000'000});
    }

    auto wrapped = mapper.map(45'000, std::nullopt);
    EXPECT_TRUE(ctx, wrapped);
    if (wrapped && wrapped.value().presentationTime)
        EXPECT_EQ(ctx, wrapped.value().presentationTime->nanoseconds(), std::int64_t{-1'000'000'000});
    EXPECT_FALSE(ctx, wrapped.value().decodeTime.has_value());

    EXPECT_FALSE(ctx, mapper.map(44'999, std::nullopt));
    auto afterRejectedRegression = mapper.map(135'000, std::nullopt);
    EXPECT_TRUE(ctx, afterRejectedRegression);
    if (afterRejectedRegression && afterRejectedRegression.value().presentationTime)
        EXPECT_EQ(ctx, afterRejectedRegression.value().presentationTime->nanoseconds(), std::int64_t{0});

    auto onlyDtsMapper = MediaTsSourceClockMapper::create(anchor);
    EXPECT_TRUE(ctx, onlyDtsMapper);
    auto onlyDts = onlyDtsMapper.value().map(std::nullopt, 0);
    EXPECT_TRUE(ctx, onlyDts);
    EXPECT_FALSE(ctx, onlyDts.value().presentationTime.has_value());
    EXPECT_TRUE(ctx, onlyDts.value().decodeTime.has_value());

    auto noAnchor = MediaTsSourceClockMapper::create(std::nullopt);
    EXPECT_FALSE(ctx, noAnchor);

    auto pairAtomic = MediaTsSourceClockMapper::create(anchor);
    EXPECT_TRUE(ctx, pairAtomic);
    EXPECT_FALSE(ctx, pairAtomic.value().map(0, std::uint64_t{1} << 33));
    auto retryPair = pairAtomic.value().map(0, 0);
    auto freshPair = MediaTsSourceClockMapper::create(anchor).value().map(0, 0);
    EXPECT_TRUE(ctx, retryPair);
    EXPECT_TRUE(ctx, freshPair);
    if (retryPair && freshPair) {
        EXPECT_EQ(ctx, retryPair.value().presentationTime,
                  freshPair.value().presentationTime);
        EXPECT_EQ(ctx, retryPair.value().decodeTime, freshPair.value().decodeTime);
    }

    MediaTsPcrCalibration halfAnchor{
        .generation = 1,
        .pcr27Mhz = ((std::int64_t{1} << 32) * 300),
        .sourceTime = MediaRunningTime::fromNanoseconds(0)};
    auto halfTie = MediaTsSourceClockMapper::create(halfAnchor).value().map(0, std::nullopt);
    EXPECT_TRUE(ctx, halfTie);
    if (halfTie && halfTie.value().presentationTime)
        EXPECT_EQ(ctx, halfTie.value().presentationTime->nanoseconds(),
                  MediaRunningTime::checkedFromTicks(-(std::int64_t{1} << 32), 1, 90'000).value().nanoseconds());

    auto dtsWrapMapper = MediaTsSourceClockMapper::create(anchor);
    EXPECT_TRUE(ctx, dtsWrapMapper);
    EXPECT_TRUE(ctx, dtsWrapMapper.value().map(std::nullopt, (std::uint64_t{1} << 33) - 1));
    EXPECT_TRUE(ctx, dtsWrapMapper.value().map(std::nullopt, 1));

    MediaTsPcrCalibration zeroAnchor{
        .generation = 2,
        .pcr27Mhz = 0,
        .sourceTime = MediaRunningTime::fromNanoseconds(0)};
    auto negativeEpoch = MediaTsSourceClockMapper::create(zeroAnchor).value().map(
        (std::uint64_t{1} << 33) - 1, std::nullopt);
    EXPECT_TRUE(ctx, negativeEpoch);
    if (negativeEpoch && negativeEpoch.value().presentationTime)
        EXPECT_EQ(ctx, negativeEpoch.value().presentationTime->nanoseconds(),
                  std::int64_t{-11'111});
}

} // namespace

void runMpegTsClockTests(TestContext& ctx)
{
    testEvidenceTimeline(ctx);
    testEvidenceTimelineBoundaries(ctx);
    testProgramClockTracker(ctx);
    testSourceClockMapper(ctx);
}
