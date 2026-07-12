#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"

#include <algorithm>
#include <unordered_set>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalidSection(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}

uint16_t readU16(std::span<const uint8_t> bytes, std::size_t offset)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
}

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

::media::Status validateLongSection(std::span<const uint8_t> section,
                                    uint8_t tableId,
                                    std::size_t minimumSize)
{
    if (section.size() < minimumSize) return invalidSection("MPEG-TS PSI section is too short");
    if (section[0] != tableId) return invalidSection("MPEG-TS PSI table id is invalid for PID");
    if ((section[1] & 0xF0) != 0xB0) return invalidSection("MPEG-TS PSI section header reserved bits are invalid");
    const std::size_t sectionLength = ((section[1] & 0x0F) << 8) | section[2];
    if (sectionLength > 1021 || sectionLength + 3 != section.size()) {
        return invalidSection("MPEG-TS PSI section length is invalid");
    }
    if ((section[5] & 0xC0) != 0xC0) return invalidSection("MPEG-TS PSI version reserved bits are invalid");
    if ((section[5] & 0x01) == 0) return invalidSection("MPEG-TS PSI current_next is not current");
    if (section[6] > section[7]) return invalidSection("MPEG-TS PSI section number exceeds last section");
    if (crc32Mpeg(section) != 0) return invalidSection("MPEG-TS PSI CRC32 is invalid");
    return ::media::Status::success();
}

} // namespace

MediaTsPsiSectionAssembler::MediaTsPsiSectionAssembler(MediaTsProgramInventorySink& sink)
    : m_sink(sink)
{
}

void MediaTsPsiSectionAssembler::resetPidGeneration(uint16_t pid)
{
    m_sections.erase(pid);
    m_lastPublished.reset();
    if (pid == 0) {
        m_patAssembly.reset();
        m_patVersion.reset();
        m_patPrograms.clear();
        m_pmtAssemblies.clear();
        m_programsByPmtPid.clear();
        return;
    }
    m_pmtAssemblies.erase(pid);
    m_programsByPmtPid.erase(pid);
}

::media::Status MediaTsPsiSectionAssembler::onContinuityLoss(uint16_t pid)
{
    resetPidGeneration(pid);
    return ::media::Status::success();
}

::media::Status MediaTsPsiSectionAssembler::onPacket(const MediaTsPacketView& packet)
{
    const bool isPat = packet.pid == 0;
    const bool isKnownPmt = std::any_of(m_patPrograms.begin(), m_patPrograms.end(),
        [&](const PatProgram& program) { return program.pmtPid == packet.pid; });
    if (!isPat && !isKnownPmt) return ::media::Status::success();

    if (packet.discontinuity) {
        resetPidGeneration(packet.pid);
        if (packet.payloadSpan.empty()) return ::media::Status::success();
        if (!packet.payloadUnitStart) return invalidSection("MPEG-TS PSI discontinuity lacks a section start");
    }
    if (packet.payloadSpan.empty()) return ::media::Status::success();
    auto& state = m_sections[packet.pid];
    if (packet.payloadUnitStart) {
        const std::size_t pointer = packet.payloadSpan[0];
        if (pointer > packet.payloadSpan.size() - 1) return invalidSection("MPEG-TS PSI pointer field exceeds payload");
        const auto prefix = packet.payloadSpan.subspan(1, pointer);
        if (!state.bytes.empty()) {
            auto status = consume(packet.pid, prefix);
            if (!status) return status;
            if (!state.bytes.empty()) return invalidSection("MPEG-TS PSI previous section is truncated");
        }
        return consume(packet.pid, packet.payloadSpan.subspan(1 + pointer));
    }

    if (state.bytes.empty()) return ::media::Status::success();
    return consume(packet.pid, packet.payloadSpan);
}

::media::Status MediaTsPsiSectionAssembler::consume(uint16_t pid, std::span<const uint8_t> bytes)
{
    auto& state = m_sections[pid];
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        if (state.bytes.empty() && bytes[offset] == 0xFF) return ::media::Status::success();
        if (state.bytes.size() < 3) {
            const std::size_t needed = 3 - state.bytes.size();
            const std::size_t take = std::min(needed, bytes.size() - offset);
            state.bytes.insert(state.bytes.end(), bytes.begin() + offset, bytes.begin() + offset + take);
            offset += take;
            if (state.bytes.size() < 3) return ::media::Status::success();
            const std::size_t sectionLength = ((state.bytes[1] & 0x0F) << 8) | state.bytes[2];
            if (sectionLength > 1021 || sectionLength < 4) return invalidSection("MPEG-TS PSI section length is out of range");
            state.expectedSize = sectionLength + 3;
        }

        const std::size_t needed = *state.expectedSize - state.bytes.size();
        const std::size_t take = std::min(needed, bytes.size() - offset);
        state.bytes.insert(state.bytes.end(), bytes.begin() + offset, bytes.begin() + offset + take);
        offset += take;
        if (state.bytes.size() != *state.expectedSize) return ::media::Status::success();

        auto completed = std::move(state.bytes);
        state = {};
        auto status = completeSection(pid, completed);
        if (!status) return status;
    }
    return ::media::Status::success();
}

::media::Status MediaTsPsiSectionAssembler::completeSection(uint16_t pid,
                                                             std::span<const uint8_t> section)
{
    return pid == 0 ? parsePat(section) : parsePmt(pid, section);
}

::media::Status MediaTsPsiSectionAssembler::parsePat(std::span<const uint8_t> section)
{
    auto status = validateLongSection(section, 0x00, 12);
    if (!status) return status;
    if ((section.size() - 12) % 4 != 0) return invalidSection("MPEG-TS PAT program loop is misaligned");

    std::vector<PatProgram> sectionPrograms;
    std::unordered_set<uint16_t> programNumbers;
    std::unordered_set<uint16_t> pmtPids;
    for (std::size_t offset = 8; offset < section.size() - 4; offset += 4) {
        const uint16_t programNumber = readU16(section, offset);
        const uint16_t pid = static_cast<uint16_t>(((section[offset + 2] & 0x1F) << 8) | section[offset + 3]);
        if ((section[offset + 2] & 0xE0) != 0xE0) return invalidSection("MPEG-TS PAT PID reserved bits are invalid");
        if (programNumber == 0) continue;
        if (pid == 0x1FFF || !programNumbers.insert(programNumber).second || !pmtPids.insert(pid).second) {
            return invalidSection("MPEG-TS PAT has invalid or duplicate program mapping");
        }
        sectionPrograms.push_back({programNumber, pid});
    }

    const uint8_t version = static_cast<uint8_t>((section[5] >> 1) & 0x1F);
    const uint16_t transportStreamId = readU16(section, 3);
    const uint8_t sectionNumber = section[6];
    const uint8_t lastSectionNumber = section[7];
    if (!m_patAssembly || m_patAssembly->version != version) {
        m_patAssembly = PatTableAssembly{transportStreamId, version, lastSectionNumber, {}};
        m_patVersion.reset();
        m_patPrograms.clear();
        m_pmtAssemblies.clear();
        m_programsByPmtPid.clear();
    } else if (m_patAssembly->lastSectionNumber != lastSectionNumber ||
               m_patAssembly->transportStreamId != transportStreamId) {
        return invalidSection("MPEG-TS PAT identity changed without a version change");
    }

    const auto existingSection = m_patAssembly->sections.find(sectionNumber);
    if (existingSection != m_patAssembly->sections.end()) {
        if (existingSection->second != sectionPrograms) {
            return invalidSection("MPEG-TS PAT section changed without a version change");
        }
        return ::media::Status::success();
    }
    m_patAssembly->sections.emplace(sectionNumber, std::move(sectionPrograms));
    if (m_patAssembly->sections.size() != static_cast<std::size_t>(lastSectionNumber) + 1) {
        return ::media::Status::success();
    }

    std::vector<PatProgram> programs;
    programNumbers.clear();
    pmtPids.clear();
    for (uint16_t number = 0; number <= lastSectionNumber; ++number) {
        const auto part = m_patAssembly->sections.find(static_cast<uint8_t>(number));
        if (part == m_patAssembly->sections.end()) return ::media::Status::success();
        for (const auto& program : part->second) {
            if (!programNumbers.insert(program.programNumber).second || !pmtPids.insert(program.pmtPid).second) {
                return invalidSection("MPEG-TS PAT has duplicate program mapping across sections");
            }
            programs.push_back(program);
        }
    }
    if (programs.empty()) return invalidSection("MPEG-TS PAT contains no programs");
    m_patVersion = version;
    m_patPrograms = std::move(programs);
    for (auto iterator = m_sections.begin(); iterator != m_sections.end();) {
        if (iterator->first == 0) {
            ++iterator;
        } else {
            iterator = m_sections.erase(iterator);
        }
    }
    return ::media::Status::success();
}

::media::Status MediaTsPsiSectionAssembler::parsePmt(uint16_t pid,
                                                     std::span<const uint8_t> section)
{
    auto status = validateLongSection(section, 0x02, 16);
    if (!status) return status;
    const auto patProgram = std::find_if(m_patPrograms.begin(), m_patPrograms.end(),
        [&](const PatProgram& program) { return program.pmtPid == pid; });
    if (patProgram == m_patPrograms.end()) return invalidSection("MPEG-TS PMT PID is absent from PAT");
    if (readU16(section, 3) != patProgram->programNumber) {
        return invalidSection("MPEG-TS PMT program number does not match PAT");
    }
    if ((section[8] & 0xE0) != 0xE0 || (section[10] & 0xF0) != 0xF0) {
        return invalidSection("MPEG-TS PMT reserved bits are invalid");
    }
    const uint16_t pcrPid = static_cast<uint16_t>(((section[8] & 0x1F) << 8) | section[9]);
    if (pcrPid == 0x1FFF) return invalidSection("MPEG-TS PMT PCR PID is null");
    const std::size_t programInfoLength = ((section[10] & 0x0F) << 8) | section[11];
    std::size_t offset = 12 + programInfoLength;
    const std::size_t crcOffset = section.size() - 4;
    if (offset > crcOffset) return invalidSection("MPEG-TS PMT program descriptors exceed section");

    std::vector<MediaTsElementaryStreamInfo> sectionStreams;
    std::unordered_set<uint16_t> streamPids;
    while (offset < crcOffset) {
        if (crcOffset - offset < 5) return invalidSection("MPEG-TS PMT elementary stream entry is truncated");
        if ((section[offset + 1] & 0xE0) != 0xE0 || (section[offset + 3] & 0xF0) != 0xF0) {
            return invalidSection("MPEG-TS PMT elementary stream reserved bits are invalid");
        }
        const uint16_t streamPid = static_cast<uint16_t>(((section[offset + 1] & 0x1F) << 8) | section[offset + 2]);
        const std::size_t infoLength = ((section[offset + 3] & 0x0F) << 8) | section[offset + 4];
        if (streamPid == 0x1FFF || !streamPids.insert(streamPid).second) {
            return invalidSection("MPEG-TS PMT has invalid or duplicate elementary PID");
        }
        if (infoLength > crcOffset - offset - 5) {
            return invalidSection("MPEG-TS PMT elementary descriptors exceed section");
        }
        sectionStreams.push_back({streamPid, section[offset]});
        offset += 5 + infoLength;
    }
    const uint8_t version = static_cast<uint8_t>((section[5] >> 1) & 0x1F);
    const uint8_t sectionNumber = section[6];
    const uint8_t lastSectionNumber = section[7];
    auto assembly = m_pmtAssemblies.find(pid);
    if (assembly == m_pmtAssemblies.end() || assembly->second.version != version) {
        m_programsByPmtPid.erase(pid);
        assembly = m_pmtAssemblies.insert_or_assign(pid,
            PmtTableAssembly{patProgram->programNumber, version, lastSectionNumber, pcrPid, {}}).first;
    } else if (assembly->second.lastSectionNumber != lastSectionNumber ||
               assembly->second.programNumber != patProgram->programNumber ||
               assembly->second.pcrPid != pcrPid) {
        return invalidSection("MPEG-TS PMT identity changed without a version change");
    }

    const auto existingSection = assembly->second.sections.find(sectionNumber);
    if (existingSection != assembly->second.sections.end()) {
        if (existingSection->second != sectionStreams) {
            return invalidSection("MPEG-TS PMT section changed without a version change");
        }
        return ::media::Status::success();
    }
    assembly->second.sections.emplace(sectionNumber, std::move(sectionStreams));
    if (assembly->second.sections.size() != static_cast<std::size_t>(lastSectionNumber) + 1) {
        return ::media::Status::success();
    }

    std::vector<MediaTsElementaryStreamInfo> streams;
    streamPids.clear();
    for (uint16_t number = 0; number <= lastSectionNumber; ++number) {
        const auto part = assembly->second.sections.find(static_cast<uint8_t>(number));
        if (part == assembly->second.sections.end()) return ::media::Status::success();
        for (const auto& stream : part->second) {
            if (!streamPids.insert(stream.pid).second) {
                return invalidSection("MPEG-TS PMT has duplicate elementary PID across sections");
            }
            streams.push_back(stream);
        }
    }
    if (streams.empty()) return invalidSection("MPEG-TS PMT contains no elementary streams");
    m_programsByPmtPid[pid] = MediaTsProgramInfo{
        patProgram->programNumber, pid, version, pcrPid, std::move(streams)};
    return publishIfComplete();
}

::media::Status MediaTsPsiSectionAssembler::publishIfComplete()
{
    if (!m_patVersion || m_programsByPmtPid.size() != m_patPrograms.size()) {
        return ::media::Status::success();
    }
    MediaTsProgramInventorySnapshot snapshot;
    snapshot.patVersion = *m_patVersion;
    snapshot.programs.reserve(m_patPrograms.size());
    for (const auto& mapping : m_patPrograms) {
        const auto program = m_programsByPmtPid.find(mapping.pmtPid);
        if (program == m_programsByPmtPid.end()) return ::media::Status::success();
        snapshot.programs.push_back(program->second);
    }
    if (m_lastPublished == snapshot) return ::media::Status::success();
    auto status = m_sink.onProgramInventory(snapshot);
    if (!status) return status;
    m_lastPublished = std::move(snapshot);
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
