#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaProjectMpegTsOutputPlan final {
public:
    static ::media::Result<MediaProjectMpegTsOutputPlan> createVideoOnly(
        const std::string& videoCodecName,
        const MediaEncodedPacketLayout& videoPacketLayout,
        MediaRunningTime transportDecodeLead,
        MediaRunningTime startupEmissionPreroll,
        MediaOutputTransportKind transportKind,
        std::uint16_t maximumPacketsPerDatagram);
    static ::media::Result<MediaProjectMpegTsOutputPlan> createAudioVideo(
        const std::string& videoCodecName,
        const MediaEncodedPacketLayout& videoPacketLayout,
        const MediaResolvedAudioOutputPlan& audioOutput,
        MediaRunningTime transportDecodeLead,
        MediaRunningTime startupEmissionPreroll,
        MediaOutputTransportKind transportKind,
        std::uint16_t maximumPacketsPerDatagram);
    static ::media::Result<MediaProjectMpegTsOutputPlan>
    fromVideoOnlyEncodedFacts(MediaTsMuxPlan muxPlan);
    static ::media::Result<MediaProjectMpegTsOutputPlan>
    fromAudioVideoEncodedFacts(MediaTsMuxPlan muxPlan);

    const MediaTsMuxPlan& muxPlan() const noexcept;

private:
    explicit MediaProjectMpegTsOutputPlan(MediaTsMuxPlan muxPlan);

    MediaTsMuxPlan m_muxPlan;
};

} // namespace media::ffmpeg::graph
