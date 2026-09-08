#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRtcpReportingFacts final {
    std::uint32_t maximumSessionMembers = 0;
    std::uint32_t activeSenders = 0;
    std::uint64_t sessionBandwidthBytesPerSecond = 0;
    std::uint64_t compoundPacketBytes = 0;
    std::string membershipAuthority;
    std::string bandwidthAuthority;
    friend bool operator==(const MediaRtcpReportingFacts&,
                           const MediaRtcpReportingFacts&) = default;
};

class MediaRtcpReportingPolicy final {
public:
    static ::media::Result<MediaRtcpReportingPolicy> create(
        MediaRtcpReportingFacts facts,
        MediaRunningTime initialBaseInterval,
        MediaRunningTime steadyBaseInterval,
        MediaRunningTime minimumAdmissionInterval,
        std::string standardsAuthority);

    const MediaRtcpReportingFacts& facts() const noexcept;
    MediaRunningTime initialBaseInterval() const noexcept;
    MediaRunningTime steadyBaseInterval() const noexcept;
    MediaRunningTime minimumAdmissionInterval() const noexcept;
    const std::string& standardsAuthority() const noexcept;

    friend bool operator==(const MediaRtcpReportingPolicy&,
                           const MediaRtcpReportingPolicy&) = default;

private:
    MediaRtcpReportingPolicy(
        MediaRtcpReportingFacts facts,
        MediaRunningTime initialBaseInterval,
        MediaRunningTime steadyBaseInterval,
        MediaRunningTime minimumAdmissionInterval,
        std::string standardsAuthority) noexcept;

    MediaRtcpReportingFacts m_facts;
    MediaRunningTime m_initialBaseInterval;
    MediaRunningTime m_steadyBaseInterval;
    MediaRunningTime m_minimumAdmissionInterval;
    std::string m_standardsAuthority;
};

} // namespace media::ffmpeg::graph
