#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaEncoderRateControlRequest final {
    MediaRateControlMode mode = MediaRateControlMode::Auto;
    std::optional<int> targetBitrateKbps;
    std::optional<int> minimumBitrateKbps;
    std::optional<int> maximumBitrateKbps;
    std::optional<int> bufferSizeKbits;
};

struct MediaEncoderPrivateRateControlOption final {
    std::string name;
    std::string value;
    std::int64_t expectedNumericValue = 0;

    friend bool operator==(
        const MediaEncoderPrivateRateControlOption&,
        const MediaEncoderPrivateRateControlOption&) = default;
};

struct MediaEncoderRateControlPlan final {
    MediaRateControlMode mode = MediaRateControlMode::Auto;
    std::optional<int> targetBitrateKbps;
    std::optional<int> minimumBitrateKbps;
    std::optional<int> maximumBitrateKbps;
    std::optional<int> bufferSizeKbits;
    std::optional<MediaEncoderPrivateRateControlOption> privateOption;

    friend bool operator==(
        const MediaEncoderRateControlPlan&,
        const MediaEncoderRateControlPlan&) = default;
};

} // namespace media::ffmpeg::graph
