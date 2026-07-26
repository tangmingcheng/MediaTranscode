#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsClockProjection.h"
#include "unit/fixtures/MpegTsCraftedUdpFixture.h"

#include <algorithm>
#include <cstdint>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;
using namespace media::ffmpeg::graph::test_fixture;

namespace {

constexpr std::int64_t kPcrInterval27Mhz = 2'700'000;

MediaTsProgramClockPolicy clockPolicy(const CraftedTsProgramIdentity& identity)
{
    return MediaTsProgramClockPolicy{
        identity.programNumber,
        identity.pmtPid,
        identity.pcrPid,
        identity.videoPid,
        identity.audioPid,
        kPcrInterval27Mhz * 3};
}

::media::Result<MediaTsClockProjection> replay(
    const CraftedTsBytes& stream,
    const CraftedUdpObservation& observation)
{
    auto projection = MediaTsClockProjection::create(
        clockPolicy(stream.identity()), 1'024, 2 * 1024 * 1024, 0, 0);
    if (!projection) return projection;
    if (auto status = projection.value().replay(observation.evidence); !status) {
        return ::media::Result<MediaTsClockProjection>::failure(status.error());
    }
    return projection;
}

std::uint64_t lastEvidenceOffset(const CraftedUdpObservation& observation)
{
    return observation.evidence.empty() ? 0 : observation.evidence.back().byteOffset;
}

::media::Result<CraftedUdpObservation> observe(
    const CraftedTsBytes& stream,
    std::uint16_t port)
{
    auto result = observeCraftedBytesOverProductionUdp(stream, port);
    if (!result) {
        std::cerr << "crafted UDP observation failed on port " << port
                  << ": " << result.error().message << '\n';
    }
    return result;
}

void testPcrWrapLocksWithoutGenerationChange(TestContext& ctx, std::uint16_t port)
{
    auto stream = CraftedTsBytes::generate();
    EXPECT_TRUE(ctx, stream);
    if (!stream) return;
    const auto first = stream.value().pcrModulus() - 2 * kPcrInterval27Mhz;
    EXPECT_TRUE(ctx, stream.value().rewritePcrSequence(first, kPcrInterval27Mhz));
    auto observed = observe(stream.value(), port);
    EXPECT_TRUE(ctx, observed);
    if (!observed) return;
    auto projection = replay(stream.value(), observed.value());
    EXPECT_TRUE(ctx, projection);
    if (!projection) return;
    auto latest = projection.value().atOrBefore(lastEvidenceOffset(observed.value()));
    EXPECT_TRUE(ctx, latest);
    if (latest) {
        EXPECT_EQ(ctx, latest.value().readiness, MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, latest.value().generation, std::uint64_t{0});
    }
}

void testDiscontinuityReacquiresAndRelocks(TestContext& ctx, std::uint16_t port)
{
    auto stream = CraftedTsBytes::generate();
    EXPECT_TRUE(ctx, stream);
    if (!stream) return;
    EXPECT_TRUE(ctx, stream.value().rewritePcrSequence(10'000'000, kPcrInterval27Mhz));
    EXPECT_TRUE(ctx, stream.value().markPcrDiscontinuity(2));
    auto observed = observe(stream.value(), port);
    EXPECT_TRUE(ctx, observed);
    if (!observed) return;
    const bool sawDiscontinuity = std::any_of(
        observed.value().evidence.begin(), observed.value().evidence.end(),
        [&stream](const MediaTsEvidenceCheckpoint& item) {
            return item.continuityEvent &&
                item.continuityEvent->pid == stream.value().identity().pcrPid &&
                item.continuityEvent->reason ==
                    MediaTsContinuityEventReason::DiscontinuityIndicator;
        });
    EXPECT_TRUE(ctx, sawDiscontinuity);
    auto projection = MediaTsClockProjection::create(
        clockPolicy(stream.value().identity()), 1'024, 2 * 1024 * 1024, 0, 0);
    EXPECT_TRUE(ctx, projection);
    if (!projection) return;
    bool sawReacquireRequired = false;
    for (const auto& item : observed.value().evidence) {
        EXPECT_TRUE(ctx, projection.value().replay({item}));
        auto checkpoint = projection.value().atOrBefore(item.byteOffset);
        EXPECT_TRUE(ctx, checkpoint);
        if (checkpoint && checkpoint.value().readiness ==
                MediaSourceClockReadiness::ReacquireRequired) {
            sawReacquireRequired = true;
        }
    }
    EXPECT_TRUE(ctx, sawReacquireRequired);
    auto latest = projection.value().atOrBefore(lastEvidenceOffset(observed.value()));
    EXPECT_TRUE(ctx, latest);
    if (latest) {
        EXPECT_EQ(ctx, latest.value().readiness, MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, latest.value().generation, std::uint64_t{1});
    }
}

void testPatPmtVersionChangePreservesImmutableIdentity(TestContext& ctx,
                                                       std::uint16_t port)
{
    auto stream = CraftedTsBytes::generate();
    EXPECT_TRUE(ctx, stream);
    if (!stream) return;
    EXPECT_TRUE(ctx, stream.value().rewritePcrSequence(10'000'000, kPcrInterval27Mhz));
    EXPECT_TRUE(ctx, stream.value().changePatAndPmtVersionAfterFirst(1));
    auto observed = observe(stream.value(), port);
    EXPECT_TRUE(ctx, observed);
    if (!observed) return;
    const bool sawVersionZero = std::any_of(
        observed.value().evidence.begin(), observed.value().evidence.end(),
        [](const MediaTsEvidenceCheckpoint& item) {
            return item.inventory.patVersion == 0 && !item.inventory.programs.empty() &&
                item.inventory.programs.front().pmtVersion == 0;
        });
    const bool sawVersionOne = std::any_of(
        observed.value().evidence.begin(), observed.value().evidence.end(),
        [](const MediaTsEvidenceCheckpoint& item) {
            return item.inventory.patVersion == 1 && !item.inventory.programs.empty() &&
                item.inventory.programs.front().pmtVersion == 1;
        });
    EXPECT_TRUE(ctx, sawVersionZero);
    EXPECT_TRUE(ctx, sawVersionOne);
    auto projection = replay(stream.value(), observed.value());
    EXPECT_TRUE(ctx, projection);
}

void testPcrPidChangeFailsClosed(TestContext& ctx, std::uint16_t port)
{
    auto stream = CraftedTsBytes::generate();
    EXPECT_TRUE(ctx, stream);
    if (!stream) return;
    EXPECT_TRUE(ctx, stream.value().rewritePcrSequence(10'000'000, kPcrInterval27Mhz));
    EXPECT_TRUE(ctx, stream.value().changePcrPidAfterFirstPmt(
        1, stream.value().identity().audioPid));
    auto observed = observe(stream.value(), port);
    EXPECT_TRUE(ctx, observed);
    if (!observed) return;
    auto projection = replay(stream.value(), observed.value());
    EXPECT_FALSE(ctx, projection);
}

void testExcessivePcrGapFailsClosed(TestContext& ctx, std::uint16_t port)
{
    auto stream = CraftedTsBytes::generate();
    EXPECT_TRUE(ctx, stream);
    if (!stream) return;
    EXPECT_TRUE(ctx, stream.value().rewritePcrSequence(10'000'000, kPcrInterval27Mhz));
    EXPECT_TRUE(ctx, stream.value().rewritePcr(
        2, 10'000'000 + kPcrInterval27Mhz * 5));
    auto observed = observe(stream.value(), port);
    EXPECT_TRUE(ctx, observed);
    if (!observed) return;
    auto projection = replay(stream.value(), observed.value());
    EXPECT_FALSE(ctx, projection);
}

void testPcrRegressionFailsClosed(TestContext& ctx, std::uint16_t port)
{
    auto stream = CraftedTsBytes::generate();
    EXPECT_TRUE(ctx, stream);
    if (!stream) return;
    EXPECT_TRUE(ctx, stream.value().rewritePcrSequence(10'000'000, kPcrInterval27Mhz));
    EXPECT_TRUE(ctx, stream.value().rewritePcr(
        2, 10'000'000 + kPcrInterval27Mhz / 2));
    auto observed = observe(stream.value(), port);
    EXPECT_TRUE(ctx, observed);
    if (!observed) return;
    auto projection = replay(stream.value(), observed.value());
    EXPECT_FALSE(ctx, projection);
}

} // namespace

int main()
{
#ifndef _WIN32
    std::cout << "Crafted MPEG-TS UDP fault tests skipped: production UDP socket "
                 "runtime is unavailable on this platform\n";
    return 77;
#else
    TestContext ctx;
    const auto base = static_cast<std::uint16_t>(48'000 +
        (GetCurrentProcessId() % 1'000) * 6);
    testPcrWrapLocksWithoutGenerationChange(ctx, base);
    testDiscontinuityReacquiresAndRelocks(ctx, static_cast<std::uint16_t>(base + 1));
    testPatPmtVersionChangePreservesImmutableIdentity(
        ctx, static_cast<std::uint16_t>(base + 2));
    testPcrPidChangeFailsClosed(ctx, static_cast<std::uint16_t>(base + 3));
    testExcessivePcrGapFailsClosed(ctx, static_cast<std::uint16_t>(base + 4));
    testPcrRegressionFailsClosed(ctx, static_cast<std::uint16_t>(base + 5));
    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " crafted MPEG-TS UDP fault expectation(s) failed\n";
        return 1;
    }
    std::cout << "Crafted MPEG-TS UDP fault tests passed\n";
    return 0;
#endif
}
