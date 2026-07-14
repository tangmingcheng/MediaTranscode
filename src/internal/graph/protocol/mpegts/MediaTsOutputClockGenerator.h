#pragma once

#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsOutputClockPolicy final {
    MediaRunningTime pcrInterval;
    MediaRunningTime maximumPcrGap;
    MediaRunningTime maximumPcrJitter;
    int timestampTimeBaseNumerator;
    int timestampTimeBaseDenominator;
};

struct MediaTsPacketClock final {
    std::int64_t extendedPts;
    std::int64_t extendedDts;
    std::uint64_t wirePts;
    std::uint64_t wireDts;
};

struct MediaTsPcrClock final {
    std::uint64_t generation;
    MediaRunningTime masterTime;
    std::int64_t extended27Mhz;
    std::uint64_t wire27Mhz;
};

class MediaTsOutputClockGenerator final {
public:
    static ::media::Result<MediaTsOutputClockGenerator> create(
        MediaTsOutputClockPolicy policy,
        MediaPlaybackEpoch epoch);

    ::media::Result<MediaTsPacketClock> project(
        std::uint64_t generation,
        MediaScheduledStream stream,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime dispatchOnMaster,
        MediaRunningTime emitOnMaster,
        MediaRunningTime transportDecodeLead);
    ::media::Result<std::vector<MediaTsPcrClock>> advancePcrThrough(
        std::uint64_t generation,
        MediaRunningTime masterTime);
    ::media::Status validateSerializedPcr(
        const MediaTsPcrClock& planned,
        std::int64_t serializedExtended27Mhz) const;

    const MediaTsOutputClockPolicy& policy() const noexcept { return m_policy; }
    const MediaPlaybackEpoch& epoch() const noexcept { return m_epoch; }

private:
    MediaTsOutputClockGenerator(
        MediaTsOutputClockPolicy policy,
        MediaPlaybackEpoch epoch) noexcept;

    ::media::Result<std::int64_t> outputNanoseconds(
        MediaRunningTime masterTime) const;
    ::media::Result<std::int64_t> timestampTicks(
        MediaRunningTime masterTime) const;
    ::media::Result<std::int64_t> pcrTicks(
        MediaRunningTime masterTime) const;
    ::media::Status validateGeneration(std::uint64_t generation) const;

    MediaTsOutputClockPolicy m_policy;
    MediaPlaybackEpoch m_epoch;
    std::optional<std::int64_t> m_lastVideoExtendedDts;
    std::optional<std::int64_t> m_lastAudioExtendedDts;
    std::optional<MediaRunningTime> m_lastPcrMasterTime;
};

} // namespace media::ffmpeg::graph
