#pragma once

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaTsH264InputLayout : std::uint8_t {
    AnnexB = 0,
    LengthPrefixed = 1
};

enum class MediaTsParameterSetPolicy : std::uint8_t {
    Never = 0,
    BeforeRandomAccess = 1
};

struct MediaTsContinuitySeeds final {
    std::uint8_t pat;
    std::uint8_t pmt;
    std::uint8_t video;
    std::uint8_t audio;
    friend bool operator==(const MediaTsContinuitySeeds&,
                           const MediaTsContinuitySeeds&) = default;
};

struct MediaTsAacAdtsPlan final {
    std::uint8_t mpegId;
    std::uint8_t audioObjectType;
    std::uint8_t samplingFrequencyIndex;
    std::uint8_t channelConfiguration;
    friend bool operator==(const MediaTsAacAdtsPlan&,
                           const MediaTsAacAdtsPlan&) = default;
};

struct MediaTsMuxPlanParameters final {
    std::uint16_t transportStreamId;
    std::uint16_t programNumber;
    std::uint16_t patPid;
    std::uint16_t programMapPid;
    std::uint16_t videoPid;
    std::uint16_t audioPid;
    std::uint16_t pcrPid;
    std::uint8_t tableVersion;
    MediaRunningTime psiRepeatInterval;
    std::uint8_t videoStreamType;
    std::uint8_t audioStreamType;
    MediaTsH264InputLayout h264InputLayout;
    std::uint8_t h264NalLengthBytes;
    MediaTsParameterSetPolicy parameterSetPolicy;
    MediaTsAacAdtsPlan aac;
    MediaTsOutputClockPolicy clock;
    MediaRunningTime transportDecodeLead;
    MediaRunningTime startupEmissionPreroll;
    std::uint16_t packetSize;
    MediaTsContinuitySeeds continuity;
    std::uint8_t maximumPacketsPerDatagram;
    MediaOutputTransportKind transportKind;
    int maximumAudioAccessUnitSamples;
    friend bool operator==(const MediaTsMuxPlanParameters&,
                           const MediaTsMuxPlanParameters&) = default;
};

class MediaTsMuxPlan final {
public:
    static ::media::Result<MediaTsMuxPlan> create(
        MediaTsMuxPlanParameters parameters);
    static ::media::Result<std::uint8_t> maximumPacketsPerRtpDatagram(
        std::size_t maximumDatagramBytes);

    const MediaTsMuxPlanParameters& parameters() const noexcept;
    const MediaTsOutputClockPolicy& clockPolicy() const noexcept;
    MediaRunningTime transportDecodeLead() const noexcept;
    MediaRunningTime startupEmissionPreroll() const noexcept;

private:
    explicit MediaTsMuxPlan(MediaTsMuxPlanParameters parameters) noexcept;

    MediaTsMuxPlanParameters m_parameters;
};

} // namespace media::ffmpeg::graph
