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
                                uint8_t lastSectionNumber = 0)
{
    const uint16_t sectionLength = static_cast<uint16_t>(5 + programs.size() * 4 + 4);
    std::vector<uint8_t> section{
        0x00,
        static_cast<uint8_t>(0xB0 | (sectionLength >> 8)),
        static_cast<uint8_t>(sectionLength),
        0x00, 0x01,
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
                                            std::span<const uint8_t> payload)
{
    std::array<uint8_t, 188> packet{};
    packet.fill(0xFF);
    packet[0] = 0x47;
    packet[1] = static_cast<uint8_t>((payloadStart ? 0x40 : 0x00) | (pid >> 8));
    packet[2] = static_cast<uint8_t>(pid);
    packet[3] = static_cast<uint8_t>(0x30 | continuity);
    packet[4] = static_cast<uint8_t>(183 - payload.size());
    if (packet[4] != 0) packet[5] = 0;
    std::copy(payload.begin(), payload.end(), packet.end() - payload.size());
    return packet;
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
    EXPECT_TRUE(ctx, parser.value()->push(both));
    EXPECT_EQ(ctx, sink.packets.size(), static_cast<std::size_t>(2));

    auto invalidLength = pcrPacket(0x121, 0, 1, 2, false);
    invalidLength[4] = 184;
    EXPECT_FALSE(ctx, parser.value()->push(invalidLength));

    RecordingPacketSink extensionSink;
    auto extensionParser = MediaTsPacketParser::create(188, extensionSink);
    EXPECT_TRUE(ctx, extensionParser);
    auto invalidExtension = pcrPacket(0x122, 0, 1, 300, false);
    invalidExtension[10] = static_cast<uint8_t>((invalidExtension[10] & 0xFE) | 1);
    invalidExtension[11] = 44;
    EXPECT_FALSE(ctx, extensionParser.value()->push(invalidExtension));

    RecordingPacketSink continuitySink;
    auto continuityParser = MediaTsPacketParser::create(188, continuitySink);
    EXPECT_TRUE(ctx, continuityParser);
    EXPECT_TRUE(ctx, continuityParser.value()->push(first));
    second[3] = 0x12;
    EXPECT_FALSE(ctx, continuityParser.value()->push(second));

    EXPECT_FALSE(ctx, MediaTsPacketParser::create(192, sink));
    EXPECT_FALSE(ctx, MediaTsPacketParser::create(204, sink));
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
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x200, 2, pmtSection(2, 6, 0x201, secondStreams))));
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
    EXPECT_FALSE(ctx, parser.value()->push(sectionPacket(0, 0, invalidPat)));

    RecordingInventorySink currentSink;
    MediaTsPsiSectionAssembler currentAssembler(currentSink);
    auto currentParser = MediaTsPacketParser::create(188, currentAssembler);
    EXPECT_TRUE(ctx, currentParser);
    auto nonCurrentPat = patSection(1, std::array{std::pair<uint16_t, uint16_t>{1, 0x100}});
    nonCurrentPat.resize(nonCurrentPat.size() - 4);
    nonCurrentPat[5] &= 0xFE;
    appendCrc(nonCurrentPat);
    EXPECT_FALSE(ctx, currentParser.value()->push(sectionPacket(0, 0, nonCurrentPat)));

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
    EXPECT_FALSE(ctx, truncatedParser.value()->push(wrongContinuation));
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
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x100, 0, pmtSection(1, 2, 0x101, streams))));
    EXPECT_EQ(ctx, inventorySink.snapshots.size(), static_cast<std::size_t>(1));

    RecordingInventorySink truncatedSink;
    MediaTsPsiSectionAssembler truncatedAssembler(truncatedSink);
    auto truncatedParser = MediaTsPacketParser::create(188, truncatedAssembler);
    EXPECT_TRUE(ctx, truncatedParser);
    if (!truncatedParser) return;
    EXPECT_TRUE(ctx, truncatedParser.value()->push(exactPayloadPacket(0, 0, true, firstPayload)));
    EXPECT_FALSE(ctx, truncatedParser.value()->push(sectionPacket(0, 1, pat)));
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
    EXPECT_EQ(ctx, sink.snapshots.size(), static_cast<std::size_t>(1));
    if (!sink.snapshots.empty()) {
        EXPECT_EQ(ctx, sink.snapshots.back().programs[0].elementaryStreams.size(), static_cast<std::size_t>(2));
    }

    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0, 3, patSection(8, programOne, 0, 1))));
    EXPECT_EQ(ctx, sink.snapshots.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0, 4, patSection(8, programTwo, 1, 1))));
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x100, 3, pmtSection(1, 4, 0x101, video))));
    EXPECT_TRUE(ctx, parser.value()->push(sectionPacket(0x200, 1, pmtSection(2, 2, 0x201, secondVideo))));
    EXPECT_EQ(ctx, sink.snapshots.size(), static_cast<std::size_t>(2));
    if (sink.snapshots.size() == 2) EXPECT_EQ(ctx, sink.snapshots.back().patVersion, static_cast<uint8_t>(8));
}

} // namespace

void runMpegTsPacketTests(TestContext& ctx)
{
    testPacketFramingAndPcr(ctx);
    testMultiplePacketsAndMalformedPackets(ctx);
    testPsiInventoryAndVersions(ctx);
    testPsiRejectsInvalidEvidence(ctx);
    testPsiCrossPacketAssemblyAndExplicitTruncation(ctx);
    testMultiSectionPsiAggregation(ctx);
}
