#include "internal/graph/protocol/rtp/MediaRtcpReportingPolicy.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaRtcpReportingPolicy::MediaRtcpReportingPolicy(
    MediaRtcpReportingFacts facts,
    MediaRunningTime initialBaseInterval,
    MediaRunningTime steadyBaseInterval,
    MediaRunningTime minimumAdmissionInterval,
    std::string standardsAuthority) noexcept
    : m_facts(std::move(facts)),
      m_initialBaseInterval(initialBaseInterval),
      m_steadyBaseInterval(steadyBaseInterval),
      m_minimumAdmissionInterval(minimumAdmissionInterval),
      m_standardsAuthority(std::move(standardsAuthority))
{
}

::media::Result<MediaRtcpReportingPolicy> MediaRtcpReportingPolicy::create(
    MediaRtcpReportingFacts facts,
    MediaRunningTime initialBaseInterval,
    MediaRunningTime steadyBaseInterval,
    MediaRunningTime minimumAdmissionInterval,
    std::string standardsAuthority)
{
    if (facts.maximumSessionMembers < 2 || facts.activeSenders == 0 ||
        facts.activeSenders > facts.maximumSessionMembers ||
        facts.sessionBandwidthBytesPerSecond == 0 ||
        facts.compoundPacketBytes == 0 || facts.membershipAuthority.empty() ||
        facts.bandwidthAuthority.empty() || standardsAuthority.empty() ||
        initialBaseInterval.nanoseconds() <= 0 ||
        steadyBaseInterval < initialBaseInterval ||
        minimumAdmissionInterval.nanoseconds() <= 0 ||
        minimumAdmissionInterval > initialBaseInterval) {
        return ::media::Result<MediaRtcpReportingPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTCP reporting policy requires authoritative membership, bandwidth, compound-size, and RFC interval facts"));
    }
    return ::media::Result<MediaRtcpReportingPolicy>::success(
        MediaRtcpReportingPolicy(
            std::move(facts), initialBaseInterval, steadyBaseInterval,
            minimumAdmissionInterval, std::move(standardsAuthority)));
}

const MediaRtcpReportingFacts&
MediaRtcpReportingPolicy::facts() const noexcept { return m_facts; }
MediaRunningTime MediaRtcpReportingPolicy::initialBaseInterval() const noexcept
{
    return m_initialBaseInterval;
}
MediaRunningTime MediaRtcpReportingPolicy::steadyBaseInterval() const noexcept
{
    return m_steadyBaseInterval;
}
MediaRunningTime MediaRtcpReportingPolicy::minimumAdmissionInterval() const noexcept
{
    return m_minimumAdmissionInterval;
}
const std::string& MediaRtcpReportingPolicy::standardsAuthority() const noexcept
{
    return m_standardsAuthority;
}

} // namespace media::ffmpeg::graph
