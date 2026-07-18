#include "unit/fixtures/ScheduledRtpDecodeReceiver.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#endif

namespace media_transcode::test {
namespace {

#if defined(_WIN32)

class WinsockScope final {
public:
    static ::media::Result<WinsockScope> create()
    {
        WSADATA data{};
        const int started = WSAStartup(MAKEWORD(2, 2), &data);
        if (started != 0) {
            return ::media::Result<WinsockScope>::failure(
                ::media::ErrorInfo::ioFailure(
                    "scheduled RTP decode WSAStartup failed", started));
        }
        return ::media::Result<WinsockScope>::success(WinsockScope());
    }

    WinsockScope(WinsockScope&& other) noexcept : m_active(other.m_active)
    {
        other.m_active = false;
    }

    WinsockScope& operator=(WinsockScope&& other) noexcept
    {
        if (this != &other) {
            if (m_active) WSACleanup();
            m_active = other.m_active;
            other.m_active = false;
        }
        return *this;
    }

    WinsockScope(const WinsockScope&) = delete;
    WinsockScope& operator=(const WinsockScope&) = delete;

    ~WinsockScope()
    {
        if (m_active) WSACleanup();
    }

private:
    WinsockScope() = default;
    bool m_active = true;
};

void closeSockets(std::array<SOCKET, 4>& sockets) noexcept
{
    for (SOCKET& handle : sockets) {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
            handle = INVALID_SOCKET;
        }
    }
}

bool bindPortBlock(std::uint16_t base) noexcept
{
    std::array<SOCKET, 4> sockets{
        INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET};
    for (std::size_t index = 0; index < sockets.size(); ++index) {
        sockets[index] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sockets[index] == INVALID_SOCKET) {
            closeSockets(sockets);
            return false;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(
            static_cast<std::uint16_t>(base + index));
        if (bind(
                sockets[index], reinterpret_cast<sockaddr*>(&address),
                sizeof(address)) == SOCKET_ERROR) {
            closeSockets(sockets);
            return false;
        }
    }
    closeSockets(sockets);
    return true;
}

::media::Result<std::set<std::uint16_t>> udpPortsOwnedBy(
    std::uint32_t processId)
{
    ULONG size = 0;
    DWORD queried = GetExtendedUdpTable(
        nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (queried != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return ::media::Result<std::set<std::uint16_t>>::failure(
            ::media::ErrorInfo::ioFailure(
                "GetExtendedUdpTable size query failed",
                static_cast<int>(queried)));
    }
    std::vector<std::uint8_t> storage(size);
    queried = GetExtendedUdpTable(
        storage.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    if (queried != NO_ERROR) {
        return ::media::Result<std::set<std::uint16_t>>::failure(
            ::media::ErrorInfo::ioFailure(
                "GetExtendedUdpTable query failed",
                static_cast<int>(queried)));
    }
    const auto* table = reinterpret_cast<const MIB_UDPTABLE_OWNER_PID*>(
        storage.data());
    std::set<std::uint16_t> ports;
    for (DWORD index = 0; index < table->dwNumEntries; ++index) {
        const auto& row = table->table[index];
        if (row.dwOwningPid == processId) {
            ports.insert(ntohs(static_cast<u_short>(row.dwLocalPort)));
        }
    }
    return ::media::Result<std::set<std::uint16_t>>::success(
        std::move(ports));
}

std::wstring quote(const std::filesystem::path& path)
{
    return L"\"" + path.wstring() + L"\"";
}

HANDLE processHandle(void* handle) noexcept
{
    return reinterpret_cast<HANDLE>(handle);
}

#endif

} // namespace

ScheduledRtpDecodeReceiver::ScheduledRtpDecodeReceiver(
    void* process,
    std::uint32_t processId,
    std::filesystem::path log) noexcept
    : m_process(process),
      m_processId(processId),
      m_log(std::move(log))
{
}

ScheduledRtpDecodeReceiver::ScheduledRtpDecodeReceiver(
    ScheduledRtpDecodeReceiver&& other) noexcept
    : m_process(other.m_process),
      m_processId(other.m_processId),
      m_log(std::move(other.m_log))
{
    other.m_process = nullptr;
    other.m_processId = 0;
}

ScheduledRtpDecodeReceiver& ScheduledRtpDecodeReceiver::operator=(
    ScheduledRtpDecodeReceiver&& other) noexcept
{
    if (this != &other) {
        stop();
        m_process = other.m_process;
        m_processId = other.m_processId;
        m_log = std::move(other.m_log);
        other.m_process = nullptr;
        other.m_processId = 0;
    }
    return *this;
}

ScheduledRtpDecodeReceiver::~ScheduledRtpDecodeReceiver()
{
    stop();
}

::media::Result<std::uint16_t>
ScheduledRtpDecodeReceiver::findAvailableIpv4PortBlock()
{
#if defined(_WIN32)
    auto winsock = WinsockScope::create();
    if (!winsock) {
        return ::media::Result<std::uint16_t>::failure(winsock.error());
    }
    constexpr std::uint16_t minimum = 30'000;
    constexpr std::uint16_t candidateCount = 6'000;
    const std::uint32_t seed = GetCurrentProcessId() * 17u;
    for (std::uint32_t attempt = 0; attempt < candidateCount; ++attempt) {
        const std::uint32_t slot = (seed + attempt) % candidateCount;
        const auto candidate = static_cast<std::uint16_t>(
            minimum + slot * 4u);
        if (candidate <= 65'531 && bindPortBlock(candidate)) {
            return ::media::Result<std::uint16_t>::success(candidate);
        }
    }
    return ::media::Result<std::uint16_t>::failure(
        ::media::ErrorInfo::ioFailure(
            "scheduled RTP decode could not reserve four adjacent UDP ports"));
#else
    return ::media::Result<std::uint16_t>::failure(
        ::media::ErrorInfo::unsupported(
            "scheduled RTP decode receiver readiness is Windows-only"));
#endif
}

::media::Result<ScheduledRtpDecodeReceiver>
ScheduledRtpDecodeReceiver::start(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& sdp,
    const std::filesystem::path& videoFrameMd5,
    const std::filesystem::path& audioFrameMd5,
    const std::filesystem::path& log)
{
#if defined(_WIN32)
    if (!std::filesystem::exists(ffmpeg) ||
        !std::filesystem::exists(sdp)) {
        return ::media::Result<ScheduledRtpDecodeReceiver>::failure(
            ::media::ErrorInfo::unsupported(
                "scheduled RTP decode receiver executable or SDP is unavailable"));
    }
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE logHandle = CreateFileW(
        log.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        return ::media::Result<ScheduledRtpDecodeReceiver>::failure(
            ::media::ErrorInfo::ioFailure(
                "scheduled RTP decode could not create receiver log",
                static_cast<int>(GetLastError())));
    }
    HANDLE nullInput = CreateFileW(
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullInput == INVALID_HANDLE_VALUE) {
        const int error = static_cast<int>(GetLastError());
        CloseHandle(logHandle);
        return ::media::Result<ScheduledRtpDecodeReceiver>::failure(
            ::media::ErrorInfo::ioFailure(
                "scheduled RTP decode could not open null input", error));
    }
    std::wstring command =
        quote(ffmpeg) +
        L" -nostdin -hide_banner -loglevel error -xerror -y" +
        L" -protocol_whitelist file,udp,rtp -i " + quote(sdp) +
        L" -map 0:v:0 -frames:v 3 -f framemd5 " +
        quote(videoFrameMd5) +
        L" -map 0:a:0 -frames:a 8 -f framemd5 " +
        quote(audioFrameMd5);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nullInput;
    startup.hStdOutput = logHandle;
    startup.hStdError = logHandle;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        ffmpeg.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    CloseHandle(nullInput);
    CloseHandle(logHandle);
    if (!created) {
        return ::media::Result<ScheduledRtpDecodeReceiver>::failure(
            ::media::ErrorInfo::ioFailure(
                "scheduled RTP decode could not start FFmpeg receiver",
                static_cast<int>(createError)));
    }
    CloseHandle(process.hThread);
    return ::media::Result<ScheduledRtpDecodeReceiver>::success(
        ScheduledRtpDecodeReceiver(
            process.hProcess, process.dwProcessId, std::move(log)));
#else
    (void)ffmpeg;
    (void)sdp;
    (void)videoFrameMd5;
    (void)audioFrameMd5;
    (void)log;
    return ::media::Result<ScheduledRtpDecodeReceiver>::failure(
        ::media::ErrorInfo::unsupported(
            "scheduled RTP decode child process is Windows-only"));
#endif
}

::media::Status ScheduledRtpDecodeReceiver::waitUntilPortsBound(
    std::span<const std::uint16_t> ports,
    std::chrono::milliseconds timeout)
{
#if defined(_WIN32)
    if (!m_process || m_processId == 0 || ports.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP decode receiver readiness inputs are invalid"));
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const DWORD state = WaitForSingleObject(processHandle(m_process), 0);
        if (state == WAIT_OBJECT_0) {
            DWORD exitCode = 0;
            GetExitCodeProcess(processHandle(m_process), &exitCode);
            return ::media::Status::failure(
                ::media::ErrorInfo::ioFailure(
                    "FFmpeg receiver exited before binding planned RTP ports",
                    static_cast<int>(exitCode)));
        }
        if (state == WAIT_FAILED) {
            return ::media::Status::failure(
                ::media::ErrorInfo::ioFailure(
                    "scheduled RTP decode receiver wait failed",
                    static_cast<int>(GetLastError())));
        }
        auto owned = udpPortsOwnedBy(m_processId);
        if (!owned) return ::media::Status::failure(owned.error());
        const bool complete = std::all_of(
            ports.begin(), ports.end(), [&](std::uint16_t port) {
                return owned.value().contains(port);
            });
        if (complete) return ::media::Status::success();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return ::media::Status::failure(
        ::media::ErrorInfo::ioFailure(
            "FFmpeg receiver did not bind all planned RTP and RTCP ports"));
#else
    (void)ports;
    (void)timeout;
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "scheduled RTP decode receiver readiness is Windows-only"));
#endif
}

::media::Status ScheduledRtpDecodeReceiver::waitForSuccess(
    std::chrono::milliseconds timeout)
{
#if defined(_WIN32)
    if (!m_process) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP decode receiver process is missing"));
    }
    const DWORD waited = WaitForSingleObject(
        processHandle(m_process), static_cast<DWORD>(timeout.count()));
    if (waited == WAIT_TIMEOUT) {
        stop();
        return ::media::Status::failure(
            ::media::ErrorInfo::ioFailure(
                "FFmpeg receiver exceeded the scheduled RTP decode timeout"));
    }
    if (waited != WAIT_OBJECT_0) {
        const int error = static_cast<int>(GetLastError());
        stop();
        return ::media::Status::failure(
            ::media::ErrorInfo::ioFailure(
                "scheduled RTP decode receiver wait failed", error));
    }
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processHandle(m_process), &exitCode)) {
        const int error = static_cast<int>(GetLastError());
        stop();
        return ::media::Status::failure(
            ::media::ErrorInfo::ioFailure(
                "scheduled RTP decode receiver exit query failed", error));
    }
    CloseHandle(processHandle(m_process));
    m_process = nullptr;
    m_processId = 0;
    return exitCode == 0
        ? ::media::Status::success()
        : ::media::Status::failure(
              ::media::ErrorInfo::ioFailure(
                  "FFmpeg receiver failed to decode scheduled RTP",
                  static_cast<int>(exitCode)));
#else
    (void)timeout;
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "scheduled RTP decode child process is Windows-only"));
#endif
}

std::string ScheduledRtpDecodeReceiver::diagnostics() const
{
    std::ifstream input(m_log, std::ios::binary);
    if (!input) return {};
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void ScheduledRtpDecodeReceiver::stop() noexcept
{
#if defined(_WIN32)
    if (!m_process) return;
    HANDLE handle = processHandle(m_process);
    DWORD exitCode = 0;
    if (GetExitCodeProcess(handle, &exitCode) && exitCode == STILL_ACTIVE) {
        TerminateProcess(handle, 1);
        WaitForSingleObject(handle, 2'000);
    }
    CloseHandle(handle);
#endif
    m_process = nullptr;
    m_processId = 0;
}

bool scheduledRtpFrameMd5HasData(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first != std::string::npos && line[first] != '#') return true;
    }
    return false;
}

} // namespace media_transcode::test
