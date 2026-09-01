#include "internal/graph/planner/realtime/MediaDatagramExecutionCapabilityProbe.h"
#include "internal/graph/planner/realtime/linux/MediaLinuxSocketProbeHandle.h"

#ifdef __linux__

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <linux/netlink.h>
#include <linux/pkt_sched.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <array>
#include <cstddef>
#include <limits>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint32_t> parseInterfaceIndex(
    std::string_view serviceScopeId) noexcept
{
    using Result = ::media::Result<std::uint32_t>;
    constexpr std::string_view Prefix = "ifindex:";
    if (!serviceScopeId.starts_with(Prefix)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Linux Datagram service scope must carry an authoritative ifindex"));
    }
    serviceScopeId.remove_prefix(Prefix.size());
    std::uint32_t value = 0;
    const auto parsed = std::from_chars(
        serviceScopeId.data(), serviceScopeId.data() + serviceScopeId.size(),
        value);
    if (serviceScopeId.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != serviceScopeId.data() + serviceScopeId.size() ||
        value == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Linux Datagram service scope contains an invalid ifindex"));
    }
    return Result::success(value);
}

struct FqQdiscEvidence final {
    bool found = false;
    std::uint32_t quantum = 0;
    std::uint32_t initialQuantum = 0;
    std::uint32_t pacingEnabled = 0;
};

::media::Result<FqQdiscEvidence> rootFqQdiscEvidence(
    std::uint32_t interfaceIndex) noexcept
{
    using Result = ::media::Result<FqQdiscEvidence>;
    MediaLinuxSocketProbeHandle netlink(
        ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE));
    if (netlink.get() < 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Linux Datagram qdisc probe socket failed", errno));
    }
    sockaddr_nl local{};
    local.nl_family = AF_NETLINK;
    if (::bind(netlink.get(), reinterpret_cast<const sockaddr*>(&local),
               sizeof(local)) != 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Linux Datagram qdisc probe bind failed", errno));
    }
    struct Request final {
        nlmsghdr header;
        tcmsg trafficControl;
    } request{};
    request.header.nlmsg_len = NLMSG_LENGTH(sizeof(tcmsg));
    request.header.nlmsg_type = RTM_GETQDISC;
    request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.header.nlmsg_seq = 1;
    request.trafficControl.tcm_family = AF_UNSPEC;
    sockaddr_nl kernel{};
    kernel.nl_family = AF_NETLINK;
    if (::sendto(netlink.get(), &request, request.header.nlmsg_len, 0,
                 reinterpret_cast<const sockaddr*>(&kernel),
                 sizeof(kernel)) < 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Linux Datagram qdisc probe request failed", errno));
    }

    std::array<std::byte, 16 * 1024> response{};
    for (;;) {
        const auto received = ::recv(
            netlink.get(), response.data(), response.size(), 0);
        if (received < 0) {
            if (errno == EINTR) continue;
            return Result::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram qdisc probe response failed", errno));
        }
        if (received == 0) {
            return Result::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram qdisc probe closed before completion"));
        }
        auto remaining = static_cast<int>(received);
        for (auto* message = reinterpret_cast<nlmsghdr*>(response.data());
             NLMSG_OK(message, remaining);
             message = NLMSG_NEXT(message, remaining)) {
            if (message->nlmsg_seq != request.header.nlmsg_seq) continue;
            if (message->nlmsg_type == NLMSG_DONE) {
                return Result::success(FqQdiscEvidence{});
            }
            if (message->nlmsg_type == NLMSG_ERROR) {
                if (message->nlmsg_len < NLMSG_LENGTH(sizeof(nlmsgerr))) {
                    return Result::failure(::media::ErrorInfo::ioFailure(
                        "Linux Datagram qdisc probe returned truncated error metadata"));
                }
                const auto* error = reinterpret_cast<const nlmsgerr*>(
                    NLMSG_DATA(message));
                return Result::failure(::media::ErrorInfo::ioFailure(
                    "Linux Datagram qdisc probe was rejected",
                    error->error < 0 ? -error->error : error->error));
            }
            if (message->nlmsg_type != RTM_NEWQDISC ||
                message->nlmsg_len < NLMSG_LENGTH(sizeof(tcmsg))) {
                continue;
            }
            const auto* qdisc = reinterpret_cast<const tcmsg*>(
                NLMSG_DATA(message));
            if (qdisc->tcm_ifindex != static_cast<int>(interfaceIndex) ||
                qdisc->tcm_parent != TC_H_ROOT) {
                continue;
            }
            const rtattr* options = nullptr;
            bool kindIsFq = false;
            auto attributesLength = TCA_PAYLOAD(message);
            for (auto* attribute = TCA_RTA(qdisc);
                 RTA_OK(attribute, attributesLength);
                 attribute = RTA_NEXT(attribute, attributesLength)) {
                if (attribute->rta_type == TCA_KIND) {
                    const auto payload = RTA_PAYLOAD(attribute);
                    const auto* kind = static_cast<const char*>(
                        RTA_DATA(attribute));
                    kindIsFq = payload == 3 &&
                        std::memcmp(kind, "fq\0", 3) == 0;
                } else if (attribute->rta_type == TCA_OPTIONS) {
                    options = attribute;
                }
            }
            if (!kindIsFq || !options) {
                return Result::success(FqQdiscEvidence{});
            }
            FqQdiscEvidence evidence;
            evidence.found = true;
            auto optionsLength = RTA_PAYLOAD(options);
            for (auto* option = static_cast<rtattr*>(RTA_DATA(options));
                 RTA_OK(option, optionsLength);
                 option = RTA_NEXT(option, optionsLength)) {
                if (RTA_PAYLOAD(option) != sizeof(std::uint32_t)) continue;
                std::uint32_t value = 0;
                std::memcpy(&value, RTA_DATA(option), sizeof(value));
                if (option->rta_type == TCA_FQ_QUANTUM) {
                    evidence.quantum = value;
                } else if (option->rta_type == TCA_FQ_INITIAL_QUANTUM) {
                    evidence.initialQuantum = value;
                } else if (option->rta_type == TCA_FQ_RATE_ENABLE) {
                    evidence.pacingEnabled = value;
                }
            }
            return Result::success(evidence);
        }
        if (remaining != 0) {
            return Result::failure(::media::ErrorInfo::ioFailure(
                "Linux Datagram qdisc probe returned malformed netlink data"));
        }
    }
}

::media::Status probeSocketPacing(
    std::uint64_t maximumWireBytesPerSecond) noexcept
{
#ifdef SO_MAX_PACING_RATE
    MediaLinuxSocketProbeHandle socketHandle(
        ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP));
    if (socketHandle.get() < 0) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "Linux SO_MAX_PACING_RATE probe socket failed", errno));
    }
    if (::setsockopt(socketHandle.get(), SOL_SOCKET, SO_MAX_PACING_RATE,
                     &maximumWireBytesPerSecond,
                     sizeof(maximumWireBytesPerSecond)) != 0) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "Linux SO_MAX_PACING_RATE capability set failed", errno));
    }
    std::uint64_t readback = 0;
    socklen_t size = sizeof(readback);
    if (::getsockopt(socketHandle.get(), SOL_SOCKET, SO_MAX_PACING_RATE,
                     &readback, &size) != 0) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "Linux SO_MAX_PACING_RATE capability read failed", errno));
    }
    if (size != sizeof(readback) || readback != maximumWireBytesPerSecond) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "Linux SO_MAX_PACING_RATE capability lacks exact set/get evidence"));
    }
    return ::media::Status::success();
#else
    (void)maximumWireBytesPerSecond;
    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "Linux headers do not expose SO_MAX_PACING_RATE"));
#endif
}

} // namespace

::media::Result<MediaDatagramExecutionCapability>
MediaDatagramExecutionCapabilityProbe::scan(
    std::string_view serviceScopeId,
    std::uint64_t maximumWireBytesPerSecond,
    std::uint64_t maximumWireDatagramBytes) noexcept
{
    using Result = ::media::Result<MediaDatagramExecutionCapability>;
    if (maximumWireBytesPerSecond == 0 || maximumWireDatagramBytes == 0 ||
        maximumWireDatagramBytes >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::uint32_t>::max)())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Linux Datagram execution probe requires positive managed capacity"));
    }
    auto interfaceIndex = parseInterfaceIndex(serviceScopeId);
    if (!interfaceIndex) return Result::failure(interfaceIndex.error());
    auto qdisc = rootFqQdiscEvidence(interfaceIndex.value());
    if (!qdisc) return Result::failure(qdisc.error());
    const auto requiredQuantum = static_cast<std::uint32_t>(
        maximumWireDatagramBytes);
    if (!qdisc.value().found ||
        qdisc.value().quantum != requiredQuantum ||
        qdisc.value().initialQuantum != requiredQuantum ||
        qdisc.value().pacingEnabled != 1) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "Linux Datagram execution requires root sch_fq with pacing enabled and quantum/initial_quantum equal to the planner wire burst"));
    }
    auto socketPacing = probeSocketPacing(maximumWireBytesPerSecond);
    if (!socketPacing) return Result::failure(socketPacing.error());
    return Result::success(MediaDatagramExecutionCapability{
        MediaDatagramTransportExecutionKind::LinuxFqSocketPacing,
        "Linux rtnetlink root sch_fq with planner wire quantum plus exact socket(7) SO_MAX_PACING_RATE set/get",
        maximumWireBytesPerSecond});
}

} // namespace media::ffmpeg::graph

#endif
