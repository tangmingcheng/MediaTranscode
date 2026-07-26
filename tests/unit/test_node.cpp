#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/audio/AudioEncodeNode.h"
#include "internal/graph/nodes/audio/AudioDecodeNode.h"
#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/nodes/audio/MediaAudioStartupTrimNode.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/audio/AudioEncoderFrameQueue.h"
#include "internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h"
#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"
#include "internal/graph/nodes/sync/MediaRtpClockGroupNode.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"
#include "internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaDecodedAudioTrimInputBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"


extern "C" {
#include <libavutil/error.h>
}

#include <array>
#include <chrono>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

void runRtpDepacketizerTests(TestContext& ctx);
void runRtpSourceClockTests(TestContext& ctx);
void runRtpOutputClockTests(TestContext& ctx);
void runScheduledRtpPacketizationTests(TestContext& ctx);
void runScheduledRtpSenderTests(TestContext& ctx);
void runMediaRtpUdpSenderTransportTests(TestContext& ctx);
void runMediaRtpUdpSenderCompositionTests(TestContext& ctx);
void runRawRtpInputLifecycleTests(TestContext& ctx);
void runMpegTsPacketTests(TestContext& ctx);
void runMpegTsPesProvenanceTimelineTests(TestContext& ctx);
void runMpegTsClockTests(TestContext& ctx);
void runMpegTsInputSessionTests(TestContext& ctx);
void runMpegTsDemuxNodeTests(TestContext& ctx);
void runAvStartupCoordinatorTests(TestContext& ctx);
void runAudioSwrCompensationExecutorTests(TestContext& ctx);
void runAudioResampleNodeTests(TestContext& ctx);
void runOutputByteSinkTests(TestContext& ctx);
void runMpegTsOutputSerializerTests(TestContext& ctx);
void runMpegTsAccessUnitFramerTests(TestContext& ctx);
void runMpegTsOutputPacketizerTests(TestContext& ctx);
void runMpegTsOutputSessionTests(TestContext& ctx);
void runMpegTsMuxRuntimeBufferTests(TestContext& ctx);
void runMpegTsFfmpegConfigMaterializerTests(TestContext& ctx);
void runProjectMpegTsMuxSessionAdapterTests(TestContext& ctx);
void runProjectMpegTsFileMuxNodeTests(TestContext& ctx);
void runFileMuxSessionTests(TestContext& ctx);
void runFileOutputResourceTests(TestContext& ctx);

namespace {

MediaTsOutputClockPolicy tsOutputClockPolicy()
{
    return MediaTsOutputClockPolicy{
        MediaRunningTime::fromNanoseconds(20'000'000),
        MediaRunningTime::fromNanoseconds(40'000'000),
        MediaRunningTime::fromNanoseconds(1'000'000), 1, 90'000};
}

::media::Result<MediaTsPacketClock> projectCommitted(
    MediaTsOutputClockGenerator& generator,
    std::uint64_t generation,
    MediaScheduledStream stream,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster,
    MediaRunningTime transportDecodeLead)
{
    auto prepared = generator.preparePacket(
        generation, stream, presentationOnMaster, dispatchOnMaster,
        emitOnMaster, transportDecodeLead);
    if (!prepared) {
        return ::media::Result<MediaTsPacketClock>::failure(prepared.error());
    }
    const auto clock = prepared.value().clock();
    auto committed = generator.commitPacket(std::move(prepared).value());
    return committed
        ? ::media::Result<MediaTsPacketClock>::success(clock)
        : ::media::Result<MediaTsPacketClock>::failure(committed.error());
}

::media::Result<MediaTsPcrClock> pcrCommitted(
    MediaTsOutputClockGenerator& generator,
    std::uint64_t generation,
    MediaRunningTime deadline)
{
    auto prepared = generator.preparePcr(generation, deadline);
    if (!prepared) {
        return ::media::Result<MediaTsPcrClock>::failure(prepared.error());
    }
    const auto clock = prepared.value().clock();
    auto committed = generator.commitPcr(std::move(prepared).value());
    return committed
        ? ::media::Result<MediaTsPcrClock>::success(clock)
        : ::media::Result<MediaTsPcrClock>::failure(committed.error());
}

void testMpegTsOutputClockUsesOneEpoch(TestContext& ctx)
{
    const MediaPlaybackEpoch epoch{
        MediaRunningTime::fromNanoseconds(2'000'000'000),
        MediaRunningTime::fromNanoseconds(10'000'000'000), 7};
    auto created = MediaTsOutputClockGenerator::create(tsOutputClockPolicy(), epoch);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto generator = std::move(created).value();

    auto reordered = projectCommitted(generator,
        7, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(10'040'000'000),
        MediaRunningTime::fromNanoseconds(10'020'000'000),
        MediaRunningTime::fromNanoseconds(9'920'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, reordered);
    if (reordered) {
        EXPECT_EQ(ctx, reordered.value().extendedPts, std::int64_t{183'600});
        EXPECT_EQ(ctx, reordered.value().extendedDts, std::int64_t{181'800});
        EXPECT_EQ(ctx, reordered.value().wirePts, std::uint64_t{183'600});
        EXPECT_EQ(ctx, reordered.value().wireDts, std::uint64_t{181'800});
    }
    auto independentAudio = projectCommitted(generator,
        7, MediaScheduledStream::Audio,
        MediaRunningTime::fromNanoseconds(10'015'000'000),
        MediaRunningTime::fromNanoseconds(10'010'000'000),
        MediaRunningTime::fromNanoseconds(9'910'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, independentAudio);
    EXPECT_FALSE(ctx, projectCommitted(generator,
        7, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(10'030'000'000),
        MediaRunningTime::fromNanoseconds(10'019'000'000),
        MediaRunningTime::fromNanoseconds(9'919'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000)));
    EXPECT_TRUE(ctx, projectCommitted(generator,
        7, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(10'041'000'000),
        MediaRunningTime::fromNanoseconds(10'021'000'000),
        MediaRunningTime::fromNanoseconds(9'921'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000)));
    EXPECT_FALSE(ctx, projectCommitted(generator,
        7, static_cast<MediaScheduledStream>(0xFF),
        MediaRunningTime::fromNanoseconds(10'020'000'000),
        MediaRunningTime::fromNanoseconds(10'010'000'000),
        MediaRunningTime::fromNanoseconds(9'910'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000)));
    EXPECT_FALSE(ctx, projectCommitted(generator,
        8, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(10'060'000'000),
        MediaRunningTime::fromNanoseconds(10'040'000'000),
        MediaRunningTime::fromNanoseconds(9'940'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000)));
    EXPECT_FALSE(ctx, projectCommitted(generator,
        7, MediaScheduledStream::Audio,
        MediaRunningTime::fromNanoseconds(10'020'000'000),
        MediaRunningTime::fromNanoseconds(10'040'000'000),
        MediaRunningTime::fromNanoseconds(9'940'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000)));

    auto pcrOrigin = pcrCommitted(generator,
        7, MediaRunningTime::fromNanoseconds(10'000'000'000));
    auto pcrTwenty = pcrCommitted(generator,
        7, MediaRunningTime::fromNanoseconds(10'020'000'000));
    auto pcrMiddle = pcrCommitted(generator,
        7, MediaRunningTime::fromNanoseconds(10'040'000'000));
    auto pcr = pcrCommitted(generator,
        7, MediaRunningTime::fromNanoseconds(10'060'000'000));
    EXPECT_TRUE(ctx, pcrOrigin);
    EXPECT_TRUE(ctx, pcrTwenty);
    EXPECT_TRUE(ctx, pcrMiddle);
    EXPECT_TRUE(ctx, pcr);
    if (pcrOrigin && pcrTwenty && pcrMiddle && pcr) {
        EXPECT_EQ(ctx, pcrOrigin.value().extended27Mhz, std::int64_t{54'000'000});
        EXPECT_EQ(ctx, pcrOrigin.value().generation, std::uint64_t{7});
        EXPECT_EQ(ctx, pcrTwenty.value().extended27Mhz, std::int64_t{54'540'000});
        EXPECT_EQ(ctx, pcr.value().extended27Mhz, std::int64_t{55'620'000});
        EXPECT_EQ(ctx, pcr.value().wire27Mhz, std::uint64_t{55'620'000});
        EXPECT_TRUE(ctx, generator.validateSerializedPcr(
            pcr.value(), pcr.value().extended27Mhz + 27'000));
        EXPECT_FALSE(ctx, generator.validateSerializedPcr(
            pcr.value(), pcr.value().extended27Mhz + 27'001));
    }
    EXPECT_FALSE(ctx, generator.preparePcr(
        7, MediaRunningTime::fromNanoseconds(10'100'000'001)));

    auto lateFirst = MediaTsOutputClockGenerator::create(
        tsOutputClockPolicy(), epoch);
    EXPECT_TRUE(ctx, lateFirst);
    if (lateFirst) EXPECT_FALSE(ctx, lateFirst.value().preparePcr(
        7, MediaRunningTime::fromNanoseconds(10'040'000'001)));

    const MediaTsPcrClock zeroPcr{
        7, epoch.masterRelease, 54'000'000, 54'000'000};
    EXPECT_FALSE(ctx, generator.validateSerializedPcr(
        zeroPcr, std::numeric_limits<std::int64_t>::min()));
    const MediaTsPcrClock wrongGeneration{
        8, epoch.masterRelease, 54'000'000, 54'000'000};
    EXPECT_FALSE(ctx, generator.validateSerializedPcr(
        wrongGeneration, wrongGeneration.extended27Mhz));
    const MediaTsPcrClock offCadence{
        7, MediaRunningTime::fromNanoseconds(10'010'000'000),
        54'270'000, 54'270'000};
    EXPECT_FALSE(ctx, generator.validateSerializedPcr(
        offCadence, offCadence.extended27Mhz));
    const MediaTsPcrClock wrongWire{
        7, epoch.masterRelease, 54'000'000, 1};
    EXPECT_FALSE(ctx, generator.validateSerializedPcr(
        wrongWire, wrongWire.extended27Mhz));
    const MediaTsPcrClock wrongExtended{
        7, epoch.masterRelease, 54'000'001, 54'000'001};
    EXPECT_FALSE(ctx, generator.validateSerializedPcr(
        wrongExtended, wrongExtended.extended27Mhz));

    auto transactional = MediaTsOutputClockGenerator::create(
        tsOutputClockPolicy(), MediaPlaybackEpoch{
            MediaRunningTime::fromNanoseconds(
                std::numeric_limits<std::int64_t>::max() - 10'000'000),
            MediaRunningTime::fromNanoseconds(0), 9});
    EXPECT_TRUE(ctx, transactional);
    if (transactional) {
        EXPECT_FALSE(ctx, transactional.value().preparePcr(
            9, MediaRunningTime::fromNanoseconds(20'000'000)));
        auto retryOrigin = transactional.value().preparePcr(
            9, MediaRunningTime::fromNanoseconds(0));
        EXPECT_TRUE(ctx, retryOrigin);
        if (retryOrigin) EXPECT_EQ(ctx, retryOrigin.value().clock().masterTime,
                                  MediaRunningTime::fromNanoseconds(0));
    }
}

void testMpegTsOutputClockRejectsInvalidPolicyAndHandlesWrap(TestContext& ctx)
{
    const MediaPlaybackEpoch epoch{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0), 1};
    EXPECT_FALSE(ctx, MediaTsOutputClockGenerator::create(
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(0),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000}, epoch));
    EXPECT_FALSE(ctx, MediaTsOutputClockGenerator::create(
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000}, epoch));
    EXPECT_FALSE(ctx, MediaTsOutputClockGenerator::create(
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(20'000'000), 1, 90'000}, epoch));

    constexpr std::int64_t wrapTicks = std::int64_t{1} << 33;
    auto wrapEpochNs = MediaRunningTime::checkedFromTicks(wrapTicks - 900, 1, 90'000);
    EXPECT_TRUE(ctx, wrapEpochNs);
    if (!wrapEpochNs) return;
    auto wrapped = MediaTsOutputClockGenerator::create(
        tsOutputClockPolicy(), MediaPlaybackEpoch{
            wrapEpochNs.value(), MediaRunningTime::fromNanoseconds(0), 3});
    EXPECT_TRUE(ctx, wrapped);
    if (!wrapped) return;
    auto before = projectCommitted(wrapped.value(),
        3, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(5'000'000),
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(-100'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000));
    auto after = projectCommitted(wrapped.value(),
        3, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(20'000'000),
        MediaRunningTime::fromNanoseconds(10'000'000),
        MediaRunningTime::fromNanoseconds(-90'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, before);
    EXPECT_TRUE(ctx, after);
    if (before && after) {
        EXPECT_EQ(ctx, before.value().wirePts, std::uint64_t{(std::uint64_t{1} << 33) - 450});
        EXPECT_EQ(ctx, after.value().wirePts, std::uint64_t{900});
        EXPECT_EQ(ctx, after.value().wireDts, std::uint64_t{0});
    }
    auto pcrBeforeWrap = pcrCommitted(wrapped.value(),
        3, MediaRunningTime::fromNanoseconds(0));
    auto pcrAfterWrap = pcrCommitted(wrapped.value(),
        3, MediaRunningTime::fromNanoseconds(20'000'000));
    EXPECT_TRUE(ctx, pcrBeforeWrap);
    EXPECT_TRUE(ctx, pcrAfterWrap);
    if (pcrBeforeWrap && pcrAfterWrap) {
        EXPECT_EQ(ctx, pcrBeforeWrap.value().wire27Mhz,
                  (std::uint64_t{1} << 33) * 300 - 270'000);
        EXPECT_EQ(ctx, pcrAfterWrap.value().wire27Mhz,
                  std::uint64_t{270'000});
    }

    auto overflow = MediaTsOutputClockGenerator::create(
        tsOutputClockPolicy(), MediaPlaybackEpoch{
            MediaRunningTime::fromNanoseconds(std::numeric_limits<std::int64_t>::max()),
            MediaRunningTime::fromNanoseconds(0), 4});
    EXPECT_TRUE(ctx, overflow);
    if (overflow) EXPECT_FALSE(ctx, projectCommitted(overflow.value(),
        4, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(-99'999'999),
        MediaRunningTime::fromNanoseconds(100'000'000)));
}

void testMpegTsOutputClockRequiresExactTransportDecodeLead(TestContext& ctx)
{
    auto plan = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0x0000, 0x0100, 0x0101, 0x0102, 0x0101, 0,
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
        MediaTsOutputTransportKind::Udp, 1024});
    EXPECT_TRUE(ctx, plan);
    if (!plan) return;
    const MediaPlaybackEpoch epoch{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0), 11};
    auto clock = MediaTsOutputClockGenerator::create(plan.value().clockPolicy(), epoch);
    EXPECT_TRUE(ctx, clock);
    if (!clock) return;

    EXPECT_TRUE(ctx, projectCommitted(clock.value(),
        epoch.generation, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(240'000'000),
        MediaRunningTime::fromNanoseconds(220'000'000),
        MediaRunningTime::fromNanoseconds(120'000'000),
        plan.value().transportDecodeLead()));
    EXPECT_FALSE(ctx, projectCommitted(clock.value(),
        epoch.generation, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(260'000'000),
        MediaRunningTime::fromNanoseconds(240'000'000),
        MediaRunningTime::fromNanoseconds(141'000'000),
        plan.value().transportDecodeLead()));
    EXPECT_TRUE(ctx, projectCommitted(clock.value(),
        epoch.generation, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(260'000'000),
        MediaRunningTime::fromNanoseconds(240'000'000),
        MediaRunningTime::fromNanoseconds(140'000'000),
        plan.value().transportDecodeLead()));
}

void testMpegTsOutputClockCommitsOnlyCompletedTransactions(TestContext& ctx)
{
    const MediaPlaybackEpoch epoch{
        MediaRunningTime::fromNanoseconds(0),
        MediaRunningTime::fromNanoseconds(0), 12};
    auto first = MediaTsOutputClockGenerator::create(tsOutputClockPolicy(), epoch);
    auto foreign = MediaTsOutputClockGenerator::create(tsOutputClockPolicy(), epoch);
    EXPECT_TRUE(ctx, first);
    EXPECT_TRUE(ctx, foreign);
    if (!first || !foreign) return;

    {
        auto abandoned = first.value().preparePacket(
            12, MediaScheduledStream::Video,
            MediaRunningTime::fromNanoseconds(30'000'000),
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(-80'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000));
        EXPECT_TRUE(ctx, abandoned);
    }
    auto retry = first.value().preparePacket(
        12, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(20'000'000),
        MediaRunningTime::fromNanoseconds(10'000'000),
        MediaRunningTime::fromNanoseconds(-90'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, retry);
    if (retry) {
        EXPECT_FALSE(ctx, foreign.value().commitPacket(std::move(retry).value()));
        EXPECT_TRUE(ctx, first.value().commitPacket(std::move(retry).value()));
    }
    EXPECT_FALSE(ctx, projectCommitted(
        first.value(), 12, MediaScheduledStream::Video,
        MediaRunningTime::fromNanoseconds(15'000'000),
        MediaRunningTime::fromNanoseconds(5'000'000),
        MediaRunningTime::fromNanoseconds(-95'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000)));

    {
        auto abandoned = first.value().preparePcr(
            12, MediaRunningTime::fromNanoseconds(0));
        EXPECT_TRUE(ctx, abandoned);
    }
    auto pcrRetry = first.value().preparePcr(
        12, MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, pcrRetry);
    if (pcrRetry) {
        EXPECT_FALSE(ctx, foreign.value().commitPcr(std::move(pcrRetry).value()));
        EXPECT_TRUE(ctx, first.value().commitPcr(std::move(pcrRetry).value()));
    }
    EXPECT_FALSE(ctx, first.value().preparePcr(
        12, MediaRunningTime::fromNanoseconds(0)));
}

void testRequiredPossiblyEmptyNodeOption(TestContext& ctx)
{
    MediaNodeOptions options;
    auto missing = requiredPossiblyEmptyNodeOption(&options, "RawRtpInputNode", "rtp.fmtp");
    EXPECT_FALSE(ctx, missing);

    options.set("rtp.fmtp", "");
    auto explicitEmpty = requiredPossiblyEmptyNodeOption(&options, "RawRtpInputNode", "rtp.fmtp");
    EXPECT_TRUE(ctx, explicitEmpty);
    if (explicitEmpty) EXPECT_TRUE(ctx, explicitEmpty.value().empty());
}

void appendU16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value));
}

void appendU32(std::vector<uint8_t>& bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value >> 24));
    bytes.push_back(static_cast<uint8_t>(value >> 16));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value));
}

std::vector<uint8_t> senderReport(uint32_t ssrc, uint32_t ntpSeconds, uint32_t ntpFraction,
                                  uint32_t rtpTimestamp)
{
    std::vector<uint8_t> bytes{0x80, 200};
    appendU16(bytes, 6);
    appendU32(bytes, ssrc);
    appendU32(bytes, ntpSeconds);
    appendU32(bytes, ntpFraction);
    appendU32(bytes, rtpTimestamp);
    appendU32(bytes, 7);
    appendU32(bytes, 900);
    return bytes;
}

struct SdesItemInput final {
    uint8_t type = 0;
    std::string value;
};

struct SdesChunkInput final {
    uint32_t ssrc = 0;
    std::vector<SdesItemInput> items;
};

std::vector<uint8_t> sourceDescription(std::span<const SdesChunkInput> chunks)
{
    std::vector<uint8_t> body;
    for (const auto& chunk : chunks) {
        appendU32(body, chunk.ssrc);
        for (const auto& item : chunk.items) {
            body.push_back(item.type);
            body.push_back(static_cast<uint8_t>(item.value.size()));
            body.insert(body.end(), item.value.begin(), item.value.end());
        }
        body.push_back(0);
        while ((body.size() % 4) != 0) body.push_back(0);
    }
    std::vector<uint8_t> bytes{
        static_cast<uint8_t>(0x80 | static_cast<uint8_t>(chunks.size())), 202};
    appendU16(bytes, static_cast<uint16_t>((body.size() + 4) / 4 - 1));
    bytes.insert(bytes.end(), body.begin(), body.end());
    return bytes;
}

std::vector<uint8_t> compoundWithSdes(uint32_t ssrc, uint32_t ntpSeconds,
                                      uint32_t rtpTimestamp, const std::string& cname)
{
    auto bytes = senderReport(ssrc, ntpSeconds, 0, rtpTimestamp);
    const std::array chunks{SdesChunkInput{ssrc, {{1, cname}}}};
    auto identity = sourceDescription(chunks);
    bytes.insert(bytes.end(), identity.begin(), identity.end());
    return bytes;
}

std::vector<uint8_t> byeWithReason(uint32_t ssrc, const std::string& reason,
                                   std::size_t alignmentBytes)
{
    std::vector<uint8_t> body;
    appendU32(body, ssrc);
    body.push_back(static_cast<uint8_t>(reason.size()));
    body.insert(body.end(), reason.begin(), reason.end());
    body.insert(body.end(), alignmentBytes, 0);
    std::vector<uint8_t> packet{0x81, 203};
    appendU16(packet, static_cast<uint16_t>((body.size() + 4) / 4 - 1));
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
}

auto parseRtcp(std::span<const uint8_t> bytes, bool requireCname)
{
    return MediaRtcpCompoundParser::parse(bytes, MediaRtcpCompoundPolicy{
        MediaRtcpCompositionMode::StrictCompoundRfc3550, requireCname});
}

auto parseReducedSizeRtcp(std::span<const uint8_t> bytes)
{
    return MediaRtcpCompoundParser::parse(bytes, MediaRtcpCompoundPolicy{
        MediaRtcpCompositionMode::ReducedSizeRfc5506, false});
}

void testRtpPacketParserStrictHeader(TestContext& ctx)
{
    const std::vector<uint8_t> bytes{
        0xB2, 0xE0, 0x12, 0x34, 0x89, 0xAB, 0xCD, 0xEF,
        0x01, 0x23, 0x45, 0x67, 0x10, 0x20, 0x30, 0x40,
        0x50, 0x60, 0x70, 0x80, 0xBE, 0xDE, 0x00, 0x01,
        0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x00, 0x02
    };
    const auto parsed = MediaRtpPacketParser::parse(bytes);
    EXPECT_TRUE(ctx, parsed);
    if (!parsed) return;
    EXPECT_EQ(ctx, parsed.value().version, static_cast<uint8_t>(2));
    EXPECT_TRUE(ctx, parsed.value().marker);
    EXPECT_EQ(ctx, parsed.value().payloadType, static_cast<uint8_t>(96));
    EXPECT_EQ(ctx, parsed.value().sequenceNumber, static_cast<uint16_t>(0x1234));
    EXPECT_EQ(ctx, parsed.value().timestamp, static_cast<uint32_t>(0x89ABCDEF));
    EXPECT_EQ(ctx, parsed.value().ssrc, static_cast<uint32_t>(0x01234567));
    EXPECT_EQ(ctx, parsed.value().csrcs.size(), static_cast<std::size_t>(2));
    EXPECT_TRUE(ctx, parsed.value().extension.has_value());
    if (parsed.value().extension) {
        EXPECT_EQ(ctx, parsed.value().extension->profile, static_cast<uint16_t>(0xBEDE));
        EXPECT_EQ(ctx, parsed.value().extension->data.size(), static_cast<std::size_t>(4));
    }
    EXPECT_EQ(ctx, parsed.value().paddingSize, static_cast<uint8_t>(2));
    EXPECT_EQ(ctx, parsed.value().payload.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, parsed.value().payload[0], static_cast<uint8_t>(0x11));

    auto invalidVersion = bytes;
    invalidVersion[0] = static_cast<uint8_t>((invalidVersion[0] & 0x3F) | 0x40);
    EXPECT_FALSE(ctx, MediaRtpPacketParser::parse(invalidVersion));
    EXPECT_FALSE(ctx, MediaRtpPacketParser::parse(std::span<const uint8_t>(bytes.data(), 11)));
    auto invalidExtension = bytes;
    invalidExtension[22] = 0x7F;
    invalidExtension[23] = 0xFF;
    EXPECT_FALSE(ctx, MediaRtpPacketParser::parse(invalidExtension));
    auto invalidPadding = bytes;
    invalidPadding.back() = 0;
    EXPECT_FALSE(ctx, MediaRtpPacketParser::parse(invalidPadding));
}

void testRtcpCompoundParserStrictPackets(TestContext& ctx)
{
    const uint32_t sender = 0x10203040;
    const auto reducedSenderReport =
        senderReport(sender, 0x11223344, 0x55667788, 0x90ABCDEF);
    EXPECT_TRUE(ctx, parseReducedSizeRtcp(reducedSenderReport));
    EXPECT_FALSE(ctx, parseRtcp(reducedSenderReport, true));

    auto compound = senderReport(sender, 0x11223344, 0x55667788, 0x90ABCDEF);
    const std::array descriptionChunks{
        SdesChunkInput{sender, {{1, "camera-a"}}},
        SdesChunkInput{0x50607080, {{1, "audio-a"}}}};
    auto descriptions = sourceDescription(descriptionChunks);
    compound.insert(compound.end(), descriptions.begin(), descriptions.end());
    const std::vector<uint8_t> unknown{0x80, 210, 0x00, 0x01, 1, 2, 3, 4};
    compound.insert(compound.end(), unknown.begin(), unknown.end());
    const std::vector<uint8_t> bye{0xA2, 203, 0x00, 0x03,
                                   0x10, 0x20, 0x30, 0x40,
                                   0x50, 0x60, 0x70, 0x80,
                                   0x00, 0x00, 0x00, 0x04};
    compound.insert(compound.end(), bye.begin(), bye.end());

    const auto parsed = parseRtcp(compound, true);
    EXPECT_TRUE(ctx, parsed);
    if (!parsed) return;
    EXPECT_EQ(ctx, parsed.value().size(), static_cast<std::size_t>(4));
    EXPECT_EQ(ctx, parsed.value()[0].kind, MediaRtcpPacketKind::SenderReport);
    EXPECT_EQ(ctx, parsed.value()[0].senderReport->ssrc, sender);
    EXPECT_EQ(ctx, parsed.value()[0].senderReport->ntp.seconds, static_cast<uint32_t>(0x11223344));
    EXPECT_EQ(ctx, parsed.value()[0].senderReport->ntp.fraction, static_cast<uint32_t>(0x55667788));
    EXPECT_EQ(ctx, parsed.value()[0].senderReport->rtpTimestamp, static_cast<uint32_t>(0x90ABCDEF));
    EXPECT_EQ(ctx, parsed.value()[1].sdesChunks.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, parsed.value()[1].sdesChunks[0].items[0].type, static_cast<uint8_t>(1));
    EXPECT_EQ(ctx, parsed.value()[1].sdesChunks[0].items[0].value,
              std::vector<uint8_t>({'c','a','m','e','r','a','-','a'}));
    EXPECT_EQ(ctx, parsed.value()[2].kind, MediaRtcpPacketKind::Unknown);
    EXPECT_EQ(ctx, parsed.value()[2].packetType, static_cast<uint8_t>(210));
    EXPECT_EQ(ctx, parsed.value()[3].kind, MediaRtcpPacketKind::Bye);
    EXPECT_EQ(ctx, parsed.value()[3].byeSources.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, parsed.value()[3].paddingSize, static_cast<uint8_t>(4));

    auto invalidVersion = compound;
    invalidVersion[0] = 0x40;
    EXPECT_FALSE(ctx, parseRtcp(invalidVersion, true));
    auto invalidLength = compound;
    invalidLength[2] = 0x7F;
    invalidLength[3] = 0xFF;
    EXPECT_FALSE(ctx, parseRtcp(invalidLength, true));
    EXPECT_FALSE(ctx, parseRtcp(
        std::span<const uint8_t>(compound.data(), compound.size() - 1), true));
    auto invalidNonFinalPadding = compound;
    invalidNonFinalPadding[0] |= 0x20;
    EXPECT_FALSE(ctx, parseRtcp(invalidNonFinalPadding, true));
    auto invalidSdes = descriptions;
    invalidSdes[9] = 0x7F;
    EXPECT_FALSE(ctx, parseRtcp(invalidSdes, true));

    EXPECT_FALSE(ctx, parseRtcp(descriptions, true));
    EXPECT_FALSE(ctx, parseRtcp(std::vector<uint8_t>{0x81, 203, 0, 1, 0, 0, 0, 7}, true));
    EXPECT_FALSE(ctx, parseRtcp(unknown, true));
    EXPECT_FALSE(ctx, parseRtcp(senderReport(sender, 1, 0, 1), true));
    auto emptyIdentity = senderReport(sender, 1, 0, 1);
    const std::array emptyChunks{SdesChunkInput{sender, {{1, ""}}}};
    auto emptySdes = sourceDescription(emptyChunks);
    emptyIdentity.insert(emptyIdentity.end(), emptySdes.begin(), emptySdes.end());
    EXPECT_FALSE(ctx, parseRtcp(emptyIdentity, true));

    std::vector<uint8_t> rr{0x80, 201, 0, 1};
    appendU32(rr, sender);
    rr.insert(rr.end(), descriptions.begin(), descriptions.end());
    EXPECT_TRUE(ctx, parseRtcp(rr, true));

    std::vector<uint8_t> byeReason{0x81, 203, 0, 2};
    appendU32(byeReason, sender);
    byeReason.insert(byeReason.end(), {3, 'e', 'n', 'd'});
    auto compoundBye = senderReport(sender, 2, 0, 2);
    compoundBye.insert(compoundBye.end(), byeReason.begin(), byeReason.end());
    const auto parsedBye = parseRtcp(compoundBye, false);
    EXPECT_TRUE(ctx, parsedBye);
    if (parsedBye) EXPECT_EQ(ctx, parsedBye.value().back().byeReason,
                             std::vector<uint8_t>({'e', 'n', 'd'}));
    std::vector<uint8_t> malformedBye{0x81, 203, 0, 2};
    appendU32(malformedBye, sender);
    malformedBye.insert(malformedBye.end(), {2, 'o', 'k', 1});
    auto malformedByeCompound = senderReport(sender, 2, 0, 2);
    malformedByeCompound.insert(malformedByeCompound.end(), malformedBye.begin(), malformedBye.end());
    EXPECT_FALSE(ctx, parseRtcp(malformedByeCompound, false));

    const std::array<std::pair<std::string, std::size_t>, 4> alignedReasons{{
        {"end", 0}, {"ok", 1}, {"x", 2}, {"", 3}
    }};
    for (const auto& [reason, alignment] : alignedReasons) {
        auto withReason = senderReport(sender, 3, 0, 3);
        auto byePacket = byeWithReason(sender, reason, alignment);
        withReason.insert(withReason.end(), byePacket.begin(), byePacket.end());
        EXPECT_TRUE(ctx, parseRtcp(withReason, false));
    }
    auto overlongBye = senderReport(sender, 3, 0, 3);
    auto overlongByePacket = byeWithReason(sender, "end", 4);
    overlongBye.insert(overlongBye.end(), overlongByePacket.begin(), overlongByePacket.end());
    EXPECT_FALSE(ctx, parseRtcp(overlongBye, false));

    auto paddedBase = compoundWithSdes(sender, 4, 4, "camera-a");
    const std::vector<uint8_t> validFinalPadding{0xA0, 210, 0, 1, 0, 0, 0, 4};
    auto validPaddedCompound = paddedBase;
    validPaddedCompound.insert(validPaddedCompound.end(), validFinalPadding.begin(), validFinalPadding.end());
    EXPECT_TRUE(ctx, parseRtcp(validPaddedCompound, true));
    auto zeroPaddingCompound = paddedBase;
    const std::vector<uint8_t> zeroFinalPadding{0xA0, 210, 0, 1, 0, 0, 0, 0};
    zeroPaddingCompound.insert(zeroPaddingCompound.end(), zeroFinalPadding.begin(), zeroFinalPadding.end());
    EXPECT_FALSE(ctx, parseRtcp(zeroPaddingCompound, true));
    auto oversizedPaddingCompound = paddedBase;
    const std::vector<uint8_t> oversizedFinalPadding{0xA0, 210, 0, 1, 0, 0, 0, 8};
    oversizedPaddingCompound.insert(oversizedPaddingCompound.end(),
                                    oversizedFinalPadding.begin(), oversizedFinalPadding.end());
    EXPECT_FALSE(ctx, parseRtcp(oversizedPaddingCompound, true));
}

void testRtcpEvidenceRequiresSameSsrcAndExpires(TestContext& ctx)
{
    const MediaRtcpSenderReportTrackerConfig config{true, true, 1'000, 2'000};
    MediaRtcpSenderReportTracker tracker(config);
    tracker.observeMedia(0x11111111, 100);
    auto report = parseRtcp(senderReport(0x11111111, 10, 0, 30), false);
    auto identity = parseRtcp(compoundWithSdes(0x11111111, 10, 30, "camera"), true);
    EXPECT_TRUE(ctx, report);
    EXPECT_TRUE(ctx, identity);
    if (!report || !identity) return;
    EXPECT_TRUE(ctx, tracker.observe(report.value(), 200));
    EXPECT_FALSE(ctx, tracker.evidence(200));
    EXPECT_TRUE(ctx, tracker.observe(identity.value(), 300));
    const auto ready = tracker.evidence(300);
    EXPECT_TRUE(ctx, ready);
    if (ready) {
        EXPECT_EQ(ctx, ready.value().observedMediaSsrc, static_cast<uint32_t>(0x11111111));
        EXPECT_EQ(ctx, ready.value().senderReportSsrc, static_cast<uint32_t>(0x11111111));
        EXPECT_EQ(ctx, ready.value().cnameSsrc, static_cast<uint32_t>(0x11111111));
        EXPECT_EQ(ctx, ready.value().cname, std::vector<uint8_t>({'c','a','m','e','r','a'}));
    }
    EXPECT_FALSE(ctx, tracker.evidence(1'301));

    MediaRtcpSenderReportTracker mismatch(config);
    mismatch.observeMedia(0x11111111, 100);
    auto otherReport = parseRtcp(senderReport(0x22222222, 10, 0, 30), false);
    EXPECT_TRUE(ctx, otherReport);
    if (otherReport) EXPECT_TRUE(ctx, mismatch.observe(otherReport.value(), 200));
    EXPECT_FALSE(ctx, mismatch.evidence(200));
    auto matchingOlderReport = parseRtcp(senderReport(0x11111111, 9, 0, 30), false);
    EXPECT_TRUE(ctx, matchingOlderReport);
    if (matchingOlderReport) EXPECT_TRUE(ctx, mismatch.observe(matchingOlderReport.value(), 210));
}

void testRtcpEvidenceScopesCnameValidationToActiveSsrc(TestContext& ctx)
{
    const MediaRtcpSenderReportTrackerConfig config{true, true, 10'000, 10'000};
    constexpr uint32_t activeSsrc = 0x10203040;
    constexpr uint32_t unrelatedSsrc = 0x50607080;

    MediaRtcpSenderReportTracker tracker(config);
    tracker.observeMedia(activeSsrc, 1);
    auto compound = senderReport(activeSsrc, 10, 0, 900);
    const std::array chunks{
        SdesChunkInput{activeSsrc, {{2, "active-name"}, {1, "camera-a"}}},
        SdesChunkInput{unrelatedSsrc, {{1, ""}}}};
    auto descriptions = sourceDescription(chunks);
    compound.insert(compound.end(), descriptions.begin(), descriptions.end());
    const auto parsed = parseRtcp(compound, true);
    EXPECT_TRUE(ctx, parsed);
    if (!parsed) return;

    const auto generation = tracker.generation();
    EXPECT_TRUE(ctx, tracker.observe(parsed.value(), 2));
    EXPECT_EQ(ctx, tracker.generation(), generation);
    const auto ready = tracker.evidence(2);
    EXPECT_TRUE(ctx, ready);
    if (ready) {
        EXPECT_EQ(ctx, ready.value().cnameSsrc, activeSsrc);
        EXPECT_EQ(ctx, ready.value().cname,
                  std::vector<uint8_t>({'c','a','m','e','r','a','-','a'}));
    }

    MediaRtcpSenderReportTracker emptyActive(config);
    emptyActive.observeMedia(activeSsrc, 1);
    auto emptyCompound = senderReport(activeSsrc, 10, 0, 900);
    const std::array emptyActiveChunks{
        SdesChunkInput{activeSsrc, {{2, "active-name"}, {1, ""}}},
        SdesChunkInput{unrelatedSsrc, {{1, "other"}}}};
    auto emptyDescriptions = sourceDescription(emptyActiveChunks);
    emptyCompound.insert(emptyCompound.end(), emptyDescriptions.begin(), emptyDescriptions.end());
    const auto parsedEmptyActive = parseRtcp(emptyCompound, false);
    EXPECT_TRUE(ctx, parsedEmptyActive);
    if (!parsedEmptyActive) return;
    const auto emptyGeneration = emptyActive.generation();
    EXPECT_FALSE(ctx, emptyActive.observe(parsedEmptyActive.value(), 2));
    EXPECT_EQ(ctx, emptyActive.generation(), emptyGeneration + 1);
    EXPECT_FALSE(ctx, emptyActive.evidence(2));
}

void testRtcpEvidenceUpdateIsOrderIndependentAndConsumedOnce(TestContext& ctx)
{
    const MediaRtcpSenderReportTrackerConfig config{true, true, 10'000, 10'000};
    constexpr uint32_t source = 0x10203040;
    const auto compound = parseRtcp(
        compoundWithSdes(source, 10, 900, "camera-a"), true);
    EXPECT_TRUE(ctx, compound);
    if (!compound) return;

    MediaRtcpSenderReportTracker rtcpFirst(config);
    EXPECT_TRUE(ctx, rtcpFirst.observe(compound.value(), 100));
    auto beforeMedia = rtcpFirst.takeEvidenceUpdate(100);
    EXPECT_TRUE(ctx, beforeMedia);
    if (beforeMedia) EXPECT_FALSE(ctx, beforeMedia.value().has_value());
    rtcpFirst.observeMedia(source, 110);
    auto promoted = rtcpFirst.takeEvidenceUpdate(110);
    EXPECT_TRUE(ctx, promoted);
    if (promoted) {
        EXPECT_TRUE(ctx, promoted.value().has_value());
        if (promoted.value()) {
            EXPECT_EQ(ctx, promoted.value()->observedMediaSsrc, source);
            EXPECT_EQ(ctx, promoted.value()->cname,
                      std::vector<uint8_t>({'c','a','m','e','r','a','-','a'}));
        }
    }
    auto consumed = rtcpFirst.takeEvidenceUpdate(110);
    EXPECT_TRUE(ctx, consumed);
    if (consumed) EXPECT_FALSE(ctx, consumed.value().has_value());
    rtcpFirst.observeMedia(source, 120);
    auto repeatedMedia = rtcpFirst.takeEvidenceUpdate(120);
    EXPECT_TRUE(ctx, repeatedMedia);
    if (repeatedMedia) EXPECT_FALSE(ctx, repeatedMedia.value().has_value());

    MediaRtcpSenderReportTracker rtpFirst(config);
    rtpFirst.observeMedia(source, 100);
    EXPECT_TRUE(ctx, rtpFirst.observe(compound.value(), 110));
    auto observed = rtpFirst.takeEvidenceUpdate(110);
    EXPECT_TRUE(ctx, observed);
    if (observed) EXPECT_TRUE(ctx, observed.value().has_value());
    auto observedConsumed = rtpFirst.takeEvidenceUpdate(110);
    EXPECT_TRUE(ctx, observedConsumed);
    if (observedConsumed) EXPECT_FALSE(ctx, observedConsumed.value().has_value());

    const MediaRtcpSenderReportTrackerConfig reducedConfig{
        true, false, 10'000, 10'000};
    const auto reducedReport = parseReducedSizeRtcp(
        senderReport(source, 11, 0, 1'800));
    EXPECT_TRUE(ctx, reducedReport);
    if (!reducedReport) return;
    MediaRtcpSenderReportTracker reducedFirst(reducedConfig);
    EXPECT_TRUE(ctx, reducedFirst.observe(reducedReport.value(), 200));
    reducedFirst.observeMedia(source, 210);
    auto reducedEvidence = reducedFirst.takeEvidenceUpdate(210);
    EXPECT_TRUE(ctx, reducedEvidence);
    if (reducedEvidence && reducedEvidence.value()) {
        EXPECT_TRUE(ctx, reducedEvidence.value()->cname.empty());
        EXPECT_EQ(ctx, reducedEvidence.value()->senderReportObservedAtNs,
                  static_cast<std::int64_t>(200));
    }
}

void testPendingRtcpEvidenceRequiresMatchingMediaAndIsCleared(TestContext& ctx)
{
    const MediaRtcpSenderReportTrackerConfig config{true, true, 10'000, 10'000};
    constexpr uint32_t pendingSource = 0x10203040;
    constexpr uint32_t mediaSource = 0x50607080;
    const auto compound = parseRtcp(
        compoundWithSdes(pendingSource, 10, 900, "camera-a"), true);
    EXPECT_TRUE(ctx, compound);
    if (!compound) return;

    MediaRtcpSenderReportTracker mismatch(config);
    EXPECT_TRUE(ctx, mismatch.observe(compound.value(), 100));
    mismatch.observeMedia(mediaSource, 110);
    auto mismatched = mismatch.takeEvidenceUpdate(110);
    EXPECT_TRUE(ctx, mismatched);
    if (mismatched) EXPECT_FALSE(ctx, mismatched.value().has_value());

    MediaRtcpSenderReportTracker cleared(config);
    EXPECT_TRUE(ctx, cleared.observe(compound.value(), 100));
    cleared.observeContinuityLoss();
    cleared.observeMedia(pendingSource, 110);
    auto afterClear = cleared.takeEvidenceUpdate(110);
    EXPECT_TRUE(ctx, afterClear);
    if (afterClear) EXPECT_FALSE(ctx, afterClear.value().has_value());
}

void testRawRtpOpusDescriptorAcceptsOnlyMappingFamilyZeroChannels(TestContext& ctx)
{
    const auto create = [](int channels) {
        return MediaRawRtpStreamDescriptorFactory::create(MediaRtpDepacketizerConfig{
            MediaStreamKind::Audio, "opus", "", 98, 48'000, channels, 0});
    };

    EXPECT_TRUE(ctx, create(1));
    EXPECT_TRUE(ctx, create(2));
    const auto missingChannels = create(0);
    EXPECT_FALSE(ctx, missingChannels);
    if (!missingChannels) {
        EXPECT_EQ(ctx, missingChannels.error().code, media::ErrorCode::InvalidArgument);
    }
    const auto unsupportedChannels = create(3);
    EXPECT_FALSE(ctx, unsupportedChannels);
    if (!unsupportedChannels) {
        EXPECT_EQ(ctx, unsupportedChannels.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testRtcpEvidenceRejectsIdentityAndClockDiscontinuities(TestContext& ctx)
{
    const MediaRtcpSenderReportTrackerConfig config{true, true, 10'000, 10'000};
    MediaRtcpSenderReportTracker tracker(config);
    tracker.observeMedia(7, 10);
    auto initialSr = parseRtcp(senderReport(7, 100, 0, 1000), false);
    auto initialCname = parseRtcp(compoundWithSdes(7, 100, 1000, "source-a"), true);
    EXPECT_TRUE(ctx, initialSr && initialCname);
    if (!initialSr || !initialCname) return;
    EXPECT_TRUE(ctx, tracker.observe(initialSr.value(), 20));
    EXPECT_TRUE(ctx, tracker.observe(initialCname.value(), 20));
    EXPECT_TRUE(ctx, tracker.evidence(20));
    const uint64_t generation = tracker.generation();

    auto regressed = parseRtcp(senderReport(7, 99, 0, 2000), false);
    EXPECT_TRUE(ctx, regressed);
    if (regressed) EXPECT_FALSE(ctx, tracker.observe(regressed.value(), 30));
    EXPECT_TRUE(ctx, tracker.generation() > generation);
    EXPECT_FALSE(ctx, tracker.evidence(30));

    tracker.observeMedia(7, 40);
    auto reacquired = parseRtcp(compoundWithSdes(7, 50, 500, "source-a"), true);
    EXPECT_TRUE(ctx, reacquired);
    if (reacquired) EXPECT_TRUE(ctx, tracker.observe(reacquired.value(), 40));
    EXPECT_TRUE(ctx, tracker.evidence(40));
    auto changed = parseRtcp(compoundWithSdes(7, 50, 500, "source-b"), true);
    EXPECT_TRUE(ctx, changed);
    if (changed) EXPECT_FALSE(ctx, tracker.observe(changed.value(), 50));
    EXPECT_FALSE(ctx, tracker.evidence(50));

    tracker.observeMedia(7, 60);
    auto afterIdentityChange = parseRtcp(compoundWithSdes(7, 10, 100, "source-a"), true);
    EXPECT_TRUE(ctx, afterIdentityChange);
    if (afterIdentityChange) EXPECT_TRUE(ctx, tracker.observe(afterIdentityChange.value(), 60));
    const std::vector<uint8_t> byeBytes{0x81, 203, 0x00, 0x01, 0, 0, 0, 7};
    auto byeCompound = senderReport(7, 100, 0, 1000);
    byeCompound.insert(byeCompound.end(), byeBytes.begin(), byeBytes.end());
    auto bye = parseRtcp(byeCompound, false);
    EXPECT_TRUE(ctx, bye);
    if (bye) EXPECT_FALSE(ctx, tracker.observe(bye.value(), 70));
    EXPECT_FALSE(ctx, tracker.evidence(70));

    MediaRtcpSenderReportTracker sameInstant(config);
    sameInstant.observeMedia(9, 1);
    auto firstInstant = parseRtcp(senderReport(9, 5, 6, 100), false);
    auto changedRtp = parseRtcp(senderReport(9, 5, 6, 101), false);
    EXPECT_TRUE(ctx, firstInstant && changedRtp);
    if (firstInstant) EXPECT_TRUE(ctx, sameInstant.observe(firstInstant.value(), 2));
    if (changedRtp) EXPECT_FALSE(ctx, sameInstant.observe(changedRtp.value(), 3));

    MediaRtcpSenderReportTracker changedSsrc(config);
    changedSsrc.observeMedia(10, 1);
    auto oldSource = parseRtcp(compoundWithSdes(10, 100, 1000, "old"), true);
    EXPECT_TRUE(ctx, oldSource);
    if (oldSource) EXPECT_TRUE(ctx, changedSsrc.observe(oldSource.value(), 2));
    changedSsrc.observeMedia(11, 3);
    auto newSource = parseRtcp(compoundWithSdes(11, 1, 10, "new"), true);
    EXPECT_TRUE(ctx, newSource);
    if (newSource) EXPECT_TRUE(ctx, changedSsrc.observe(newSource.value(), 4));
    EXPECT_TRUE(ctx, changedSsrc.evidence(4));
    const auto continuityGeneration = changedSsrc.generation();
    changedSsrc.observeContinuityLoss();
    EXPECT_EQ(ctx, changedSsrc.generation(), continuityGeneration + 1);
    EXPECT_FALSE(ctx, changedSsrc.evidence(5));

    MediaRtcpSenderReportTracker emptyCname(config);
    emptyCname.observeMedia(12, 1);
    const MediaRtcpPacket emptyIdentity{
        MediaRtcpPacketKind::SourceDescription, 202, 1, 0, std::nullopt, std::nullopt,
        {{12, {{1, {}}}}}, {}, {}};
    const auto emptyGeneration = emptyCname.generation();
    EXPECT_FALSE(ctx, emptyCname.observe({emptyIdentity}, 2));
    EXPECT_TRUE(ctx, emptyCname.generation() > emptyGeneration);
    EXPECT_FALSE(ctx, emptyCname.evidence(2));
}

void testRtpIngressClockLifecycleEventsAreStructured(TestContext& ctx)
{
    MediaRtpIngressEventBuffer discontinuity(
        MediaRtpDiscontinuity{MediaRtpDiscontinuityReason::SsrcChanged, 1, 2}, 7, 1);
    EXPECT_EQ(ctx, discontinuity.kind(), MediaRtpIngressEventKind::Discontinuity);
    EXPECT_EQ(ctx, discontinuity.discontinuityGeneration(),
              std::optional<std::uint64_t>(7));

    MediaRtpIngressEventBuffer observation(MediaRtpClockObservation{123456}, 2);
    EXPECT_EQ(ctx, observation.kind(), MediaRtpIngressEventKind::ClockObservation);
    EXPECT_TRUE(ctx, observation.clockObservation().has_value());
    if (observation.clockObservation()) {
        EXPECT_EQ(ctx, observation.clockObservation()->observedAtNs, static_cast<std::int64_t>(123456));
    }

    MediaRtpIngressEventBuffer invalidated(MediaRtpClockInvalidation{9}, 3);
    EXPECT_EQ(ctx, invalidated.kind(), MediaRtpIngressEventKind::ClockInvalidation);
    EXPECT_TRUE(ctx, invalidated.clockInvalidation().has_value());
    if (invalidated.clockInvalidation()) {
        EXPECT_EQ(ctx, invalidated.clockInvalidation()->generation, static_cast<std::uint64_t>(9));
    }
    EXPECT_TRUE(ctx, hasFlag(invalidated.flags(), MediaBufferFlag::Discontinuity));
}

class ScriptedAudioEncoderCodecApi final : public AudioEncoderCodecApi {
public:
    std::deque<int> sendResults;
    std::deque<int> receiveResults;
    std::vector<int> sentSamples;
    std::vector<int64_t> sentPts;
    std::vector<const AVFrame*> sentFramePointers;
    int flushCount = 0;

    int sendFrame(AVCodecContext*, const AVFrame* frame) noexcept override
    {
        sentSamples.push_back(frame ? frame->nb_samples : -1);
        sentPts.push_back(frame ? frame->pts : AV_NOPTS_VALUE);
        sentFramePointers.push_back(frame);
        if (sendResults.empty()) return 0;
        const int result = sendResults.front();
        sendResults.pop_front();
        return result;
    }

    int receivePacket(AVCodecContext*, AVPacket* packet) noexcept override
    {
        if (receiveResults.empty()) return AVERROR(EAGAIN);
        const int result = receiveResults.front();
        receiveResults.pop_front();
        if (result == 0) {
            if (av_new_packet(packet, 1) < 0) return AVERROR(ENOMEM);
            packet->pts = 0;
            packet->dts = 0;
            packet->duration = 1024;
        }
        return result;
    }

    void flushBuffers(AVCodecContext*) noexcept override
    {
        ++flushCount;
        receiveResults.clear();
    }
};

::media::ffmpeg::FramePtr makeAudioFrame(int samples, int64_t pts)
{
    auto frame = ::media::ffmpeg::makeFrame();
    if (!frame) return {};
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 48000;
    frame->nb_samples = samples;
    frame->pts = pts;
    av_channel_layout_default(&frame->ch_layout, 2);
    if (av_frame_get_buffer(frame.get(), 0) < 0) return {};
    return frame;
}

void testAudioEncodeFixedFrameStateMachine(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(1);
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "test.codec");
    const MediaNodeId frameSource = graph.addNode(MediaNodeKind::DebugDump, "test.frame");
    const MediaNodeId encoder = graph.addNode(MediaNodeKind::AudioEncode, "test.encoder");
    const MediaNodeId packetSink = graph.addNode(MediaNodeKind::DebugDump, "test.packet");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addInputPort(encoder, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(encoder, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Unknown);
    graph.addOutputPort(encoder, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    graph.addInputPort(packetSink, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Unknown);
    graph.connect(codecSource, "codec", encoder, "codec", "test.codec", policy);
    graph.connect(frameSource, "frame", encoder, "frame", "test.frame", policy);
    graph.connect(encoder, "packet", packetSink, "packet", "test.packet", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaChannel* codecInput = execution.findInputChannel(encoder, "codec");
    MediaChannel* frameInput = execution.findInputChannel(encoder, "frame");
    MediaChannel* packetOutput = execution.findOutputChannel(encoder, "packet");
    EXPECT_TRUE(ctx, codecInput != nullptr);
    EXPECT_TRUE(ctx, frameInput != nullptr);
    EXPECT_TRUE(ctx, packetOutput != nullptr);
    if (!codecInput || !frameInput || !packetOutput) return;

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, codec != nullptr);
    if (!codec) return;
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48000;
    codec->frame_size = 1024;
    codec->time_base = AVRational{1, 48000};
    av_channel_layout_default(&codec->ch_layout, 2);
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    EXPECT_TRUE(ctx, codecBuffer);
    if (!codecBuffer) return;
    EXPECT_TRUE(ctx, codecInput->push(codecBuffer.value()));

    auto api = std::make_shared<ScriptedAudioEncoderCodecApi>();
    api->sendResults = {AVERROR(EAGAIN), 0, 0, 0};
    api->receiveResults = {AVERROR(EAGAIN), 0, AVERROR(EAGAIN), AVERROR(EAGAIN), AVERROR_EOF};
    AudioEncodeNode node(
        encoder, MediaAudioLineageExecutionMode::LegacyPlainPacket,
        std::make_shared<AudioEncodeLineageState>(
            MediaAudioLineageExecutionMode::LegacyPlainPacket, 0), api);
    EXPECT_TRUE(ctx, node.process(execution));

    auto blockerPacket = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, blockerPacket != nullptr);
    if (!blockerPacket) return;
    EXPECT_TRUE(ctx, av_new_packet(blockerPacket.get(), 1) >= 0);
    auto blocker = FFmpegBufferFactory::wrapPacket(std::move(blockerPacket), MediaStreamKind::Audio, std::nullopt);
    EXPECT_TRUE(ctx, blocker);
    if (!blocker) return;
    EXPECT_TRUE(ctx, packetOutput->push(blocker.value()));

    auto fullBuffer = FFmpegBufferFactory::wrapFrame(makeAudioFrame(1024, 0), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, fullBuffer);
    if (!fullBuffer) return;
    EXPECT_TRUE(ctx, frameInput->push(fullBuffer.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(2));
    if (api->sentSamples.size() >= 2 && api->sentPts.size() >= 2) {
        EXPECT_EQ(ctx, api->sentSamples[0], 1024);
        EXPECT_EQ(ctx, api->sentSamples[1], 1024);
        EXPECT_EQ(ctx, api->sentPts[0], api->sentPts[1]);
    }

    MediaBufferRef output;
    const bool blockerPopped = packetOutput->tryPop(output);
    EXPECT_TRUE(ctx, blockerPopped);
    if (blockerPopped) EXPECT_TRUE(ctx, output == blocker.value());
    EXPECT_TRUE(ctx, node.process(execution));
    const bool packetPopped = packetOutput->tryPop(output);
    EXPECT_TRUE(ctx, packetPopped);
    if (packetPopped) EXPECT_FALSE(ctx, output->isEof());
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(2));

    auto tailBuffer = FFmpegBufferFactory::wrapFrame(makeAudioFrame(100, 1024), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, tailBuffer);
    if (!tailBuffer) return;
    EXPECT_TRUE(ctx, frameInput->push(tailBuffer.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(2));
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, eof);
    if (!eof) return;
    EXPECT_TRUE(ctx, frameInput->push(eof.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(4));
    if (api->sentSamples.size() >= 4) {
        EXPECT_EQ(ctx, api->sentSamples[2], 100);
        EXPECT_EQ(ctx, api->sentSamples[3], -1);
    }
    const bool eofPopped = packetOutput->tryPop(output);
    EXPECT_TRUE(ctx, eofPopped);
    if (eofPopped) EXPECT_TRUE(ctx, output->isEof());
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_EQ(ctx, api->sentSamples.size(), static_cast<std::size_t>(4));
    EXPECT_FALSE(ctx, packetOutput->tryPop(output));
}

void testAudioLineageStagesExposeExactStablePurgeTargets(TestContext& ctx)
{
    const std::array stages{
        std::pair{MediaNodeKind::AudioDecode, MediaAudioDecodeLineageIdentity},
        std::pair{MediaNodeKind::AudioStartupTrim,
                  MediaAudioStartupTrimLineageIdentity},
        std::pair{MediaNodeKind::AudioResample, MediaAudioResampleLineageIdentity},
        std::pair{MediaNodeKind::AudioEncode, MediaAudioEncodeLineageIdentity}};

    const auto targetOf = [](MediaRuntimeNode* runtime) {
        if (auto* node = dynamic_cast<AudioDecodeNode*>(runtime))
            return node->generationPurgeTarget();
        if (auto* node = dynamic_cast<MediaAudioStartupTrimNode*>(runtime))
            return node->generationPurgeTarget();
        if (auto* node = dynamic_cast<AudioResampleNode*>(runtime))
            return node->generationPurgeTarget();
        if (auto* node = dynamic_cast<AudioEncodeNode*>(runtime))
            return node->generationPurgeTarget();
        return std::shared_ptr<MediaAvGenerationPurgeTarget>{};
    };

    std::uint64_t nodeId = 100;
    for (const auto& [kind, identity] : stages) {
        MediaNode missing{
            MediaNodeId::fromValue(nodeId++), kind, "missing_identity"};
        missing.options.set(std::string(MediaAudioLineageModeOptionKey),
                            "synchronized_released_audio");
        missing.options.set("audio.lineage.capacity", "8");
        EXPECT_FALSE(ctx, MediaRuntimeNodeFactory::create(missing));

        MediaNode wrong{
            MediaNodeId::fromValue(nodeId++), kind, "wrong_identity"};
        wrong.options.set(std::string(MediaAudioLineageModeOptionKey),
                          "synchronized_released_audio");
        wrong.options.set("audio.lineage.identity", "wrong_identity");
        wrong.options.set("audio.lineage.capacity", "8");
        EXPECT_FALSE(ctx, MediaRuntimeNodeFactory::create(wrong));

        MediaNode planned{
            MediaNodeId::fromValue(nodeId++), kind, "planned_identity"};
        planned.options.set(std::string(MediaAudioLineageModeOptionKey),
                            "synchronized_released_audio");
        planned.options.set("audio.lineage.identity", std::string(identity));
        planned.options.set("audio.lineage.capacity", "8");
        auto runtime = MediaRuntimeNodeFactory::create(planned);
        EXPECT_TRUE(ctx, runtime);
        if (!runtime) continue;
        auto firstTarget = targetOf(runtime.value().get());
        auto secondTarget = targetOf(runtime.value().get());
        EXPECT_TRUE(ctx, firstTarget != nullptr);
        EXPECT_TRUE(ctx, firstTarget == secondTarget);
        auto state = std::dynamic_pointer_cast<MediaAudioLineageState>(firstTarget);
        EXPECT_TRUE(ctx, state != nullptr);
        if (!state) continue;
        EXPECT_TRUE(ctx, state->observe(7));
        EXPECT_TRUE(ctx, state->purge(MediaAvGenerationPurge{7, 8, 1}));
        EXPECT_FALSE(ctx, state->isCurrent(7));
        EXPECT_TRUE(ctx, state->isCurrent(8));
        state.reset();
        secondTarget.reset();
        firstTarget.reset();
        runtime.value().reset();
    }
}

void testRtpClockGroupRejectsStaleCrossPortEvidence(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaBlockingEdgePolicyPlanner::planQueue(8);
    const MediaNodeId videoClockSource = graph.addNode(MediaNodeKind::DebugDump, "test.video_clock");
    const MediaNodeId videoEventSource = graph.addNode(MediaNodeKind::DebugDump, "test.video_event");
    const MediaNodeId audioClockSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio_clock");
    const MediaNodeId audioEventSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio_event");
    const MediaNodeId group = graph.addNode(MediaNodeKind::RtpClockGroup, "test.clock_group");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "test.clock_group_sink");

    graph.addOutputPort(videoClockSource, "clock", MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addOutputPort(videoEventSource, "event", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(audioClockSource, "clock", MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addOutputPort(audioEventSource, "event", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(group, "video_clock", MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addInputPort(group, "video_event", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(group, "audio_clock", MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addInputPort(group, "audio_event", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(group, "clock_group", MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addInputPort(sink, "clock_group", MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.connect(videoClockSource, "clock", group, "video_clock", "test.video_clock", policy);
    graph.connect(videoEventSource, "event", group, "video_event", "test.video_event", policy);
    graph.connect(audioClockSource, "clock", group, "audio_clock", "test.audio_clock", policy);
    graph.connect(audioEventSource, "event", group, "audio_event", "test.audio_event", policy);
    graph.connect(group, "clock_group", sink, "clock_group", "test.clock_group", policy);

    graph.setNodeOption(group, "rtp_clock_group.video_clock_rate", "90000");
    graph.setNodeOption(group, "rtp_clock_group.audio_clock_rate", "48000");
    graph.setNodeOption(group, "rtp_clock_group.sender_report_timeout_ns", "3000000000");
    graph.setNodeOption(group, "rtp_clock_group.maximum_extrapolation_ns", "5000000000");
    graph.setNodeOption(
        group,
        "rtp_clock_group.maximum_inter_stream_clock_offset_skew_ns",
        "50000000");
    graph.setNodeOption(group, "rtp_clock_group.maximum_sender_clock_residual_ns", "250000000");
    graph.setNodeOption(group, "rtp_clock_group.video_cname_timeout_ns", "5000000000");
    graph.setNodeOption(group, "rtp_clock_group.audio_cname_timeout_ns", "5000000000");
    graph.setNodeOption(group, "rtp_clock_group.require_matching_cname", "true");
    graph.setNodeOption(group, "rtp_clock_group.maximum_sender_clock_rate_error_ppm", "1000");
    graph.setNodeOption(
        group, "rtp_clock_group.common_epoch_policy",
        "earliest_locked_sender_report_source_time");

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaChannel* videoClock = execution.findInputChannel(group, "video_clock");
    MediaChannel* videoEvent = execution.findInputChannel(group, "video_event");
    MediaChannel* audioClock = execution.findInputChannel(group, "audio_clock");
    MediaChannel* audioEvent = execution.findInputChannel(group, "audio_event");
    MediaChannel* output = execution.findOutputChannel(group, "clock_group");
    EXPECT_TRUE(ctx, videoClock != nullptr);
    EXPECT_TRUE(ctx, videoEvent != nullptr);
    EXPECT_TRUE(ctx, audioClock != nullptr);
    EXPECT_TRUE(ctx, audioEvent != nullptr);
    EXPECT_TRUE(ctx, output != nullptr);
    if (!videoClock || !videoEvent || !audioClock || !audioEvent || !output) return;

    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto evidence = [now](std::uint32_t ssrc, std::uint32_t rtpTimestamp,
                          std::uint64_t generation) {
        return MediaRtcpClockEvidence{ssrc, ssrc, ssrc, {100, 0}, rtpTimestamp,
                                      {'c', 'a', 'm', 'e', 'r', 'a'}, now, now, generation};
    };
    auto pushEvidence = [](MediaChannel& channel, MediaRtcpClockEvidence value,
                           std::uint64_t sequence) {
        return channel.push(makeMediaBufferRef<MediaRtpIngressEventBuffer>(
            std::move(value), sequence));
    };
    auto drainSnapshots = [&]() {
        std::vector<MediaRtpClockGroupSnapshot> snapshots;
        MediaBufferRef buffer;
        while (output->tryPop(buffer)) {
            const auto* groupBuffer = dynamic_cast<const MediaRtpClockGroupBuffer*>(buffer.get());
            EXPECT_TRUE(ctx, groupBuffer != nullptr);
            if (groupBuffer) snapshots.push_back(groupBuffer->snapshot());
        }
        return snapshots;
    };
    auto drainStates = [&]() {
        std::vector<MediaRtpClockGroupState> states;
        for (const auto& snapshot : drainSnapshots()) states.push_back(snapshot.state);
        return states;
    };

    MediaRtpClockGroupNode node(group);
    EXPECT_TRUE(ctx, node.process(execution));
    drainStates();

    EXPECT_TRUE(ctx, videoEvent->push(
        makeMediaBufferRef<MediaRtpIngressEventBuffer>(MediaRtpClockObservation{-1}, 1)));
    EXPECT_FALSE(ctx, node.process(execution));
    drainStates();

    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(ctx, videoEvent->push(
            makeMediaBufferRef<MediaRtpIngressEventBuffer>(
                MediaRtpClockObservation{now}, static_cast<std::uint64_t>(i + 2))));
    }
    EXPECT_TRUE(ctx, audioEvent->push(
        makeMediaBufferRef<MediaRtpIngressEventBuffer>(MediaRtpClockObservation{now}, 1)));
    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 1), 5));
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 1), 2));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, videoEvent->size() <= static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, audioEvent->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, videoClock->size(), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, audioClock->size(), static_cast<std::size_t>(0));
    const auto fairStates = drainStates();
    EXPECT_TRUE(ctx, !fairStates.empty());
    if (!fairStates.empty()) {
        EXPECT_EQ(ctx, fairStates.back(), MediaRtpClockGroupState::Locked);
    }
    videoEvent->clear();

    EXPECT_TRUE(ctx, videoEvent->push(makeMediaBufferRef<MediaRtpIngressEventBuffer>(
        MediaRtpDiscontinuity{MediaRtpDiscontinuityReason::SequenceGap, 10, 11}, 2, 6)));
    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 1), 7));
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 1), 3));
    EXPECT_TRUE(ctx, node.process(execution));
    const auto discontinuityStates = drainStates();
    for (const auto state : discontinuityStates) {
        EXPECT_FALSE(ctx, state == MediaRtpClockGroupState::Locked);
    }
    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 2), 8));
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 1), 4));
    EXPECT_TRUE(ctx, node.process(execution));
    const auto discontinuityRecoveryStates = drainStates();
    EXPECT_TRUE(ctx, !discontinuityRecoveryStates.empty());
    if (!discontinuityRecoveryStates.empty()) {
        EXPECT_EQ(ctx, discontinuityRecoveryStates.back(), MediaRtpClockGroupState::Locked);
    }

    EXPECT_TRUE(ctx, videoEvent->push(
        makeMediaBufferRef<MediaRtpIngressEventBuffer>(MediaRtpClockInvalidation{2}, 9)));
    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 1), 10));
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 1), 5));
    for (int i = 0; i < 3; ++i) EXPECT_TRUE(ctx, node.process(execution));
    const auto staleStates = drainStates();
    for (const auto state : staleStates) {
        EXPECT_FALSE(ctx, state == MediaRtpClockGroupState::Locked);
    }

    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 1), 11));
    EXPECT_TRUE(ctx, node.process(execution));
    const auto rejectedStates = drainStates();
    for (const auto state : rejectedStates) {
        EXPECT_FALSE(ctx, state == MediaRtpClockGroupState::Locked);
    }

    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 2), 12));
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 1), 6));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, node.process(execution));
    const auto reacquiredStates = drainStates();
    EXPECT_TRUE(ctx, !reacquiredStates.empty());
    if (!reacquiredStates.empty()) {
        EXPECT_EQ(ctx, reacquiredStates.back(), MediaRtpClockGroupState::Locked);
    }

    EXPECT_TRUE(ctx, videoEvent->push(
        makeMediaBufferRef<MediaRtpIngressEventBuffer>(MediaRtpClockInvalidation{2}, 13)));
    EXPECT_TRUE(ctx, videoEvent->push(
        makeMediaBufferRef<MediaRtpIngressEventBuffer>(MediaRtpClockInvalidation{3}, 14)));
    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 2), 15));
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 1), 7));
    EXPECT_TRUE(ctx, node.process(execution));
    const auto stackedInvalidationSnapshots = drainSnapshots();
    for (const auto& snapshot : stackedInvalidationSnapshots) {
        EXPECT_FALSE(ctx, snapshot.state == MediaRtpClockGroupState::Locked);
    }
    EXPECT_TRUE(ctx, !stackedInvalidationSnapshots.empty());
    const std::uint64_t generationBeforeBacklog = stackedInvalidationSnapshots.empty()
        ? 0
        : stackedInvalidationSnapshots.back().groupGeneration;

    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 3), 16));
    for (std::uint64_t generation = 4; generation <= 8; ++generation) {
        EXPECT_TRUE(ctx, videoEvent->push(
            makeMediaBufferRef<MediaRtpIngressEventBuffer>(
                MediaRtpClockInvalidation{generation}, generation + 13)));
    }
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 2), 8));
    EXPECT_TRUE(ctx, node.process(execution));
    const auto firstBacklogTurn = drainSnapshots();
    EXPECT_TRUE(ctx, !firstBacklogTurn.empty());
    for (const auto& snapshot : firstBacklogTurn) {
        EXPECT_FALSE(ctx, snapshot.state == MediaRtpClockGroupState::Locked);
    }
    if (!firstBacklogTurn.empty()) {
        EXPECT_EQ(ctx, firstBacklogTurn.back().state,
                  MediaRtpClockGroupState::Acquiring);
        EXPECT_EQ(ctx, firstBacklogTurn.back().groupGeneration,
                  generationBeforeBacklog + 2);
    }

    EXPECT_TRUE(ctx, node.process(execution));
    const auto secondBacklogTurn = drainSnapshots();
    EXPECT_TRUE(ctx, !secondBacklogTurn.empty());
    for (const auto& snapshot : secondBacklogTurn) {
        EXPECT_FALSE(ctx, snapshot.state == MediaRtpClockGroupState::Locked);
    }

    EXPECT_TRUE(ctx, pushEvidence(*videoClock, evidence(11, 90'000, 8), 22));
    EXPECT_TRUE(ctx, pushEvidence(*audioClock, evidence(22, 48'000, 3), 9));
    EXPECT_TRUE(ctx, node.process(execution));
    const auto boundedFairnessSnapshots = drainSnapshots();
    EXPECT_TRUE(ctx, !boundedFairnessSnapshots.empty());
    for (std::size_t index = 0; index + 1 < boundedFairnessSnapshots.size(); ++index) {
        EXPECT_FALSE(ctx, boundedFairnessSnapshots[index].state ==
                          MediaRtpClockGroupState::Locked);
    }
    if (!boundedFairnessSnapshots.empty()) {
        const auto& locked = boundedFairnessSnapshots.back();
        EXPECT_EQ(ctx, locked.state, MediaRtpClockGroupState::Locked);
        EXPECT_TRUE(ctx, locked.locked.has_value());
        if (locked.locked) {
            EXPECT_EQ(ctx, locked.locked->video.generation,
                      static_cast<std::uint64_t>(8));
            EXPECT_EQ(ctx, locked.locked->audio.generation,
                      static_cast<std::uint64_t>(3));
        }
    }
    EXPECT_TRUE(ctx, node.stop(execution));
    execution.reset();
}

void testReservedNodeKind41FailsClosedInRuntimeFactory(TestContext& ctx)
{
    EXPECT_EQ(ctx, static_cast<int>(MediaNodeKind::ReservedNodeKind41), 41);
    EXPECT_FALSE(ctx,
                 MediaRuntimeNodeFactory::supported(
                     MediaNodeKind::ReservedNodeKind41));

    const MediaNode reserved{MediaNodeId{41},
                             MediaNodeKind::ReservedNodeKind41,
                             "reserved.node_kind_41"};
    EXPECT_FALSE(ctx, MediaRuntimeNodeFactory::create(reserved));
}

} // namespace

int main()
{
    TestContext ctx;
    testAudioLineageStagesExposeExactStablePurgeTargets(ctx);

    testMpegTsOutputClockUsesOneEpoch(ctx);
    testMpegTsOutputClockRejectsInvalidPolicyAndHandlesWrap(ctx);
    testMpegTsOutputClockRequiresExactTransportDecodeLead(ctx);
    testMpegTsOutputClockCommitsOnlyCompletedTransactions(ctx);
    RtpMuxStateMachine state;
    EXPECT_TRUE(ctx, state.bindExpectations(true, false, true, 0));
    EXPECT_TRUE(ctx, state.bindOutput());
    EXPECT_TRUE(ctx, state.markHeaderWritten());
    state.setExpectedInputs({"video"}, {"packet"});
    EXPECT_TRUE(ctx, state.markConfigReady("video"));
    EXPECT_FALSE(ctx, state.markTrailerWritten());
    EXPECT_TRUE(ctx, state.markInputEof("packet"));
    EXPECT_TRUE(ctx, state.markTrailerWritten());
    EXPECT_TRUE(ctx, state.finished());

    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    EXPECT_TRUE(ctx, codec != nullptr);
    if (!codec) return 1;
    codec->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codec->sample_rate = 48000;
    codec->frame_size = 1024;
    av_channel_layout_default(&codec->ch_layout, 2);

    AudioEncoderFrameQueue frameQueue(
        MediaAudioLineageExecutionMode::LegacyPlainPacket, 0);
    EXPECT_TRUE(ctx, frameQueue.configure(*codec));

    auto makeFrame = [](int samples, int64_t pts) {
        auto frame = ::media::ffmpeg::makeFrame();
        frame->format = AV_SAMPLE_FMT_FLTP;
        frame->sample_rate = 48000;
        frame->nb_samples = samples;
        frame->pts = pts;
        av_channel_layout_default(&frame->ch_layout, 2);
        if (av_frame_get_buffer(frame.get(), 0) < 0) {
            frame.reset();
        }
        return frame;
    };

    auto firstInput = makeFrame(1098, 0);
    EXPECT_TRUE(ctx, firstInput != nullptr);
    if (!firstInput) return 1;
    EXPECT_TRUE(ctx, frameQueue.push(*firstInput, {}));
    EXPECT_TRUE(ctx, frameQueue.hasFullFrame());
    auto firstOutput = frameQueue.popFullFrame();
    EXPECT_TRUE(ctx, firstOutput);
    if (firstOutput) {
        EXPECT_EQ(ctx, firstOutput.value().media->nb_samples, 1024);
        EXPECT_EQ(ctx, firstOutput.value().media->pts, static_cast<int64_t>(0));
    }
    EXPECT_FALSE(ctx, frameQueue.hasFullFrame());
    EXPECT_EQ(ctx, frameQueue.queuedSamples(), 74);

    auto secondInput = makeFrame(950, 1098);
    EXPECT_TRUE(ctx, secondInput != nullptr);
    if (!secondInput) return 1;
    EXPECT_TRUE(ctx, frameQueue.push(*secondInput, {}));
    EXPECT_TRUE(ctx, frameQueue.hasFullFrame());
    auto secondOutput = frameQueue.popFullFrame();
    EXPECT_TRUE(ctx, secondOutput);
    if (secondOutput) {
        EXPECT_EQ(ctx, secondOutput.value().media->nb_samples, 1024);
        EXPECT_EQ(ctx, secondOutput.value().media->pts, static_cast<int64_t>(1024));
    }
    EXPECT_EQ(ctx, frameQueue.queuedSamples(), 0);

    auto tailInput = makeFrame(100, 2048);
    EXPECT_TRUE(ctx, tailInput != nullptr);
    if (!tailInput) return 1;
    EXPECT_TRUE(ctx, frameQueue.push(*tailInput, {}));
    auto tailOutput = frameQueue.popRemainingFrame();
    EXPECT_TRUE(ctx, tailOutput);
    if (tailOutput) {
        EXPECT_EQ(ctx, tailOutput.value().media->nb_samples, 100);
        EXPECT_EQ(ctx, tailOutput.value().media->pts, static_cast<int64_t>(2048));
    }
    EXPECT_EQ(ctx, frameQueue.queuedSamples(), 0);

    AudioEncoderFrameQueue overlapQueue(
        MediaAudioLineageExecutionMode::LegacyPlainPacket, 0);
    EXPECT_TRUE(ctx, overlapQueue.configure(*codec));
    auto overlapFirst = makeFrame(512, 0);
    auto overlapSecond = makeFrame(512, 256);
    EXPECT_TRUE(ctx, overlapFirst != nullptr);
    EXPECT_TRUE(ctx, overlapSecond != nullptr);
    if (overlapFirst && overlapSecond) {
        EXPECT_TRUE(ctx, overlapQueue.push(*overlapFirst, {}));
        const auto overlapStatus = overlapQueue.push(*overlapSecond, {});
        EXPECT_FALSE(ctx, overlapStatus);
        if (!overlapStatus) EXPECT_EQ(ctx, overlapStatus.error().code, media::ErrorCode::InvalidArgument);
    }

    AudioEncoderFrameQueue gapQueue(
        MediaAudioLineageExecutionMode::LegacyPlainPacket, 0);
    EXPECT_TRUE(ctx, gapQueue.configure(*codec));
    auto gapFirst = makeFrame(512, 0);
    auto gapSecond = makeFrame(512, 768);
    EXPECT_TRUE(ctx, gapFirst != nullptr);
    EXPECT_TRUE(ctx, gapSecond != nullptr);
    if (gapFirst && gapSecond) {
        EXPECT_TRUE(ctx, gapQueue.push(*gapFirst, {}));
        const auto gapStatus = gapQueue.push(*gapSecond, {});
        EXPECT_FALSE(ctx, gapStatus);
        if (!gapStatus) EXPECT_EQ(ctx, gapStatus.error().code, media::ErrorCode::InvalidArgument);
    }
    testRtpPacketParserStrictHeader(ctx);
    testRequiredPossiblyEmptyNodeOption(ctx);
    testRtcpCompoundParserStrictPackets(ctx);
    testRtcpEvidenceRequiresSameSsrcAndExpires(ctx);
    testRtcpEvidenceScopesCnameValidationToActiveSsrc(ctx);
    testRtcpEvidenceUpdateIsOrderIndependentAndConsumedOnce(ctx);
    testPendingRtcpEvidenceRequiresMatchingMediaAndIsCleared(ctx);
    testRtcpEvidenceRejectsIdentityAndClockDiscontinuities(ctx);
    testRawRtpOpusDescriptorAcceptsOnlyMappingFamilyZeroChannels(ctx);
    testRtpIngressClockLifecycleEventsAreStructured(ctx);
    runRtpDepacketizerTests(ctx);
    runRtpSourceClockTests(ctx);
    runRtpOutputClockTests(ctx);
    runScheduledRtpPacketizationTests(ctx);
    runScheduledRtpSenderTests(ctx);
    runMediaRtpUdpSenderTransportTests(ctx);
    runMediaRtpUdpSenderCompositionTests(ctx);
    runRawRtpInputLifecycleTests(ctx);
    runMpegTsPacketTests(ctx);
    runMpegTsPesProvenanceTimelineTests(ctx);
    runMpegTsClockTests(ctx);
    runMpegTsInputSessionTests(ctx);
    runMpegTsDemuxNodeTests(ctx);
    runAvStartupCoordinatorTests(ctx);
    runAudioSwrCompensationExecutorTests(ctx);
    runAudioResampleNodeTests(ctx);
    runOutputByteSinkTests(ctx);
    runMpegTsOutputSerializerTests(ctx);
    runMpegTsAccessUnitFramerTests(ctx);
    runMpegTsOutputPacketizerTests(ctx);
    runMpegTsOutputSessionTests(ctx);
    runMpegTsMuxRuntimeBufferTests(ctx);
    runMpegTsFfmpegConfigMaterializerTests(ctx);
    runProjectMpegTsMuxSessionAdapterTests(ctx);
    runProjectMpegTsFileMuxNodeTests(ctx);
    runFileMuxSessionTests(ctx);
    runFileOutputResourceTests(ctx);
    testAudioEncodeFixedFrameStateMachine(ctx);
    testRtpClockGroupRejectsStaleCrossPortEvidence(ctx);
    testReservedNodeKind41FailsClosedInRuntimeFactory(ctx);
    EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::supported(MediaNodeKind::CanonicalInput));
    EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::supported(
                         MediaNodeKind::AudioStartupTrim));
    EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::supported(
                         MediaNodeKind::AvBoundReleaseExtractor));
    return ctx.failures == 0 ? 0 : 1;
}
