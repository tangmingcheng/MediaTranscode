#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaEncoderCbrRateControlFacts final {
    int targetBitrateKbps = 0;
    int bufferSizeKbits = 0;
};

struct MediaEncoderVbrRateControlFacts final {
    int minimumBitrateKbps = 0;
    int targetBitrateKbps = 0;
    int maximumBitrateKbps = 0;
    std::optional<int> bufferSizeKbits;
};

struct MediaEncoderGeneralRateControlFacts final {
    MediaRateControlMode mode = MediaRateControlMode::Auto;
    std::optional<int> targetBitrateKbps;
    std::optional<int> minimumBitrateKbps;
    std::optional<int> maximumBitrateKbps;
    std::optional<int> bufferSizeKbits;
};

class MediaEncoderRateControlRequest final {
public:
    using Facts = std::variant<MediaEncoderGeneralRateControlFacts,
        MediaEncoderCbrRateControlFacts, MediaEncoderVbrRateControlFacts>;

    MediaEncoderRateControlRequest() = default;
    static ::media::Result<MediaEncoderRateControlRequest> create(
        MediaRateControlMode mode,
        std::optional<int> targetBitrateKbps,
        std::optional<int> minimumBitrateKbps,
        std::optional<int> maximumBitrateKbps,
        std::optional<int> bufferSizeKbits);

    MediaRateControlMode mode() const noexcept;
    std::optional<int> targetBitrateKbps() const noexcept;
    std::optional<int> minimumBitrateKbps() const noexcept;
    std::optional<int> maximumBitrateKbps() const noexcept;
    std::optional<int> bufferSizeKbits() const noexcept;
    ::media::Status setPlannerDerivedTargetBitrate(int bitrateKbps) noexcept;

private:
    explicit MediaEncoderRateControlRequest(Facts facts) noexcept
        : m_facts(std::move(facts)) {}
    Facts m_facts = MediaEncoderGeneralRateControlFacts{};
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
