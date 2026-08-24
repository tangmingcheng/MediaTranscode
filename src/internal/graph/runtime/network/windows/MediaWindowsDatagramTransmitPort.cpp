#include "internal/graph/runtime/network/windows/MediaWindowsDatagramTransmitPort.h"

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <mstcpip.h>
#include <ws2tcpip.h>

#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

struct PendingTimestamp final {
    std::uint32_t platformId;
    std::uint64_t evidenceId;
};

::media::Status fillAddress(const MediaUdpDatagramEndpoint& endpoint,
                            sockaddr_storage& storage,
                            int& length) noexcept
{
    std::memset(&storage, 0, sizeof(storage));
    const int family = endpoint.addressFamily() == MediaIpAddressFamily::Ipv4
        ? AF_INET : AF_INET6;
    if (family == AF_INET) {
        auto* value = reinterpret_cast<sockaddr_in*>(&storage);
        value->sin_family = AF_INET;
        value->sin_port = htons(endpoint.port());
        length = sizeof(*value);
        if (InetPtonA(AF_INET, endpoint.numericAddress().c_str(),
                      &value->sin_addr) == 1) {
            return ::media::Status::success();
        }
    } else {
        auto* value = reinterpret_cast<sockaddr_in6*>(&storage);
        value->sin6_family = AF_INET6;
        value->sin6_port = htons(endpoint.port());
        length = sizeof(*value);
        if (InetPtonA(AF_INET6, endpoint.numericAddress().c_str(),
                      &value->sin6_addr) == 1) {
            return ::media::Status::success();
        }
    }
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "invalid numeric Windows Datagram address"));
}

class MediaWindowsDatagramTransmitPort final
    : public MediaDatagramTransmitPort {
public:
    explicit MediaWindowsDatagramTransmitPort(
        std::shared_ptr<MediaSocketRuntime> runtime) noexcept
        : m_runtime(std::move(runtime))
    {
    }

    ~MediaWindowsDatagramTransmitPort() override { close(); }

    ::media::Result<MediaDatagramTransmitPortCapabilities> open(
        const MediaDatagramTransmitPortOpenRequest& request) override
    {
        using ResultType =
            ::media::Result<MediaDatagramTransmitPortCapabilities>;
        if (!m_runtime || m_openAttempted || request.sessionKey.empty() ||
            request.serviceScopeId.empty() || request.generation == 0 ||
            request.endpoint.endpointId == 0 ||
            request.endpoint.socketHardBoundBytes == 0 ||
            request.endpoint.socketHardBoundBytes >
                static_cast<std::uint64_t>((std::numeric_limits<int>::max)()) ||
            request.localEndpoint.addressFamily() !=
                request.endpoint.addressFamily) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Windows Datagram port open request"));
        }
        m_openAttempted = true;
        if (request.executionMode !=
            MediaDatagramTransmitExecutionMode::UserspaceNonblocking) {
            return ResultType::failure(::media::ErrorInfo::unsupported(
                "SO_TXTIME is not a Windows Datagram capability"));
        }
        const int family = request.endpoint.addressFamily ==
                MediaIpAddressFamily::Ipv4 ? AF_INET : AF_INET6;
        SOCKET handle = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
        if (handle == INVALID_SOCKET) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram socket creation failed", WSAGetLastError()));
        }
        const auto fail = [&handle](::media::ErrorInfo error) {
            closesocket(handle);
            return ResultType::failure(std::move(error));
        };
        const BOOL exclusive = TRUE;
        if (setsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                       reinterpret_cast<const char*>(&exclusive),
                       sizeof(exclusive)) == SOCKET_ERROR) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram exclusive bind configuration failed",
                WSAGetLastError()));
        }
        const int requestedBuffer =
            static_cast<int>(request.endpoint.socketHardBoundBytes);
        if (setsockopt(handle, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<const char*>(&requestedBuffer),
                       sizeof(requestedBuffer)) == SOCKET_ERROR) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram SO_SNDBUF configuration failed",
                WSAGetLastError()));
        }
        u_long nonblocking = 1;
        if (ioctlsocket(handle, FIONBIO, &nonblocking) == SOCKET_ERROR) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram nonblocking configuration failed",
                WSAGetLastError()));
        }
        sockaddr_storage local{};
        int localLength = 0;
        auto converted = fillAddress(request.localEndpoint, local, localLength);
        if (!converted) return fail(converted.error());
        if (bind(handle, reinterpret_cast<const sockaddr*>(&local),
                 localLength) == SOCKET_ERROR) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram bind failed", WSAGetLastError()));
        }
        auto remote = MediaUdpDatagramEndpoint::create(
            request.endpoint.addressFamily, request.endpoint.numericAddress,
            request.endpoint.port);
        if (!remote) return fail(remote.error());
        converted = fillAddress(remote.value(), m_remote, m_remoteLength);
        if (!converted) return fail(converted.error());

        DWORD bytes = 0;
        GUID guid = WSAID_WSASENDMSG;
        if (WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER,
                     &guid, sizeof(guid), &m_sendMsg, sizeof(m_sendMsg),
                     &bytes, nullptr, nullptr) == SOCKET_ERROR) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram WSASendMsg capability lookup failed",
                WSAGetLastError()));
        }
        MediaDatagramTransmitTimestampAvailability timestampAvailability =
            MediaDatagramTransmitTimestampAvailability::NotRequested;
        if (request.evidence) {
            timestampAvailability =
                MediaDatagramTransmitTimestampAvailability::Unavailable;
            if (request.evidence->maximumCorrelationEntries <=
                static_cast<std::uint64_t>(
                    (std::numeric_limits<USHORT>::max)())) {
                TIMESTAMPING_CONFIG config{};
                config.Flags = TIMESTAMPING_FLAG_TX;
                config.TxTimestampsBuffered = static_cast<USHORT>(
                    request.evidence->maximumCorrelationEntries);
                if (WSAIoctl(handle, SIO_TIMESTAMPING, &config, sizeof(config),
                             nullptr, 0, &bytes, nullptr, nullptr) == 0) {
                    try {
                        m_pending.reserve(static_cast<std::size_t>(
                            request.evidence->maximumCorrelationEntries));
                        timestampAvailability =
                            MediaDatagramTransmitTimestampAvailability::Available;
                    } catch (const std::bad_alloc&) {
                        return fail(::media::ErrorInfo::allocationFailed(
                            "Windows transmit timestamp correlation"));
                    }
                }
            }
        }
        int effectiveBuffer = 0;
        int optionLength = sizeof(effectiveBuffer);
        if (getsockopt(handle, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<char*>(&effectiveBuffer),
                       &optionLength) == SOCKET_ERROR || effectiveBuffer <= 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram effective SO_SNDBUF query failed",
                WSAGetLastError()));
        }
        m_socket = handle;
        m_endpointId = request.endpoint.endpointId;
        m_generation = request.generation;
        m_maximumDatagramBytes = request.endpoint.maximumDatagramBytes;
        m_timestampAvailable = timestampAvailability ==
            MediaDatagramTransmitTimestampAvailability::Available;
        return ResultType::success(MediaDatagramTransmitPortCapabilities{
            request.endpoint.socketHardBoundBytes,
            static_cast<std::uint64_t>(effectiveBuffer),
            timestampAvailability, false, false});
    }

    ::media::Result<MediaDatagramTransmitAttempt> trySubmit(
        std::span<const MediaDatagramTransmitRequest> requests) override
    {
        using ResultType = ::media::Result<MediaDatagramTransmitAttempt>;
        if (m_socket == INVALID_SOCKET || requests.empty()) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Windows Datagram submit request"));
        }
        if (m_timestampAvailable &&
            requests.size() > m_pending.capacity() - m_pending.size()) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows transmit timestamp correlation cannot admit batch"));
        }
        for (const auto& request : requests) {
            if (request.bytes.empty() ||
                request.bytes.size() > m_maximumDatagramBytes ||
                request.bytes.size() > static_cast<std::size_t>(
                    (std::numeric_limits<ULONG>::max)()) ||
                request.kernelTransmitTimeNanoseconds) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "invalid Windows Datagram payload or launch time"));
            }
        }
        std::size_t submitted = 0;
        for (const auto& request : requests) {
            WSABUF buffer{static_cast<ULONG>(request.bytes.size()),
                          reinterpret_cast<char*>(
                              const_cast<std::uint8_t*>(request.bytes.data()))};
            std::array<char, WSA_CMSG_SPACE(sizeof(UINT32))> control{};
            WSAMSG message{};
            message.name = reinterpret_cast<sockaddr*>(&m_remote);
            message.namelen = m_remoteLength;
            message.lpBuffers = &buffer;
            message.dwBufferCount = 1;
            std::uint32_t platformId = 0;
            if (m_timestampAvailable) {
                platformId = m_nextTimestampId++;
                message.Control.buf = control.data();
                message.Control.len = static_cast<ULONG>(control.size());
                auto* header = WSA_CMSG_FIRSTHDR(&message);
                header->cmsg_level = SOL_SOCKET;
                header->cmsg_type = SO_TIMESTAMP_ID;
                header->cmsg_len = WSA_CMSG_LEN(sizeof(UINT32));
                std::memcpy(WSA_CMSG_DATA(header), &platformId,
                            sizeof(platformId));
                m_pending.push_back(
                    PendingTimestamp{platformId, request.evidenceId});
            }
            DWORD accepted = 0;
            if (m_sendMsg(m_socket, &message, 0, &accepted,
                          nullptr, nullptr) == SOCKET_ERROR) {
                const int native = WSAGetLastError();
                if (m_timestampAvailable) m_pending.pop_back();
                if ((native == WSAEWOULDBLOCK || native == WSAENOBUFS) &&
                    submitted == 0) {
                    return ResultType::success(
                        MediaDatagramTransmitAttempt::WouldBlock);
                }
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    submitted == 0
                        ? "Windows Datagram submit failed"
                        : "Windows Datagram batch delivery is ambiguous",
                    native));
            }
            if (accepted != request.bytes.size()) {
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Windows Datagram short or partial batch submit"));
            }
            ++submitted;
        }
        return ResultType::success(MediaDatagramTransmitAttempt::Submitted);
    }

    ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        MediaRunningTime maximumWait) override
    {
        using ResultType =
            ::media::Result<MediaDatagramWritableWaitResult>;
        if (m_socket == INVALID_SOCKET || maximumWait.nanoseconds() < 0) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Windows Datagram writable wait"));
        }
        const std::uint64_t milliseconds = static_cast<std::uint64_t>(
            maximumWait.nanoseconds() / 1'000'000) +
            (maximumWait.nanoseconds() % 1'000'000 != 0 ? 1 : 0);
        if (milliseconds > static_cast<std::uint64_t>(
            (std::numeric_limits<int>::max)())) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "Windows Datagram writable wait exceeds platform range"));
        }
        WSAPOLLFD descriptor{m_socket, POLLWRNORM, 0};
        const int result = WSAPoll(&descriptor, 1,
                                   static_cast<int>(milliseconds));
        if (result == 0) {
            return ResultType::success(
                MediaDatagramWritableWaitResult::TimedOut);
        }
        if (result == SOCKET_ERROR) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram writable wait failed", WSAGetLastError()));
        }
        if ((descriptor.revents & POLLWRNORM) == 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram writable wait returned terminal events"));
        }
        return ResultType::success(MediaDatagramWritableWaitResult::Writable);
    }

    ::media::Result<std::vector<MediaDatagramTransmitEvidence>>
    drainAvailableEvidence() override
    {
        using ResultType =
            ::media::Result<std::vector<MediaDatagramTransmitEvidence>>;
        std::vector<MediaDatagramTransmitEvidence> result;
        if (!m_timestampAvailable) return ResultType::success(std::move(result));
        try {
            result.reserve(m_pending.size());
        } catch (const std::bad_alloc&) {
            return ResultType::failure(::media::ErrorInfo::allocationFailed(
                "Windows transmit evidence drain"));
        }
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            UINT32 id = it->platformId;
            UINT64 timestamp = 0;
            DWORD bytes = 0;
            if (WSAIoctl(m_socket, SIO_GET_TX_TIMESTAMP,
                         &id, sizeof(id), &timestamp, sizeof(timestamp),
                         &bytes, nullptr, nullptr) == SOCKET_ERROR) {
                const int native = WSAGetLastError();
                if (native == WSAEWOULDBLOCK) {
                    ++it;
                    continue;
                }
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Windows transmit timestamp drain failed", native));
            }
            result.push_back(MediaDatagramTransmitEvidence{
                m_endpointId, m_generation, it->evidenceId, timestamp});
            it = m_pending.erase(it);
        }
        return ResultType::success(std::move(result));
    }

    ::media::Status close() noexcept override
    {
        if (m_socket == INVALID_SOCKET) return ::media::Status::success();
        const SOCKET handle = m_socket;
        m_socket = INVALID_SOCKET;
        m_pending.clear();
        if (closesocket(handle) == SOCKET_ERROR) {
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram close failed", WSAGetLastError()));
        }
        return ::media::Status::success();
    }

private:
    std::shared_ptr<MediaSocketRuntime> m_runtime;
    SOCKET m_socket = INVALID_SOCKET;
    LPFN_WSASENDMSG m_sendMsg = nullptr;
    sockaddr_storage m_remote{};
    int m_remoteLength = 0;
    std::uint64_t m_endpointId = 0;
    std::uint64_t m_generation = 0;
    std::size_t m_maximumDatagramBytes = 0;
    std::uint32_t m_nextTimestampId = 0;
    std::vector<PendingTimestamp> m_pending;
    bool m_timestampAvailable = false;
    bool m_openAttempted = false;
};

} // namespace

MediaWindowsDatagramTransmitPortFactory::
MediaWindowsDatagramTransmitPortFactory(
    std::shared_ptr<MediaSocketRuntime> runtime) noexcept
    : m_runtime(std::move(runtime))
{
}

::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>
MediaWindowsDatagramTransmitPortFactory::create()
{
    if (!m_runtime) {
        return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Windows Datagram factory requires socket runtime"));
    }
    try {
        std::unique_ptr<MediaDatagramTransmitPort> port =
            std::make_unique<MediaWindowsDatagramTransmitPort>(m_runtime);
        return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::success(
            std::move(port));
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaWindowsDatagramTransmitPort"));
    }
}

} // namespace media::ffmpeg::graph
#else
namespace media::ffmpeg::graph {
MediaWindowsDatagramTransmitPortFactory::
MediaWindowsDatagramTransmitPortFactory(
    std::shared_ptr<MediaSocketRuntime> runtime) noexcept
    : m_runtime(std::move(runtime))
{
}
::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>
MediaWindowsDatagramTransmitPortFactory::create()
{
    return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::failure(
        ::media::ErrorInfo::unsupported(
            "Windows Datagram adapter is unavailable on this platform"));
}
} // namespace media::ffmpeg::graph
#endif
