#include "unit/fixtures/AvSyncProductionRuntimeIntegrationSupport.h"

#include <array>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

namespace media_transcode::test {
namespace {

::media::ErrorInfo supportError(std::string message, int code = 0)
{
    return ::media::ErrorInfo::ioFailure(std::move(message), code);
}

#if defined(_WIN32)

constexpr std::uintptr_t InvalidSocket =
    static_cast<std::uintptr_t>(INVALID_SOCKET);

SOCKET nativeSocket(std::uintptr_t value) noexcept
{
    return static_cast<SOCKET>(value);
}

::media::Status startWinsock()
{
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    return result == 0
        ? ::media::Status::success()
        : ::media::Status::failure(
              supportError("production integration WSAStartup failed", result));
}

std::wstring quote(const std::filesystem::path& path)
{
    return L"\"" + path.wstring() + L"\"";
}

::media::Result<void*> launchProcess(
    const std::filesystem::path& executable,
    std::wstring command)
{
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &process)) {
        return ::media::Result<void*>::failure(
            supportError("production fixture process failed",
                         static_cast<int>(GetLastError())));
    }
    CloseHandle(process.hThread);
    return ::media::Result<void*>::success(process.hProcess);
}

struct ProcessHandleCloser final {
    void operator()(void* process) const noexcept
    {
        if (process) CloseHandle(reinterpret_cast<HANDLE>(process));
    }
};

#endif

} // namespace

PreparedAvFixture::PreparedAvFixture(std::filesystem::path path)
    : m_path(std::move(path))
{
}

PreparedAvFixture::PreparedAvFixture(PreparedAvFixture&& other) noexcept
    : m_path(std::move(other.m_path))
{
    other.m_path.clear();
}

PreparedAvFixture& PreparedAvFixture::operator=(
    PreparedAvFixture&& other) noexcept
{
    if (this != &other) {
        remove();
        m_path = std::move(other.m_path);
        other.m_path.clear();
    }
    return *this;
}

PreparedAvFixture::~PreparedAvFixture()
{
    remove();
}

::media::Result<PreparedAvFixture> PreparedAvFixture::create(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& source,
    int videoWidth,
    int videoHeight)
{
#if defined(_WIN32)
    if (videoWidth <= 0 || videoHeight <= 0 ||
        !std::filesystem::is_regular_file(ffmpeg) ||
        !std::filesystem::is_regular_file(source)) {
        return ::media::Result<PreparedAvFixture>::failure(
            ::media::ErrorInfo::invalidArgument(
                "production fixture requires source, executable, and dimensions"));
    }
    const auto output = std::filesystem::temp_directory_path() /
        ("av_sync_production_input_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()) +
         ".mp4");
    std::wstring command =
        quote(ffmpeg) +
        L" -nostdin -hide_banner -loglevel error -i " + quote(source) +
        L" -map 0:v:0 -vf \"scale=" + std::to_wstring(videoWidth) + L":" +
        std::to_wstring(videoHeight) +
        L":flags=fast_bilinear\" -c:v libx264 -preset ultrafast "
        L"-tune zerolatency -profile:v baseline -level:v 3.0 -g 30 "
        L"-keyint_min 30 -sc_threshold 0 -map 0:a:0 -c:a copy "
        L"-movflags +faststart -y " + quote(output);
    auto launched = launchProcess(ffmpeg, std::move(command));
    if (!launched) {
        return ::media::Result<PreparedAvFixture>::failure(launched.error());
    }
    std::unique_ptr<void, ProcessHandleCloser> process(launched.value());
    const DWORD wait = WaitForSingleObject(
        reinterpret_cast<HANDLE>(process.get()), 60'000);
    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(reinterpret_cast<HANDLE>(process.get()), 1);
        WaitForSingleObject(reinterpret_cast<HANDLE>(process.get()), 2'000);
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
        return ::media::Result<PreparedAvFixture>::failure(
            supportError("production fixture preparation timed out",
                         static_cast<int>(wait)));
    }
    DWORD exitCode = 0;
    if (!GetExitCodeProcess(reinterpret_cast<HANDLE>(process.get()),
                            &exitCode) ||
        exitCode != 0 || !std::filesystem::is_regular_file(output)) {
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
        return ::media::Result<PreparedAvFixture>::failure(
            supportError("production fixture preparation failed",
                         static_cast<int>(exitCode)));
    }
    return ::media::Result<PreparedAvFixture>::success(
        PreparedAvFixture(output));
#else
    (void)ffmpeg;
    (void)source;
    (void)videoWidth;
    (void)videoHeight;
    return ::media::Result<PreparedAvFixture>::failure(
        ::media::ErrorInfo::unsupported(
            "production fixture preparation is Windows-only"));
#endif
}

void PreparedAvFixture::remove() noexcept
{
    if (!m_path.empty()) {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
        m_path.clear();
    }
}

UdpDatagramReceiver::UdpDatagramReceiver(std::uintptr_t socket) noexcept
    : m_socket(socket)
{
}

UdpDatagramReceiver::UdpDatagramReceiver(
    UdpDatagramReceiver&& other) noexcept
    : m_socket(other.m_socket)
{
    other.m_socket = 0;
}

UdpDatagramReceiver& UdpDatagramReceiver::operator=(
    UdpDatagramReceiver&& other) noexcept
{
    if (this != &other) {
        close();
        m_socket = other.m_socket;
        other.m_socket = 0;
    }
    return *this;
}

UdpDatagramReceiver::~UdpDatagramReceiver()
{
    close();
}

::media::Result<UdpDatagramReceiver>
UdpDatagramReceiver::bindLoopback(std::uint16_t port)
{
#if defined(_WIN32)
    if (port == 0) {
        return ::media::Result<UdpDatagramReceiver>::failure(
            ::media::ErrorInfo::invalidArgument(
                "production integration receiver requires a nonzero port"));
    }
    if (auto started = startWinsock(); !started) {
        return ::media::Result<UdpDatagramReceiver>::failure(started.error());
    }
    SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        WSACleanup();
        return ::media::Result<UdpDatagramReceiver>::failure(
            supportError("production integration receiver socket failed", error));
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::bind(socket, reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        closesocket(socket);
        WSACleanup();
        return ::media::Result<UdpDatagramReceiver>::failure(
            supportError("production integration receiver bind failed", error));
    }
    return ::media::Result<UdpDatagramReceiver>::success(
        UdpDatagramReceiver(static_cast<std::uintptr_t>(socket)));
#else
    (void)port;
    return ::media::Result<UdpDatagramReceiver>::failure(
        ::media::ErrorInfo::unsupported(
            "production integration UDP receiver is Windows-only"));
#endif
}

::media::Result<std::size_t> UdpDatagramReceiver::receiveBytes(
    std::chrono::milliseconds timeout)
{
#if defined(_WIN32)
    if (m_socket == 0 || m_socket == InvalidSocket || timeout.count() <= 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "production integration receive inputs are invalid"));
    }
    const DWORD milliseconds = static_cast<DWORD>(timeout.count());
    if (setsockopt(nativeSocket(m_socket), SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&milliseconds),
                   sizeof(milliseconds)) == SOCKET_ERROR) {
        return ::media::Result<std::size_t>::failure(
            supportError("production integration receive timeout failed",
                         WSAGetLastError()));
    }
    std::array<char, 65'536> buffer{};
    const int received = recvfrom(nativeSocket(m_socket), buffer.data(),
                                  static_cast<int>(buffer.size()), 0,
                                  nullptr, nullptr);
    if (received <= 0) {
        return ::media::Result<std::size_t>::failure(
            supportError("production integration received no UDP payload",
                         WSAGetLastError()));
    }
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(received));
#else
    (void)timeout;
    return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::unsupported(
            "production integration UDP receiver is Windows-only"));
#endif
}

void UdpDatagramReceiver::close() noexcept
{
#if defined(_WIN32)
    if (m_socket != 0 && m_socket != InvalidSocket) {
        closesocket(nativeSocket(m_socket));
        WSACleanup();
    }
#endif
    m_socket = 0;
}

FfmpegRealtimeFeeder::FfmpegRealtimeFeeder(void* process) noexcept
    : m_process(process)
{
}

FfmpegRealtimeFeeder::FfmpegRealtimeFeeder(
    FfmpegRealtimeFeeder&& other) noexcept
    : m_process(other.m_process)
{
    other.m_process = nullptr;
}

FfmpegRealtimeFeeder& FfmpegRealtimeFeeder::operator=(
    FfmpegRealtimeFeeder&& other) noexcept
{
    if (this != &other) {
        stop();
        m_process = other.m_process;
        other.m_process = nullptr;
    }
    return *this;
}

FfmpegRealtimeFeeder::~FfmpegRealtimeFeeder()
{
    stop();
}

::media::Result<FfmpegRealtimeFeeder>
FfmpegRealtimeFeeder::startMpegTs(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& sample,
    std::uint16_t destinationPort)
{
#if defined(_WIN32)
    if (destinationPort == 0 || !std::filesystem::is_regular_file(ffmpeg) ||
        !std::filesystem::is_regular_file(sample)) {
        return ::media::Result<FfmpegRealtimeFeeder>::failure(
            ::media::ErrorInfo::unsupported(
                "production TS feeder prerequisites are unavailable"));
    }
    const std::wstring output =
        L"udp://127.0.0.1:" + std::to_wstring(destinationPort) +
        L"?pkt_size=1316";
    std::wstring command =
        quote(ffmpeg) +
        L" -nostdin -hide_banner -loglevel error -re -stream_loop -1 -i " +
        quote(sample) +
        L" -map 0:v:0 -map 0:a:0 -c copy -f mpegts \"" + output + L"\"";
    auto feeder = startProcess(ffmpeg, std::move(command));
    if (feeder) std::this_thread::sleep_for(std::chrono::milliseconds(1'250));
    return feeder;
#else
    (void)ffmpeg;
    (void)sample;
    (void)destinationPort;
    return ::media::Result<FfmpegRealtimeFeeder>::failure(
        ::media::ErrorInfo::unsupported(
            "production TS feeder is Windows-only"));
#endif
}

::media::Result<FfmpegRealtimeFeeder>
FfmpegRealtimeFeeder::startSeparateRtp(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& sample,
    std::uint16_t videoRtpPort,
    std::uint16_t audioRtpPort,
    std::string cname)
{
#if defined(_WIN32)
    if (videoRtpPort == 0 || videoRtpPort == 65'535 ||
        audioRtpPort == 0 || audioRtpPort == 65'535 || cname.empty() ||
        !std::filesystem::is_regular_file(ffmpeg) ||
        !std::filesystem::is_regular_file(sample)) {
        return ::media::Result<FfmpegRealtimeFeeder>::failure(
            ::media::ErrorInfo::invalidArgument(
                "production RTP feeder requires executable, sample, ports, and CNAME"));
    }
    const std::wstring videoUrl =
        L"rtp://127.0.0.1:" + std::to_wstring(videoRtpPort) +
        L"?rtcpport=" + std::to_wstring(videoRtpPort + 1) +
        L"&pkt_size=1200";
    const std::wstring audioUrl =
        L"rtp://127.0.0.1:" + std::to_wstring(audioRtpPort) +
        L"?rtcpport=" + std::to_wstring(audioRtpPort + 1) +
        L"&pkt_size=1200";
    const std::wstring wideCname(cname.begin(), cname.end());
    std::wstring command =
        quote(ffmpeg) +
        L" -nostdin -hide_banner -loglevel error -re -stream_loop -1 -i " +
        quote(sample) +
        L" -map 0:v:0 -c:v copy -an -f rtp -payload_type 96 -cname \"" +
        wideCname + L"\" \"" + videoUrl + L"\"" +
        L" -map 0:a:0 -c:a aac -ar 44100 -ac 2 -vn -f rtp "
        L"-payload_type 97 -cname \"" + wideCname + L"\" \"" +
        audioUrl + L"\"";
    auto feeder = startProcess(ffmpeg, std::move(command));
    if (feeder) std::this_thread::sleep_for(std::chrono::milliseconds(1'250));
    return feeder;
#else
    (void)ffmpeg;
    (void)sample;
    (void)videoRtpPort;
    (void)audioRtpPort;
    (void)cname;
    return ::media::Result<FfmpegRealtimeFeeder>::failure(
        ::media::ErrorInfo::unsupported(
            "production RTP feeder is Windows-only"));
#endif
}

::media::Result<FfmpegRealtimeFeeder>
FfmpegRealtimeFeeder::startProcess(
    const std::filesystem::path& executable,
    std::wstring command)
{
#if defined(_WIN32)
    auto launched = launchProcess(executable, std::move(command));
    if (!launched) {
        return ::media::Result<FfmpegRealtimeFeeder>::failure(
            launched.error());
    }
    HANDLE process = reinterpret_cast<HANDLE>(launched.value());
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        CloseHandle(process);
        return ::media::Result<FfmpegRealtimeFeeder>::failure(
            supportError("production feeder exited before runtime input",
                         static_cast<int>(code)));
    }
    return ::media::Result<FfmpegRealtimeFeeder>::success(
        FfmpegRealtimeFeeder(process));
#else
    (void)executable;
    (void)command;
    return ::media::Result<FfmpegRealtimeFeeder>::failure(
        ::media::ErrorInfo::unsupported(
            "production feeder process is Windows-only"));
#endif
}

void FfmpegRealtimeFeeder::stop() noexcept
{
#if defined(_WIN32)
    if (m_process) {
        HANDLE process = reinterpret_cast<HANDLE>(m_process);
        DWORD code = 0;
        if (GetExitCodeProcess(process, &code) && code == STILL_ACTIVE) {
            TerminateProcess(process, 0);
            WaitForSingleObject(process, 2'000);
        }
        CloseHandle(process);
    }
#endif
    m_process = nullptr;
}

::media::Result<std::uint16_t> findAvailableLoopbackUdpPort()
{
#if defined(_WIN32)
    if (auto started = startWinsock(); !started) {
        return ::media::Result<std::uint16_t>::failure(started.error());
    }
    SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        WSACleanup();
        return ::media::Result<std::uint16_t>::failure(
            supportError("production integration port socket failed", error));
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(socket, reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        closesocket(socket);
        WSACleanup();
        return ::media::Result<std::uint16_t>::failure(
            supportError("production integration port bind failed", error));
    }
    int length = sizeof(address);
    const bool found = getsockname(
        socket, reinterpret_cast<sockaddr*>(&address), &length) == 0;
    const std::uint16_t port = found ? ntohs(address.sin_port) : 0;
    closesocket(socket);
    WSACleanup();
    return port != 0
        ? ::media::Result<std::uint16_t>::success(port)
        : ::media::Result<std::uint16_t>::failure(
              supportError("production integration could not select a port",
                           WSAGetLastError()));
#else
    return ::media::Result<std::uint16_t>::failure(
        ::media::ErrorInfo::unsupported(
            "production integration port selection is Windows-only"));
#endif
}

} // namespace media_transcode::test
