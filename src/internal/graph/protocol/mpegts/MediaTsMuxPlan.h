#pragma once

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/protocol/mpegts/MediaMpegTsTimingPolicy.h"
#include "internal/graph/protocol/mpegts/MediaTsVideoElementaryStreamContract.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <variant>

namespace media::ffmpeg::graph {

enum class MediaTsParameterSetPolicy : std::uint8_t {
    Never = 0,
    BeforeRandomAccess = 1
};

struct MediaTsVideoContinuitySeeds final {
    std::uint8_t pat;
    std::uint8_t pmt;
    std::uint8_t video;
    friend bool operator==(const MediaTsVideoContinuitySeeds&,
                           const MediaTsVideoContinuitySeeds&) = default;
};

struct MediaTsAacAdtsPlan final {
    std::uint8_t mpegId;
    std::uint8_t audioObjectType;
    std::uint8_t samplingFrequencyIndex;
    std::uint8_t channelConfiguration;
    friend bool operator==(const MediaTsAacAdtsPlan&,
                           const MediaTsAacAdtsPlan&) = default;
};

struct MediaTsVideoOnlyProgramPlan final {
    std::uint16_t videoPid;
    std::uint16_t pcrPid;
    std::uint8_t videoStreamType;
    MediaTsVideoContinuitySeeds continuity;
    friend bool operator==(const MediaTsVideoOnlyProgramPlan&,
                           const MediaTsVideoOnlyProgramPlan&) = default;
};

struct MediaTsAudioVideoContinuitySeeds final {
    std::uint8_t pat;
    std::uint8_t pmt;
    std::uint8_t video;
    std::uint8_t audio;
    friend bool operator==(const MediaTsAudioVideoContinuitySeeds&,
                           const MediaTsAudioVideoContinuitySeeds&) = default;
};

struct MediaTsAudioVideoProgramPlan final {
    std::uint16_t videoPid;
    std::uint16_t audioPid;
    std::uint16_t pcrPid;
    std::uint8_t videoStreamType;
    std::uint8_t audioStreamType;
    MediaTsAacAdtsPlan aac;
    MediaTsAudioVideoContinuitySeeds continuity;
    int maximumAudioAccessUnitSamples;
    friend bool operator==(const MediaTsAudioVideoProgramPlan&,
                           const MediaTsAudioVideoProgramPlan&) = default;
};

using MediaTsProgramPlan = std::variant<
    MediaTsVideoOnlyProgramPlan,
    MediaTsAudioVideoProgramPlan>;

struct MediaTsMuxPlanParameters final {
    std::uint16_t transportStreamId;
    std::uint16_t programNumber;
    std::uint16_t patPid;
    std::uint16_t programMapPid;
    std::uint8_t tableVersion;
    MediaMpegTsTimingPolicy timing;
    MediaTsProgramPlan program;
    MediaTsVideoElementaryStreamContract video;
    MediaTsParameterSetPolicy parameterSetPolicy;
    MediaRunningTime transportDecodeLead;
    MediaRunningTime startupEmissionPreroll;
    std::uint16_t packetSize;
    std::uint16_t maximumPacketsPerDatagram;
    MediaOutputTransportKind transportKind;
    friend bool operator==(const MediaTsMuxPlanParameters&,
                           const MediaTsMuxPlanParameters&) = default;
};

class MediaTsMuxPlan final {
public:
    static ::media::Result<MediaTsMuxPlan> create(
        MediaTsMuxPlanParameters parameters);
    static ::media::Result<std::uint16_t> maximumPacketsPerRtpDatagram(
        std::size_t maximumDatagramBytes);
    static ::media::Result<std::uint16_t> maximumPacketsPerDatagram(
        std::size_t maximumUdpPayloadBytes,
        MediaOutputTransportKind transportKind);

    const MediaTsMuxPlanParameters& parameters() const noexcept;
    const MediaTsVideoOnlyProgramPlan* videoOnlyProgram() const noexcept;
    const MediaTsAudioVideoProgramPlan* audioVideoProgram() const noexcept;
    std::uint16_t videoPid() const noexcept;
    std::uint16_t pcrPid() const noexcept;
    std::uint8_t videoStreamType() const noexcept;
    const MediaTsOutputClockPolicy& clockPolicy() const noexcept;
    const MediaMpegTsTimingPolicy& timingPolicy() const noexcept;
    MediaRunningTime transportDecodeLead() const noexcept;
    MediaRunningTime startupEmissionPreroll() const noexcept;

private:
    explicit MediaTsMuxPlan(MediaTsMuxPlanParameters parameters) noexcept;

    MediaTsMuxPlanParameters m_parameters;
    MediaTsOutputClockPolicy m_clockPolicy;
};

} // namespace media::ffmpeg::graph
