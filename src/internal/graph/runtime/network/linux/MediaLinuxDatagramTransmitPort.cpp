#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "internal/graph/runtime/network/linux/MediaLinuxDatagramTransmitPort.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <linux/version.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <thread>
#include <utility>
#include <vector>

#if defined(LINUX_VERSION_CODE) && defined(KERNEL_VERSION) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0) && \
    defined(SO_TXTIME) && defined(SCM_TXTIME) && \
    defined(SO_EE_ORIGIN_TXTIME) && \
    defined(SO_EE_CODE_TXTIME_MISSED) && \
    defined(SO_EE_CODE_TXTIME_INVALID_PARAM)
#define MEDIA_DATAGRAM_HAS_LINUX_TXTIME 1
#else
#define MEDIA_DATAGRAM_HAS_LINUX_TXTIME 0
#endif

#if defined(LINUX_VERSION_CODE) && defined(KERNEL_VERSION) && \
    LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0) && \
    defined(SO_TIMESTAMPING) && defined(SCM_TIMESTAMPING) && \
    defined(SOF_TIMESTAMPING_MASK) && \
    defined(SO_EE_ORIGIN_TIMESTAMPING)
#define MEDIA_DATAGRAM_HAS_LINUX_TIMESTAMPING 1
#else
#define MEDIA_DATAGRAM_HAS_LINUX_TIMESTAMPING 0
#endif

namespace media::ffmpeg::graph {

bool detail::mediaLinuxDatagramSubmitWouldBlock(int nativeError) noexcept
{
    return nativeError == EAGAIN || nativeError == EWOULDBLOCK;
}

namespace {

struct MediaLinuxDatagramControlBuffer final {
    alignas(cmsghdr) std::array<std::byte,
        CMSG_SPACE(sizeof(std::uint32_t)) +
            CMSG_SPACE(sizeof(std::uint64_t))> bytes{};
};

::media::Status fillAddress(const MediaUdpDatagramEndpoint& endpoint,
                            sockaddr_storage& storage,
                            socklen_t& length) noexcept
{
    std::memset(&storage, 0, sizeof(storage));
    if (endpoint.addressFamily() == MediaIpAddressFamily::Ipv4) {
        auto* value = reinterpret_cast<sockaddr_in*>(&storage);
        value->sin_family = AF_INET;
        value->sin_port = htons(endpoint.port());
        length = sizeof(*value);
        if (::inet_pton(AF_INET, endpoint.numericAddress().c_str(),
                        &value->sin_addr) == 1) {
            return ::media::Status::success();
        }
    } else if (endpoint.addressFamily() == MediaIpAddressFamily::Ipv6) {
        auto* value = reinterpret_cast<sockaddr_in6*>(&storage);
        value->sin6_family = AF_INET6;
        value->sin6_port = htons(endpoint.port());
        length = sizeof(*value);
        if (::inet_pton(AF_INET6, endpoint.numericAddress().c_str(),
                        &value->sin6_addr) == 1) {
            return ::media::Status::success();
        }
    }
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "invalid numeric Linux Datagram address"));
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

class MediaLinuxDatagramTransmitPort final : public MediaDatagramTransmitPort {
public:
    explicit MediaLinuxDatagramTransmitPort(
        std::shared_ptr<MediaSocketRuntime> runtime) noexcept
        : m_runtime(std::move(runtime))
    {
    }

    ~MediaLinuxDatagramTransmitPort() override { forceCloseForDestruction(); }

    ::media::Result<MediaDatagramTransmitPortCapabilities> open(
        const MediaDatagramTransmitPortOpenRequest& request) override
    {
        using ResultType =
            ::media::Result<MediaDatagramTransmitPortCapabilities>;
        if (!m_runtime || m_openAttempted || request.sessionKey.empty() ||
            request.serviceScopeId.empty() || request.generation == 0 ||
            request.endpoint.endpointId == 0 ||
            request.endpoint.egressInterfaceIndex == 0 ||
            request.endpoint.socketBuffer.accounting !=
                MediaDatagramSocketBufferAccounting::LinuxDoubled ||
            request.endpoint.socketBuffer.apiRequestedBytes == 0 ||
            request.maximumBatchDatagrams == 0 ||
            request.maximumBatchDatagrams > static_cast<std::uint64_t>(
                (std::numeric_limits<unsigned int>::max)()) ||
            request.endpoint.socketBuffer.apiRequestedBytes >
                static_cast<std::uint64_t>((std::numeric_limits<int>::max)()) ||
            request.localEndpoint.addressFamily() !=
                request.endpoint.addressFamily ||
            (request.executionMode !=
                 MediaDatagramTransmitExecutionMode::UserspaceNonblocking &&
             request.executionMode !=
                 MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) ||
            ((request.executionMode ==
                  MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) !=
             request.kernelSchedule.has_value())) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Linux Datagram port open request"));
        }
        m_openAttempted = true;
        m_ownerThread = std::this_thread::get_id();
        m_hasOwner = true;
        const int family = request.endpoint.addressFamily ==
                MediaIpAddressFamily::Ipv4 ? AF_INET : AF_INET6;
        const int handle = ::socket(family, SOCK_DGRAM | SOCK_NONBLOCK |
                                             SOCK_CLOEXEC, IPPROTO_UDP);
        if (handle < 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram socket creation failed", errno));
        }
        int stopFd = -1;
        const auto fail = [&](::media::ErrorInfo error) {
            if (stopFd >= 0) ::close(stopFd);
            ::close(handle);
            return ResultType::failure(std::move(error));
        };
        const int interfaceLevel = family == AF_INET
            ? IPPROTO_IP
            : IPPROTO_IPV6;
        const int interfaceOption = family == AF_INET
            ? IP_UNICAST_IF
            : IPV6_UNICAST_IF;
        const auto interfaceIndex = family == AF_INET
            ? htonl(request.endpoint.egressInterfaceIndex)
            : request.endpoint.egressInterfaceIndex;
        if (::setsockopt(handle, interfaceLevel, interfaceOption,
                         &interfaceIndex, sizeof(interfaceIndex)) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram egress interface binding failed", errno));
        }
        const int pathMtuDiscovery = IP_PMTUDISC_DO;
        const int pathMtuLevel = family == AF_INET
            ? IPPROTO_IP
            : IPPROTO_IPV6;
        const int pathMtuOption = family == AF_INET
            ? IP_MTU_DISCOVER
            : IPV6_MTU_DISCOVER;
        if (::setsockopt(handle, pathMtuLevel, pathMtuOption,
                         &pathMtuDiscovery,
                         sizeof(pathMtuDiscovery)) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram path MTU discovery configuration failed",
                errno));
        }
        const int requestedBuffer = static_cast<int>(
            request.endpoint.socketBuffer.apiRequestedBytes);
        if (::setsockopt(handle, SOL_SOCKET, SO_SNDBUF, &requestedBuffer,
                         sizeof(requestedBuffer)) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram SO_SNDBUF configuration failed", errno));
        }
        sockaddr_storage local{};
        socklen_t localLength = 0;
        auto converted = fillAddress(request.localEndpoint, local, localLength);
        if (!converted) return fail(converted.error());
        if (::bind(handle, reinterpret_cast<const sockaddr*>(&local),
                   localLength) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram bind failed", errno));
        }
        auto remote = MediaUdpDatagramEndpoint::create(
            request.endpoint.addressFamily, request.endpoint.numericAddress,
            request.endpoint.port);
        if (!remote) return fail(remote.error());
        converted = fillAddress(remote.value(), m_remote, m_remoteLength);
        if (!converted) return fail(converted.error());
        if (::connect(handle, reinterpret_cast<const sockaddr*>(&m_remote),
                      m_remoteLength) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram connected route activation failed", errno));
        }
        int activatedMtu = 0;
        socklen_t activatedMtuLength = sizeof(activatedMtu);
        const int activatedMtuOption = family == AF_INET ? IP_MTU : IPV6_MTU;
        if (::getsockopt(handle, pathMtuLevel, activatedMtuOption,
                         &activatedMtu, &activatedMtuLength) != 0 ||
            activatedMtu <= 0 ||
            static_cast<std::uint64_t>(activatedMtu) <
                request.endpoint.mtuEvidence.maximumIpPacketBytes) {
            return fail(::media::ErrorInfo::unsupported(
                "Linux Datagram activated route MTU is below the planned contract"));
        }
        stopFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (stopFd < 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram stop wakeup creation failed", errno));
        }

        bool txtimeAvailable = false;
        if (request.executionMode ==
            MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) {
#if MEDIA_DATAGRAM_HAS_LINUX_TXTIME
            sock_txtime configuration{};
            configuration.clockid = CLOCK_MONOTONIC;
            configuration.flags = SOF_TXTIME_REPORT_ERRORS;
            if (::setsockopt(handle, SOL_SOCKET, SO_TXTIME, &configuration,
                             sizeof(configuration)) != 0) {
                return fail(::media::ErrorInfo::ioFailure(
                    "required Linux SO_TXTIME capability probe failed", errno));
            }
            const int enabled = 1;
            const int level = family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;
            const int option = family == AF_INET ? IP_RECVERR : IPV6_RECVERR;
            if (::setsockopt(handle, level, option, &enabled,
                             sizeof(enabled)) != 0) {
                return fail(::media::ErrorInfo::ioFailure(
                    "Linux SO_TXTIME error queue configuration failed", errno));
            }
            txtimeAvailable = true;
#else
            return fail(::media::ErrorInfo::unsupported(
                "Linux headers do not expose the required SO_TXTIME API"));
#endif
        }

        auto timestampAvailability =
            MediaDatagramTransmitTimestampAvailability::NotRequested;
        if (request.evidence) {
#if MEDIA_DATAGRAM_HAS_LINUX_TIMESTAMPING
            const int flags = SOF_TIMESTAMPING_SOFTWARE |
                              SOF_TIMESTAMPING_OPT_ID |
                              SOF_TIMESTAMPING_OPT_TSONLY;
            if (::setsockopt(handle, SOL_SOCKET, SO_TIMESTAMPING, &flags,
                             sizeof(flags)) == 0) {
                timestampAvailability =
                    MediaDatagramTransmitTimestampAvailability::Available;
            } else {
                timestampAvailability =
                    MediaDatagramTransmitTimestampAvailability::Unavailable;
            }
#else
            timestampAvailability =
                MediaDatagramTransmitTimestampAvailability::Unavailable;
#endif
        }
        int effectiveBuffer = 0;
        socklen_t optionLength = sizeof(effectiveBuffer);
        if (::getsockopt(handle, SOL_SOCKET, SO_SNDBUF, &effectiveBuffer,
                         &optionLength) != 0 || effectiveBuffer <= 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram effective SO_SNDBUF query failed", errno));
        }
        if (static_cast<std::uint64_t>(effectiveBuffer) <
                request.endpoint.socketBuffer.minimumEffectiveBytes ||
            static_cast<std::uint64_t>(effectiveBuffer) >
                request.endpoint.socketBuffer.maximumAdmittedEffectiveBytes) {
            return fail(::media::ErrorInfo::unsupported(
                "Linux effective SO_SNDBUF is outside the admitted planner range: target=" +
                std::to_string(request.endpoint.socketBuffer.targetEffectiveBytes) +
                " api_request=" + std::to_string(
                    request.endpoint.socketBuffer.apiRequestedBytes) +
                " minimum=" +
                std::to_string(request.endpoint.socketBuffer.minimumEffectiveBytes) +
                " maximum=" +
                std::to_string(request.endpoint.socketBuffer.maximumAdmittedEffectiveBytes) +
                " effective=" + std::to_string(effectiveBuffer)));
        }
        try {
            const auto batchCapacity = static_cast<std::size_t>(
                request.maximumBatchDatagrams);
            m_messages.resize(batchCapacity);
            m_vectors.resize(batchCapacity);
            m_controls.resize(batchCapacity);
        } catch (const std::bad_alloc&) {
            return fail(::media::ErrorInfo::allocationFailed(
                "Linux Datagram batch scratch allocation"));
        }
        m_socket = handle;
        m_stopFd = stopFd;
        m_endpointId = request.endpoint.endpointId;
        m_generation = request.generation;
        m_maximumDatagramBytes = request.endpoint.maximumDatagramBytes;
        m_timestampAvailable = timestampAvailability ==
            MediaDatagramTransmitTimestampAvailability::Available;
        m_txtimeAvailable = txtimeAvailable;
        if (request.kernelSchedule) {
            m_maximumRunDatagrams =
                request.kernelSchedule->maximumRunDatagrams;
            m_maximumScheduleAheadNanoseconds =
                request.kernelSchedule->maximumScheduleAheadNanoseconds;
        }
        return ResultType::success(MediaDatagramTransmitPortCapabilities{
            request.endpoint.socketBuffer.targetEffectiveBytes,
            request.endpoint.socketBuffer.apiRequestedBytes,
            static_cast<std::uint64_t>(effectiveBuffer), timestampAvailability,
            m_timestampAvailable
                ? MediaDatagramTransmitTimestampSource::LinuxSoftwareRealtime
                : MediaDatagramTransmitTimestampSource::Unknown,
            m_timestampAvailable ? 1'000'000'000ULL : 0,
            m_timestampAvailable
                ? MediaDatagramTransmitCorrelationMode::KernelSequentialUint32
                : MediaDatagramTransmitCorrelationMode::None,
            m_txtimeAvailable, false});
    }

    MediaDatagramTransmitSubmitResult trySubmit(
        std::span<const MediaDatagramTransmitPortRequest> requests) override
    {
        if (!isOwnerThread() || m_socket < 0 || requests.empty() ||
            requests.size() > m_messages.size()) {
            return MediaDatagramTransmitSubmitResult::failure(
                mediaDatagramTransmitError(::media::ErrorInfo::invalidArgument(
                    "invalid Linux Datagram submit request")));
        }
        if (m_txtimeAvailable &&
            (m_runSubmittedDatagrams > m_maximumRunDatagrams ||
             requests.size() >
                 m_maximumRunDatagrams - m_runSubmittedDatagrams)) {
            return MediaDatagramTransmitSubmitResult::failure(
                mediaDatagramTransmitError(::media::ErrorInfo::invalidArgument(
                    "Linux SO_TXTIME run exceeds its typed Datagram budget")));
        }
        std::uint64_t kernelNowNanoseconds = 0;
        if (m_txtimeAvailable) {
            timespec now{};
            const int clockResult = ::clock_gettime(CLOCK_MONOTONIC, &now);
            const int clockError = clockResult != 0 ? errno : 0;
            if (clockResult != 0 ||
                now.tv_sec < 0 || now.tv_nsec < 0 ||
                now.tv_nsec >= 1'000'000'000L ||
                static_cast<std::uint64_t>(now.tv_sec) >
                    ((std::numeric_limits<std::uint64_t>::max)() -
                     static_cast<std::uint64_t>(now.tv_nsec)) /
                        1'000'000'000ULL) {
                return MediaDatagramTransmitSubmitResult::failure(
                    mediaDatagramTransmitError(::media::ErrorInfo::ioFailure(
                        "Linux SO_TXTIME monotonic clock query failed",
                        clockError)));
            }
            kernelNowNanoseconds = static_cast<std::uint64_t>(now.tv_sec) *
                    1'000'000'000ULL +
                static_cast<std::uint64_t>(now.tv_nsec);
        }
        auto expectedId = m_nextKernelTimestampId;
        for (const auto& request : requests) {
            if (request.bytes.empty() ||
                request.bytes.size() > m_maximumDatagramBytes ||
                request.bytes.size() > static_cast<std::size_t>(
                    (std::numeric_limits<ssize_t>::max)()) ||
                (!m_timestampAvailable &&
                 request.platformCorrelationId.has_value()) ||
                (request.platformCorrelationId &&
                 *request.platformCorrelationId != expectedId) ||
                (m_txtimeAvailable !=
                 request.kernelTransmitTimeNanoseconds.has_value()) ||
                (m_txtimeAvailable &&
                 (*request.kernelTransmitTimeNanoseconds <
                      kernelNowNanoseconds ||
                  *request.kernelTransmitTimeNanoseconds -
                          kernelNowNanoseconds >
                      m_maximumScheduleAheadNanoseconds))) {
                return MediaDatagramTransmitSubmitResult::failure(
                    mediaDatagramTransmitError(
                        ::media::ErrorInfo::invalidArgument(
                            "invalid Linux Datagram payload metadata")));
            }
            ++expectedId;
        }

        for (std::size_t index = 0; index < requests.size(); ++index) {
            const auto& request = requests[index];
            m_vectors[index] = {
                const_cast<std::uint8_t*>(request.bytes.data()),
                request.bytes.size()};
            m_controls[index].bytes.fill(std::byte{});
            m_messages[index] = {};
            auto& message = m_messages[index].msg_hdr;
            message.msg_name = nullptr;
            message.msg_namelen = 0;
            message.msg_iov = &m_vectors[index];
            message.msg_iovlen = 1;
            if (request.platformCorrelationId || m_txtimeAvailable) {
                message.msg_control = m_controls[index].bytes.data();
                message.msg_controllen = m_controls[index].bytes.size();
                auto* header = CMSG_FIRSTHDR(&message);
                std::size_t controlBytes = 0;
                if (request.platformCorrelationId) {
#if MEDIA_DATAGRAM_HAS_LINUX_TIMESTAMPING
                    header->cmsg_level = SOL_SOCKET;
                    header->cmsg_type = SO_TIMESTAMPING;
                    header->cmsg_len = CMSG_LEN(sizeof(std::uint32_t));
                    const std::uint32_t timestampFlags =
                        SOF_TIMESTAMPING_TX_SOFTWARE;
                    std::memcpy(CMSG_DATA(header), &timestampFlags,
                                sizeof(timestampFlags));
                    controlBytes += CMSG_SPACE(sizeof(timestampFlags));
#endif
                }
                if (m_txtimeAvailable) {
#if MEDIA_DATAGRAM_HAS_LINUX_TXTIME
                    if (controlBytes != 0) {
                        header = CMSG_NXTHDR(&message, header);
                    }
                    header->cmsg_level = SOL_SOCKET;
                    header->cmsg_type = SCM_TXTIME;
                    header->cmsg_len = CMSG_LEN(sizeof(std::uint64_t));
                    const auto launch = *request.kernelTransmitTimeNanoseconds;
                    std::memcpy(CMSG_DATA(header), &launch, sizeof(launch));
                    controlBytes += CMSG_SPACE(sizeof(launch));
#endif
                }
                message.msg_controllen = controlBytes;
            }
        }
        const int accepted = ::sendmmsg(
            m_socket, m_messages.data(),
            static_cast<unsigned int>(requests.size()), MSG_DONTWAIT);
        if (accepted < 0) {
            const int native = errno;
            if (detail::mediaLinuxDatagramSubmitWouldBlock(native)) {
                return MediaDatagramTransmitSubmitResult::success(
                    MediaDatagramTransmitAttempt::WouldBlock);
            }
            return submitFailure(
                "Linux Datagram batch submit failed", native,
                MediaDatagramTransmitFailureKind::TerminalNoSubmit, 0);
        }
        if (accepted == 0) {
            return submitFailure(
                "Linux Datagram batch submit accepted no messages", 0,
                MediaDatagramTransmitFailureKind::TerminalNoSubmit, 0);
        }
        const auto submitted = static_cast<std::uint64_t>(accepted);
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(accepted); ++index) {
            if (static_cast<std::size_t>(m_messages[index].msg_len) !=
                requests[index].bytes.size()) {
                return submitFailure(
                    "Linux Datagram short submit has ambiguous delivery", 0,
                    MediaDatagramTransmitFailureKind::AmbiguousSubmittedPrefix,
                    index);
            }
        }
        m_runSubmittedDatagrams += submitted;
        m_nextKernelTimestampId += submitted;
        if (submitted != requests.size()) {
            return submitFailure(
                "Linux Datagram batch stopped after a submitted prefix", 0,
                MediaDatagramTransmitFailureKind::PartialSubmittedPrefix,
                submitted);
        }
        return MediaDatagramTransmitSubmitResult::success(
            MediaDatagramTransmitAttempt::Submitted);
    }

    ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        MediaRunningTime maximumWait,
        std::stop_token stopToken) override
    {
        using ResultType = ::media::Result<MediaDatagramWritableWaitResult>;
        if (!isOwnerThread() || m_socket < 0 || m_stopFd < 0 ||
            maximumWait.nanoseconds() < 0) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Linux Datagram writable wait"));
        }
        if (stopToken.stop_requested()) {
            return ResultType::success(MediaDatagramWritableWaitResult::Stopped);
        }
        std::stop_callback callback(stopToken, [fd = m_stopFd]() noexcept {
            const std::uint64_t one = 1;
            const auto result = ::write(fd, &one, sizeof(one));
            (void)result;
        });
        const auto nanoseconds =
            static_cast<std::uint64_t>(maximumWait.nanoseconds());
        const auto milliseconds = nanoseconds / 1'000'000ULL +
            (nanoseconds % 1'000'000ULL != 0 ? 1 : 0);
        if (milliseconds >
            static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "Linux Datagram writable wait exceeds platform range"));
        }
        pollfd descriptors[2]{{m_socket, POLLOUT, 0},
                              {m_stopFd, POLLIN, 0}};
        const int result = ::poll(descriptors, 2,
                                  static_cast<int>(milliseconds));
        if (result < 0) {
            if (errno == EINTR && stopToken.stop_requested()) {
                return ResultType::success(
                    MediaDatagramWritableWaitResult::Stopped);
            }
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram writable wait failed", errno));
        }
        if (result == 0) {
            return ResultType::success(
                MediaDatagramWritableWaitResult::TimedOut);
        }
        if ((descriptors[1].revents & POLLIN) != 0) {
            std::uint64_t count = 0;
            while (::read(m_stopFd, &count, sizeof(count)) < 0 &&
                   errno == EINTR) {
            }
            if (stopToken.stop_requested()) {
                return ResultType::success(
                    MediaDatagramWritableWaitResult::Stopped);
            }
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram stop wakeup lacked worker causality"));
        }
        if ((descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram socket failed during writable wait"));
        }
        if ((descriptors[0].revents & POLLOUT) == 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram writable wait returned no writable event"));
        }
        return ResultType::success(MediaDatagramWritableWaitResult::Writable);
    }

    ::media::Result<std::vector<MediaDatagramTransmitPlatformEvent>>
    drainAvailableEvents(
        std::span<const std::uint32_t> outstandingTimestampIds) override
    {
        using ResultType = ::media::Result<
            std::vector<MediaDatagramTransmitPlatformEvent>>;
        std::vector<MediaDatagramTransmitPlatformEvent> events;
        if (!isOwnerThread() || m_socket < 0) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "Linux Datagram evidence drain violated single-owner state"));
        }
        (void)outstandingTimestampIds;
        if (!m_timestampAvailable && !m_txtimeAvailable) {
            return ResultType::success(std::move(events));
        }
        try {
            for (;;) {
                std::array<std::byte, 1> payload{};
                iovec vector{payload.data(), payload.size()};
                alignas(cmsghdr) std::array<std::byte, 512> control{};
                msghdr message{};
                message.msg_iov = &vector;
                message.msg_iovlen = 1;
                message.msg_control = control.data();
                message.msg_controllen = control.size();
                const auto received = ::recvmsg(
                    m_socket, &message, MSG_ERRQUEUE | MSG_DONTWAIT);
                if (received < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    return ResultType::failure(::media::ErrorInfo::ioFailure(
                        "Linux Datagram error queue drain failed", errno));
                }
                if ((message.msg_flags & MSG_CTRUNC) != 0) {
                    return ResultType::failure(::media::ErrorInfo::ioFailure(
                        "Linux Datagram error queue control data was truncated"));
                }
                const sock_extended_err* extended = nullptr;
#if MEDIA_DATAGRAM_HAS_LINUX_TIMESTAMPING
                const timespec* timestamps = nullptr;
#endif
                for (auto* header = CMSG_FIRSTHDR(&message); header;
                     header = CMSG_NXTHDR(&message, header)) {
                    if ((header->cmsg_level == IPPROTO_IP &&
                         header->cmsg_type == IP_RECVERR) ||
                        (header->cmsg_level == IPPROTO_IPV6 &&
                         header->cmsg_type == IPV6_RECVERR)) {
                        if (header->cmsg_len <
                            CMSG_LEN(sizeof(sock_extended_err))) {
                            return ResultType::failure(
                                ::media::ErrorInfo::ioFailure(
                                    "Linux Datagram extended error was truncated"));
                        }
                        extended = reinterpret_cast<const sock_extended_err*>(
                            CMSG_DATA(header));
                    }
#if MEDIA_DATAGRAM_HAS_LINUX_TIMESTAMPING
                    else if (header->cmsg_level == SOL_SOCKET &&
                               header->cmsg_type == SCM_TIMESTAMPING) {
                        if (header->cmsg_len <
                            CMSG_LEN(sizeof(timespec) * 3)) {
                            return ResultType::failure(
                                ::media::ErrorInfo::ioFailure(
                                    "Linux Datagram timestamp data was truncated"));
                        }
                        timestamps = reinterpret_cast<const timespec*>(
                            CMSG_DATA(header));
                    }
#endif
                }
                if (!extended) {
                    return ResultType::failure(::media::ErrorInfo::ioFailure(
                        "Linux Datagram error queue lacked extended metadata"));
                }
#if MEDIA_DATAGRAM_HAS_LINUX_TIMESTAMPING
                if (extended->ee_origin == SO_EE_ORIGIN_TIMESTAMPING) {
                    if (!m_timestampAvailable || extended->ee_errno != ENOMSG ||
                        extended->ee_info != SCM_TSTAMP_SND || !timestamps ||
                        timestamps[0].tv_sec < 0 ||
                        timestamps[0].tv_nsec < 0 ||
                        timestamps[0].tv_nsec >= 1'000'000'000L ||
                        static_cast<std::uint64_t>(timestamps[0].tv_sec) >
                            ((std::numeric_limits<std::uint64_t>::max)() -
                             static_cast<std::uint64_t>(timestamps[0].tv_nsec)) /
                                1'000'000'000ULL) {
                        return ResultType::failure(::media::ErrorInfo::ioFailure(
                            "Linux Datagram transmit timestamp metadata is invalid"));
                    }
                    events.push_back(MediaDatagramTransmitPlatformEvent{
                        m_endpointId, m_generation,
                        MediaDatagramTransmitPlatformEventKind::Timestamp,
                        extended->ee_data,
                        MediaDatagramTransmitTimestampSource::LinuxSoftwareRealtime,
                        static_cast<std::uint64_t>(timestamps[0].tv_sec) *
                                1'000'000'000ULL +
                            static_cast<std::uint64_t>(timestamps[0].tv_nsec),
                        1'000'000'000ULL, 0});
                    continue;
                }
#endif
#if MEDIA_DATAGRAM_HAS_LINUX_TXTIME
                if (extended->ee_origin == SO_EE_ORIGIN_TXTIME) {
                    if (!m_txtimeAvailable || extended->ee_errno != ECANCELED) {
                        return ResultType::failure(::media::ErrorInfo::ioFailure(
                            "Linux SO_TXTIME error metadata is invalid"));
                    }
                    MediaDatagramTransmitPlatformEventKind kind{};
                    if (extended->ee_code == SO_EE_CODE_TXTIME_MISSED) {
                        kind = MediaDatagramTransmitPlatformEventKind::TxTimeMissed;
                    } else if (extended->ee_code ==
                               SO_EE_CODE_TXTIME_INVALID_PARAM) {
                        kind = MediaDatagramTransmitPlatformEventKind::TxTimeInvalid;
                    } else {
                        return ResultType::failure(::media::ErrorInfo::ioFailure(
                            "Linux SO_TXTIME returned an unknown error code"));
                    }
                    events.push_back(MediaDatagramTransmitPlatformEvent{
                        m_endpointId, m_generation, kind, 0,
                        MediaDatagramTransmitTimestampSource::Unknown,
                        0, 0, extended->ee_data});
                    continue;
                }
#endif
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Linux Datagram error queue returned an unknown origin"));
            }
        } catch (const std::bad_alloc&) {
            return ResultType::failure(::media::ErrorInfo::allocationFailed(
                "Linux Datagram error queue events"));
        }
        return ResultType::success(std::move(events));
    }

    ::media::Status close() noexcept override
    {
        if (m_hasOwner && !isOwnerThread()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Linux Datagram close violated its single-owner contract"));
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
        if (m_socket >= 0) {
            const int handle = m_socket;
            m_socket = -1;
            if (::close(handle) != 0) {
                status = ::media::Status::failure(::media::ErrorInfo::ioFailure(
                    "Linux Datagram close failed", errno));
            }
        }
        if (m_stopFd >= 0) {
            const int handle = m_stopFd;
            m_stopFd = -1;
            if (::close(handle) != 0 && status) {
                status = ::media::Status::failure(::media::ErrorInfo::ioFailure(
                    "Linux Datagram stop wakeup close failed", errno));
            }
        }
        return status;
    }

    bool isOwnerThread() const noexcept
    {
        return m_hasOwner && std::this_thread::get_id() == m_ownerThread;
    }

    std::shared_ptr<MediaSocketRuntime> m_runtime;
    int m_socket = -1;
    int m_stopFd = -1;
    sockaddr_storage m_remote{};
    socklen_t m_remoteLength = 0;
    std::uint64_t m_endpointId = 0;
    std::uint64_t m_generation = 0;
    std::size_t m_maximumDatagramBytes = 0;
    std::uint64_t m_maximumRunDatagrams = 0;
    std::uint64_t m_runSubmittedDatagrams = 0;
    std::uint64_t m_maximumScheduleAheadNanoseconds = 0;
    std::uint32_t m_nextKernelTimestampId = 0;
    std::vector<mmsghdr> m_messages;
    std::vector<iovec> m_vectors;
    std::vector<MediaLinuxDatagramControlBuffer> m_controls;
    bool m_timestampAvailable = false;
    bool m_txtimeAvailable = false;
    bool m_openAttempted = false;
    bool m_hasOwner = false;
    std::thread::id m_ownerThread;
};

} // namespace

MediaLinuxDatagramTransmitPortFactory::MediaLinuxDatagramTransmitPortFactory(
    std::shared_ptr<MediaSocketRuntime> runtime) noexcept
    : m_runtime(std::move(runtime))
{
}

::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>
MediaLinuxDatagramTransmitPortFactory::create()
{
    if (!m_runtime) {
        return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Linux Datagram factory requires socket runtime"));
    }
    try {
        std::unique_ptr<MediaDatagramTransmitPort> port =
            std::make_unique<MediaLinuxDatagramTransmitPort>(m_runtime);
        return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::success(
            std::move(port));
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaLinuxDatagramTransmitPort"));
    }
}

} // namespace media::ffmpeg::graph
#else
namespace media::ffmpeg::graph {
MediaLinuxDatagramTransmitPortFactory::MediaLinuxDatagramTransmitPortFactory(
    std::shared_ptr<MediaSocketRuntime> runtime) noexcept
    : m_runtime(std::move(runtime))
{
}
::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>
MediaLinuxDatagramTransmitPortFactory::create()
{
    return ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>>::failure(
        ::media::ErrorInfo::unsupported(
            "Linux Datagram adapter is unavailable on this platform"));
}
} // namespace media::ffmpeg::graph
#endif
