#pragma once

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsProgramInventory.h"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsPsiSectionAssembler final : public MediaTsPacketSink {
public:
    explicit MediaTsPsiSectionAssembler(MediaTsProgramInventorySink& sink);

    ::media::Status onPacket(const MediaTsPacketView& packet) override;

private:
    struct SectionState final {
        std::vector<uint8_t> bytes;
        std::optional<std::size_t> expectedSize;
    };

    struct PatProgram final {
        uint16_t programNumber = 0;
        uint16_t pmtPid = 0;

        bool operator==(const PatProgram&) const = default;
    };

    struct PatTableAssembly final {
        uint8_t version = 0;
        uint8_t lastSectionNumber = 0;
        std::unordered_map<uint8_t, std::vector<PatProgram>> sections;
    };

    struct PmtTableAssembly final {
        uint16_t programNumber = 0;
        uint8_t version = 0;
        uint8_t lastSectionNumber = 0;
        uint16_t pcrPid = 0;
        std::unordered_map<uint8_t, std::vector<MediaTsElementaryStreamInfo>> sections;
    };

    ::media::Status consume(uint16_t pid, std::span<const uint8_t> bytes);
    ::media::Status completeSection(uint16_t pid, std::span<const uint8_t> section);
    ::media::Status parsePat(std::span<const uint8_t> section);
    ::media::Status parsePmt(uint16_t pid, std::span<const uint8_t> section);
    ::media::Status publishIfComplete();

    MediaTsProgramInventorySink& m_sink;
    std::unordered_map<uint16_t, SectionState> m_sections;
    std::optional<PatTableAssembly> m_patAssembly;
    std::optional<uint8_t> m_patVersion;
    std::vector<PatProgram> m_patPrograms;
    std::unordered_map<uint16_t, PmtTableAssembly> m_pmtAssemblies;
    std::unordered_map<uint16_t, MediaTsProgramInfo> m_programsByPmtPid;
    std::optional<MediaTsProgramInventorySnapshot> m_lastPublished;
};

} // namespace media::ffmpeg::graph
