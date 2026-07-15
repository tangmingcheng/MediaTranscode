#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaTsMuxPlan framerPlan(MediaTsH264InputLayout layout,
                          MediaTsParameterSetPolicy policy,
                          std::uint8_t nalLengthBytes = 4)
{
    auto plan = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x100, 0x101, 0x102, 0x101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        layout, nalLengthBytes, policy, MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024});
    return std::move(plan).value();
}

MediaTsMaterializedVideoConfig videoConfig(MediaTsH264InputLayout layout,
                                            std::uint8_t nalLengthBytes)
{
    return MediaTsMaterializedVideoConfig::create(
        layout, nalLengthBytes,
        {0x00, 0x00, 0x00, 0x01, 0x67, 0x64},
        {0x00, 0x00, 0x00, 0x01, 0x68, 0xEE}).value();
}

void testAnnexBIsBorrowedAndInjectionIsExplicit(TestContext& ctx)
{
    const std::array<std::uint8_t, 6> payload{0, 0, 0, 1, 0x65, 0x88};
    auto never = framerPlan(MediaTsH264InputLayout::AnnexB, MediaTsParameterSetPolicy::Never);
    auto config = videoConfig(MediaTsH264InputLayout::AnnexB, 4);
    auto borrowed = MediaTsH264AccessUnitFramer::frame(never, config, payload, true);
    EXPECT_TRUE(ctx, borrowed);
    if (borrowed) {
        EXPECT_EQ(ctx, borrowed.value().bytes().data(), payload.data());
        EXPECT_EQ(ctx, borrowed.value().bytes().size(), payload.size());
    }

    auto injectPlan = framerPlan(
        MediaTsH264InputLayout::AnnexB,
        MediaTsParameterSetPolicy::BeforeRandomAccess);
    auto nonRandom = MediaTsH264AccessUnitFramer::frame(
        injectPlan, config, payload, false);
    EXPECT_TRUE(ctx, nonRandom);
    if (nonRandom) EXPECT_EQ(ctx, nonRandom.value().bytes().data(), payload.data());
    auto injected = MediaTsH264AccessUnitFramer::frame(
        injectPlan, config, payload, true);
    EXPECT_TRUE(ctx, injected);
    if (injected) {
        const std::array<std::uint8_t, 18> expected{
            0, 0, 0, 1, 0x67, 0x64, 0, 0, 0, 1, 0x68, 0xEE,
            0, 0, 0, 1, 0x65, 0x88};
        EXPECT_EQ(ctx, injected.value().bytes().size(), expected.size());
        EXPECT_TRUE(ctx, std::equal(expected.begin(), expected.end(), injected.value().bytes().begin()));
        EXPECT_FALSE(ctx, injected.value().bytes().data() == payload.data());
    }
}

void testLengthPrefixConversionUsesExactOwnedStorage(TestContext& ctx)
{
    const std::array<std::uint8_t, 13> payload{
        0, 0, 0, 2, 0x65, 0x88,
        0, 0, 0, 3, 0x41, 0xAA, 0xBB};
    auto plan = framerPlan(
        MediaTsH264InputLayout::LengthPrefixed,
        MediaTsParameterSetPolicy::Never);
    auto framed = MediaTsH264AccessUnitFramer::frame(
        plan, videoConfig(MediaTsH264InputLayout::LengthPrefixed, 4), payload, false);
    EXPECT_TRUE(ctx, framed);
    if (!framed) return;
    const std::array<std::uint8_t, 13> expected{
        0, 0, 0, 1, 0x65, 0x88,
        0, 0, 0, 1, 0x41, 0xAA, 0xBB};
    EXPECT_EQ(ctx, framed.value().bytes().size(), expected.size());
    EXPECT_TRUE(ctx, std::equal(expected.begin(), expected.end(), framed.value().bytes().begin()));
    EXPECT_FALSE(ctx, framed.value().bytes().data() == payload.data());
}

void testLengthPrefixInjectionUsesExplicitRandomAccess(TestContext& ctx)
{
    const std::array<std::uint8_t, 6> payload{0, 0, 0, 2, 0x41, 0x88};
    auto plan = framerPlan(
        MediaTsH264InputLayout::LengthPrefixed,
        MediaTsParameterSetPolicy::BeforeRandomAccess);
    auto config = videoConfig(MediaTsH264InputLayout::LengthPrefixed, 4);
    auto nonRandom = MediaTsH264AccessUnitFramer::frame(
        plan, config, payload, false);
    EXPECT_TRUE(ctx, nonRandom);
    if (nonRandom) EXPECT_EQ(ctx, nonRandom.value().bytes().size(), std::size_t{6});
    auto explicitRandom = MediaTsH264AccessUnitFramer::frame(
        plan, config, payload, true);
    EXPECT_TRUE(ctx, explicitRandom);
    if (explicitRandom) {
        EXPECT_EQ(ctx, explicitRandom.value().bytes().size(), std::size_t{18});
        EXPECT_EQ(ctx, explicitRandom.value().bytes()[4] & 0x1F, std::uint8_t{7});
        EXPECT_EQ(ctx, explicitRandom.value().bytes()[10] & 0x1F, std::uint8_t{8});
        EXPECT_EQ(ctx, explicitRandom.value().bytes()[16] & 0x1F, std::uint8_t{1});
    }
}

void testH264RejectsMalformedOrMismatchedContracts(TestContext& ctx)
{
    auto plan = framerPlan(MediaTsH264InputLayout::LengthPrefixed,
                           MediaTsParameterSetPolicy::BeforeRandomAccess);
    auto config = videoConfig(MediaTsH264InputLayout::LengthPrefixed, 4);
    const std::array<std::uint8_t, 5> truncated{0, 0, 0, 2, 0x65};
    const std::array<std::uint8_t, 4> zeroNal{0, 0, 0, 0};
    EXPECT_FALSE(ctx, MediaTsH264AccessUnitFramer::frame(plan, config, truncated, false));
    EXPECT_FALSE(ctx, MediaTsH264AccessUnitFramer::frame(plan, config, zeroNal, false));
    EXPECT_FALSE(ctx, MediaTsH264AccessUnitFramer::frame(plan, config, {}, false));
    auto mismatchLayout = videoConfig(MediaTsH264InputLayout::AnnexB, 4);
    EXPECT_FALSE(ctx, MediaTsH264AccessUnitFramer::frame(plan, mismatchLayout, truncated, false));
    auto mismatchWidth = videoConfig(MediaTsH264InputLayout::LengthPrefixed, 3);
    EXPECT_FALSE(ctx, MediaTsH264AccessUnitFramer::frame(plan, mismatchWidth, truncated, false));
    EXPECT_FALSE(ctx, MediaTsMaterializedVideoConfig::create(
        MediaTsH264InputLayout::LengthPrefixed, 4, {},
        {0, 0, 0, 1, 0x68}));
    EXPECT_FALSE(ctx, MediaTsMaterializedVideoConfig::create(
        MediaTsH264InputLayout::LengthPrefixed, 4,
        {0, 0, 0, 1, 0x67}, {0, 0, 0, 1, 0x67}));
    const std::array<std::uint8_t, 5> forbidden{0, 0, 0, 1, 0xE5};
    EXPECT_FALSE(ctx, MediaTsH264AccessUnitFramer::frame(plan, config, forbidden, false));
}

void testAacAdtsGoldenBytesAndBounds(TestContext& ctx)
{
    const MediaTsAacAdtsPlan plan{0, 2, 3, 2};
    const std::array<std::uint8_t, 4> raw{0x11, 0x22, 0x33, 0x44};
    auto framed = MediaTsAacAdtsFramer::frame(plan, raw);
    EXPECT_TRUE(ctx, framed);
    if (framed) {
        const std::array<std::uint8_t, 11> expected{
            0xFF, 0xF1, 0x4C, 0x80, 0x01, 0x7F, 0xFC,
            0x11, 0x22, 0x33, 0x44};
        EXPECT_EQ(ctx, framed.value().size(), expected.size());
        EXPECT_TRUE(ctx, std::equal(expected.begin(), expected.end(), framed.value().begin()));
    }

    std::vector<std::uint8_t> maximum(8191 - 7, 0x55);
    EXPECT_TRUE(ctx, MediaTsAacAdtsFramer::frame(plan, maximum));
    maximum.push_back(0x55);
    EXPECT_FALSE(ctx, MediaTsAacAdtsFramer::frame(plan, maximum));
    EXPECT_FALSE(ctx, MediaTsAacAdtsFramer::frame(MediaTsAacAdtsPlan{2, 2, 3, 2}, raw));
    EXPECT_FALSE(ctx, MediaTsAacAdtsFramer::frame(MediaTsAacAdtsPlan{0, 0, 3, 2}, raw));
    EXPECT_FALSE(ctx, MediaTsAacAdtsFramer::frame(MediaTsAacAdtsPlan{0, 5, 3, 2}, raw));
    EXPECT_FALSE(ctx, MediaTsAacAdtsFramer::frame(MediaTsAacAdtsPlan{0, 2, 13, 2}, raw));
    EXPECT_FALSE(ctx, MediaTsAacAdtsFramer::frame(MediaTsAacAdtsPlan{0, 2, 3, 0}, raw));
    EXPECT_FALSE(ctx, MediaTsAacAdtsFramer::frame(MediaTsAacAdtsPlan{0, 2, 3, 8}, raw));
}

} // namespace

void runMpegTsAccessUnitFramerTests(TestContext& ctx)
{
    static_assert(!std::is_copy_constructible_v<MediaTsFramedAccessUnit>);
    static_assert(std::is_nothrow_move_constructible_v<MediaTsFramedAccessUnit>);
    testAnnexBIsBorrowedAndInjectionIsExplicit(ctx);
    testLengthPrefixConversionUsesExactOwnedStorage(ctx);
    testLengthPrefixInjectionUsesExplicitRandomAccess(ctx);
    testH264RejectsMalformedOrMismatchedContracts(ctx);
    testAacAdtsGoldenBytesAndBounds(ctx);
}
