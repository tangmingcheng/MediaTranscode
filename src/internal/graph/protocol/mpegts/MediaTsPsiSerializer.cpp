#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"

#include "internal/graph/protocol/mpegts/MediaTsCrc32.h"

#include <array>

namespace media::ffmpeg::graph {
namespace {

void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}

void appendPid(std::vector<std::uint8_t>& output,
               std::uint16_t pid,
               std::uint8_t reservedPrefix)
{
    output.push_back(static_cast<std::uint8_t>(reservedPrefix | (pid >> 8)));
    output.push_back(static_cast<std::uint8_t>(pid));
}

void appendCrc(std::vector<std::uint8_t>& section)
{
    const std::uint32_t crc = MediaTsCrc32::compute(section);
    section.push_back(static_cast<std::uint8_t>(crc >> 24));
    section.push_back(static_cast<std::uint8_t>(crc >> 16));
    section.push_back(static_cast<std::uint8_t>(crc >> 8));
    section.push_back(static_cast<std::uint8_t>(crc));
}

std::vector<std::uint8_t> serializePat(const MediaTsMuxPlanParameters& parameters)
{
    std::vector<std::uint8_t> section;
    section.reserve(16);
    section.insert(section.end(), {0x00, 0xB0, 0x0D});
    appendU16(section, parameters.transportStreamId);
    section.push_back(static_cast<std::uint8_t>(0xC1 | (parameters.tableVersion << 1)));
    section.insert(section.end(), {0x00, 0x00});
    appendU16(section, parameters.programNumber);
    appendPid(section, parameters.programMapPid, 0xE0);
    appendCrc(section);
    return section;
}

std::vector<std::uint8_t> serializePmt(const MediaTsMuxPlanParameters& parameters)
{
    std::vector<std::uint8_t> section;
    section.reserve(26);
    section.insert(section.end(), {0x02, 0xB0, 0x17});
    appendU16(section, parameters.programNumber);
    section.push_back(static_cast<std::uint8_t>(0xC1 | (parameters.tableVersion << 1)));
    section.insert(section.end(), {0x00, 0x00});
    appendPid(section, parameters.pcrPid, 0xE0);
    section.insert(section.end(), {0xF0, 0x00});
    section.push_back(parameters.videoStreamType);
    appendPid(section, parameters.videoPid, 0xE0);
    section.insert(section.end(), {0xF0, 0x00});
    section.push_back(parameters.audioStreamType);
    appendPid(section, parameters.audioPid, 0xE0);
    section.insert(section.end(), {0xF0, 0x00});
    appendCrc(section);
    return section;
}

} // namespace

MediaTsPsiPlanIdentity::MediaTsPsiPlanIdentity(
    const MediaTsMuxPlanParameters& parameters) noexcept
    : m_patPid(parameters.patPid),
      m_transportStreamId(parameters.transportStreamId),
      m_programNumber(parameters.programNumber),
      m_programMapPid(parameters.programMapPid),
      m_pcrPid(parameters.pcrPid),
      m_videoPid(parameters.videoPid),
      m_audioPid(parameters.audioPid),
      m_tableVersion(parameters.tableVersion),
      m_videoStreamType(parameters.videoStreamType),
      m_audioStreamType(parameters.audioStreamType)
{
}

bool MediaTsPsiPlanIdentity::matches(const MediaTsMuxPlan& plan) const noexcept
{
    const auto& parameters = plan.parameters();
    return m_patPid == parameters.patPid &&
           m_transportStreamId == parameters.transportStreamId &&
           m_programNumber == parameters.programNumber &&
           m_programMapPid == parameters.programMapPid &&
           m_pcrPid == parameters.pcrPid &&
           m_videoPid == parameters.videoPid &&
           m_audioPid == parameters.audioPid &&
           m_tableVersion == parameters.tableVersion &&
           m_videoStreamType == parameters.videoStreamType &&
           m_audioStreamType == parameters.audioStreamType;
}

::media::Result<MediaTsProgramTables> MediaTsPsiSerializer::serialize(
    const MediaTsMuxPlan& plan)
{
    const auto& parameters = plan.parameters();
    const MediaTsPsiPlanIdentity identity(parameters);
    return ::media::Result<MediaTsProgramTables>::success(
        MediaTsProgramTables(
            MediaTsPatSection(identity, serializePat(parameters)),
            MediaTsPmtSection(identity, serializePmt(parameters))));
}

} // namespace media::ffmpeg::graph
