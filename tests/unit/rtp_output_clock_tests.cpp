#include "common/TestAssert.h"

#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportGenerator.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <chrono>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

constexpr std::int64_t Second = 1'000'000'000;
constexpr std::uint64_t UnixToNtpSeconds = 2'208'988'800ULL;

static_assert(!std::is_constructible_v<
              MediaNtpTimestamp, std::uint64_t, std::uint32_t>);
static_assert(!std::is_constructible_v<
              MediaRtpTimestamp, std::uint64_t, std::uint32_t>);
static_assert(!std::is_default_constructible_v<
              MediaRtcpSenderReportCommitToken>);

auto parseStrict(std::span<const std::uint8_t> bytes)
{
    return MediaRtcpCompoundParser::parse(
        bytes,
        MediaRtcpCompoundPolicy{
            MediaRtcpCompositionMode::StrictCompoundRfc3550, true});
}

void testSharedNtpEpochBoundariesAndOverflow(TestContext& ctx)
{
    auto unixEpoch = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::nanoseconds(0));
    EXPECT_TRUE(ctx, unixEpoch);
    if (!unixEpoch) return;

    auto exact = unixEpoch.value().map(MediaRunningTime::fromNanoseconds(0));
    auto half = unixEpoch.value().map(MediaRunningTime::fromNanoseconds(Second / 2));
    EXPECT_TRUE(ctx, exact && half);
    if (exact) {
        EXPECT_EQ(ctx, exact.value().seconds(), UnixToNtpSeconds);
        EXPECT_EQ(ctx, exact.value().fraction(), static_cast<std::uint32_t>(0));
        EXPECT_EQ(ctx, exact.value().wire().seconds, static_cast<std::uint32_t>(UnixToNtpSeconds));
    }
    if (half) {
        EXPECT_EQ(ctx, half.value().seconds(), UnixToNtpSeconds);
        EXPECT_EQ(ctx, half.value().fraction(), 0x80000000u);
    }

    auto negativeUnix = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::nanoseconds(-1));
    EXPECT_TRUE(ctx, negativeUnix);
    if (negativeUnix) {
        auto mapped = negativeUnix.value().map(MediaRunningTime::fromNanoseconds(0));
        EXPECT_TRUE(ctx, mapped);
        if (mapped) {
            EXPECT_EQ(ctx, mapped.value().seconds(), UnixToNtpSeconds - 1);
            EXPECT_EQ(ctx, mapped.value().fraction(), 4'294'967'291u);
        }
    }

    constexpr std::int64_t EraWrapUnixSeconds =
        static_cast<std::int64_t>((std::uint64_t{1} << 32) - UnixToNtpSeconds);
    auto era = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0),
        std::chrono::nanoseconds(EraWrapUnixSeconds * Second));
    EXPECT_TRUE(ctx, era);
    if (era) {
        auto mapped = era.value().map(MediaRunningTime::fromNanoseconds(0));
        EXPECT_TRUE(ctx, mapped);
        if (mapped) {
            EXPECT_EQ(ctx, mapped.value().seconds(), std::uint64_t{1} << 32);
            EXPECT_EQ(ctx, mapped.value().wire().seconds, 0u);
        }
    }

    const auto beforeNtp = -(static_cast<std::int64_t>(UnixToNtpSeconds) * Second) - 1;
    EXPECT_FALSE(ctx, MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::nanoseconds(beforeNtp)));

    auto nearMaximum = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0),
        std::chrono::nanoseconds(std::numeric_limits<std::int64_t>::max()));
    EXPECT_TRUE(ctx, nearMaximum);
    if (nearMaximum) {
        EXPECT_FALSE(ctx, nearMaximum.value().map(MediaRunningTime::fromNanoseconds(1)));
    }
}

void testRtpClockMappingRatesWrapAndFailures(TestContext& ctx)
{
    auto video = MediaRtpOutputClockMapper::create(
        90'000, 0xFFFFFF00u, MediaRunningTime::fromNanoseconds(1'000));
    auto audio = MediaRtpOutputClockMapper::create(
        48'000, 0u, MediaRunningTime::fromNanoseconds(1'000));
    EXPECT_TRUE(ctx, video && audio);
    if (!video || !audio) return;

    auto videoExact = video.value().map(MediaRunningTime::fromNanoseconds(Second + 1'000));
    auto videoWrap = video.value().map(MediaRunningTime::fromNanoseconds(2 * Second + 1'000));
    auto audioExact = audio.value().map(MediaRunningTime::fromNanoseconds(Second + 1'000));
    EXPECT_TRUE(ctx, videoExact && videoWrap && audioExact);
    if (videoExact) {
        EXPECT_EQ(ctx, videoExact.value().extendedTicks(), 4'295'057'040ULL);
        EXPECT_EQ(ctx, videoExact.value().wire(), 89'744u);
    }
    if (videoWrap) {
        EXPECT_EQ(ctx, videoWrap.value().extendedTicks(), 4'295'147'040ULL);
        EXPECT_EQ(ctx, videoWrap.value().wire(), 179'744u);
    }
    if (audioExact) {
        EXPECT_EQ(ctx, audioExact.value().extendedTicks(), 48'000);
        EXPECT_EQ(ctx, audioExact.value().wire(), 48'000u);
    }

    EXPECT_FALSE(ctx, video.value().map(MediaRunningTime::fromNanoseconds(999)));
    EXPECT_FALSE(ctx, MediaRtpOutputClockMapper::create(
        0, 0, MediaRunningTime::fromNanoseconds(0)));
    auto overflow = MediaRtpOutputClockMapper::create(
        std::numeric_limits<int>::max(), 0,
        MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, overflow);
    if (overflow) {
        EXPECT_FALSE(ctx, overflow.value().map(
            MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max())));
    }
}

void testSenderReportSerializationAndCorrespondence(TestContext& ctx)
{
    auto epoch = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::nanoseconds(0));
    auto video = MediaRtpOutputClockMapper::create(
        90'000, 100u, MediaRunningTime::fromNanoseconds(0));
    auto audio = MediaRtpOutputClockMapper::create(
        48'000, 200u, MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, epoch && video && audio);
    if (!epoch || !video || !audio) return;

    const MediaRunningTime instant = MediaRunningTime::fromNanoseconds(2 * Second);
    auto videoTime = MediaRtcpSenderReportGenerator::mapTimestamp(
        instant, epoch.value(), video.value());
    auto audioTime = MediaRtcpSenderReportGenerator::mapTimestamp(
        instant, epoch.value(), audio.value());
    EXPECT_TRUE(ctx, videoTime && audioTime);
    if (!videoTime || !audioTime) return;
    EXPECT_EQ(ctx, videoTime.value().ntp(), audioTime.value().ntp());
    EXPECT_EQ(ctx, videoTime.value().rtp().wire(), 180'100u);
    EXPECT_EQ(ctx, audioTime.value().rtp().wire(), 96'200u);

    MediaRtcpSenderReportParameters videoParameters(
        0x11223344u, "shared-cname", videoTime.value(), 7u, 900u);
    auto bytes = MediaRtcpSenderReportGenerator::serialize(videoParameters);
    EXPECT_TRUE(ctx, bytes);
    if (!bytes) return;
    EXPECT_EQ(ctx, bytes.value().size(), static_cast<std::size_t>(52));
    EXPECT_EQ(ctx, bytes.value()[0], 0x80u);
    EXPECT_EQ(ctx, bytes.value()[1], 200u);
    EXPECT_EQ(ctx, bytes.value()[2], 0u);
    EXPECT_EQ(ctx, bytes.value()[3], 6u);
    const std::vector<std::uint8_t> expected{
        0x80, 0xC8, 0x00, 0x06,
        0x11, 0x22, 0x33, 0x44,
        0x83, 0xAA, 0x7E, 0x82,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x02, 0xBF, 0x84,
        0x00, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x03, 0x84,
        0x81, 0xCA, 0x00, 0x05,
        0x11, 0x22, 0x33, 0x44,
        0x01, 0x0C,
        's', 'h', 'a', 'r', 'e', 'd', '-', 'c', 'n', 'a', 'm', 'e',
        0x00, 0x00};
    EXPECT_EQ(ctx, bytes.value(), expected);

    auto parsed = parseStrict(bytes.value());
    EXPECT_TRUE(ctx, parsed);
    if (parsed) {
        EXPECT_EQ(ctx, parsed.value().size(), static_cast<std::size_t>(2));
        const auto& report = *parsed.value()[0].senderReport;
        EXPECT_EQ(ctx, report.ssrc, 0x11223344u);
        EXPECT_EQ(ctx, report.ntp.seconds, static_cast<std::uint32_t>(UnixToNtpSeconds + 2));
        EXPECT_EQ(ctx, report.ntp.fraction, 0u);
        EXPECT_EQ(ctx, report.rtpTimestamp, 180'100u);
        EXPECT_EQ(ctx, report.senderPacketCount, 7u);
        EXPECT_EQ(ctx, report.senderOctetCount, 900u);
        EXPECT_EQ(ctx, parsed.value()[1].sdesChunks[0].ssrc, 0x11223344u);
        EXPECT_EQ(ctx, std::string(parsed.value()[1].sdesChunks[0].items[0].value.begin(),
                                   parsed.value()[1].sdesChunks[0].items[0].value.end()),
                  std::string("shared-cname"));
    }

    auto withBye = MediaRtcpSenderReportGenerator::serializeWithBye(videoParameters);
    EXPECT_TRUE(ctx, withBye);
    if (withBye) {
        auto parsedBye = parseStrict(withBye.value());
        EXPECT_TRUE(ctx, parsedBye);
        if (parsedBye) {
            EXPECT_EQ(ctx, parsedBye.value().size(), static_cast<std::size_t>(3));
            EXPECT_EQ(ctx, parsedBye.value()[2].kind, MediaRtcpPacketKind::Bye);
            EXPECT_EQ(ctx, parsedBye.value()[2].byeSources[0], 0x11223344u);
        }
    }

    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(0u, "shared-cname", videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(1u, "", videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(1u, std::string(256, 'x'), videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(1u, std::string("bad\nname"), videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            1u, "shared-cname", videoTime.value(),
            std::uint64_t{1} << 32, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            1u, "shared-cname", videoTime.value(),
            0u, std::uint64_t{1} << 32)));

    const std::string unicodeCname("sync-\xE5\x90\x8C\xE6\xAD\xA5");
    EXPECT_TRUE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            1u, unicodeCname, videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            1u, std::string("\xC0\xAF", 2), videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            1u, std::string("\xE5\x90", 2), videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            1u, std::string("\xED\xA0\x80", 3), videoTime.value(), 0u, 0u)));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportGenerator::serialize(
        MediaRtcpSenderReportParameters(
            1u, std::string("\xC2\x80", 2), videoTime.value(), 0u, 0u)));
}

void testSenderReportScheduleAlignmentLatenessAndGeneration(TestContext& ctx)
{
    auto schedule = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(10),
        MediaRunningTime::fromNanoseconds(10),
        MediaRunningTime::fromNanoseconds(20), 1);
    EXPECT_TRUE(ctx, schedule);
    if (!schedule) return;
    EXPECT_EQ(ctx, schedule.value().nextDeadline(), MediaRunningTime::fromNanoseconds(10));

    auto early = schedule.value().prepare(MediaRunningTime::fromNanoseconds(9), 1);
    EXPECT_TRUE(ctx, early);
    if (early) EXPECT_FALSE(ctx, early.value().has_value());
    EXPECT_EQ(ctx, schedule.value().nextDeadline(), MediaRunningTime::fromNanoseconds(10));

    auto exact = schedule.value().prepare(MediaRunningTime::fromNanoseconds(10), 1);
    EXPECT_TRUE(ctx, exact);
    if (exact && exact.value()) {
        EXPECT_EQ(ctx, exact.value()->scheduledDeadline, MediaRunningTime::fromNanoseconds(10));
        EXPECT_EQ(ctx, exact.value()->reportInstant, MediaRunningTime::fromNanoseconds(10));
        EXPECT_EQ(ctx, exact.value()->nextDeadline, MediaRunningTime::fromNanoseconds(20));
        EXPECT_EQ(ctx, exact.value()->commitToken.generation(), static_cast<std::uint64_t>(1));
        EXPECT_EQ(ctx, exact.value()->commitToken.expectedDeadline(), MediaRunningTime::fromNanoseconds(10));
    }
    EXPECT_EQ(ctx, schedule.value().nextDeadline(), MediaRunningTime::fromNanoseconds(10));
    auto exactRetry = schedule.value().prepare(MediaRunningTime::fromNanoseconds(10), 1);
    EXPECT_TRUE(ctx, exactRetry && exactRetry.value());
    if (!exact || !exact.value() || !exactRetry || !exactRetry.value()) return;
    EXPECT_TRUE(ctx, schedule.value().commit(exact.value()->commitToken));
    EXPECT_EQ(ctx, schedule.value().nextDeadline(), MediaRunningTime::fromNanoseconds(20));
    EXPECT_FALSE(ctx, schedule.value().commit(exact.value()->commitToken));
    EXPECT_FALSE(ctx, schedule.value().commit(exactRetry.value()->commitToken));

    auto late = schedule.value().prepare(MediaRunningTime::fromNanoseconds(35), 1);
    EXPECT_TRUE(ctx, late);
    if (late && late.value()) {
        EXPECT_EQ(ctx, late.value()->scheduledDeadline, MediaRunningTime::fromNanoseconds(20));
        EXPECT_EQ(ctx, late.value()->reportInstant, MediaRunningTime::fromNanoseconds(35));
        EXPECT_EQ(ctx, late.value()->nextDeadline, MediaRunningTime::fromNanoseconds(40));
        EXPECT_EQ(ctx, late.value()->skippedIntervals, static_cast<std::uint64_t>(1));
    }
    EXPECT_EQ(ctx, schedule.value().nextDeadline(), MediaRunningTime::fromNanoseconds(20));
    if (!late || !late.value()) return;
    EXPECT_TRUE(ctx, schedule.value().commit(late.value()->commitToken));
    auto noBurst = schedule.value().prepare(MediaRunningTime::fromNanoseconds(35), 1);
    EXPECT_TRUE(ctx, noBurst);
    if (noBurst) EXPECT_FALSE(ctx, noBurst.value().has_value());

    EXPECT_FALSE(ctx, schedule.value().prepare(MediaRunningTime::fromNanoseconds(61), 1));
    EXPECT_EQ(ctx, schedule.value().nextDeadline(), MediaRunningTime::fromNanoseconds(40));
    EXPECT_FALSE(ctx, schedule.value().prepare(MediaRunningTime::fromNanoseconds(40), 2));
    EXPECT_FALSE(ctx, schedule.value().reset(
        MediaRunningTime::fromNanoseconds(50), 1));
    auto stale = schedule.value().prepare(MediaRunningTime::fromNanoseconds(40), 1);
    EXPECT_TRUE(ctx, stale && stale.value());
    EXPECT_TRUE(ctx, schedule.value().reset(
        MediaRunningTime::fromNanoseconds(50), 2));
    EXPECT_EQ(ctx, schedule.value().generation(), static_cast<std::uint64_t>(2));
    EXPECT_EQ(ctx, schedule.value().nextDeadline(), MediaRunningTime::fromNanoseconds(50));
    if (stale && stale.value()) {
        EXPECT_FALSE(ctx, schedule.value().commit(stale.value()->commitToken));
    }

    EXPECT_FALSE(ctx, MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(1), 1));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(-1), 1));
    EXPECT_FALSE(ctx, MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(1), 0));

    auto overflow = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(
            std::numeric_limits<std::int64_t>::max() - 5),
        MediaRunningTime::fromNanoseconds(10),
        MediaRunningTime::fromNanoseconds(10), 1);
    EXPECT_TRUE(ctx, overflow);
    if (overflow) {
        EXPECT_FALSE(ctx, overflow.value().prepare(
            MediaRunningTime::fromNanoseconds(
                std::numeric_limits<std::int64_t>::max() - 5), 1));
        EXPECT_EQ(ctx, overflow.value().nextDeadline(),
                  MediaRunningTime::fromNanoseconds(
                      std::numeric_limits<std::int64_t>::max() - 5));
    }

    auto extreme = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(
            std::numeric_limits<std::int64_t>::max()), 1);
    EXPECT_TRUE(ctx, extreme);
    if (extreme) {
        EXPECT_FALSE(ctx, extreme.value().prepare(
            MediaRunningTime::fromNanoseconds(
                std::numeric_limits<std::int64_t>::max()), 1));
        EXPECT_EQ(ctx, extreme.value().nextDeadline(),
                  MediaRunningTime::fromNanoseconds(0));
    }
}

} // namespace

void runRtpOutputClockTests(TestContext& ctx)
{
    testSharedNtpEpochBoundariesAndOverflow(ctx);
    testRtpClockMappingRatesWrapAndFailures(ctx);
    testSenderReportSerializationAndCorrespondence(ctx);
    testSenderReportScheduleAlignmentLatenessAndGeneration(ctx);
}
