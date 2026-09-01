#include "internal/graph/runtime/network/windows/MediaWindowsDatagramTransmitPort.h"

#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <mstcpip.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <thread>
#include <utility>

namespace media::ffmpeg::graph {

bool detail::mediaWindowsDatagramSubmitWouldBlock(int nativeError) noexcept
{
    return nativeError == WSAEWOULDBLOCK;
}

namespace {

struct WindowsTimestampClockEvidence final {
    MediaDatagramTransmitTimestampSource source;
    std::uint64_t frequency;
};

std::optional<WindowsTimestampClockEvidence> queryTimestampClockEvidence(
    const sockaddr_storage& remote) noexcept
{
    HMODULE library = LoadLibraryExW(
        L"iphlpapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return std::nullopt;
    const auto release = [&]() noexcept { FreeLibrary(library); };
    using GetBestInterfaceExFunction = DWORD(WINAPI*)(const sockaddr*, PULONG);
    using ConvertInterfaceIndexToLuidFunction =
        NETIO_STATUS(WINAPI*)(NET_IFINDEX, PNET_LUID);
    using GetActiveCapabilitiesFunction = DWORD(WINAPI*)(
        const NET_LUID*, PINTERFACE_TIMESTAMP_CAPABILITIES);
    const auto getBestInterface = reinterpret_cast<GetBestInterfaceExFunction>(
        GetProcAddress(library, "GetBestInterfaceEx"));
    const auto convertIndex =
        reinterpret_cast<ConvertInterfaceIndexToLuidFunction>(
            GetProcAddress(library, "ConvertInterfaceIndexToLuid"));
    const auto getActive = reinterpret_cast<GetActiveCapabilitiesFunction>(
        GetProcAddress(library, "GetInterfaceActiveTimestampCapabilities"));
    if (!getBestInterface || !convertIndex || !getActive) {
        release();
        return std::nullopt;
    }
    ULONG index = 0;
    NET_LUID luid{};
    INTERFACE_TIMESTAMP_CAPABILITIES capabilities{};
    if (getBestInterface(reinterpret_cast<const sockaddr*>(&remote), &index) !=
            NO_ERROR ||
        convertIndex(index, &luid) != NO_ERROR ||
        getActive(&luid, &capabilities) != NO_ERROR) {
        release();
        return std::nullopt;
    }
    release();
    const bool hardware = capabilities.HardwareCapabilities.TaggedTransmit ||
                          capabilities.HardwareCapabilities.AllTransmit;
    const bool software = capabilities.SoftwareCapabilities.TaggedTransmit ||
                          capabilities.SoftwareCapabilities.AllTransmit;
    if (hardware == software) return std::nullopt;
    if (hardware) {
        if (capabilities.HardwareClockFrequencyHz == 0) return std::nullopt;
        return WindowsTimestampClockEvidence{
            MediaDatagramTransmitTimestampSource::WindowsHardwareCounter,
            capabilities.HardwareClockFrequencyHz};
    }
    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0) {
        return std::nullopt;
    }
    return WindowsTimestampClockEvidence{
        MediaDatagramTransmitTimestampSource::WindowsPerformanceCounter,
        static_cast<std::uint64_t>(frequency.QuadPart)};
}

::media::Status fillAddress(const MediaUdpDatagramEndpoint& endpoint,
                            sockaddr_storage& storage,
                            int& length) noexcept
{
    std::memset(&storage, 0, sizeof(storage));
    if (endpoint.addressFamily() == MediaIpAddressFamily::Ipv4) {
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

MediaDatagramTransmitSubmitResult submitFailure(
    const char* message,
    int nativeCode,
    MediaDatagramTransmitFailureKind kind,
    std::uint64_t prefix)
{
    return MediaDatagramTransmitSubmitResult::failure(
        mediaDatagramTransmitError(
            ::media::ErrorInfo::ioFailure(message, nativeCode), kind, prefix));
}

class MediaWindowsDatagramTransmitPort final
    : public MediaDatagramTransmitPort {
public:
    explicit MediaWindowsDatagramTransmitPort(
        std::shared_ptr<MediaSocketRuntime> runtime) noexcept
        : m_runtime(std::move(runtime))
    {
    }

    ~MediaWindowsDatagramTransmitPort() override { forceCloseForDestruction(); }

    ::media::Result<MediaDatagramTransmitPortCapabilities> open(
        const MediaDatagramTransmitPortOpenRequest& request) override
    {
        using ResultType =
            ::media::Result<MediaDatagramTransmitPortCapabilities>;
        if (!m_runtime || m_openAttempted || request.sessionKey.empty() ||
            request.serviceScopeId.empty() || request.generation == 0 ||
            request.endpoint.endpointId == 0 ||
            request.endpoint.socketBuffer.accounting !=
                MediaDatagramSocketBufferAccounting::Exact ||
            request.endpoint.socketBuffer.apiRequestedBytes == 0 ||
            request.maximumBatchDatagrams == 0 ||
            request.endpoint.socketBuffer.apiRequestedBytes >
                static_cast<std::uint64_t>((std::numeric_limits<int>::max)()) ||
            request.localEndpoint.addressFamily() !=
                request.endpoint.addressFamily ||
            request.executionMode !=
                MediaDatagramTransmitExecutionMode::UserspaceNonblocking ||
            request.kernelSchedule ||
            request.kernelSocketPacingRateBytesPerSecond ||
            (request.evidence && request.evidence->lastEvidenceId >
                (std::numeric_limits<std::uint32_t>::max)())) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Windows Datagram port open request"));
        }
        m_openAttempted = true;
        m_ownerThread = std::this_thread::get_id();
        m_hasOwner = true;
        const int family = request.endpoint.addressFamily ==
                MediaIpAddressFamily::Ipv4 ? AF_INET : AF_INET6;
        SOCKET handle = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
        if (handle == INVALID_SOCKET) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram socket creation failed", WSAGetLastError()));
        }
        WSAEVENT socketEvent = WSA_INVALID_EVENT;
        HANDLE stopEvent = nullptr;
        const auto fail = [&](::media::ErrorInfo error) {
            if (socketEvent != WSA_INVALID_EVENT) WSACloseEvent(socketEvent);
            if (stopEvent) CloseHandle(stopEvent);
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
        const DWORD pathMtuDiscovery = IP_PMTUDISC_DO;
        const int pathMtuLevel = family == AF_INET
            ? IPPROTO_IP
            : IPPROTO_IPV6;
        const int pathMtuOption = family == AF_INET
            ? IP_MTU_DISCOVER
            : IPV6_MTU_DISCOVER;
        if (setsockopt(handle, pathMtuLevel, pathMtuOption,
                       reinterpret_cast<const char*>(&pathMtuDiscovery),
                       sizeof(pathMtuDiscovery)) == SOCKET_ERROR) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram path MTU discovery configuration failed",
                WSAGetLastError()));
        }
        const int requestedBuffer = static_cast<int>(
            request.endpoint.socketBuffer.apiRequestedBytes);
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
        socketEvent = WSACreateEvent();
        if (socketEvent == WSA_INVALID_EVENT) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram socket-event creation failed",
                WSAGetLastError()));
        }
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stopEvent) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram stop-event creation failed",
                static_cast<int>(GetLastError())));
        }
        if (WSAEventSelect(handle, socketEvent, FD_WRITE | FD_CLOSE) ==
            SOCKET_ERROR) {
            return fail(::media::ErrorInfo::ioFailure(
                "Windows Datagram wait wakeup initialization failed",
                WSAGetLastError()));
        }

        auto timestampAvailability =
            MediaDatagramTransmitTimestampAvailability::NotRequested;
        auto timestampSource = MediaDatagramTransmitTimestampSource::Unknown;
        std::uint64_t timestampFrequency = 0;
        if (request.evidence) {
            timestampAvailability =
                MediaDatagramTransmitTimestampAvailability::Unavailable;
            const auto clockEvidence = queryTimestampClockEvidence(m_remote);
            if (clockEvidence &&
                request.evidence->maximumCorrelationEntries <=
                static_cast<std::uint64_t>(
                    (std::numeric_limits<USHORT>::max)())) {
                TIMESTAMPING_CONFIG config{};
                config.Flags = TIMESTAMPING_FLAG_TX;
                config.TxTimestampsBuffered = static_cast<USHORT>(
                    request.evidence->maximumCorrelationEntries);
                if (WSAIoctl(handle, SIO_TIMESTAMPING, &config, sizeof(config),
                             nullptr, 0, &bytes, nullptr, nullptr) == 0 &&
                    clockEvidence->frequency > 0) {
                    timestampAvailability =
                        MediaDatagramTransmitTimestampAvailability::Available;
                    timestampSource = clockEvidence->source;
                    timestampFrequency = clockEvidence->frequency;
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
        if (static_cast<std::uint64_t>(effectiveBuffer) <
                request.endpoint.socketBuffer.minimumEffectiveBytes ||
            static_cast<std::uint64_t>(effectiveBuffer) >
                request.endpoint.socketBuffer.maximumAdmittedEffectiveBytes) {
            return fail(::media::ErrorInfo::unsupported(
                "Windows effective SO_SNDBUF is outside the admitted planner range"));
        }
        m_socket = handle;
        m_socketEvent = socketEvent;
        m_stopEvent = stopEvent;
        m_endpointId = request.endpoint.endpointId;
        m_generation = request.generation;
        m_maximumDatagramBytes = request.endpoint.maximumDatagramBytes;
        m_timestampAvailable = timestampAvailability ==
            MediaDatagramTransmitTimestampAvailability::Available;
        m_timestampSource = timestampSource;
        m_timestampFrequency = timestampFrequency;
        return ResultType::success(MediaDatagramTransmitPortCapabilities{
            request.endpoint.socketBuffer.targetEffectiveBytes,
            request.endpoint.socketBuffer.apiRequestedBytes,
            static_cast<std::uint64_t>(effectiveBuffer), timestampAvailability,
            timestampSource, timestampFrequency,
            m_timestampAvailable
                ? MediaDatagramTransmitCorrelationMode::CallerSelectedUint32
                : MediaDatagramTransmitCorrelationMode::None,
            false, std::nullopt, false});
    }

    MediaDatagramTransmitSubmitResult trySubmit(
        std::span<const MediaDatagramTransmitPortRequest> requests) override
    {
        if (!isOwnerThread() || m_socket == INVALID_SOCKET || requests.empty()) {
            return MediaDatagramTransmitSubmitResult::failure(
                mediaDatagramTransmitError(::media::ErrorInfo::invalidArgument(
                    "invalid Windows Datagram submit request")));
        }
        for (const auto& request : requests) {
            if (request.bytes.empty() ||
                request.bytes.size() > m_maximumDatagramBytes ||
                request.bytes.size() > static_cast<std::size_t>(
                    (std::numeric_limits<ULONG>::max)()) ||
                request.kernelTransmitTimeNanoseconds ||
                (!m_timestampAvailable && request.platformCorrelationId)) {
                return MediaDatagramTransmitSubmitResult::failure(
                    mediaDatagramTransmitError(
                        ::media::ErrorInfo::invalidArgument(
                            "invalid Windows Datagram payload metadata")));
            }
        }
        std::uint64_t submitted = 0;
        for (const auto& request : requests) {
            WSABUF buffer{static_cast<ULONG>(request.bytes.size()),
                          reinterpret_cast<char*>(
                              const_cast<std::uint8_t*>(request.bytes.data()))};
            std::array<std::uint64_t,
                WSA_CMSG_SPACE(sizeof(UINT32)) / sizeof(std::uint64_t) + 1>
                control{};
            WSAMSG message{};
            message.name = reinterpret_cast<sockaddr*>(&m_remote);
            message.namelen = m_remoteLength;
            message.lpBuffers = &buffer;
            message.dwBufferCount = 1;
            if (request.platformCorrelationId) {
                message.Control.buf = reinterpret_cast<char*>(control.data());
                message.Control.len = WSA_CMSG_SPACE(sizeof(UINT32));
                auto* header = WSA_CMSG_FIRSTHDR(&message);
                header->cmsg_level = SOL_SOCKET;
                header->cmsg_type = SO_TIMESTAMP_ID;
                header->cmsg_len = WSA_CMSG_LEN(sizeof(UINT32));
                const UINT32 id = *request.platformCorrelationId;
                std::memcpy(WSA_CMSG_DATA(header), &id, sizeof(id));
            }
            DWORD accepted = 0;
            if (m_sendMsg(m_socket, &message, 0, &accepted,
                          nullptr, nullptr) == SOCKET_ERROR) {
                const int native = WSAGetLastError();
                if (detail::mediaWindowsDatagramSubmitWouldBlock(native) &&
                    submitted == 0) {
                    return MediaDatagramTransmitSubmitResult::success(
                        MediaDatagramTransmitAttempt::WouldBlock);
                }
                return submitFailure(
                    submitted == 0
                        ? "Windows Datagram submit failed"
                        : "Windows Datagram batch stopped after a submitted prefix",
                    native,
                    submitted == 0
                        ? MediaDatagramTransmitFailureKind::TerminalNoSubmit
                        : MediaDatagramTransmitFailureKind::PartialSubmittedPrefix,
                    submitted);
            }
            if (accepted != request.bytes.size()) {
                return submitFailure(
                    "Windows Datagram short submit has ambiguous delivery",
                    0,
                    MediaDatagramTransmitFailureKind::AmbiguousSubmittedPrefix,
                    submitted);
            }
            ++submitted;
        }
        return MediaDatagramTransmitSubmitResult::success(
            MediaDatagramTransmitAttempt::Submitted);
    }

    ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        MediaRunningTime maximumWait,
        std::stop_token stopToken) override
    {
        using ResultType =
            ::media::Result<MediaDatagramWritableWaitResult>;
        if (!isOwnerThread() || m_socket == INVALID_SOCKET ||
            maximumWait.nanoseconds() < 0) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Windows Datagram writable wait"));
        }
        if (stopToken.stop_requested()) {
            return ResultType::success(MediaDatagramWritableWaitResult::Stopped);
        }
        ResetEvent(m_stopEvent);
        std::stop_callback callback(stopToken, [event = m_stopEvent]() noexcept {
            SetEvent(event);
        });
        const std::uint64_t milliseconds = static_cast<std::uint64_t>(
            maximumWait.nanoseconds() / 1'000'000) +
            (maximumWait.nanoseconds() % 1'000'000 != 0 ? 1 : 0);
        if (milliseconds > MAXDWORD - 1) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "Windows Datagram writable wait exceeds platform range"));
        }
        const HANDLE handles[2] = {m_socketEvent, m_stopEvent};
        const DWORD result = WaitForMultipleObjects(
            2, handles, FALSE, static_cast<DWORD>(milliseconds));
        if (result == WAIT_TIMEOUT) {
            return ResultType::success(MediaDatagramWritableWaitResult::TimedOut);
        }
        if (result == WAIT_OBJECT_0 + 1) {
            return ResultType::success(MediaDatagramWritableWaitResult::Stopped);
        }
        if (result != WAIT_OBJECT_0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram writable wait failed",
                static_cast<int>(GetLastError())));
        }
        WSANETWORKEVENTS events{};
        if (WSAEnumNetworkEvents(m_socket, m_socketEvent, &events) ==
            SOCKET_ERROR) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram network-event drain failed",
                WSAGetLastError()));
        }
        if ((events.lNetworkEvents & FD_CLOSE) != 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram socket closed during writable wait",
                events.iErrorCode[FD_CLOSE_BIT]));
        }
        if ((events.lNetworkEvents & FD_WRITE) == 0 ||
            events.iErrorCode[FD_WRITE_BIT] != 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Windows Datagram writable event is invalid",
                events.iErrorCode[FD_WRITE_BIT]));
        }
        return ResultType::success(MediaDatagramWritableWaitResult::Writable);
    }

    ::media::Result<std::vector<MediaDatagramTransmitPlatformEvent>>
    drainAvailableEvents(
        std::span<const std::uint32_t> outstandingTimestampIds) override
    {
        using ResultType = ::media::Result<
            std::vector<MediaDatagramTransmitPlatformEvent>>;
        std::vector<MediaDatagramTransmitPlatformEvent> result;
        if (!isOwnerThread() || m_socket == INVALID_SOCKET) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "Windows Datagram evidence drain violated single-owner state"));
        }
        if (!m_timestampAvailable) return ResultType::success(std::move(result));
        try {
            result.reserve(outstandingTimestampIds.size());
        } catch (const std::bad_alloc&) {
            return ResultType::failure(::media::ErrorInfo::allocationFailed(
                "Windows transmit evidence drain"));
        }
        for (const auto correlationId : outstandingTimestampIds) {
            UINT32 id = correlationId;
            UINT64 timestamp = 0;
            DWORD bytes = 0;
            if (WSAIoctl(m_socket, SIO_GET_TX_TIMESTAMP,
                         &id, sizeof(id), &timestamp, sizeof(timestamp),
                         &bytes, nullptr, nullptr) == SOCKET_ERROR) {
                const int native = WSAGetLastError();
                if (native == WSAEWOULDBLOCK) continue;
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Windows transmit timestamp drain failed", native));
            }
            if (bytes != sizeof(timestamp)) {
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Windows transmit timestamp has an invalid size"));
            }
            result.push_back(MediaDatagramTransmitPlatformEvent{
                m_endpointId, m_generation,
                MediaDatagramTransmitPlatformEventKind::Timestamp,
                correlationId,
                m_timestampSource,
                timestamp, m_timestampFrequency, 0});
        }
        return ResultType::success(std::move(result));
    }

    ::media::Status close() noexcept override
    {
        if (m_hasOwner && !isOwnerThread()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Windows Datagram close violated its single-owner contract"));
        }
        return releaseResources();
    }

private:
    void forceCloseForDestruction() noexcept
    {
        (void)releaseResources();
    }

    ::media::Status releaseResources() noexcept
    {
        ::media::Status status = ::media::Status::success();
        if (m_socket != INVALID_SOCKET) {
            const SOCKET handle = m_socket;
            m_socket = INVALID_SOCKET;
            if (closesocket(handle) == SOCKET_ERROR) {
                status = ::media::Status::failure(::media::ErrorInfo::ioFailure(
                    "Windows Datagram close failed", WSAGetLastError()));
            }
        }
        if (m_socketEvent != WSA_INVALID_EVENT) {
            WSACloseEvent(m_socketEvent);
            m_socketEvent = WSA_INVALID_EVENT;
        }
        if (m_stopEvent) {
            CloseHandle(m_stopEvent);
            m_stopEvent = nullptr;
        }
        return status;
    }

    bool isOwnerThread() const noexcept
    {
        return m_hasOwner && std::this_thread::get_id() == m_ownerThread;
    }

    std::shared_ptr<MediaSocketRuntime> m_runtime;
    SOCKET m_socket = INVALID_SOCKET;
    LPFN_WSASENDMSG m_sendMsg = nullptr;
    WSAEVENT m_socketEvent = WSA_INVALID_EVENT;
    HANDLE m_stopEvent = nullptr;
    sockaddr_storage m_remote{};
    int m_remoteLength = 0;
    std::uint64_t m_endpointId = 0;
    std::uint64_t m_generation = 0;
    std::size_t m_maximumDatagramBytes = 0;
    std::uint64_t m_timestampFrequency = 0;
    MediaDatagramTransmitTimestampSource m_timestampSource =
        MediaDatagramTransmitTimestampSource::Unknown;
    bool m_timestampAvailable = false;
    bool m_openAttempted = false;
    bool m_hasOwner = false;
    std::thread::id m_ownerThread;
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
