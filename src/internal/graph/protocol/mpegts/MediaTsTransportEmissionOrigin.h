#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

namespace media::ffmpeg::graph {

inline ::media::Result<MediaRunningTime> mediaTsTransportEmissionOrigin(
    const MediaTsMuxPlan& plan,
    const MediaPlaybackEpoch& epoch)
{
    auto decodeOrigin = epoch.masterRelease.checkedSubtract(
        plan.transportDecodeLead());
    if (!decodeOrigin) {
        return ::media::Result<MediaRunningTime>::failure(
            decodeOrigin.error());
    }
    auto origin = decodeOrigin.value().checkedSubtract(
        plan.startupEmissionPreroll());
    if (!origin) {
        return ::media::Result<MediaRunningTime>::failure(origin.error());
    }
    if (origin.value().nanoseconds() < 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS playback epoch has insufficient transport decode lead"));
    }
    return origin;
}

} // namespace media::ffmpeg::graph
