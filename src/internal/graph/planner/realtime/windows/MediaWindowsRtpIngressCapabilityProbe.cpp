#include "internal/graph/planner/realtime/MediaRtpIngressPlatformCapabilityProbe.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class WinsockSession final {
public:
    WinsockSession() noexcept
    {
        m_error = WSAStartup(MAKEWORD(2, 2), &m_data);
    }

    ~WinsockSession()
    {
        if (m_error == 0) WSACleanup();
    }

    int error() const noexcept { return m_error; }

private:
    WSADATA m_data{};
    int m_error = 0;
};

class SocketHandle final {
public:
    explicit SocketHandle(SOCKET value = INVALID_SOCKET) noexcept
        : m_value(value)
    {
    }

    ~SocketHandle()
    {
        if (m_value != INVALID_SOCKET) closesocket(m_value);
    }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    SOCKET get() const noexcept { return m_value; }
    void close() noexcept
    {
        if (m_value == INVALID_SOCKET) return;
        closesocket(m_value);
        m_value = INVALID_SOCKET;
    }

private:
    SOCKET m_value;
};

::media::Status bindLoopback(SOCKET socketHandle)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socketHandle, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP ingress capability probe bind failed", WSAGetLastError()));
    }
    return ::media::Status::success();
}

MediaRtpIngressAdapterAvailability unavailable(
    MediaRtpIngressAdapterKind kind,
    std::string operation,
    int error)
{
    return {kind, false, std::move(operation) + " failed with Winsock error " +
        std::to_string(error)};
}

MediaRtpIngressAdapterAvailability probeRegisteredIo()
{
    constexpr auto kind = MediaRtpIngressAdapterKind::WindowsRegisteredIo;
    SocketHandle socketHandle(WSASocketW(
        AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0,
        WSA_FLAG_REGISTERED_IO | WSA_FLAG_OVERLAPPED));
    if (socketHandle.get() == INVALID_SOCKET) {
        return unavailable(kind, "registered UDP socket creation",
                           WSAGetLastError());
    }
    if (auto status = bindLoopback(socketHandle.get()); !status) {
        return unavailable(kind, "registered UDP socket bind",
                           status.error().nativeCode);
    }
    GUID id = WSAID_MULTIPLE_RIO;
    RIO_EXTENSION_FUNCTION_TABLE table{};
    DWORD returnedBytes = 0;
    if (WSAIoctl(socketHandle.get(),
                 SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,
                 &id, sizeof(id), &table, sizeof(table), &returnedBytes,
                 nullptr, nullptr) == SOCKET_ERROR) {
        return unavailable(kind, "RIO extension table query",
                           WSAGetLastError());
    }
    if (returnedBytes != sizeof(table)) {
        return {kind, false,
                "RIO extension table query returned " +
                std::to_string(returnedBytes) + " bytes instead of " +
                std::to_string(sizeof(table))};
    }
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    if (systemInfo.dwPageSize == 0) {
        return {kind, false, "Windows reported zero memory page size"};
    }
    char* storage = static_cast<char*>(VirtualAlloc(
        nullptr, systemInfo.dwPageSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!storage) {
        return {kind, false, "RIO probe VirtualAlloc failed with Windows error " +
            std::to_string(GetLastError())};
    }
    const RIO_BUFFERID buffer = table.RIORegisterBuffer(
        storage, systemInfo.dwPageSize);
    const RIO_CQ completionQueue =
        buffer == RIO_INVALID_BUFFERID
        ? RIO_INVALID_CQ
        : table.RIOCreateCompletionQueue(1, nullptr);
    const RIO_RQ requestQueue =
        completionQueue == RIO_INVALID_CQ
        ? RIO_INVALID_RQ
        : table.RIOCreateRequestQueue(
              socketHandle.get(), 1, 1, 0, 1,
              completionQueue, completionQueue, nullptr);
    const int error = WSAGetLastError();
    socketHandle.close();
    if (completionQueue != RIO_INVALID_CQ) {
        table.RIOCloseCompletionQueue(completionQueue);
    }
    if (buffer != RIO_INVALID_BUFFERID) {
        table.RIODeregisterBuffer(buffer);
    }
    VirtualFree(storage, 0, MEM_RELEASE);
    if (buffer == RIO_INVALID_BUFFERID ||
        completionQueue == RIO_INVALID_CQ ||
        requestQueue == RIO_INVALID_RQ) {
        return unavailable(kind, "RIO registered storage or queue initialization",
                           error);
    }
    return {kind, true, {}};
}

MediaRtpIngressAdapterAvailability probeOverlappedCompletionQueue()
{
    constexpr auto kind =
        MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue;
    SocketHandle socketHandle(WSASocketW(
        AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0,
        WSA_FLAG_OVERLAPPED));
    if (socketHandle.get() == INVALID_SOCKET) {
        return unavailable(kind, "overlapped UDP socket creation",
                           WSAGetLastError());
    }
    if (auto status = bindLoopback(socketHandle.get()); !status) {
        return unavailable(kind, "overlapped UDP socket bind",
                           status.error().nativeCode);
    }
    HANDLE completionPort = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(socketHandle.get()), nullptr, 0, 1);
    if (!completionPort) {
        return {kind, false, "IOCP association failed with Windows error " +
            std::to_string(GetLastError())};
    }
    CloseHandle(completionPort);
    return {kind, true, {}};
}

} // namespace

::media::Result<std::vector<MediaRtpIngressAdapterAvailability>>
MediaRtpIngressPlatformCapabilityProbe::scan()
{
    WinsockSession session;
    if (session.error() != 0) {
        return ::media::Result<
            std::vector<MediaRtpIngressAdapterAvailability>>::failure(
                ::media::ErrorInfo::ioFailure(
                    "RTP ingress Winsock initialization failed",
                    session.error()));
    }
    std::vector<MediaRtpIngressAdapterAvailability> candidates;
    candidates.reserve(2);
    candidates.push_back(probeRegisteredIo());
    candidates.push_back(probeOverlappedCompletionQueue());
    return ::media::Result<
        std::vector<MediaRtpIngressAdapterAvailability>>::success(
            std::move(candidates));
}

} // namespace media::ffmpeg::graph

#endif
