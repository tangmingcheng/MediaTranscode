#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketBuilder.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <type_traits>
#include <optional>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaTsMuxPlanParameters packetizerParameters(
    MediaTsContinuitySeeds seeds = {3, 5, 7, 9})
{
    return MediaTsMuxPlanParameters{
        1, 1, 0, 0x0100, 0x0101, 0x0102, 0x0101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, 4,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188, seeds, 7,
        MediaTsOutputTransportKind::Udp};
}

MediaTsMuxPlan packetizerPlan(MediaTsContinuitySeeds seeds = {3, 5, 7, 9})
{
    return MediaTsMuxPlan::create(packetizerParameters(seeds)).value();
}

std::uint16_t pid(const std::array<std::uint8_t, 188>& packet)
{
    return static_cast<std::uint16_t>(((packet[1] & 0x1F) << 8) | packet[2]);
}

std::uint8_t continuity(const std::array<std::uint8_t, 188>& packet)
{
    return packet[3] & 0x0F;
}

std::span<const std::uint8_t> payload(const std::array<std::uint8_t, 188>& packet)
{
    const auto control = static_cast<std::uint8_t>((packet[3] >> 4) & 0x03);
    if ((control & 1) == 0) return {};
    const std::size_t offset = (control & 2) != 0 ? 5 + packet[4] : 4;
    return std::span<const std::uint8_t>(packet).subspan(offset);
}

MediaTsPesHeader videoHeader(std::size_t bytes)
{
    return MediaTsPesSerializer::header(
        MediaScheduledStream::Video, MediaTsPacketClock{9000, 8000, 9000, 8000},
        bytes).value();
}

MediaTsPesHeader audioHeader(std::size_t bytes)
{
    return MediaTsPesSerializer::header(
        MediaScheduledStream::Audio, MediaTsPacketClock{9000, 9000, 9000, 9000},
        bytes).value();
}

std::vector<std::array<std::uint8_t, 188>> drain(
    TestContext& ctx, MediaTsPacketCursor& cursor, std::size_t batchLimit)
{
    std::vector<std::array<std::uint8_t, 188>> packets;
    while (!cursor.finished()) {
        auto prepared = cursor.prepare(batchLimit);
        EXPECT_TRUE(ctx, prepared);
        if (!prepared) break;
        packets.insert(packets.end(), prepared.value().packets().begin(),
                       prepared.value().packets().end());
        EXPECT_TRUE(ctx, cursor.commit(prepared.value().takeCommitToken()));
    }
    return packets;
}

class RecordingSink final : public MediaTsPacketSink {
public:
    ::media::Status onPacket(const MediaTsPacketView& packet) override
    {
        views.push_back(MediaTsPacketEvidenceView{
            packet.byteOffset, packet.pid, packet.payloadUnitStart,
            packet.continuityCounter, packet.discontinuity, packet.pcr27Mhz});
        return ::media::Status::success();
    }

    ::media::Status onContinuityEvent(const MediaTsContinuityEvent& event) override
    {
        events.push_back(event);
        return ::media::Status::success();
    }

    std::vector<MediaTsPacketEvidenceView> views;
    std::vector<MediaTsContinuityEvent> events;
};

void testPsiPacketsUsePlannedPidsAndExactWireShape(TestContext& ctx)
{
    auto packetizer = MediaTsTransportPacketizer::create(packetizerPlan()).value();
    auto tables = MediaTsPsiSerializer::serialize(packetizerPlan()).value();
    auto patCursor = packetizer.beginPat(tables.pat());
    EXPECT_TRUE(ctx, patCursor);
    if (!patCursor) return;
    auto packets = drain(ctx, patCursor.value(), 7);
    EXPECT_EQ(ctx, packets.size(), std::size_t{1});
    const auto& packet = packets.front();
    EXPECT_EQ(ctx, packet.size(), std::size_t{188});
    EXPECT_EQ(ctx, packet[0], std::uint8_t{0x47});
    EXPECT_EQ(ctx, pid(packet), std::uint16_t{0});
    EXPECT_EQ(ctx, packet[1] & 0x80, std::uint8_t{0});
    EXPECT_EQ(ctx, packet[1] & 0x40, std::uint8_t{0x40});
    EXPECT_EQ(ctx, packet[3] & 0xC0, std::uint8_t{0});
    EXPECT_EQ(ctx, (packet[3] >> 4) & 3, std::uint8_t{3});
    EXPECT_EQ(ctx, continuity(packet), std::uint8_t{3});
    EXPECT_EQ(ctx, payload(packet).front(), std::uint8_t{0});
    EXPECT_TRUE(ctx, std::equal(tables.pat().bytes().begin(), tables.pat().bytes().end(),
                               payload(packet).begin() + 1));

    auto pmtCursor = packetizer.beginPmt(tables.pmt());
    EXPECT_TRUE(ctx, pmtCursor);
    if (!pmtCursor) return;
    auto pmtPackets = drain(ctx, pmtCursor.value(), 7);
    EXPECT_EQ(ctx, pid(pmtPackets.front()), std::uint16_t{0x0100});
    EXPECT_EQ(ctx, continuity(pmtPackets.front()), std::uint8_t{5});
    EXPECT_EQ(ctx, payload(pmtPackets.front()).front(), std::uint8_t{0});
}

void testPcrOnlyUsesPlannedPidExactBitsAndDoesNotAdvanceContinuity(TestContext& ctx)
{
    auto packetizer = MediaTsTransportPacketizer::create(packetizerPlan()).value();
    constexpr std::uint64_t modulus = (std::uint64_t{1} << 33) * 300;
    const MediaTsPcrClock pcr{1, MediaRunningTime::fromNanoseconds(0), 0,
                              modulus - 1};
    auto pcrCursor = packetizer.beginPcrOnly(pcr);
    EXPECT_TRUE(ctx, pcrCursor);
    if (!pcrCursor) return;
    auto pcrPackets = drain(ctx, pcrCursor.value(), 1);
    const auto& packet = pcrPackets.front();
    EXPECT_EQ(ctx, pid(packet), std::uint16_t{0x0101});
    EXPECT_EQ(ctx, (packet[3] >> 4) & 3, std::uint8_t{2});
    EXPECT_EQ(ctx, continuity(packet), std::uint8_t{6});
    EXPECT_EQ(ctx, packet[4], std::uint8_t{183});
    EXPECT_EQ(ctx, packet[5], std::uint8_t{0x10});
    EXPECT_EQ(ctx, packet[10] & 0x7E, std::uint8_t{0x7E});
    EXPECT_TRUE(ctx, std::all_of(packet.begin() + 12, packet.end(),
                                [](std::uint8_t value) { return value == 0xFF; }));

    std::vector<std::uint8_t> video(8, 0x55);
    auto pes = packetizer.beginPes(
        MediaScheduledStream::Video, videoHeader(video.size()), video, false);
    EXPECT_TRUE(ctx, pes);
    if (!pes) return;
    auto pesPackets = drain(ctx, pes.value(), 7);
    EXPECT_EQ(ctx, continuity(pesPackets.front()), std::uint8_t{7});

    RecordingSink sink;
    auto parser = MediaTsPacketParser::create(188, sink, nullptr).value();
    std::array<std::uint8_t, 188 * 3> bytes{};
    std::copy(pcrPackets.front().begin(), pcrPackets.front().end(), bytes.begin());
    std::copy(pesPackets.front().begin(), pesPackets.front().end(), bytes.begin() + 188);
    std::copy(pesPackets.front().begin(), pesPackets.front().end(), bytes.begin() + 376);
    bytes[376 + 3] = static_cast<std::uint8_t>((bytes[376 + 3] & 0xF0) | 8);
    EXPECT_TRUE(ctx, parser->push(bytes));
    EXPECT_TRUE(ctx, sink.events.empty());
    EXPECT_EQ(ctx, sink.views.front().pcr27Mhz.value(), modulus - 1);
}

void testPesPacketizationHasExactPayloadAndContinuity(TestContext& ctx)
{
    auto packetizer = MediaTsTransportPacketizer::create(packetizerPlan()).value();
    std::vector<std::uint8_t> source(500);
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<std::uint8_t>(i);
    const auto header = videoHeader(source.size());
    auto cursor = packetizer.beginPes(MediaScheduledStream::Video, header, source, true);
    EXPECT_TRUE(ctx, cursor);
    if (!cursor) return;
    EXPECT_FALSE(ctx, cursor.value().prepare(0));
    EXPECT_FALSE(ctx, cursor.value().prepare(8));
    auto first = cursor.value().prepare(1);
    EXPECT_TRUE(ctx, first);
    if (!first) return;
    EXPECT_FALSE(ctx, cursor.value().prepare(1));
    auto tables = MediaTsPsiSerializer::serialize(packetizerPlan()).value();
    EXPECT_FALSE(ctx, packetizer.beginPat(tables.pat()));
    const auto* storageStart = first.value().packets().data();
    auto token = first.value().takeCommitToken();
    EXPECT_FALSE(ctx, cursor.value().commit(first.value().takeCommitToken()));
    EXPECT_TRUE(ctx, cursor.value().commit(std::move(token)));
    std::vector<std::array<std::uint8_t, 188>> packets(
        first.value().packets().begin(), first.value().packets().end());
    auto second = cursor.value().prepare(2);
    EXPECT_TRUE(ctx, second);
    if (!second) return;
    EXPECT_EQ(ctx, second.value().packets().data(), storageStart + 1);
    packets.insert(packets.end(), second.value().packets().begin(),
                   second.value().packets().end());
    EXPECT_TRUE(ctx, cursor.value().commit(second.value().takeCommitToken()));
    EXPECT_TRUE(ctx, packets.size() >= 3);

    std::vector<std::uint8_t> reconstructed;
    for (std::size_t i = 0; i < packets.size(); ++i) {
        const auto& packet = packets[i];
        EXPECT_EQ(ctx, pid(packet), std::uint16_t{0x0101});
        EXPECT_EQ(ctx, continuity(packet), static_cast<std::uint8_t>((7 + i) & 0x0F));
        EXPECT_EQ(ctx, packet[1] & 0x40, static_cast<std::uint8_t>(i == 0 ? 0x40 : 0));
        auto packetPayload = payload(packet);
        reconstructed.insert(reconstructed.end(), packetPayload.begin(), packetPayload.end());
    }
    std::vector<std::uint8_t> expected(header.bytes().begin(), header.bytes().end());
    expected.insert(expected.end(), source.begin(), source.end());
    EXPECT_EQ(ctx, reconstructed.size(), expected.size());
    EXPECT_TRUE(ctx, std::equal(expected.begin(), expected.end(), reconstructed.begin()));
    EXPECT_EQ(ctx, packets.front()[5] & 0x40, std::uint8_t{0x40});
    EXPECT_EQ(ctx, (packets[1][3] >> 4) & 3, std::uint8_t{1});
    EXPECT_EQ(ctx, (packets.back()[3] >> 4) & 3, std::uint8_t{3});
    EXPECT_EQ(ctx, packets.back()[5], std::uint8_t{0});
}

void testStreamHeaderValidationAndForeignTokenRejection(TestContext& ctx)
{
    auto firstPacketizer = MediaTsTransportPacketizer::create(packetizerPlan()).value();
    auto secondPacketizer = MediaTsTransportPacketizer::create(packetizerPlan()).value();
    const std::array<std::uint8_t, 4> bytes{1, 2, 3, 4};
    EXPECT_FALSE(ctx, firstPacketizer.beginPes(
        MediaScheduledStream::Video, audioHeader(bytes.size()), bytes, false));
    EXPECT_FALSE(ctx, firstPacketizer.beginPes(
        MediaScheduledStream::Audio, videoHeader(bytes.size()), bytes, false));
    EXPECT_FALSE(ctx, firstPacketizer.beginPes(
        static_cast<MediaScheduledStream>(99), videoHeader(bytes.size()), bytes, false));
    EXPECT_FALSE(ctx, firstPacketizer.beginPes(
        MediaScheduledStream::Audio, audioHeader(bytes.size()), bytes, true));
    EXPECT_FALSE(ctx, firstPacketizer.beginPcrOnly(MediaTsPcrClock{
        1, MediaRunningTime::fromNanoseconds(0), 0,
        (std::uint64_t{1} << 33) * 300}));

    auto first = firstPacketizer.beginPes(
        MediaScheduledStream::Audio, audioHeader(bytes.size()), bytes, false).value();
    auto second = secondPacketizer.beginPes(
        MediaScheduledStream::Audio, audioHeader(bytes.size()), bytes, false).value();
    auto own = first.prepare(1).value();
    auto foreign = second.prepare(1).value();
    EXPECT_FALSE(ctx, first.commit(foreign.takeCommitToken()));
    EXPECT_TRUE(ctx, first.commit(own.takeCommitToken()));
}

void testPesHeaderBindsExactPayloadLengthWithoutTakingLease(TestContext& ctx)
{
    auto packetizer = MediaTsTransportPacketizer::create(packetizerPlan()).value();
    const std::array<std::uint8_t, 3> shorter{1, 2, 3};
    const std::array<std::uint8_t, 4> exact{1, 2, 3, 4};
    const std::array<std::uint8_t, 5> larger{1, 2, 3, 4, 5};

    const auto video = videoHeader(exact.size());
    EXPECT_FALSE(ctx, packetizer.beginPes(
        MediaScheduledStream::Video, video, shorter, false));
    EXPECT_FALSE(ctx, packetizer.beginPes(
        MediaScheduledStream::Video, video, larger, false));
    auto validVideo = packetizer.beginPes(
        MediaScheduledStream::Video, video, exact, false);
    EXPECT_TRUE(ctx, validVideo);
    if (!validVideo) return;
    drain(ctx, validVideo.value(), 7);

    const auto audio = audioHeader(exact.size());
    EXPECT_FALSE(ctx, packetizer.beginPes(
        MediaScheduledStream::Audio, audio, shorter, false));
    EXPECT_FALSE(ctx, packetizer.beginPes(
        MediaScheduledStream::Audio, audio, larger, false));
    auto validAudio = packetizer.beginPes(
        MediaScheduledStream::Audio, audio, exact, false);
    EXPECT_TRUE(ctx, validAudio);
    if (!validAudio) return;
    drain(ctx, validAudio.value(), 7);

    const auto emptyHeader = audioHeader(0);
    auto empty = packetizer.beginPes(
        MediaScheduledStream::Audio, emptyHeader, {}, false);
    EXPECT_TRUE(ctx, empty);
    if (empty) drain(ctx, empty.value(), 7);
}

void testPsiSectionsBindFullPlanIdentityWithoutTakingLease(TestContext& ctx)
{
    const auto plan = packetizerPlan();
    auto packetizer = MediaTsTransportPacketizer::create(plan).value();
    auto ownTables = MediaTsPsiSerializer::serialize(plan).value();

    auto foreignPatParameters = packetizerParameters();
    foreignPatParameters.transportStreamId = 2;
    auto foreignPatPlan = MediaTsMuxPlan::create(foreignPatParameters).value();
    auto foreignPatTables = MediaTsPsiSerializer::serialize(foreignPatPlan).value();
    EXPECT_FALSE(ctx, packetizer.beginPat(foreignPatTables.pat()));
    auto ownPat = packetizer.beginPat(ownTables.pat());
    EXPECT_TRUE(ctx, ownPat);
    if (!ownPat) return;
    drain(ctx, ownPat.value(), 7);

    auto foreignPmtParameters = packetizerParameters();
    foreignPmtParameters.videoPid = 0x0103;
    foreignPmtParameters.pcrPid = 0x0103;
    auto foreignPmtPlan = MediaTsMuxPlan::create(foreignPmtParameters).value();
    auto foreignPmtTables = MediaTsPsiSerializer::serialize(foreignPmtPlan).value();
    EXPECT_FALSE(ctx, packetizer.beginPmt(foreignPmtTables.pmt()));
    auto ownPmt = packetizer.beginPmt(ownTables.pmt());
    EXPECT_TRUE(ctx, ownPmt);
    if (ownPmt) drain(ctx, ownPmt.value(), 7);
}

void appendPackets(std::vector<std::uint8_t>& bytes,
                   const std::vector<std::array<std::uint8_t, 188>>& packets)
{
    for (const auto& packet : packets) {
        bytes.insert(bytes.end(), packet.begin(), packet.end());
    }
}

void testContinuityWrapPcrInterleaveAndPidIndependence(TestContext& ctx)
{
    auto plan = packetizerPlan(MediaTsContinuitySeeds{3, 5, 15, 9});
    auto tables = MediaTsPsiSerializer::serialize(plan).value();
    auto packetizer = MediaTsTransportPacketizer::create(plan).value();
    std::vector<std::uint8_t> bytes;

    auto emitPat = [&] {
        auto cursor = packetizer.beginPat(tables.pat()).value();
        appendPackets(bytes, drain(ctx, cursor, 7));
    };
    auto emitPmt = [&] {
        auto cursor = packetizer.beginPmt(tables.pmt()).value();
        appendPackets(bytes, drain(ctx, cursor, 7));
    };
    emitPat();
    emitPat();
    emitPmt();
    emitPmt();

    std::vector<std::uint8_t> largeVideo(300, 0x44);
    auto video = packetizer.beginPes(
        MediaScheduledStream::Video, videoHeader(largeVideo.size()), largeVideo,
        false).value();
    auto videoPackets = drain(ctx, video, 7);
    EXPECT_EQ(ctx, continuity(videoPackets[0]), std::uint8_t{15});
    EXPECT_EQ(ctx, continuity(videoPackets[1]), std::uint8_t{0});
    appendPackets(bytes, videoPackets);

    auto pcr = packetizer.beginPcrOnly(MediaTsPcrClock{
        1, MediaRunningTime::fromNanoseconds(0), 0, 27'000'001}).value();
    auto pcrPackets = drain(ctx, pcr, 1);
    EXPECT_EQ(ctx, continuity(pcrPackets.front()), std::uint8_t{0});
    appendPackets(bytes, pcrPackets);

    const std::array<std::uint8_t, 1> singleVideo{0x45};
    auto nextVideo = packetizer.beginPes(
        MediaScheduledStream::Video, videoHeader(singleVideo.size()), singleVideo,
        false).value();
    auto nextVideoPackets = drain(ctx, nextVideo, 1);
    EXPECT_EQ(ctx, continuity(nextVideoPackets.front()), std::uint8_t{1});
    appendPackets(bytes, nextVideoPackets);

    const std::array<std::uint8_t, 1> audioBytes{0x11};
    auto audio = packetizer.beginPes(
        MediaScheduledStream::Audio, audioHeader(audioBytes.size()), audioBytes,
        false).value();
    auto audioPackets = drain(ctx, audio, 1);
    EXPECT_EQ(ctx, pid(audioPackets.front()), std::uint16_t{0x0102});
    EXPECT_EQ(ctx, continuity(audioPackets.front()), std::uint8_t{9});
    appendPackets(bytes, audioPackets);

    emitPat(); // trailing sync lets the parser publish every preceding packet
    RecordingSink sink;
    auto parser = MediaTsPacketParser::create(188, sink, nullptr).value();
    EXPECT_TRUE(ctx, parser->push(bytes));
    EXPECT_TRUE(ctx, sink.events.empty());
    EXPECT_TRUE(ctx, std::any_of(
        sink.views.begin(), sink.views.end(), [](const auto& view) {
            return view.pid == 0x0101 && view.pcr27Mhz == 27'000'001;
        }));
}

void testAbandonMoveAndStaleTransactionsAreSafe(TestContext& ctx)
{
    auto plan = packetizerPlan(MediaTsContinuitySeeds{0, 0, 4, 0});
    auto packetizer = MediaTsTransportPacketizer::create(plan).value();
    const std::array<std::uint8_t, 1> bytes{0x55};

    std::optional<MediaTsPacketCursor> abandoned(
        packetizer.beginPes(MediaScheduledStream::Video, videoHeader(1), bytes,
                            false).value());
    auto abandonedBatch = abandoned->prepare(1).value();
    EXPECT_FALSE(ctx, abandoned->finished());
    const auto retainedPacket = abandonedBatch.packets().front();
    auto stale = abandonedBatch.takeCommitToken();
    abandoned.reset();
    EXPECT_TRUE(ctx, std::equal(
        retainedPacket.begin(), retainedPacket.end(),
        abandonedBatch.packets().front().begin()));

    auto replacement = packetizer.beginPes(
        MediaScheduledStream::Video, videoHeader(1), bytes, false).value();
    auto replacementBatch = replacement.prepare(1).value();
    EXPECT_EQ(ctx, continuity(replacementBatch.packets().front()), std::uint8_t{4});
    EXPECT_FALSE(ctx, replacement.commit(std::move(stale)));
    EXPECT_TRUE(ctx, replacement.commit(replacementBatch.takeCommitToken()));

    auto cursor = packetizer.beginPes(
        MediaScheduledStream::Video, videoHeader(1), bytes, false).value();
    MediaTsPacketCursor moved(std::move(cursor));
    EXPECT_FALSE(ctx, cursor.prepare(1));
    auto movedBatch = moved.prepare(1).value();
    EXPECT_EQ(ctx, continuity(movedBatch.packets().front()), std::uint8_t{5});
    EXPECT_TRUE(ctx, moved.commit(movedBatch.takeCommitToken()));

    auto tables = MediaTsPsiSerializer::serialize(plan).value();
    MediaTsTransportPacketizer movedPacketizer(std::move(packetizer));
    EXPECT_FALSE(ctx, packetizer.beginPat(tables.pat()));
    auto pat = movedPacketizer.beginPat(tables.pat());
    EXPECT_TRUE(ctx, pat);
    if (pat) EXPECT_EQ(ctx, drain(ctx, pat.value(), 1).size(), std::size_t{1});

    std::optional<MediaTsTransportPacketizer> owner(
        MediaTsTransportPacketizer::create(plan).value());
    auto survivingCursor = owner->beginPes(
        MediaScheduledStream::Audio, audioHeader(1), bytes, false).value();
    owner.reset();
    EXPECT_EQ(ctx, drain(ctx, survivingCursor, 1).size(), std::size_t{1});
}

void testPacketBuilderRejectsTruncationAndCountsExactly(TestContext& ctx)
{
    const std::array<std::uint8_t, 185> bytes{};
    const std::array<std::span<const std::uint8_t>, 1> segments{bytes};
    EXPECT_FALSE(ctx, MediaTsTransportPacketBuilder::payload(
        0x1FFF, 0, segments, false));
    EXPECT_FALSE(ctx, MediaTsTransportPacketBuilder::payload(
        0x0101, 16, segments, false));
    EXPECT_FALSE(ctx, MediaTsTransportPacketBuilder::pcrOnly(0x2000, 0, 0));
    EXPECT_FALSE(ctx, MediaTsTransportPacketBuilder::pcrOnly(0x0101, 16, 0));
    auto normal = MediaTsTransportPacketBuilder::payload(
        0x0101, 0, segments, false);
    EXPECT_TRUE(ctx, normal);
    if (normal) EXPECT_EQ(ctx, normal.value().size(), std::size_t{2});
    auto randomAccess = MediaTsTransportPacketBuilder::payload(
        0x0101, 0, segments, true);
    EXPECT_TRUE(ctx, randomAccess);
    if (randomAccess) EXPECT_EQ(ctx, randomAccess.value().size(), std::size_t{2});
}

template <std::size_t Size>
void expectSinglePacketBoundary(TestContext& ctx,
                                const std::array<std::uint8_t, Size>& bytes,
                                bool randomAccess,
                                std::uint8_t expectedControl,
                                std::uint8_t expectedAdaptationLength,
                                std::size_t expectedPayloadOffset)
{
    const std::array<std::span<const std::uint8_t>, 1> segments{bytes};
    auto built = MediaTsTransportPacketBuilder::payload(
        0x0101, 2, segments, randomAccess);
    EXPECT_TRUE(ctx, built);
    if (!built) return;
    EXPECT_EQ(ctx, built.value().size(), std::size_t{1});
    const auto& packet = built.value().front();
    EXPECT_EQ(ctx, packet[0], std::uint8_t{0x47});
    EXPECT_EQ(ctx, packet[1] & 0xC0, std::uint8_t{0x40});
    EXPECT_EQ(ctx, packet[3] & 0xC0, std::uint8_t{0});
    EXPECT_EQ(ctx, (packet[3] >> 4) & 3, expectedControl);
    EXPECT_EQ(ctx, packet[3] & 0x0F, std::uint8_t{2});
    if (expectedControl == 3) {
        EXPECT_EQ(ctx, packet[4], expectedAdaptationLength);
    }
    EXPECT_TRUE(ctx, std::equal(
        bytes.begin(), bytes.end(), packet.begin() + expectedPayloadOffset));
}

void testPacketBuilderExactPayloadBoundaries(TestContext& ctx)
{
    std::array<std::uint8_t, 184> normalFull{};
    for (std::size_t index = 0; index < normalFull.size(); ++index) {
        normalFull[index] = static_cast<std::uint8_t>(index);
    }
    expectSinglePacketBoundary(ctx, normalFull, false, 1, 0, 4);

    std::array<std::uint8_t, 183> normalAdapted{};
    for (std::size_t index = 0; index < normalAdapted.size(); ++index) {
        normalAdapted[index] = static_cast<std::uint8_t>(index + 1);
    }
    expectSinglePacketBoundary(ctx, normalAdapted, false, 3, 0, 5);

    std::array<std::uint8_t, 182> randomAccess{};
    randomAccess.fill(0x33);
    expectSinglePacketBoundary(ctx, randomAccess, true, 3, 1, 6);
    const std::array<std::span<const std::uint8_t>, 1> randomSegments{randomAccess};
    auto randomPacket = MediaTsTransportPacketBuilder::payload(
        0x0101, 2, randomSegments, true).value();
    EXPECT_EQ(ctx, randomPacket.front()[5], std::uint8_t{0x40});

    const std::array<std::uint8_t, 1> single{0xA5};
    expectSinglePacketBoundary(ctx, single, false, 3, 182, 187);
    const std::array<std::span<const std::uint8_t>, 1> singleSegment{single};
    auto singlePacket = MediaTsTransportPacketBuilder::payload(
        0x0101, 2, singleSegment, false).value();
    EXPECT_EQ(ctx, singlePacket.front()[5], std::uint8_t{0});
    EXPECT_TRUE(ctx, std::all_of(
        singlePacket.front().begin() + 6, singlePacket.front().begin() + 187,
        [](std::uint8_t value) { return value == 0xFF; }));
    EXPECT_EQ(ctx, singlePacket.front()[187], std::uint8_t{0xA5});

    const std::array<std::span<const std::uint8_t>, 0> noSegments{};
    EXPECT_FALSE(ctx, MediaTsTransportPacketBuilder::payload(
        0x0101, 0, noSegments, false));
    const std::array<std::span<const std::uint8_t>, 2> emptySegments{
        std::span<const std::uint8_t>{}, std::span<const std::uint8_t>{}};
    EXPECT_FALSE(ctx, MediaTsTransportPacketBuilder::payload(
        0x0101, 0, emptySegments, false));
}

} // namespace

void runMpegTsOutputPacketizerTests(TestContext& ctx)
{
    static_assert(!std::is_copy_constructible_v<MediaTsPacketCommitToken>);
    static_assert(!std::is_copy_assignable_v<MediaTsPacketCommitToken>);
    static_assert(std::is_nothrow_move_constructible_v<MediaTsPacketCommitToken>);
    static_assert(!std::is_constructible_v<MediaTsPatSection,
                                            std::vector<std::uint8_t>>);
    static_assert(!std::is_constructible_v<MediaTsPmtSection,
                                            std::vector<std::uint8_t>>);
    testPsiPacketsUsePlannedPidsAndExactWireShape(ctx);
    testPcrOnlyUsesPlannedPidExactBitsAndDoesNotAdvanceContinuity(ctx);
    testPesPacketizationHasExactPayloadAndContinuity(ctx);
    testStreamHeaderValidationAndForeignTokenRejection(ctx);
    testPesHeaderBindsExactPayloadLengthWithoutTakingLease(ctx);
    testPsiSectionsBindFullPlanIdentityWithoutTakingLease(ctx);
    testContinuityWrapPcrInterleaveAndPidIndependence(ctx);
    testAbandonMoveAndStaleTransactionsAreSafe(ctx);
    testPacketBuilderRejectsTruncationAndCountsExactly(ctx);
    testPacketBuilderExactPayloadBoundaries(ctx);
}
