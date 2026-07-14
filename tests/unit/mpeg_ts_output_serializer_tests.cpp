#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsCrc32.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsTimestampFieldSerializer.h"

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaTsMuxPlan serializerPlan()
{
    auto plan = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x0100, 0x0101, 0x0102, 0x0101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, 4,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp});
    return std::move(plan).value();
}

class InventorySink final : public MediaTsProgramInventorySink {
public:
    ::media::Status onProgramInventory(MediaTsProgramInventorySnapshot snapshot) override
    {
        inventory = std::move(snapshot);
        return ::media::Status::success();
    }

    std::optional<MediaTsProgramInventorySnapshot> inventory;
};

void testProgramTableGoldenBytesAndParserCrossCheck(TestContext& ctx)
{
    const std::array<std::uint8_t, 16> expectedPat{
        0x00, 0xB0, 0x0D, 0x00, 0x01, 0xC1, 0x00, 0x00,
        0x00, 0x01, 0xE1, 0x00, 0xE8, 0xF9, 0x5E, 0x7D};
    const std::array<std::uint8_t, 26> expectedPmt{
        0x02, 0xB0, 0x17, 0x00, 0x01, 0xC1, 0x00, 0x00,
        0xE1, 0x01, 0xF0, 0x00,
        0x1B, 0xE1, 0x01, 0xF0, 0x00,
        0x0F, 0xE1, 0x02, 0xF0, 0x00,
        0x9E, 0x28, 0xC6, 0xDD};

    auto tables = MediaTsPsiSerializer::serialize(serializerPlan());
    EXPECT_TRUE(ctx, tables);
    if (!tables) return;
    EXPECT_TRUE(ctx, std::equal(tables.value().pat.begin(), tables.value().pat.end(),
                               expectedPat.begin(), expectedPat.end()));
    EXPECT_TRUE(ctx, std::equal(tables.value().pmt.begin(), tables.value().pmt.end(),
                               expectedPmt.begin(), expectedPmt.end()));
    EXPECT_EQ(ctx, MediaTsCrc32::compute(tables.value().pat), std::uint32_t{0});
    EXPECT_EQ(ctx, MediaTsCrc32::compute(tables.value().pmt), std::uint32_t{0});

    InventorySink sink;
    MediaTsPsiSectionAssembler assembler(sink);
    std::vector<std::uint8_t> patPayload{0};
    patPayload.insert(patPayload.end(), tables.value().pat.begin(), tables.value().pat.end());
    std::vector<std::uint8_t> pmtPayload{0};
    pmtPayload.insert(pmtPayload.end(), tables.value().pmt.begin(), tables.value().pmt.end());
    EXPECT_TRUE(ctx, assembler.onPacket(MediaTsPacketView{
        0, 0, true, 0, false, std::nullopt, patPayload}));
    EXPECT_TRUE(ctx, assembler.onPacket(MediaTsPacketView{
        188, 0x0100, true, 0, false, std::nullopt, pmtPayload}));
    EXPECT_TRUE(ctx, sink.inventory.has_value());
    if (!sink.inventory) return;
    EXPECT_EQ(ctx, sink.inventory->patVersion, std::uint8_t{0});
    EXPECT_EQ(ctx, sink.inventory->programs.size(), std::size_t{1});
    const auto& program = sink.inventory->programs.front();
    EXPECT_EQ(ctx, program.programNumber, std::uint16_t{1});
    EXPECT_EQ(ctx, program.pmtPid, std::uint16_t{0x0100});
    EXPECT_EQ(ctx, program.pcrPid, std::uint16_t{0x0101});
    EXPECT_EQ(ctx, program.elementaryStreams.size(), std::size_t{2});
    EXPECT_EQ(ctx, program.elementaryStreams[0], (MediaTsElementaryStreamInfo{0x0101, 0x1B}));
    EXPECT_EQ(ctx, program.elementaryStreams[1], (MediaTsElementaryStreamInfo{0x0102, 0x0F}));
}

void testPesTimestampGoldenBytes(TestContext& ctx)
{
    const MediaTsPacketClock ptsOnly{0x1234567, 0x1234567, 0x1234567, 0x1234567};
    auto audio = MediaTsPesSerializer::header(MediaScheduledStream::Audio, ptsOnly, 100);
    EXPECT_TRUE(ctx, audio);
    if (audio) {
        const std::array<std::uint8_t, 14> expected{
            0x00, 0x00, 0x01, 0xC0, 0x00, 0x6C, 0x80, 0x80, 0x05,
            0x21, 0x04, 0x8D, 0x8A, 0xCF};
        EXPECT_EQ(ctx, audio.value().size, expected.size());
        EXPECT_TRUE(ctx, std::equal(expected.begin(), expected.end(), audio.value().bytes.begin()));
    }

    const MediaTsPacketClock reordered{0x1234567, 0x1020304, 0x1234567, 0x1020304};
    auto video = MediaTsPesSerializer::header(MediaScheduledStream::Video, reordered, 5);
    EXPECT_TRUE(ctx, video);
    if (video) {
        const std::array<std::uint8_t, 19> expected{
            0x00, 0x00, 0x01, 0xE0, 0x00, 0x00, 0x80, 0xC0, 0x0A,
            0x31, 0x04, 0x8D, 0x8A, 0xCF,
            0x11, 0x04, 0x09, 0x06, 0x09};
        EXPECT_EQ(ctx, video.value().size, expected.size());
        EXPECT_TRUE(ctx, std::equal(expected.begin(), expected.end(), video.value().bytes.begin()));
    }

    const auto wrap = (std::uint64_t{1} << 33) - 1;
    const MediaTsPacketClock wrapped{static_cast<std::int64_t>(wrap), static_cast<std::int64_t>(wrap), wrap, wrap};
    auto wrapHeader = MediaTsPesSerializer::header(MediaScheduledStream::Audio, wrapped, 0);
    EXPECT_TRUE(ctx, wrapHeader);
    if (wrapHeader) {
        const std::array<std::uint8_t, 5> expected{0x2F, 0xFF, 0xFF, 0xFF, 0xFF};
        EXPECT_TRUE(ctx, std::equal(expected.begin(), expected.end(), wrapHeader.value().bytes.begin() + 9));
    }
}

void testPesRejectsInvalidStreamClockAndAudioLengthOverflow(TestContext& ctx)
{
    const MediaTsPacketClock clock{10, 10, 10, 10};
    EXPECT_FALSE(ctx, MediaTsPesSerializer::header(
        static_cast<MediaScheduledStream>(99), clock, 1));
    const MediaTsPacketClock mismatch{10, 10, 10, 11};
    EXPECT_FALSE(ctx, MediaTsPesSerializer::header(MediaScheduledStream::Audio, mismatch, 1));
    const MediaTsPacketClock dtsAfterPts{10, 11, 10, 11};
    EXPECT_FALSE(ctx, MediaTsPesSerializer::header(MediaScheduledStream::Video, dtsAfterPts, 1));
    EXPECT_TRUE(ctx, MediaTsPesSerializer::header(MediaScheduledStream::Audio, clock, 65'527));
    EXPECT_FALSE(ctx, MediaTsPesSerializer::header(MediaScheduledStream::Audio, clock, 65'528));
    const MediaTsPacketClock reordered{11, 10, 11, 10};
    EXPECT_TRUE(ctx, MediaTsPesSerializer::header(MediaScheduledStream::Audio, reordered, 65'522));
    EXPECT_FALSE(ctx, MediaTsPesSerializer::header(MediaScheduledStream::Audio, reordered, 65'523));
    EXPECT_FALSE(ctx, MediaTsPesSerializer::header(
        MediaScheduledStream::Audio, clock, std::numeric_limits<std::size_t>::max()));
}

void testTimestampFieldRejectsInvalidPrefixAndOutOfRangeWireValue(TestContext& ctx)
{
    EXPECT_FALSE(ctx, MediaTsTimestampFieldSerializer::serialize(0, 0));
    EXPECT_FALSE(ctx, MediaTsTimestampFieldSerializer::serialize(
        2, std::uint64_t{1} << 33));
    auto field = MediaTsTimestampFieldSerializer::serialize(
        3, (std::uint64_t{1} << 33) - 1);
    EXPECT_TRUE(ctx, field);
    if (field) {
        EXPECT_EQ(ctx, field.value()[0] & 0xF1, std::uint8_t{0x31});
        EXPECT_EQ(ctx, field.value()[2] & 0x01, std::uint8_t{1});
        EXPECT_EQ(ctx, field.value()[4] & 0x01, std::uint8_t{1});
    }
}

void testPesUsesExtendedTimestampsAcrossEqualWireWrap(TestContext& ctx)
{
    constexpr std::int64_t Wrap = std::int64_t{1} << 33;
    const MediaTsPacketClock clock{Wrap + 5, 5, 5, 5};
    auto header = MediaTsPesSerializer::header(
        MediaScheduledStream::Video, clock, 1);
    EXPECT_TRUE(ctx, header);
    if (!header) return;
    EXPECT_EQ(ctx, header.value().size, std::size_t{19});
    EXPECT_EQ(ctx, header.value().bytes[7], std::uint8_t{0xC0});
    EXPECT_EQ(ctx, header.value().bytes[8], std::uint8_t{10});
    EXPECT_EQ(ctx, header.value().bytes[9] >> 4, std::uint8_t{3});
    EXPECT_EQ(ctx, header.value().bytes[14] >> 4, std::uint8_t{1});
    EXPECT_EQ(ctx, header.value().bytes[9] & 0x0F,
              header.value().bytes[14] & 0x0F);
    EXPECT_TRUE(ctx, std::equal(
        header.value().bytes.begin() + 10,
        header.value().bytes.begin() + 14,
        header.value().bytes.begin() + 15));
}

void testPesAcceptsNegativeExtendedTimestampPositiveModulo(TestContext& ctx)
{
    constexpr std::uint64_t WrappedNegativeOne =
        (std::uint64_t{1} << 33) - 1;
    const MediaTsPacketClock valid{-1, -1, WrappedNegativeOne,
                                   WrappedNegativeOne};
    auto header = MediaTsPesSerializer::header(
        MediaScheduledStream::Audio, valid, 1);
    EXPECT_TRUE(ctx, header);
    if (header) {
        EXPECT_EQ(ctx, header.value().size, std::size_t{14});
        const std::array<std::uint8_t, 5> expected{
            0x2F, 0xFF, 0xFF, 0xFF, 0xFF};
        EXPECT_TRUE(ctx, std::equal(
            expected.begin(), expected.end(), header.value().bytes.begin() + 9));
    }

    const MediaTsPacketClock mismatched{-1, -1, WrappedNegativeOne - 1,
                                        WrappedNegativeOne};
    EXPECT_FALSE(ctx, MediaTsPesSerializer::header(
        MediaScheduledStream::Audio, mismatched, 1));
}

} // namespace

void runMpegTsOutputSerializerTests(TestContext& ctx)
{
    testProgramTableGoldenBytesAndParserCrossCheck(ctx);
    testPesTimestampGoldenBytes(ctx);
    testPesRejectsInvalidStreamClockAndAudioLengthOverflow(ctx);
    testTimestampFieldRejectsInvalidPrefixAndOutOfRangeWireValue(ctx);
    testPesUsesExtendedTimestampsAcrossEqualWireWrap(ctx);
    testPesAcceptsNegativeExtendedTimestampPositiveModulo(ctx);
}
