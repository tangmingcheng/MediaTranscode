#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaProjectMpegTsOutputPlan final {
public:
    static ::media::Result<MediaProjectMpegTsOutputPlan> create(
        const std::string& videoCodecName,
        const MediaEncodedPacketLayout& videoPacketLayout,
        const MediaResolvedAudioOutputPlan& audioOutput,
        MediaRunningTime transportDecodeLead,
        MediaRunningTime startupEmissionPreroll,
        MediaOutputTransportKind transportKind,
        std::uint8_t maximumPacketsPerDatagram);
    static ::media::Result<MediaProjectMpegTsOutputPlan> fromEncodedFacts(
        int audioSampleRate,
        MediaTsMuxPlan muxPlan);

    int audioSampleRate() const noexcept;
    const MediaTsMuxPlan& muxPlan() const noexcept;

private:
    MediaProjectMpegTsOutputPlan(int audioSampleRate, MediaTsMuxPlan muxPlan);

    int m_audioSampleRate;
    MediaTsMuxPlan m_muxPlan;
};

} // namespace media::ffmpeg::graph
