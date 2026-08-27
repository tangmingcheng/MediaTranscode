#include "internal/graph/model/MediaEncoderRateControlPlan.h"

namespace media::ffmpeg::graph {

::media::Result<MediaEncoderRateControlRequest>
MediaEncoderRateControlRequest::create(
    MediaRateControlMode mode,
    std::optional<int> target,
    std::optional<int> minimum,
    std::optional<int> maximum,
    std::optional<int> buffer)
{
    using Result = ::media::Result<MediaEncoderRateControlRequest>;
    if (mode == MediaRateControlMode::Cbr) {
        if (!target || minimum || maximum || buffer) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "CBR facts require only target bitrate"));
        }
        return Result::success(MediaEncoderRateControlRequest(
            MediaEncoderCbrRateControlFacts{*target}));
    }
    if (mode == MediaRateControlMode::Vbr) {
        if (!target || !minimum || !maximum || buffer) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "VBR facts require only minimum, target, and maximum bitrate"));
        }
        return Result::success(MediaEncoderRateControlRequest(
            MediaEncoderVbrRateControlFacts{
                *minimum, *target, *maximum}));
    }
    return Result::success(MediaEncoderRateControlRequest(
        MediaEncoderGeneralRateControlFacts{
            mode, target, minimum, maximum, buffer}));
}

MediaRateControlMode MediaEncoderRateControlRequest::mode() const noexcept
{
    if (std::holds_alternative<MediaEncoderCbrRateControlFacts>(m_facts)) {
        return MediaRateControlMode::Cbr;
    }
    if (std::holds_alternative<MediaEncoderVbrRateControlFacts>(m_facts)) {
        return MediaRateControlMode::Vbr;
    }
    return std::get<MediaEncoderGeneralRateControlFacts>(m_facts).mode;
}

std::optional<int> MediaEncoderRateControlRequest::targetBitrateKbps() const noexcept
{
    return std::visit([](const auto& facts) -> std::optional<int> {
        return facts.targetBitrateKbps;
    }, m_facts);
}

std::optional<int> MediaEncoderRateControlRequest::minimumBitrateKbps() const noexcept
{
    if (const auto* vbr = std::get_if<MediaEncoderVbrRateControlFacts>(&m_facts)) {
        return vbr->minimumBitrateKbps;
    }
    if (const auto* general = std::get_if<MediaEncoderGeneralRateControlFacts>(&m_facts)) {
        return general->minimumBitrateKbps;
    }
    return std::nullopt;
}

std::optional<int> MediaEncoderRateControlRequest::maximumBitrateKbps() const noexcept
{
    if (const auto* vbr = std::get_if<MediaEncoderVbrRateControlFacts>(&m_facts)) {
        return vbr->maximumBitrateKbps;
    }
    if (const auto* general = std::get_if<MediaEncoderGeneralRateControlFacts>(&m_facts)) {
        return general->maximumBitrateKbps;
    }
    return std::nullopt;
}

std::optional<int> MediaEncoderRateControlRequest::bufferSizeKbits() const noexcept
{
    const auto* general =
        std::get_if<MediaEncoderGeneralRateControlFacts>(&m_facts);
    return general ? general->bufferSizeKbits : std::nullopt;
}

::media::Status MediaEncoderRateControlRequest::setPlannerDerivedTargetBitrate(
    int bitrateKbps) noexcept
{
    auto* general = std::get_if<MediaEncoderGeneralRateControlFacts>(&m_facts);
    if (!general || general->targetBitrateKbps || bitrateKbps <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "only an unspecified general rate-control request accepts a derived target"));
    }
    general->targetBitrateKbps = bitrateKbps;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
