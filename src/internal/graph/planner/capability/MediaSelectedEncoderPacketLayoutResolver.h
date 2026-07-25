#pragma once

#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <string_view>

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
                    "Output protocol requires a selected video encoder with an authoritative encoded packet layout"));
        }
        if (!videoPlan.selected.encoder.encodedPacketLayout) {
            return ::media::Result<MediaEncodedPacketLayout>::failure(
                ::media::ErrorInfo::unsupported(
                    "Selected video encoder does not publish an authoritative encoded packet layout"));
        }
        return ::media::Result<MediaEncodedPacketLayout>::success(
            *videoPlan.selected.encoder.encodedPacketLayout);
    }

    static ::media::Result<MediaEncodedPacketLayout> require(
        const MediaPipelinePlan& videoPlan,
        MediaEncodedPacketLayoutKind requiredKind,
        std::string_view consumer)
    {
        auto resolved = resolve(videoPlan);
        if (!resolved) return resolved;
        if (resolved.value().kind() != requiredKind || consumer.empty()) {
            return ::media::Result<MediaEncodedPacketLayout>::failure(
                ::media::ErrorInfo::unsupported(
                    std::string(consumer) +
                    " requires a different authoritative encoder packet layout"));
        }
        return resolved;
    }

    MediaSelectedEncoderPacketLayoutResolver() = delete;
};

} // namespace media::ffmpeg::graph
