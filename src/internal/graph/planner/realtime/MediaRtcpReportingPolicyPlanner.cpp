#include "internal/graph/planner/realtime/MediaRtcpReportingPolicyPlanner.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/protocol/rtp/MediaRtcpInterval.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t RtcpBandwidthPercent = 5;
constexpr std::uint64_t SenderBandwidthPercent = 25;
constexpr std::int64_t InitialMinimumIntervalNs = 2'500'000'000;
constexpr std::int64_t SteadyMinimumIntervalNs = 5'000'000'000;
constexpr const char* RfcAuthority =
    "RFC 3550 sections 6.2 and 6.3.1";

} // namespace

::media::Result<MediaRtcpReportingPolicy>
MediaRtcpReportingPolicyPlanner::plan(
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaNumericIpAddress& remoteAddress,
    std::uint64_t sessionBandwidthBytesPerSecond,
    std::string bandwidthAuthority,
    std::uint64_t compoundPacketBytes)
{
    const auto& encoded = deployment.encode();
    std::uint32_t members = 2;
    std::string membershipAuthority = "unicast RTP endpoint topology";
    if (remoteAddress.isMulticast()) {
        if (!encoded.rtcpSession) {
            return ::media::Result<MediaRtcpReportingPolicy>::failure(
                ::media::ErrorInfo::notInitialized(
                    "multicast RTP requires authoritative maximum RTCP session members"));
        }
        members = encoded.rtcpSession->maximumSessionMembers;
        membershipAuthority = encoded.rtcpSession->authority;
    }
    auto rtcpBandwidth = MediaCheckedArithmetic::ceilScale(
        sessionBandwidthBytesPerSecond, RtcpBandwidthPercent, 100,
        "RTCP session bandwidth share");
    auto senderBandwidth = rtcpBandwidth
        ? MediaCheckedArithmetic::ceilScale(
              rtcpBandwidth.value(), SenderBandwidthPercent, 100,
              "RTCP sender bandwidth share")
        : rtcpBandwidth;
    auto memberBytes = MediaCheckedArithmetic::multiply(
        compoundPacketBytes, members, "RTCP member-weighted compound bytes");
    auto nominalNs = senderBandwidth && memberBytes
        ? MediaCheckedArithmetic::ceilScale(
              memberBytes.value(), NanosecondsPerSecond,
              senderBandwidth.value(), "RTCP deterministic interval")
        : (!senderBandwidth ? senderBandwidth : memberBytes);
    if (!nominalNs || nominalNs.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRtcpReportingPolicy>::failure(
            !nominalNs ? nominalNs.error() :
            ::media::ErrorInfo::invalidArgument(
                "RTCP deterministic interval exceeds running-time range"));
    }
    const auto nominal = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(nominalNs.value()));
    const auto initial = (std::max)(
        nominal, MediaRunningTime::fromNanoseconds(InitialMinimumIntervalNs));
    const auto steady = (std::max)(
        nominal, MediaRunningTime::fromNanoseconds(SteadyMinimumIntervalNs));
    auto compensatedLower = mediaRtcpIntervalForFactor(initial, MediaRtcpRandomFactorMinimum);
    if (!compensatedLower)
        return ::media::Result<MediaRtcpReportingPolicy>::failure(compensatedLower.error());
    return MediaRtcpReportingPolicy::create(
        {members, 1, sessionBandwidthBytesPerSecond, compoundPacketBytes,
         std::move(membershipAuthority), std::move(bandwidthAuthority)},
        initial, steady,
        compensatedLower.value(),
        RfcAuthority);
}

} // namespace media::ffmpeg::graph
