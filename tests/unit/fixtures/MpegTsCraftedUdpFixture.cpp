#include "unit/fixtures/MpegTsCraftedUdpFixture.h"

#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpSocket.h"

extern "C" {
#include <libavutil/dict.h>
}

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace media::ffmpeg::graph::test_fixture {
namespace {

constexpr std::size_t kPacketBytes = 188;
constexpr std::size_t kPacketsPerDatagram = 7;
constexpr std::uint64_t kPcrModulus = (std::uint64_t{1} << 33) * 300;

::media::Status configureRuntimeBinding(
    MediaTsInputSession& session,
    const CraftedTsProgramIdentity& identity,
    std::size_t pesProvenanceCapacity)
{
    const auto& programs = session.programSnapshots();
    const auto program = std::find_if(
        programs.begin(), programs.end(), [&identity](const auto& item) {
            return item.programNumber == identity.programNumber &&
                   item.pmtPid == identity.pmtPid;
        });
    if (program == programs.end()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "crafted fixture selected program snapshot is unavailable"));
    }
    const auto streamIndexFor = [program](std::uint16_t pid) -> std::optional<int> {
        const auto binding = std::find_if(
            program->streamBindings.begin(), program->streamBindings.end(),
            [pid](const auto& item) { return item.elementaryPid == pid; });
        return binding == program->streamBindings.end()
            ? std::nullopt
            : std::optional<int>{binding->streamIndex};
    };
    const auto videoStream = streamIndexFor(identity.videoPid);
    const auto audioStream = streamIndexFor(identity.audioPid);
    if (!videoStream || !audioStream) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "crafted fixture selected A/V stream snapshot is unavailable"));
    }
    return session.configureRuntimeBinding(MediaTsRuntimeBinding{
        MediaTsPacketOriginPolicy::PerStreamPesCarry,
        MediaTsRuntimeStreamBinding{*videoStream, identity.videoPid},
        MediaTsRuntimeStreamBinding{*audioStream, identity.audioPid},
        static_cast<std::uint16_t>(program->pcrPid),
        pesProvenanceCapacity});
}

std::filesystem::path ffmpegPath()
{
#ifdef _WIN32
    std::vector<wchar_t> modulePath(32'768);
    const DWORD length = GetModuleFileNameW(
        nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length > 0 && length < modulePath.size()) {
        const auto sibling = std::filesystem::path(
            std::wstring(modulePath.data(), length)).parent_path() / "ffmpeg.exe";
        if (std::filesystem::exists(sibling)) return sibling;
    }
#endif
    return "ffmpeg";
}

::media::Status generateBaseFile(const std::filesystem::path& output)
{
    const auto sample = std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
        "tests" / "samples" / "sample_h264_aac_2560x1440.mp4";
#ifdef _WIN32
    const std::wstring command = L"\"" + ffmpegPath().wstring() +
        L"\" -hide_banner -loglevel error -stream_loop -1 -i \"" +
        sample.wstring() +
        L"\" -t 1 -map 0:v:0 -map 0:a:0 -c copy -mpegts_pmt_start_pid 4096 "
        L"-streamid 0:256 -streamid 1:257 -f mpegts -y \"" +
        output.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "failed to generate crafted MPEG-TS fixture", GetLastError()));
    }
    CloseHandle(process.hThread);
    const DWORD wait = WaitForSingleObject(process.hProcess, 30'000);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    if (wait != WAIT_OBJECT_0 || exitCode != 0) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "FFmpeg failed to generate crafted MPEG-TS fixture",
            static_cast<int>(exitCode)));
    }
    return ::media::Status::success();
#else
    (void)output;
    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "crafted MPEG-TS UDP fixture is currently implemented for Windows"));
#endif
}

std::uint16_t packetPid(std::span<const std::uint8_t> packet)
{
    return static_cast<std::uint16_t>(
        ((packet[1] & 0x1F) << 8) | packet[2]);
}

std::optional<std::size_t> payloadOffset(std::span<const std::uint8_t> packet)
{
    const auto control = static_cast<std::uint8_t>((packet[3] >> 4) & 0x03);
    if (control == 1) return 4;
    if (control != 3 || packet[4] > 182) return std::nullopt;
    return static_cast<std::size_t>(5 + packet[4]);
}

bool hasPcr(std::span<const std::uint8_t> packet)
{
    const auto control = static_cast<std::uint8_t>((packet[3] >> 4) & 0x03);
    return (control == 2 || control == 3) && packet[4] >= 7 &&
        (packet[5] & 0x10) != 0;
}

void encodePcr(std::span<std::uint8_t> packet, std::uint64_t value)
{
    const std::uint64_t base = value / 300;
    const std::uint16_t extension = static_cast<std::uint16_t>(value % 300);
    packet[6] = static_cast<std::uint8_t>(base >> 25);
    packet[7] = static_cast<std::uint8_t>(base >> 17);
    packet[8] = static_cast<std::uint8_t>(base >> 9);
    packet[9] = static_cast<std::uint8_t>(base >> 1);
    packet[10] = static_cast<std::uint8_t>(
        ((base & 1) << 7) | 0x7E | (extension >> 8));
    packet[11] = static_cast<std::uint8_t>(extension);
}

std::uint32_t crc32Mpeg(std::span<const std::uint8_t> bytes)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto byte : bytes) {
        crc ^= static_cast<std::uint32_t>(byte) << 24;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80000000U) != 0
                ? (crc << 1) ^ 0x04C11DB7U
                : crc << 1;
        }
    }
    return crc;
}

::media::Status rewritePsiPacket(std::span<std::uint8_t> packet,
                                 std::uint8_t version,
                                 std::optional<std::uint16_t> pcrPid)
{
    const auto payload = payloadOffset(packet);
    if (!payload || *payload >= packet.size() || (packet[1] & 0x40) == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted PSI packet does not contain a section start"));
    }
    const auto sectionOffset = *payload + 1 + packet[*payload];
    if (sectionOffset + 12 > packet.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted PSI section is truncated"));
    }
    const auto sectionLength = static_cast<std::size_t>(
        ((packet[sectionOffset + 1] & 0x0F) << 8) |
        packet[sectionOffset + 2]);
    const auto sectionSize = sectionLength + 3;
    if (sectionOffset + sectionSize > packet.size() || sectionSize < 12) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted PSI section size is invalid"));
    }
    auto section = packet.subspan(sectionOffset, sectionSize);
    section[5] = static_cast<std::uint8_t>(
        (section[5] & 0xC1) | ((version & 0x1F) << 1));
    if (pcrPid) {
        if (section[0] != 0x02) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "crafted PCR PID change requires a PMT section"));
        }
        section[8] = static_cast<std::uint8_t>(0xE0 | (*pcrPid >> 8));
        section[9] = static_cast<std::uint8_t>(*pcrPid);
    }
    const auto crc = crc32Mpeg(section.first(section.size() - 4));
    section[section.size() - 4] = static_cast<std::uint8_t>(crc >> 24);
    section[section.size() - 3] = static_cast<std::uint8_t>(crc >> 16);
    section[section.size() - 2] = static_cast<std::uint8_t>(crc >> 8);
    section[section.size() - 1] = static_cast<std::uint8_t>(crc);
    return ::media::Status::success();
}

} // namespace

CraftedTsBytes::CraftedTsBytes(std::vector<std::uint8_t> bytes,
                               std::vector<std::size_t> pcrPacketOffsets) noexcept
    : m_bytes(std::move(bytes))
    , m_pcrPacketOffsets(std::move(pcrPacketOffsets))
{
}

::media::Result<CraftedTsBytes> CraftedTsBytes::generate()
{
    static std::optional<CraftedTsBytes> cached;
    if (cached) {
        return ::media::Result<CraftedTsBytes>::success(*cached);
    }
    const auto output = std::filesystem::temp_directory_path() /
        ("media_transcode_crafted_" +
#ifdef _WIN32
         std::to_string(GetCurrentProcessId()) +
#else
         std::string("unsupported") +
#endif
         ".ts");
    if (auto status = generateBaseFile(output); !status) {
        return ::media::Result<CraftedTsBytes>::failure(status.error());
    }
    std::ifstream input(output, std::ios::binary | std::ios::ate);
    if (!input) {
        std::filesystem::remove(output);
        return ::media::Result<CraftedTsBytes>::failure(
            ::media::ErrorInfo::ioFailure("failed to open crafted MPEG-TS fixture"));
    }
    const auto size = input.tellg();
    if (size <= 0 || static_cast<std::uint64_t>(size) % kPacketBytes != 0) {
        std::filesystem::remove(output);
        return ::media::Result<CraftedTsBytes>::failure(
            ::media::ErrorInfo::invalidArgument(
                "generated crafted MPEG-TS fixture is not 188-byte framed"));
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    input.close();
    std::filesystem::remove(output);
    if (!input) {
        return ::media::Result<CraftedTsBytes>::failure(
            ::media::ErrorInfo::ioFailure("failed to read crafted MPEG-TS fixture"));
    }
    std::vector<std::size_t> pcrOffsets;
    for (std::size_t offset = 0; offset < bytes.size(); offset += kPacketBytes) {
        const auto packet = std::span<const std::uint8_t>(bytes).subspan(
            offset, kPacketBytes);
        if (packet[0] != 0x47) {
            return ::media::Result<CraftedTsBytes>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "generated crafted MPEG-TS fixture lost sync"));
        }
        if (packetPid(packet) == 0x0100 && hasPcr(packet)) {
            pcrOffsets.push_back(offset);
        }
    }
    if (pcrOffsets.size() < 6) {
        return ::media::Result<CraftedTsBytes>::failure(
            ::media::ErrorInfo::notInitialized(
                "generated crafted MPEG-TS fixture has insufficient PCR observations"));
    }
    cached = CraftedTsBytes(std::move(bytes), std::move(pcrOffsets));
    return ::media::Result<CraftedTsBytes>::success(*cached);
}

std::size_t CraftedTsBytes::pcrCount() const noexcept
{
    return m_pcrPacketOffsets.size();
}

std::uint64_t CraftedTsBytes::pcrModulus() const noexcept
{
    return kPcrModulus;
}

::media::Status CraftedTsBytes::rewritePcrSequence(
    std::uint64_t first,
    std::int64_t interval27Mhz)
{
    if (first >= kPcrModulus || interval27Mhz <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted PCR sequence parameters are invalid"));
    }
    auto value = first;
    for (std::size_t index = 0; index < m_pcrPacketOffsets.size(); ++index) {
        if (auto status = rewritePcr(index, value); !status) return status;
        value = (value + static_cast<std::uint64_t>(interval27Mhz)) % kPcrModulus;
    }
    return ::media::Status::success();
}

::media::Status CraftedTsBytes::rewritePcr(std::size_t index,
                                            std::uint64_t pcr27Mhz)
{
    if (index >= m_pcrPacketOffsets.size() || pcr27Mhz >= kPcrModulus) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted PCR rewrite is outside the fixture"));
    }
    auto packet = std::span<std::uint8_t>(m_bytes).subspan(
        m_pcrPacketOffsets[index], kPacketBytes);
    if (!hasPcr(packet)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted PCR rewrite target is not a PCR packet"));
    }
    encodePcr(packet, pcr27Mhz);
    return ::media::Status::success();
}

::media::Status CraftedTsBytes::markPcrDiscontinuity(std::size_t index)
{
    if (index >= m_pcrPacketOffsets.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted discontinuity target is outside the fixture"));
    }
    auto packet = std::span<std::uint8_t>(m_bytes).subspan(
        m_pcrPacketOffsets[index], kPacketBytes);
    if (!hasPcr(packet)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted discontinuity target is not a PCR packet"));
    }
    packet[5] |= 0x80;
    return ::media::Status::success();
}

::media::Status CraftedTsBytes::changePatAndPmtVersionAfterFirst(
    std::uint8_t version)
{
    return rewritePsiVersions(version, std::nullopt);
}

::media::Status CraftedTsBytes::changePcrPidAfterFirstPmt(
    std::uint8_t version,
    std::uint16_t pcrPid)
{
    if (pcrPid == 0 || pcrPid > 0x1FFE) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "crafted replacement PCR PID is invalid"));
    }
    return rewritePsiVersions(version, pcrPid);
}

::media::Status CraftedTsBytes::rewritePsiVersions(
    std::uint8_t version,
    std::optional<std::uint16_t> pcrPid)
{
    std::size_t patCount = 0;
    std::size_t pmtCount = 0;
    for (std::size_t offset = 0; offset < m_bytes.size(); offset += kPacketBytes) {
        auto packet = std::span<std::uint8_t>(m_bytes).subspan(offset, kPacketBytes);
        const auto pid = packetPid(packet);
        if (pid == 0) {
            ++patCount;
            if (patCount > 1) {
                if (auto status = rewritePsiPacket(packet, version, std::nullopt);
                    !status) return status;
            }
        } else if (pid == m_identity.pmtPid && ++pmtCount > 1) {
            if (auto status = rewritePsiPacket(packet, version, pcrPid);
                !status) return status;
        }
    }
    if (patCount < 2 || pmtCount < 2) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "crafted fixture has insufficient repeated PSI tables"));
    }
    return ::media::Status::success();
}

::media::Result<CraftedUdpObservation> observeCraftedBytesOverProductionUdp(
    const CraftedTsBytes& stream,
    std::uint16_t port)
{
    auto runtime = MediaSocketRuntime::create();
    if (!runtime) return ::media::Result<CraftedUdpObservation>::failure(runtime.error());
    auto socket = MediaUdpSocket::bind(runtime.value(), MediaUdpSocketConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 0, 262'144});
    if (!socket) return ::media::Result<CraftedUdpObservation>::failure(socket.error());

    std::atomic<bool> senderDone = false;
    std::optional<::media::ErrorInfo> senderError;
    std::thread sender([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        constexpr std::size_t datagramBytes = kPacketBytes * kPacketsPerDatagram;
        for (std::size_t offset = 0; offset < stream.bytes().size();
             offset += datagramBytes) {
            const auto count = (std::min)(
                datagramBytes, stream.bytes().size() - offset);
            auto status = socket.value().sendTo(
                "127.0.0.1", port,
                std::span<const std::uint8_t>(stream.bytes()).subspan(offset, count));
            if (!status) {
                senderError = status.error();
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        senderDone = true;
    });

    AVDictionary* demuxOptions = nullptr;
    av_dict_set(&demuxOptions, "scan_all_pmts", "1", 0);
    av_dict_set(&demuxOptions, "probesize", "524288", 0);
    av_dict_set(&demuxOptions, "analyzeduration", "500000", 0);
    MediaTsInputSessionOptions options;
    options.protocolUrl = "udp://127.0.0.1:" + std::to_string(port) +
        "?fifo_size=1000000&overrun_nonfatal=0&timeout=200000";
    options.demuxOptions = demuxOptions;
    options.avioBufferBytes = 32 * 1024;
    options.packetStride = kPacketBytes;
    options.evidenceCapacity = 1'024;
    options.pesProvenanceCapacity = 32'768;
    options.maximumPositionRegressionBytes = 2 * 1024 * 1024;
    auto session = MediaTsInputSession::open(options);
    av_dict_free(&demuxOptions);
    if (!session) {
        sender.join();
        return ::media::Result<CraftedUdpObservation>::failure(session.error());
    }
    if (auto configured = configureRuntimeBinding(
            *session.value(), stream.identity(), options.pesProvenanceCapacity);
        !configured) {
        session.value()->close();
        sender.join();
        return ::media::Result<CraftedUdpObservation>::failure(configured.error());
    }

    for (std::size_t reads = 0; reads < 4'096; ++reads) {
        auto observed = session.value()->evidenceSnapshotAfter(std::nullopt);
        if (!observed) {
            session.value()->close();
            sender.join();
            return ::media::Result<CraftedUdpObservation>::failure(observed.error());
        }
        const auto pcrCount = std::count_if(
            observed.value().begin(), observed.value().end(),
            [](const MediaTsEvidenceCheckpoint& item) {
                return item.pcrObservation.has_value();
            });
        if (senderDone && pcrCount >= 6) break;
        auto frame = session.value()->readFrame();
        if (!frame) {
            session.value()->close();
            sender.join();
            return ::media::Result<CraftedUdpObservation>::failure(frame.error());
        }
    }
    sender.join();
    if (senderError) {
        session.value()->close();
        return ::media::Result<CraftedUdpObservation>::failure(*senderError);
    }
    auto evidence = session.value()->evidenceSnapshotAfter(std::nullopt);
    const auto closeStatus = session.value()->close();
    if (!evidence) {
        return ::media::Result<CraftedUdpObservation>::failure(evidence.error());
    }
    if (!closeStatus) {
        return ::media::Result<CraftedUdpObservation>::failure(closeStatus.error());
    }
    return ::media::Result<CraftedUdpObservation>::success(
        CraftedUdpObservation{std::move(evidence.value())});
}

} // namespace media::ffmpeg::graph::test_fixture
