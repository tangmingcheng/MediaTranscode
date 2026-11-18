#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlan.h"

namespace media::ffmpeg::graph {

struct MediaTsPrefixReportFacts final {
    std::uint64_t endpointId;
    std::size_t cnameBytes;
    MediaRunningTime minimumReportInterval;
};

// Peak-rate serialization demand. A known batch reserves one possible SR;
// unprepared ranges use canonical release coverage and the RTCP interval.
// Both are conservative admission, not a claim that the SR is always due.
class MediaTsPrefixServicePlan final {
public:
    static ::media::Result<MediaTsPrefixServicePlan> create(
        const MediaTsDatagramEmissionPlan& emission,
        const MediaDatagramShapingPlan& shaping,
        std::uint64_t mediaEndpointId,
        std::optional<MediaTsPrefixReportFacts> report);
    ::media::Result<MediaRunningTime> durationForPayload(std::size_t payloadBytes,
        std::size_t materializationBatches) const;
    ::media::Result<MediaRunningTime> durationForReleaseRange(std::size_t payloadBytes,
        std::size_t possiblePrefixes, MediaRunningTime firstRelease, MediaRunningTime lastRelease) const;
    ::media::Result<MediaRunningTime> durationForSerialized(std::uint64_t endpointId,
        std::uint64_t regularBytes, std::uint64_t lastBytes, std::size_t datagrams) const;
    std::size_t materializationBatches(std::size_t datagrams) const noexcept;
    std::size_t maximumPayloadBytes() const noexcept { return m_maximumPayload; }
    std::size_t maximumBatchDatagrams() const noexcept { return m_maximumBatchDatagrams; }

private:
    ::media::Result<MediaRunningTime> durationWithReports(std::size_t payloadBytes, std::size_t reports) const;
    struct Endpoint final {
        std::uint64_t id;
        std::uint64_t networkOverhead;
        std::uint64_t maximumPayload;
    };
    MediaTsPrefixServicePlan(std::uint64_t rate, std::size_t maximumPayload,
        std::size_t packetBytes, std::uint64_t overhead, MediaRunningTime reportDuration,
        Endpoint media, std::optional<Endpoint> control, std::size_t maximumBatchDatagrams,
        std::optional<MediaRunningTime> minimumReportInterval)
        : m_rate(rate), m_maximumPayload(maximumPayload), m_packetBytes(packetBytes),
          m_overhead(overhead), m_reportDuration(reportDuration), m_media(media),
          m_control(control), m_maximumBatchDatagrams(maximumBatchDatagrams),
          m_minimumReportInterval(minimumReportInterval) {}
    std::uint64_t m_rate;
    std::size_t m_maximumPayload;
    std::size_t m_packetBytes;
    std::uint64_t m_overhead;
    MediaRunningTime m_reportDuration;
    Endpoint m_media;
    std::optional<Endpoint> m_control;
    std::size_t m_maximumBatchDatagrams;
    std::optional<MediaRunningTime> m_minimumReportInterval;
};

} // namespace media::ffmpeg::graph
