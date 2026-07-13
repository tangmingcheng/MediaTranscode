#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"
#include "internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h"

#include <cstdint>
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

    auto events = MediaTsProgramClockTracker::create(clockPolicy(), 3);
    EXPECT_TRUE(ctx, events);
    if (!events) return;
    EXPECT_TRUE(ctx, events.value().observe(pcr(1'000'000, 0)));
    EXPECT_TRUE(ctx, events.value().observeContinuityLoss(0x101));
    EXPECT_FALSE(ctx, events.value().ready());
    EXPECT_EQ(ctx, events.value().generation(), std::uint64_t{4});
    EXPECT_TRUE(ctx, events.value().observe(pcr(2'000'000, 188, true)));
    EXPECT_EQ(ctx, events.value().generation(), std::uint64_t{5});
    EXPECT_FALSE(ctx, events.value().ready());
    EXPECT_TRUE(ctx, events.value().observe(pcr(4'700'000, 376)));
    EXPECT_TRUE(ctx, events.value().ready());
    EXPECT_FALSE(ctx, events.value().observeProgramIdentity(2, 0x100, 0x101));
    EXPECT_FALSE(ctx, events.value().observeProgramIdentity(1, 0x100, 0x102));
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
}

} // namespace

void runMpegTsClockTests(TestContext& ctx)
{
    testEvidenceTimeline(ctx);
    testProgramClockTracker(ctx);
    testSourceClockMapper(ctx);
}
