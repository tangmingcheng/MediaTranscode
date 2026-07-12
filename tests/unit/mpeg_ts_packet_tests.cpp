#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

namespace {

uint32_t crc32Mpeg(std::span<const uint8_t> bytes)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (const uint8_t byte : bytes) {
        crc ^= static_cast<uint32_t>(byte) << 24;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80000000U) != 0 ? (crc << 1) ^ 0x04C11DB7U : crc << 1;
        }
    }
    return crc;
}

void appendCrc(std::vector<uint8_t>& section)
{
    const uint32_t crc = crc32Mpeg(section);
    section.push_back(static_cast<uint8_t>(crc >> 24));
    section.push_back(static_cast<uint8_t>(crc >> 16));
    section.push_back(static_cast<uint8_t>(crc >> 8));
    section.push_back(static_cast<uint8_t>(crc));
}

std::vector<uint8_t> patSection(uint8_t version,
                                std::span<const std::pair<uint16_t, uint16_t>> programs,
                                uint8_t sectionNumber = 0,
                                uint8_t lastSectionNumber = 0,
                                uint16_t transportStreamId = 1)
{
    const uint16_t sectionLength = static_cast<uint16_t>(5 + programs.size() * 4 + 4);
    std::vector<uint8_t> section{
        0x00,
        static_cast<uint8_t>(0xB0 | (sectionLength >> 8)),
        static_cast<uint8_t>(sectionLength),
        static_cast<uint8_t>(transportStreamId >> 8), static_cast<uint8_t>(transportStreamId),
        static_cast<uint8_t>(0xC1 | ((version & 0x1F) << 1)),
        sectionNumber, lastSectionNumber};
    for (const auto [program, pmtPid] : programs) {
        section.push_back(static_cast<uint8_t>(program >> 8));
        section.push_back(static_cast<uint8_t>(program));
        section.push_back(static_cast<uint8_t>(0xE0 | (pmtPid >> 8)));
        section.push_back(static_cast<uint8_t>(pmtPid));
    }
    appendCrc(section);
    return section;
}

std::vector<uint8_t> pmtSection(uint16_t program,
                                uint8_t version,
                                uint16_t pcrPid,
                                std::span<const MediaTsElementaryStreamInfo> streams,
                                uint8_t sectionNumber = 0,
                                uint8_t lastSectionNumber = 0)
{
    const uint16_t sectionLength = static_cast<uint16_t>(9 + streams.size() * 5 + 4);
    std::vector<uint8_t> section{
        0x02,
        static_cast<uint8_t>(0xB0 | (sectionLength >> 8)),
        static_cast<uint8_t>(sectionLength),
        static_cast<uint8_t>(program >> 8), static_cast<uint8_t>(program),
        static_cast<uint8_t>(0xC1 | ((version & 0x1F) << 1)),
        sectionNumber, lastSectionNumber,
        static_cast<uint8_t>(0xE0 | (pcrPid >> 8)), static_cast<uint8_t>(pcrPid),
        0xF0, 0x00};
    for (const auto& stream : streams) {
        section.push_back(stream.streamType);
        section.push_back(static_cast<uint8_t>(0xE0 | (stream.pid >> 8)));
        section.push_back(static_cast<uint8_t>(stream.pid));
        section.push_back(0xF0);
        section.push_back(0x00);
    }
    appendCrc(section);
    return section;
}

std::array<uint8_t, 188> payloadPacket(uint16_t pid,
                                       uint8_t continuity,
                                       bool payloadStart,
                                       std::span<const uint8_t> payload)
{
    std::array<uint8_t, 188> packet{};
    packet.fill(0xFF);
    packet[0] = 0x47;
    packet[1] = static_cast<uint8_t>((payloadStart ? 0x40 : 0x00) | (pid >> 8));
    packet[2] = static_cast<uint8_t>(pid);
    packet[3] = static_cast<uint8_t>(0x10 | continuity);
    std::copy(payload.begin(), payload.end(), packet.begin() + 4);
    return packet;
}

std::array<uint8_t, 188> sectionPacket(uint16_t pid,
                                       uint8_t continuity,
                                       std::span<const uint8_t> section)
{
    std::vector<uint8_t> payload{0x00};
    payload.insert(payload.end(), section.begin(), section.end());
    return payloadPacket(pid, continuity, true, payload);
}

std::array<uint8_t, 188> exactPayloadPacket(uint16_t pid,
                                            uint8_t continuity,
                                            bool payloadStart,
                                            std::span<const uint8_t> payload,
                                            bool discontinuity = false)
{
    std::array<uint8_t, 188> packet{};
    packet.fill(0xFF);
    packet[0] = 0x47;
    packet[1] = static_cast<uint8_t>((payloadStart ? 0x40 : 0x00) | (pid >> 8));
    packet[2] = static_cast<uint8_t>(pid);
    packet[3] = static_cast<uint8_t>(0x30 | continuity);
    packet[4] = static_cast<uint8_t>(183 - payload.size());
    if (packet[4] != 0) packet[5] = discontinuity ? 0x80 : 0;
    std::copy(payload.begin(), payload.end(), packet.end() - payload.size());
    return packet;
}

std::array<uint8_t, 188> adaptationPacket(uint16_t pid,
                                          uint8_t continuity,
                                          std::span<const uint8_t> adaptation)
{
    std::array<uint8_t, 188> packet{};
    packet.fill(0xFF);
    packet[0] = 0x47;
    packet[1] = static_cast<uint8_t>(pid >> 8);
    packet[2] = static_cast<uint8_t>(pid);
    packet[3] = static_cast<uint8_t>(0x30 | continuity);
    packet[4] = static_cast<uint8_t>(adaptation.size());
    std::copy(adaptation.begin(), adaptation.end(), packet.begin() + 5);
    return packet;
}

std::vector<uint8_t> packets(std::span<const std::array<uint8_t, 188>> input)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(input.size() * 188);
    for (const auto& packet : input) bytes.insert(bytes.end(), packet.begin(), packet.end());
    return bytes;
}

std::vector<uint8_t> confirmedPacket(const std::array<uint8_t, 188>& packet)
{
    std::vector<uint8_t> bytes(packet.begin(), packet.end());
    bytes.push_back(0x47);
    return bytes;
}

std::array<uint8_t, 188> pcrPacket(uint16_t pid,
                                   uint8_t continuity,
                                   uint64_t base,
                                   uint16_t extension,
                                   bool discontinuity)
{
    std::array<uint8_t, 188> packet{};
    packet.fill(0xFF);
    packet[0] = 0x47;
    packet[1] = static_cast<uint8_t>(pid >> 8);
    packet[2] = static_cast<uint8_t>(pid);
    packet[3] = static_cast<uint8_t>(0x20 | continuity);
    packet[4] = 183;
    packet[5] = static_cast<uint8_t>(0x10 | (discontinuity ? 0x80 : 0));
    packet[6] = static_cast<uint8_t>(base >> 25);
    packet[7] = static_cast<uint8_t>(base >> 17);
    packet[8] = static_cast<uint8_t>(base >> 9);
    packet[9] = static_cast<uint8_t>(base >> 1);
    packet[10] = static_cast<uint8_t>((base << 7) | 0x7E | (extension >> 8));
    packet[11] = static_cast<uint8_t>(extension);
    return packet;
}

struct RecordedPacket final {
    uint64_t byteOffset = 0;
    uint16_t pid = 0;
    bool payloadUnitStart = false;
    uint8_t continuityCounter = 0;
    bool discontinuity = false;
    std::optional<uint64_t> pcr27Mhz;
    std::vector<uint8_t> payload;
};

class RecordingPacketSink final : public MediaTsPacketSink {
public:
    ::media::Status onPacket(const MediaTsPacketView& packet) override
    {
        packets.push_back({packet.byteOffset, packet.pid, packet.payloadUnitStart,
                           packet.continuityCounter, packet.discontinuity,
                           packet.pcr27Mhz,
                           std::vector<uint8_t>(packet.payloadSpan.begin(), packet.payloadSpan.end())});
        return ::media::Status::success();
    }

    std::vector<RecordedPacket> packets;
};

class CountingPacketSink final : public MediaTsPacketSink {
public:
    ::media::Status onPacket(const MediaTsPacketView&) override
    {
        ++packetCount;
        return ::media::Status::success();
    }

    std::size_t packetCount = 0;
};

class RecordingInventorySink final : public MediaTsProgramInventorySink {
public:
    ::media::Status onProgramInventory(MediaTsProgramInventorySnapshot snapshot) override
    {
        snapshots.push_back(std::move(snapshot));
        return ::media::Status::success();
    }

    std::vector<MediaTsProgramInventorySnapshot> snapshots;
};

void testPacketFramingAndPcr(TestContext& ctx)
{
    RecordingPacketSink sink;
    auto parser = MediaTsPacketParser::create(188, sink);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;

    const auto pat = sectionPacket(0, 0, patSection(1, std::array{std::pair<uint16_t, uint16_t>{1, 0x100}}));
    const auto pcr = pcrPacket(0x101, 7, 0x1ABCDEFFFULL, 299, true);
    std::vector<uint8_t> input{0x12, 0x34, 0x56};
    input.insert(input.end(), pat.begin(), pat.end());
    input.insert(input.end(), pcr.begin(), pcr.end());
    input.push_back(0x47);
    for (const uint8_t byte : input) EXPECT_TRUE(ctx, parser.value()->push(std::span(&byte, 1)));

    EXPECT_EQ(ctx, sink.packets.size(), static_cast<std::size_t>(2));
    if (sink.packets.size() != 2) return;
    EXPECT_EQ(ctx, sink.packets[0].byteOffset, static_cast<uint64_t>(3));
    EXPECT_EQ(ctx, sink.packets[0].pid, static_cast<uint16_t>(0));
    EXPECT_TRUE(ctx, sink.packets[0].payloadUnitStart);
    EXPECT_EQ(ctx, sink.packets[0].continuityCounter, static_cast<uint8_t>(0));
    EXPECT_EQ(ctx, sink.packets[1].byteOffset, static_cast<uint64_t>(191));
    EXPECT_TRUE(ctx, sink.packets[1].discontinuity);
    EXPECT_TRUE(ctx, sink.packets[1].pcr27Mhz.has_value());
    if (sink.packets[1].pcr27Mhz) {
        EXPECT_EQ(ctx, *sink.packets[1].pcr27Mhz, 0x1ABCDEFFFULL * 300 + 299);
    }
    EXPECT_TRUE(ctx, sink.packets[1].payload.empty());
}

void testMultiplePacketsAndMalformedPackets(TestContext& ctx)
{
    RecordingPacketSink sink;
    auto parser = MediaTsPacketParser::create(188, sink);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;
    auto first = payloadPacket(0x120, 0, false, std::array<uint8_t, 2>{1, 2});
    auto second = payloadPacket(0x120, 1, false, std::array<uint8_t, 2>{3, 4});
    std::vector<uint8_t> both(first.begin(), first.end());
    both.insert(both.end(), second.begin(), second.end());
    const auto thirdLock = payloadPacket(0x123, 0, false, std::array<uint8_t, 1>{5});
    both.insert(both.end(), thirdLock.begin(), thirdLock.end());
    EXPECT_TRUE(ctx, parser.value()->push(both));
    EXPECT_EQ(ctx, sink.packets.size(), static_cast<std::size_t>(2));

    auto invalidLength = pcrPacket(0x121, 0, 1, 2, false);
    invalidLength[4] = 184;
    EXPECT_FALSE(ctx, parser.value()->push(confirmedPacket(invalidLength)));

    RecordingPacketSink extensionSink;
    auto extensionParser = MediaTsPacketParser::create(188, extensionSink);
    EXPECT_TRUE(ctx, extensionParser);
    const std::array extensionLock{first, second};
    EXPECT_TRUE(ctx, extensionParser.value()->push(packets(extensionLock)));
    auto invalidExtension = pcrPacket(0x122, 0, 1, 300, false);
    invalidExtension[10] = static_cast<uint8_t>((invalidExtension[10] & 0xFE) | 1);
    invalidExtension[11] = 44;
    EXPECT_FALSE(ctx, extensionParser.value()->push(confirmedPacket(invalidExtension)));

    RecordingPacketSink continuitySink;
    auto continuityParser = MediaTsPacketParser::create(188, continuitySink);
    EXPECT_TRUE(ctx, continuityParser);
    EXPECT_TRUE(ctx, continuityParser.value()->push(first));
    second[3] = 0x12;
    EXPECT_FALSE(ctx, continuityParser.value()->push(confirmedPacket(second)));

    EXPECT_FALSE(ctx, MediaTsPacketParser::create(192, sink));
    EXPECT_FALSE(ctx, MediaTsPacketParser::create(204, sink));
}

void testStrideAcquisitionAndReacquisition(TestContext& ctx)
{
    const auto first = payloadPacket(0x120, 0, false, std::array<uint8_t, 1>{1});
    const auto second = payloadPacket(0x121, 0, false, std::array<uint8_t, 1>{2});

    RecordingPacketSink falseSyncSink;
    auto falseSyncParser = MediaTsPacketParser::create(188, falseSyncSink);
    EXPECT_TRUE(ctx, falseSyncParser);
    std::vector<uint8_t> falseSync{0x47, 0x00, 0x11, 0x22};
    const std::array falseSyncPackets{first, second};
    const auto realBytes = packets(falseSyncPackets);
    falseSync.insert(falseSync.end(), realBytes.begin(), realBytes.end());
    falseSync.push_back(0x47);
    EXPECT_TRUE(ctx, falseSyncParser.value()->push(falseSync));
    EXPECT_EQ(ctx, falseSyncSink.packets.size(), static_cast<std::size_t>(2));
    if (falseSyncSink.packets.size() == 2) {
        EXPECT_EQ(ctx, falseSyncSink.packets[0].byteOffset, static_cast<uint64_t>(4));
        EXPECT_EQ(ctx, falseSyncSink.packets[1].byteOffset, static_cast<uint64_t>(192));
    }

    RecordingPacketSink fragmentedSink;
    auto fragmentedParser = MediaTsPacketParser::create(188, fragmentedSink);
    EXPECT_TRUE(ctx, fragmentedParser);
    EXPECT_TRUE(ctx, fragmentedParser.value()->push(std::span(realBytes).first(188)));
    EXPECT_TRUE(ctx, fragmentedSink.packets.empty());
    std::vector<uint8_t> fragmentedTail(std::span(realBytes).subspan(188).begin(),
                                        std::span(realBytes).subspan(188).end());
    fragmentedTail.push_back(0x47);
    EXPECT_TRUE(ctx, fragmentedParser.value()->push(fragmentedTail));
    EXPECT_EQ(ctx, fragmentedSink.packets.size(), static_cast<std::size_t>(2));

    RecordingPacketSink insertionSink;
    auto insertionParser = MediaTsPacketParser::create(188, insertionSink);
    EXPECT_TRUE(ctx, insertionParser);
    const auto third = payloadPacket(0x122, 0, false, std::array<uint8_t, 1>{3});
    const auto fourth = payloadPacket(0x123, 0, false, std::array<uint8_t, 1>{4});
    std::vector<uint8_t> inserted = realBytes;
    inserted.push_back(0x55);
    inserted.insert(inserted.end(), third.begin(), third.end());
    inserted.insert(inserted.end(), fourth.begin(), fourth.end());
    inserted.push_back(0x47);
    EXPECT_TRUE(ctx, insertionParser.value()->push(inserted));
    EXPECT_EQ(ctx, insertionSink.packets.size(), static_cast<std::size_t>(3));
    if (insertionSink.packets.size() == 3) {
        EXPECT_EQ(ctx, insertionSink.packets[1].byteOffset, static_cast<uint64_t>(377));
    }

    RecordingPacketSink deletionSink;
    auto deletionParser = MediaTsPacketParser::create(188, deletionSink);
    EXPECT_TRUE(ctx, deletionParser);
    const auto fifth = payloadPacket(0x124, 0, false, std::array<uint8_t, 1>{5});
    std::vector<uint8_t> deleted = realBytes;
    deleted.insert(deleted.end(), third.begin() + 1, third.end());
    deleted.insert(deleted.end(), fourth.begin(), fourth.end());
    deleted.insert(deleted.end(), fifth.begin(), fifth.end());
    deleted.push_back(0x47);
    EXPECT_TRUE(ctx, deletionParser.value()->push(deleted));
    EXPECT_EQ(ctx, deletionSink.packets.size(), static_cast<std::size_t>(3));
    if (deletionSink.packets.size() == 3) {
        EXPECT_EQ(ctx, deletionSink.packets[1].pid, static_cast<uint16_t>(0x123));
        EXPECT_EQ(ctx, deletionSink.packets[1].byteOffset, static_cast<uint64_t>(563));
    }

    RecordingPacketSink malformedSink;
    auto malformedParser = MediaTsPacketParser::create(188, malformedSink);
    EXPECT_TRUE(ctx, malformedParser);
    EXPECT_TRUE(ctx, malformedParser.value()->push(realBytes));
    auto malformed = adaptationPacket(0x130, 0, std::array<uint8_t, 1>{0x10});
    EXPECT_FALSE(ctx, malformedParser.value()->push(confirmedPacket(malformed)));
    EXPECT_EQ(ctx, malformedSink.packets.size(), static_cast<std::size_t>(2));
}

void testEveryPacketRequiresNextStrideConfirmation(TestContext& ctx)
{
    auto first = payloadPacket(0x150, 0, false, std::array<uint8_t, 1>{1});
    auto shifted = payloadPacket(0x130, 0, false, std::array<uint8_t, 8>{
        0x10, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0xAA});
    auto third = payloadPacket(0x151, 0, false, std::array<uint8_t, 1>{3});
    auto fourth = payloadPacket(0x152, 0, false, std::array<uint8_t, 1>{4});

    RecordingPacketSink sink;
    auto parser = MediaTsPacketParser::create(188, sink);
    EXPECT_TRUE(ctx, parser);
    EXPECT_TRUE(ctx, parser.value()->push(first));
    EXPECT_TRUE(ctx, sink.packets.empty());

    std::vector<uint8_t> remainder;
    remainder.push_back(0x47);
    remainder.insert(remainder.end(), shifted.begin(), shifted.end());
    remainder.insert(remainder.end(), third.begin(), third.end());
    remainder.insert(remainder.end(), fourth.begin(), fourth.end());
    remainder.push_back(0x47);
    EXPECT_TRUE(ctx, parser.value()->push(remainder));
    EXPECT_EQ(ctx, sink.packets.size(), static_cast<std::size_t>(4));
    if (sink.packets.size() == 4) {
        EXPECT_EQ(ctx, sink.packets[0].pid, static_cast<uint16_t>(0x150));
        EXPECT_EQ(ctx, sink.packets[1].pid, static_cast<uint16_t>(0x130));
        EXPECT_EQ(ctx, sink.packets[1].byteOffset, static_cast<uint64_t>(189));
        EXPECT_FALSE(ctx, sink.packets[1].pcr27Mhz.has_value());
        EXPECT_EQ(ctx, sink.packets[3].pid, static_cast<uint16_t>(0x152));
    }

    CountingPacketSink largeSink;
    auto largeParser = MediaTsPacketParser::create(188, largeSink);
    EXPECT_TRUE(ctx, largeParser);
    std::vector<uint8_t> largeFragment;
    constexpr std::size_t PacketCount = 4096;
    largeFragment.reserve(PacketCount * 188);
    for (std::size_t index = 0; index < PacketCount; ++index) {
        const auto packet = payloadPacket(static_cast<uint16_t>(0x400 + index),
                                          0, false,
                                          std::array<uint8_t, 1>{0});
        largeFragment.insert(largeFragment.end(), packet.begin(), packet.end());
    }
    EXPECT_TRUE(ctx, largeParser.value()->push(largeFragment));
    EXPECT_EQ(ctx, largeSink.packetCount, PacketCount - 1);
    EXPECT_TRUE(ctx, largeParser.value()->retainedByteCount() <= 188);
    EXPECT_EQ(ctx, largeParser.value()->copiedPacketByteCount(), static_cast<uint64_t>(0));
}

void testCompleteAdaptationStructureValidation(TestContext& ctx)
{
    const auto lockOne = payloadPacket(0x140, 0, false, std::array<uint8_t, 1>{1});
    const auto lockTwo = payloadPacket(0x141, 0, false, std::array<uint8_t, 1>{2});
    const std::array lockPackets{lockOne, lockTwo};
    const auto lockBytes = packets(lockPackets);

    auto rejects = [&](std::span<const uint8_t> adaptation) {
        RecordingPacketSink sink;
        auto parser = MediaTsPacketParser::create(188, sink);
        if (!parser || !parser.value()->push(lockBytes)) return false;
        return !parser.value()->push(confirmedPacket(adaptationPacket(0x142, 0, adaptation)));
    };

    const std::array<uint8_t, 29> complete{
        0x1F,
        0x00, 0x00, 0x00, 0x00, 0x7E, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x7E, 0x00,
        0x00,
        0x02, 0xAA, 0xBB,
        0x0B, 0xE0, 0x00, 0x00, 0xC0, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00};
    RecordingPacketSink validSink;
    auto validParser = MediaTsPacketParser::create(188, validSink);
    EXPECT_TRUE(ctx, validParser);
    EXPECT_TRUE(ctx, validParser.value()->push(lockBytes));
    EXPECT_TRUE(ctx, validParser.value()->push(confirmedPacket(adaptationPacket(0x142, 0, complete))));

    EXPECT_TRUE(ctx, rejects(std::array<uint8_t, 1>{0x08}));
    EXPECT_TRUE(ctx, rejects(std::array<uint8_t, 2>{0x02, 0x02}));
    EXPECT_TRUE(ctx, rejects(std::array<uint8_t, 2>{0x01, 0x03}));
    EXPECT_TRUE(ctx, rejects(std::array<uint8_t, 2>{0x01, 0x01}));
    EXPECT_TRUE(ctx, rejects(std::array<uint8_t, 3>{0x01, 0x02, 0x80}));
    EXPECT_TRUE(ctx, rejects(std::array<uint8_t, 4>{0x01, 0x03, 0x40, 0x00}));
    EXPECT_TRUE(ctx, rejects(std::array<uint8_t, 6>{0x01, 0x05, 0x20, 0x00, 0x00, 0x00}));
}

void testPsiInventoryAndVersions(TestContext& ctx)
{
    RecordingInventorySink inventorySink;
    MediaTsPsiSectionAssembler assembler(inventorySink);
    auto parser = MediaTsPacketParser::create(188, assembler);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;

    const std::array programs{
        std::pair<uint16_t, uint16_t>{1, 0x100},
        std::pair<uint16_t, uint16_t>{2, 0x200}};
    const std::array firstStreams{
        MediaTsElementaryStreamInfo{0x101, 0x1B},
        MediaTsElementaryStreamInfo{0x102, 0x0F}};
    const std::array secondStreams{MediaTsElementaryStreamInfo{0x201, 0x24}};
    const auto pat = sectionPacket(0, 0, patSection(3, programs));
    const auto pmtOne = sectionPacket(0x100, 0, pmtSection(1, 4, 0x101, firstStreams));
    const auto pmtTwo = sectionPacket(0x200, 0, pmtSection(2, 5, 0x201, secondStreams));

    EXPECT_TRUE(ctx, parser.value()->push(pat));
    EXPECT_TRUE(ctx, parser.value()->push(pmtOne));
    EXPECT_TRUE(ctx, inventorySink.snapshots.empty());
    EXPECT_TRUE(ctx, parser.value()->push(pmtTwo));
    EXPECT_TRUE(ctx, parser.value()->push(payloadPacket(0x710, 0, false, std::array<uint8_t, 1>{0})));
    EXPECT_EQ(ctx, inventorySink.snapshots.size(), static_cast<std::size_t>(1));
    if (inventorySink.snapshots.empty()) return;
    const auto& snapshot = inventorySink.snapshots.back();
    EXPECT_EQ(ctx, snapshot.patVersion, static_cast<uint8_t>(3));
    EXPECT_EQ(ctx, snapshot.programs.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, snapshot.programs[0].programNumber, static_cast<uint16_t>(1));
    EXPECT_EQ(ctx, snapshot.programs[0].pmtPid, static_cast<uint16_t>(0x100));
    EXPECT_EQ(ctx, snapshot.programs[0].pmtVersion, static_cast<uint8_t>(4));
    EXPECT_EQ(ctx, snapshot.programs[0].pcrPid, static_cast<uint16_t>(0x101));
    EXPECT_EQ(ctx, snapshot.programs[0].elementaryStreams.size(), static_cast<std::size_t>(2));

    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x200, 1, pmtSection(2, 5, 0x201, secondStreams))));
    EXPECT_EQ(ctx, inventorySink.snapshots.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, parser.value()->push(confirmedPacket(
        sectionPacket(0x200, 2, pmtSection(2, 6, 0x201, secondStreams)))));
    EXPECT_EQ(ctx, inventorySink.snapshots.size(), static_cast<std::size_t>(2));
}

void testPsiRejectsInvalidEvidence(TestContext& ctx)
{
    RecordingInventorySink inventorySink;
    MediaTsPsiSectionAssembler assembler(inventorySink);
    auto parser = MediaTsPacketParser::create(188, assembler);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;

    auto invalidPat = patSection(1, std::array{std::pair<uint16_t, uint16_t>{1, 0x100}});
    invalidPat.back() ^= 1;
    const std::array invalidPatPackets{
        sectionPacket(0, 0, invalidPat),
        payloadPacket(0x300, 0, false, std::array<uint8_t, 1>{0})};
    EXPECT_FALSE(ctx, parser.value()->push(packets(invalidPatPackets)));

    RecordingInventorySink currentSink;
    MediaTsPsiSectionAssembler currentAssembler(currentSink);
    auto currentParser = MediaTsPacketParser::create(188, currentAssembler);
    EXPECT_TRUE(ctx, currentParser);
    auto nonCurrentPat = patSection(1, std::array{std::pair<uint16_t, uint16_t>{1, 0x100}});
    nonCurrentPat.resize(nonCurrentPat.size() - 4);
    nonCurrentPat[5] &= 0xFE;
    appendCrc(nonCurrentPat);
    const std::array nonCurrentPackets{
        sectionPacket(0, 0, nonCurrentPat),
        payloadPacket(0x301, 0, false, std::array<uint8_t, 1>{0})};
    EXPECT_FALSE(ctx, currentParser.value()->push(packets(nonCurrentPackets)));

    RecordingInventorySink truncatedSink;
    MediaTsPsiSectionAssembler truncatedAssembler(truncatedSink);
    auto truncatedParser = MediaTsPacketParser::create(188, truncatedAssembler);
    EXPECT_TRUE(ctx, truncatedParser);
    auto longPat = patSection(1, std::array{std::pair<uint16_t, uint16_t>{1, 0x100}});
    std::vector<uint8_t> firstPayload{0};
    firstPayload.insert(firstPayload.end(), longPat.begin(), longPat.begin() + 8);
    auto firstHalf = exactPayloadPacket(0, 0, true, firstPayload);
    EXPECT_TRUE(ctx, truncatedParser.value()->push(firstHalf));
    auto unrelated = payloadPacket(0x300, 0, false, std::array<uint8_t, 1>{0xAA});
    EXPECT_TRUE(ctx, truncatedParser.value()->push(unrelated));
    auto wrongContinuation = exactPayloadPacket(0, 2, false, std::span(longPat).subspan(8));
    EXPECT_FALSE(ctx, truncatedParser.value()->push(confirmedPacket(wrongContinuation)));
}

void testPsiCrossPacketAssemblyAndExplicitTruncation(TestContext& ctx)
{
    const auto pat = patSection(1, std::array{std::pair<uint16_t, uint16_t>{1, 0x100}});

    RecordingInventorySink inventorySink;
    MediaTsPsiSectionAssembler assembler(inventorySink);
    auto parser = MediaTsPacketParser::create(188, assembler);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;
    std::vector<uint8_t> firstPayload{0};
    firstPayload.insert(firstPayload.end(), pat.begin(), pat.begin() + 8);
    EXPECT_TRUE(ctx, parser.value()->push(exactPayloadPacket(0, 0, true, firstPayload)));
    EXPECT_TRUE(ctx, parser.value()->push(exactPayloadPacket(0, 1, false, std::span(pat).subspan(8))));
    const std::array streams{MediaTsElementaryStreamInfo{0x101, 0x1B}};
    EXPECT_TRUE(ctx, parser.value()->push(confirmedPacket(
        sectionPacket(0x100, 0, pmtSection(1, 2, 0x101, streams)))));
    EXPECT_EQ(ctx, inventorySink.snapshots.size(), static_cast<std::size_t>(1));

    RecordingInventorySink truncatedSink;
    MediaTsPsiSectionAssembler truncatedAssembler(truncatedSink);
    auto truncatedParser = MediaTsPacketParser::create(188, truncatedAssembler);
    EXPECT_TRUE(ctx, truncatedParser);
    if (!truncatedParser) return;
    EXPECT_TRUE(ctx, truncatedParser.value()->push(exactPayloadPacket(0, 0, true, firstPayload)));
    EXPECT_FALSE(ctx, truncatedParser.value()->push(confirmedPacket(sectionPacket(0, 1, pat))));
}

void testMultiSectionPsiAggregation(TestContext& ctx)
{
    RecordingInventorySink sink;
    MediaTsPsiSectionAssembler assembler(sink);
    auto parser = MediaTsPacketParser::create(188, assembler);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;
    const std::array programOne{std::pair<uint16_t, uint16_t>{1, 0x100}};
    const std::array programTwo{std::pair<uint16_t, uint16_t>{2, 0x200}};
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0, 0, patSection(7, programTwo, 1, 1))));
    EXPECT_TRUE(ctx, sink.snapshots.empty());
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0, 1, patSection(7, programTwo, 1, 1))));
    EXPECT_TRUE(ctx, sink.snapshots.empty());
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0, 2, patSection(7, programOne, 0, 1))));

    const std::array video{MediaTsElementaryStreamInfo{0x101, 0x1B}};
    const std::array audio{MediaTsElementaryStreamInfo{0x102, 0x0F}};
    const std::array secondVideo{MediaTsElementaryStreamInfo{0x201, 0x24}};
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x100, 0, pmtSection(1, 3, 0x101, audio, 1, 1))));
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x100, 1, pmtSection(1, 3, 0x101, audio, 1, 1))));
    EXPECT_TRUE(ctx, sink.snapshots.empty());
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x100, 2, pmtSection(1, 3, 0x101, video, 0, 1))));
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x200, 0, pmtSection(2, 1, 0x201, secondVideo))));
    EXPECT_TRUE(ctx, parser.value()->push(payloadPacket(0x711, 0, false, std::array<uint8_t, 1>{0})));
    EXPECT_EQ(ctx, sink.snapshots.size(), static_cast<std::size_t>(1));
    if (!sink.snapshots.empty()) {
        EXPECT_EQ(ctx, sink.snapshots.back().programs[0].elementaryStreams.size(), static_cast<std::size_t>(2));
    }

    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0, 3, patSection(8, programOne, 0, 1))));
    EXPECT_EQ(ctx, sink.snapshots.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0, 4, patSection(8, programTwo, 1, 1))));
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x100, 3, pmtSection(1, 4, 0x101, video))));
    EXPECT_TRUE(ctx, parser.value()->push(confirmedPacket(
        sectionPacket(0x200, 1, pmtSection(2, 2, 0x201, secondVideo)))));
    EXPECT_EQ(ctx, sink.snapshots.size(), static_cast<std::size_t>(2));
    if (sink.snapshots.size() == 2) EXPECT_EQ(ctx, sink.snapshots.back().patVersion, static_cast<uint8_t>(8));
}

void testPsiAggregateIdentityAndDiscontinuity(TestContext& ctx)
{
    const std::array programOne{std::pair<uint16_t, uint16_t>{1, 0x100}};
    const std::array programTwo{std::pair<uint16_t, uint16_t>{2, 0x200}};

    RecordingInventorySink identitySink;
    MediaTsPsiSectionAssembler identityAssembler(identitySink);
    auto identityParser = MediaTsPacketParser::create(188, identityAssembler);
    EXPECT_TRUE(ctx, identityParser);
    EXPECT_TRUE(ctx, identityParser.value()->push(sectionPacket(0, 0, patSection(1, programOne, 0, 1, 0x10))));
    EXPECT_FALSE(ctx, identityParser.value()->push(confirmedPacket(
        sectionPacket(0, 1, patSection(1, programTwo, 1, 1, 0x11)))));
    EXPECT_TRUE(ctx, identitySink.snapshots.empty());

    RecordingInventorySink patDiscontinuitySink;
    MediaTsPsiSectionAssembler patDiscontinuityAssembler(patDiscontinuitySink);
    auto patDiscontinuityParser = MediaTsPacketParser::create(188, patDiscontinuityAssembler);
    EXPECT_TRUE(ctx, patDiscontinuityParser);
    EXPECT_TRUE(ctx, patDiscontinuityParser.value()->push(sectionPacket(0, 0, patSection(2, programOne, 0, 1))));
    const auto patSecond = patSection(2, programTwo, 1, 1);
    std::vector<uint8_t> patSecondPayload{0};
    patSecondPayload.insert(patSecondPayload.end(), patSecond.begin(), patSecond.end());
    EXPECT_TRUE(ctx, patDiscontinuityParser.value()->push(
        exactPayloadPacket(0, 1, true, patSecondPayload, true)));
    EXPECT_TRUE(ctx, patDiscontinuitySink.snapshots.empty());
    EXPECT_TRUE(ctx, patDiscontinuityParser.value()->push(sectionPacket(0, 2, patSection(2, programOne, 0, 1))));

    const std::array video{MediaTsElementaryStreamInfo{0x101, 0x1B}};
    const std::array audio{MediaTsElementaryStreamInfo{0x102, 0x0F}};
    RecordingInventorySink pmtDiscontinuitySink;
    MediaTsPsiSectionAssembler pmtDiscontinuityAssembler(pmtDiscontinuitySink);
    auto pmtDiscontinuityParser = MediaTsPacketParser::create(188, pmtDiscontinuityAssembler);
    EXPECT_TRUE(ctx, pmtDiscontinuityParser);
    EXPECT_TRUE(ctx, pmtDiscontinuityParser.value()->push(sectionPacket(0, 0, patSection(3, programOne))));
    EXPECT_TRUE(ctx, pmtDiscontinuityParser.value()->push(sectionPacket(0x100, 0, pmtSection(1, 1, 0x101, video, 0, 1))));
    const auto pmtSecond = pmtSection(1, 1, 0x101, audio, 1, 1);
    std::vector<uint8_t> pmtSecondPayload{0};
    pmtSecondPayload.insert(pmtSecondPayload.end(), pmtSecond.begin(), pmtSecond.end());
    EXPECT_TRUE(ctx, pmtDiscontinuityParser.value()->push(
        exactPayloadPacket(0x100, 1, true, pmtSecondPayload, true)));
    EXPECT_TRUE(ctx, pmtDiscontinuitySink.snapshots.empty());
    EXPECT_TRUE(ctx, pmtDiscontinuityParser.value()->push(
        sectionPacket(0x100, 2, pmtSection(1, 1, 0x101, video, 0, 1))));
    EXPECT_TRUE(ctx, pmtDiscontinuityParser.value()->push(
        payloadPacket(0x712, 0, false, std::array<uint8_t, 1>{0})));
    EXPECT_EQ(ctx, pmtDiscontinuitySink.snapshots.size(), static_cast<std::size_t>(1));

    RecordingInventorySink continuitySink;
    MediaTsPsiSectionAssembler continuityAssembler(continuitySink);
    auto continuityParser = MediaTsPacketParser::create(188, continuityAssembler);
    EXPECT_TRUE(ctx, continuityParser);
    EXPECT_TRUE(ctx, continuityParser.value()->push(sectionPacket(0, 0, patSection(4, programOne, 0, 1))));
    EXPECT_FALSE(ctx, continuityParser.value()->push(confirmedPacket(
        sectionPacket(0, 2, patSection(4, programTwo, 1, 1)))));
    EXPECT_TRUE(ctx, continuityParser.value()->push(sectionPacket(0, 3, patSection(4, programTwo, 1, 1))));
    EXPECT_TRUE(ctx, continuitySink.snapshots.empty());

    RecordingInventorySink generationSink;
    MediaTsPsiSectionAssembler generationAssembler(generationSink);
    auto generationParser = MediaTsPacketParser::create(188, generationAssembler);
    EXPECT_TRUE(ctx, generationParser);
    EXPECT_TRUE(ctx, generationParser.value()->push(sectionPacket(0, 0, patSection(5, programOne))));
    EXPECT_TRUE(ctx, generationParser.value()->push(sectionPacket(0x100, 0, pmtSection(1, 2, 0x101, video))));
    const auto repeatedPmt = pmtSection(1, 2, 0x101, video);
    std::vector<uint8_t> repeatedPmtPayload{0};
    repeatedPmtPayload.insert(repeatedPmtPayload.end(), repeatedPmt.begin(), repeatedPmt.end());
    EXPECT_TRUE(ctx, generationParser.value()->push(
        exactPayloadPacket(0x100, 1, true, repeatedPmtPayload, true)));
    EXPECT_EQ(ctx, generationSink.snapshots.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, generationParser.value()->push(
        payloadPacket(0x713, 0, false, std::array<uint8_t, 1>{0})));
    EXPECT_EQ(ctx, generationSink.snapshots.size(), static_cast<std::size_t>(2));
}

void testAdaptationOnlyDiscontinuityResetsPsiGeneration(TestContext& ctx)
{
    const std::array program{std::pair<uint16_t, uint16_t>{1, 0x100}};
    const std::array video{MediaTsElementaryStreamInfo{0x101, 0x1B}};

    RecordingInventorySink patSink;
    MediaTsPsiSectionAssembler patAssembler(patSink);
    auto patParser = MediaTsPacketParser::create(188, patAssembler);
    EXPECT_TRUE(ctx, patParser);
    EXPECT_TRUE(ctx, patParser.value()->push(sectionPacket(0, 0, patSection(6, program))));
    EXPECT_TRUE(ctx, patParser.value()->push(sectionPacket(0x100, 0, pmtSection(1, 2, 0x101, video))));
    EXPECT_TRUE(ctx, patParser.value()->push(pcrPacket(0, 0, 1, 0, true)));
    EXPECT_EQ(ctx, patSink.snapshots.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, patParser.value()->push(sectionPacket(0, 1, patSection(6, program))));
    EXPECT_TRUE(ctx, patParser.value()->push(sectionPacket(0x100, 1, pmtSection(1, 2, 0x101, video))));
    EXPECT_TRUE(ctx, patParser.value()->push(payloadPacket(0x700, 0, false, std::array<uint8_t, 1>{0})));
    EXPECT_EQ(ctx, patSink.snapshots.size(), static_cast<std::size_t>(2));

    RecordingInventorySink pmtSink;
    MediaTsPsiSectionAssembler pmtAssembler(pmtSink);
    auto pmtParser = MediaTsPacketParser::create(188, pmtAssembler);
    EXPECT_TRUE(ctx, pmtParser);
    EXPECT_TRUE(ctx, pmtParser.value()->push(sectionPacket(0, 0, patSection(7, program))));
    EXPECT_TRUE(ctx, pmtParser.value()->push(sectionPacket(0x100, 0, pmtSection(1, 3, 0x101, video))));
    EXPECT_TRUE(ctx, pmtParser.value()->push(pcrPacket(0x100, 0, 1, 0, true)));
    EXPECT_EQ(ctx, pmtSink.snapshots.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, pmtParser.value()->push(sectionPacket(0x100, 1, pmtSection(1, 3, 0x101, video))));
    EXPECT_TRUE(ctx, pmtParser.value()->push(payloadPacket(0x701, 0, false, std::array<uint8_t, 1>{0})));
    EXPECT_EQ(ctx, pmtSink.snapshots.size(), static_cast<std::size_t>(2));
}

} // namespace

void runMpegTsPacketTests(TestContext& ctx)
{
    testPacketFramingAndPcr(ctx);
    testMultiplePacketsAndMalformedPackets(ctx);
    testStrideAcquisitionAndReacquisition(ctx);
    testCompleteAdaptationStructureValidation(ctx);
    testPsiInventoryAndVersions(ctx);
    testPsiRejectsInvalidEvidence(ctx);
    testPsiCrossPacketAssemblyAndExplicitTruncation(ctx);
    testMultiSectionPsiAggregation(ctx);
    testPsiAggregateIdentityAndDiscontinuity(ctx);
    testAdaptationOnlyDiscontinuityResetsPsiGeneration(ctx);
}
