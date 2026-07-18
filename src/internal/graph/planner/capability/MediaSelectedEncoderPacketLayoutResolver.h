#pragma once

#include "internal/graph/planner/MediaPipelinePlanner.h"

namespace media::ffmpeg::graph {

class MediaSelectedEncoderPacketLayoutResolver final {
public:
    static ::media::Result<MediaEncodedPacketLayout> resolve(
        const MediaPipelinePlan& videoPlan)
    {
        if (videoPlan.branchMode != MediaBranchMode::TranscodeFrame ||
            videoPlan.selected.encoder.ffmpegName.empty()) {
            return ::media::Result<MediaEncodedPacketLayout>::failure(
                ::media::ErrorInfo::unsupported(
                    "Project MPEG-TS output requires a selected video encoder with an authoritative encoded packet layout"));
        }
        if (!videoPlan.selected.encoder.encodedPacketLayout) {
            return ::media::Result<MediaEncodedPacketLayout>::failure(
                ::media::ErrorInfo::unsupported(
                    "Selected video encoder does not publish an authoritative encoded packet layout for project MPEG-TS output"));
        }
        return ::media::Result<MediaEncodedPacketLayout>::success(
            *videoPlan.selected.encoder.encodedPacketLayout);
    }

    MediaSelectedEncoderPacketLayoutResolver() = delete;
};

} // namespace media::ffmpeg::graph
