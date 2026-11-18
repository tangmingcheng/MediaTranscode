#pragma once

#include "internal/graph/model/MediaEncoderRateControlPlan.h"
#include "internal/graph/model/MediaGraphTypes.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaEncoderOutputBuffering {
    BackendManaged,
    Immediate
};

struct MediaEncoderOpenContract final {
    std::string codecName;
    int width = 0;
    int height = 0;
    MediaRational frameRate;
    MediaEncoderRateControlPlan rateControl;
    std::optional<int> quality;
    std::string preset;
    std::string tune;
    std::string profile;
    std::string level;
    std::optional<int> gop;
    std::optional<int> bFrames;
    std::optional<bool> globalHeader;
    bool lowLatency = false;
    MediaEncoderOutputBuffering outputBuffering = MediaEncoderOutputBuffering::BackendManaged;

    friend bool operator==(
        const MediaEncoderOpenContract&,
        const MediaEncoderOpenContract&) = default;
};

} // namespace media::ffmpeg::graph
