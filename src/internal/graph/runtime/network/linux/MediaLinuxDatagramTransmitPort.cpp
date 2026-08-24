#include "internal/graph/runtime/network/linux/MediaLinuxDatagramTransmitPort.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <cerno>
#include <fcntl.h>
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

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
                            socklen_t& length) noexcept
{
    std::memset(&storage, 0, sizeof(storage));
    if (endpoint.addressFamily() == MediaIpAddressFamily::Ipv4) {
        auto* value = reinterpret_cast<sockaddr_in*>(&storage);
        value->sin_family = AF_INET;
        value->sin_port = htons(endpoint.port());
        length = sizeof(*value);
        if (inet_pton(AF_INET, endpoint.numericAddress().c_str(),
                      &value->sin_addr) == 1) {
            return ::media::Status::success();
        }
    } else {
        auto* value = reinterpret_cast<sockaddr_in6*>(&storage);
        value->sin6_family = AF_INET6;
        value->sin6_port = htons(endpoint.port());
        length = sizeof(*value);
        if (inet_pton(AF_INET6, endpoint.numericAddress().c_str(),
                      &value->sin6_addr) == 1) {
            return ::media::Status::success();
        }
    }
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "invalid numeric Linux Datagram address"));
}

class MediaLinuxDatagramTransmitPort final : public MediaDatagramTransmitPort {
public:
    explicit MediaLinuxDatagramTransmitPort(
        std::shared_ptr<MediaSocketRuntime> runtime) noexcept
        : m_runtime(std::move(runtime))
    {
    }

    ~MediaLinuxDatagramTransmitPort() override { close(); }

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
                "invalid Linux Datagram port open request"));
        }
        m_openAttempted = true;
        if (request.executionMode !=
                MediaDatagramTransmitExecutionMode::UserspaceNonblocking &&
            request.executionMode !=
                MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "unknown Linux Datagram execution mode"));
        }
        const int family = request.endpoint.addressFamily ==
                MediaIpAddressFamily::Ipv4 ? AF_INET : AF_INET6;
        int handle = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
        if (handle < 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram socket creation failed", erno));
        }
        const auto fail = [&handle](::media::ErrorInfo error) {
            ::close(handle);
            return ResultType::failure(std::move(error));
        };
        const int requestedBuffer =
            static_cast<int>(request.endpoint.socketHardBoundBytes);
        if (setsockopt(handle, SOL_SOCKET, SO_SNDBUF,
                       &requestedBuffer, sizeof(requestedBuffer)) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram SO_SNDBUF configuration failed", erno));
        }
        const int flags = fcntl(handle, F_GETFL, 0);
        if (flags < 0 || fcntl(handle, F_SETFL, flags | O_NONBLOCK) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram nonblocking configuration failed", erno));
        }
        sockaddr_storage local{};
        socklen_t localLength = 0;
        auto converted = fillAddress(request.localEndpoint, local, localLength);
        if (!converted) return fail(converted.error());
        if (bind(handle, reinterpret_cast<const sockaddr*>(&local),
                 localLength) != 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram bind failed", erno));
        }
        auto remote = MediaUdpDatagramEndpoint::create(
            request.endpoint.addressFamily, request.endpoint.numericAddress,
            request.endpoint.port);
        if (!remote) return fail(remote.error());
        converted = fillAddress(remote.value(), m_remote, m_remoteLength);
        if (!converted) return fail(converted.error());

        bool txtimeAvailable = false;
        if (request.executionMode ==
            MediaDatagramTransmitExecutionMode::LinuxSocketTxTime) {
            sock_txtime config{};
            config.clockid = CLOCK_MONOTONIC;
            config.flags = SOF_TXTIME_REPORT_ERRORS;
            if (setsockopt(handle, SOL_SOCKET, SO_TXTIME,
                           &config, sizeof(config)) != 0) {
                return fail(::media::ErrorInfo::unsupported(
                    "required Linux SO_TXTIME capability probe failed"));
            }
            txtimeAvailable = true;
        }
        MediaDatagramTransmitTimestampAvailability timestampAvailability =
            MediaDatagramTransmitTimestampAvailability::NotRequested;
        if (request.evidence) {
            timestampAvailability =
                MediaDatagramTransmitTimestampAvailability::Unavailable;
            const int timestampFlags =
                SOF_TIMESTAMPING_TX_SOFTWARE |
                SOF_TIMESTAMPING_SOFTWARE |
                SOF_TIMESTAMPING_OPT_ID |
                SOF_TIMESTAMPING_OPT_TSONLY;
            if (setsockopt(handle, SOL_SOCKET, SO_TIMESTAMPING,
                           &timestampFlags, sizeof(timestampFlags)) == 0) {
                try {
                    m_pending.reserve(static_cast<std::size_t>(
                        request.evidence->maximumCorrelationEntries));
                    timestampAvailability =
                        MediaDatagramTransmitTimestampAvailability::Available;
                } catch (const std::bad_alloc&) {
                    return fail(::media::ErrorInfo::allocationFailed(
                        "Linux transmit timestamp correlation"));
                }
            }
        }
        int effectiveBuffer = 0;
        socklen_t optionLength = sizeof(effectiveBuffer);
        if (getsockopt(handle, SOL_SOCKET, SO_SNDBUF,
                       &effectiveBuffer, &optionLength) != 0 ||
            effectiveBuffer <= 0) {
            return fail(::media::ErrorInfo::ioFailure(
                "Linux Datagram effective SO_SNDBUF query failed", erno));
        }
        m_socket = handle;
        m_endpointId = request.endpoint.endpointId;
        m_generation = request.generation;
        m_maximumDatagramBytes = request.endpoint.maximumDatagramBytes;
        m_timestampAvailable = timestampAvailability ==
            MediaDatagramTransmitTimestampAvailability::Available;
        m_txtimeAvailable = txtimeAvailable;
        return ResultType::success(MediaDatagramTransmitPortCapabilities{
            request.endpoint.socketHardBoundBytes,
            static_cast<std::uint64_t>(effectiveBuffer),
            timestampAvailability, txtimeAvailable, false});
    }

    ::media::Result<MediaDatagramTransmitAttempt> trySubmit(
        std::span<const MediaDatagramTransmitRequest> requests) override
    {
        using ResultType = ::media::Result<MediaDatagramTransmitAttempt>;
        if (m_socket < 0 || requests.empty()) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Linux Datagram submit request"));
        }
        if (m_timestampAvailable &&
            requests.size() > m_pending.capacity() - m_pending.size()) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux transmit timestamp correlation cannot admit batch"));
        }
        for (const auto& request : requests) {
            if (request.bytes.empty() ||
                request.bytes.size() > m_maximumDatagramBytes ||
                (m_txtimeAvailable !=
                 request.kernelTransmitTimeNanoseconds.has_value())) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "invalid Linux Datagram payload or launch time"));
            }
        }
        std::size_t submitted = 0;
        for (const auto& request : requests) {
            iovec vector{const_cast<std::uint8_t*>(request.bytes.data()),
                         request.bytes.size()};
            std::array<char, CMSG_SPACE(sizeof(std::uint64_t))> control{};
            msghdr message{};
            message.msg_name = &m_remote;
            message.msg_namelen = m_remoteLength;
            message.msg_iov = &vector;
            message.msg_iovlen = 1;
            if (m_txtimeAvailable) {
                message.msg_control = control.data();
                message.msg_controllen = control.size();
                auto* header = CMSG_FIRSTHDR(&message);
                header->cmsg_level = SOL_SOCKET;
                header->cmsg_type = SCM_TXTIME;
                header->cmsg_len = CMSG_LEN(sizeof(std::uint64_t));
                std::memcpy(CMSG_DATA(header),
                            &*request.kernelTransmitTimeNanoseconds,
                            sizeof(std::uint64_t));
            }
            if (m_timestampAvailable) {
                m_pending.push_back(PendingTimestamp{
                    m_nextTimestampId, request.evidenceId});
            }
            const ssize_t accepted = sendmsg(m_socket, &message, 0);
            if (accepted < 0) {
                const int native = erno;
                if (m_timestampAvailable) m_pending.pop_back();
                if ((native == EAGAIN || native == EWOULDBLOCK ||
                     native == ENOBUFS) && submitted == 0) {
                    return ResultType::success(
                        MediaDatagramTransmitAttempt::WouldBlock);
                }
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    submitted == 0
                        ? "Linux Datagram submit failed"
                        : "Linux Datagram batch delivery is ambiguous",
                    native));
            }
            if (static_cast<std::size_t>(accepted) != request.bytes.size()) {
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Linux Datagram short or partial batch submit"));
            }
            if (m_timestampAvailable) ++m_nextTimestampId;
            ++submitted;
        }
        return ResultType::success(MediaDatagramTransmitAttempt::Submitted);
    }

    ::media::Result<MediaDatagramWritableWaitResult> waitWritable(
        MediaRunningTime maximumWait) override
    {
        using ResultType =
            ::media::Result<MediaDatagramWritableWaitResult>;
        if (m_socket < 0 || maximumWait.nanoseconds() < 0) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "invalid Linux Datagram writable wait"));
        }
        const std::uint64_t milliseconds = static_cast<std::uint64_t>(
            maximumWait.nanoseconds() / 1'000'000) +
            (maximumWait.nanoseconds() % 1'000'000 != 0 ? 1 : 0);
        if (milliseconds > static_cast<std::uint64_t>(
            (std::numeric_limits<int>::max)())) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "Linux Datagram writable wait exceeds platform range"));
        }
        pollfd descriptor{m_socket, POLLOUT, 0};
        const int result = poll(&descriptor, 1, static_cast<int>(milliseconds));
        if (result == 0) {
            return ResultType::success(
                MediaDatagramWritableWaitResult::TimedOut);
        }
        if (result < 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram writable wait failed", erno));
        }
        if ((descriptor.revents & POLLOUT) == 0) {
            return ResultType::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram writable wait returned terminal events"));
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
                "Linux transmit evidence drain"));
        }
        for (;;) {
            std::array<char, 256> control{};
            std::uint8_t byte = 0;
            iovec vector{&byte, sizeof(byte)};
            msghdr message{};
            message.msg_iov = &vector;
            message.msg_iovlen = 1;
            message.msg_control = control.data();
            message.msg_controllen = control.size();
            if (recvmsg(m_socket, &message, MSG_ERRQUEUE | MSG_DONTWAIT) < 0) {
                if (erno == EAGAIN || erno == EWOULDBLOCK) break;
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Linux transmit timestamp drain failed", erno));
            }
            const sock_extended_err* extended = nullptr;
            const timespec* timestamps = nullptr;
            for (auto* header = CMSG_FIRSTHDR(&message); header;
                 header = CMSG_NXTHDR(&message, header)) {
                if (header->cmsg_level == SOL_SOCKET &&
                    header->cmsg_type == SCM_TIMESTAMPING) {
                    timestamps = reinterpret_cast<const timespec*>(
                        CMSG_DATA(header));
                } else if ((header->cmsg_level == SOL_IP &&
                            header->cmsg_type == IP_RECVERR) ||
                           (header->cmsg_level == SOL_IPV6 &&
                            header->cmsg_type == IPV6_RECVERR)) {
                    extended = reinterpret_cast<const sock_extended_err*>(
                        CMSG_DATA(header));
                }
            }
            if (!extended || extended->ee_origin != SO_EE_ORIGIN_TIMESTAMPING ||
                !timestamps) {
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Linux Datagram error queue contained non-timestamp data"));
            }
            auto pending = m_pending.end();
            for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
                if (it->platformId == extended->ee_data) {
                    pending = it;
                    break;
                }
            }
            if (pending == m_pending.end()) {
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Linux transmit timestamp id is not correlated"));
            }
            const timespec& timestamp = timestamps[0].tv_sec != 0 ||
                    timestamps[0].tv_nsec != 0 ? timestamps[0] : timestamps[2];
            if (timestamp.tv_sec < 0 || timestamp.tv_nsec < 0 ||
                timestamp.tv_nsec >= 1'000'000'000 ||
                static_cast<std::uint64_t>(timestamp.tv_sec) >
                    ((std::numeric_limits<std::uint64_t>::max)() -
                     static_cast<std::uint64_t>(timestamp.tv_nsec)) /
                        1'000'000'000ULL) {
                return ResultType::failure(::media::ErrorInfo::ioFailure(
                    "Linux transmit timestamp is not representable"));
            }
            const std::uint64_t nanoseconds =
                static_cast<std::uint64_t>(timestamp.tv_sec) *
                    1'000'000'000ULL +
                static_cast<std::uint64_t>(timestamp.tv_nsec);
            result.push_back(MediaDatagramTransmitEvidence{
                m_endpointId, m_generation, pending->evidenceId, nanoseconds});
            m_pending.erase(pending);
        }
        return ResultType::success(std::move(result));
    }

    ::media::Status close() noexcept override
    {
        if (m_socket < 0) return ::media::Status::success();
        const int handle = m_socket;
        m_socket = -1;
        m_pending.clear();
        if (::close(handle) != 0) {
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram close failed", erno));
        }
        return ::media::Status::success();
    }

private:
    std::shared_ptr<MediaSocketRuntime> m_runtime;
    int m_socket = -1;
    sockaddr_storage m_remote{};
    socklen_t m_remoteLength = 0;
    std::uint64_t m_endpointId = 0;
    std::uint64_t m_generation = 0;
    std::size_t m_maximumDatagramBytes = 0;
    std::uint32_t m_nextTimestampId = 0;
    std::vector<PendingTimestamp> m_pending;
    bool m_timestampAvailable = false;
    bool m_txtimeAvailable = false;
    bool m_openAttempted = false;
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
