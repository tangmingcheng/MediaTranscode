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
    const auto* videoOnly = std::get_if<MediaTsVideoOnlyProgramPlan>(
        &parameters.program);
    const auto* audioVideo = std::get_if<MediaTsAudioVideoProgramPlan>(
        &parameters.program);
    std::vector<std::uint8_t> section;
    section.reserve(videoOnly ? 21 : 26);
    section.insert(
        section.end(),
        videoOnly ? std::initializer_list<std::uint8_t>{0x02, 0xB0, 0x12}
                  : std::initializer_list<std::uint8_t>{0x02, 0xB0, 0x17});
    appendU16(section, parameters.programNumber);
    section.push_back(static_cast<std::uint8_t>(0xC1 | (parameters.tableVersion << 1)));
    section.insert(section.end(), {0x00, 0x00});
    const std::uint16_t pcrPid = videoOnly
        ? videoOnly->pcrPid : audioVideo->pcrPid;
    const std::uint16_t videoPid = videoOnly
        ? videoOnly->videoPid : audioVideo->videoPid;
    const std::uint8_t videoStreamType = videoOnly
        ? videoOnly->videoStreamType : audioVideo->videoStreamType;
    appendPid(section, pcrPid, 0xE0);
    section.insert(section.end(), {0xF0, 0x00});
    section.push_back(videoStreamType);
    appendPid(section, videoPid, 0xE0);
    section.insert(section.end(), {0xF0, 0x00});
    if (audioVideo) {
        section.push_back(audioVideo->audioStreamType);
        appendPid(section, audioVideo->audioPid, 0xE0);
        section.insert(section.end(), {0xF0, 0x00});
    }
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
      m_tableVersion(parameters.tableVersion),
      m_program(parameters.program)
{
}

bool MediaTsPsiPlanIdentity::matches(const MediaTsMuxPlan& plan) const noexcept
{
    const auto& parameters = plan.parameters();
    return m_patPid == parameters.patPid &&
           m_transportStreamId == parameters.transportStreamId &&
           m_programNumber == parameters.programNumber &&
           m_programMapPid == parameters.programMapPid &&
           m_tableVersion == parameters.tableVersion &&
           m_program == parameters.program;
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
