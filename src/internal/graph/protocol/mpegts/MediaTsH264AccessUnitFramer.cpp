#include "internal/graph/protocol/mpegts/MediaTsH264AccessUnitFramer.h"

#include "internal/graph/protocol/codec/MediaH264AnnexBAccessUnitValidator.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaTsFramedAccessUnit> invalid(const char* message)
{
    return ::media::Result<MediaTsFramedAccessUnit>::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

bool checkedAdd(std::size_t left, std::size_t right, std::size_t& output) noexcept
{
    if (right > std::numeric_limits<std::size_t>::max() - left) return false;
    output = left + right;
    return true;
}

::media::Result<std::size_t> convertedSize(std::span<const std::uint8_t> payload,
                                           std::uint8_t lengthBytes)
{
    std::size_t offset = 0;
    std::size_t outputSize = 0;
    while (offset < payload.size()) {
        if (payload.size() - offset < lengthBytes) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS H.264 NAL length field is truncated"));
        }
        std::size_t nalSize = 0;
        for (std::size_t index = 0; index < lengthBytes; ++index) {
            nalSize = (nalSize << 8) | payload[offset + index];
        }
        offset += lengthBytes;
        if (nalSize == 0 || nalSize > payload.size() - offset) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS H.264 NAL length is invalid"));
        }
        if ((payload[offset] & 0x80) != 0) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS H.264 NAL forbidden_zero_bit is set"));
        }
        if (!checkedAdd(outputSize, 4, outputSize) ||
            !checkedAdd(outputSize, nalSize, outputSize)) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS H.264 converted size overflows"));
        }
        offset += nalSize;
    }
    if (outputSize == 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS H.264 access unit is empty"));
    }
    return ::media::Result<std::size_t>::success(outputSize);
}

void writeConverted(std::span<const std::uint8_t> payload,
                    std::uint8_t lengthBytes,
                    std::span<std::uint8_t> output)
{
    std::size_t inputOffset = 0;
    std::size_t outputOffset = 0;
    while (inputOffset < payload.size()) {
        std::size_t nalSize = 0;
        for (std::size_t index = 0; index < lengthBytes; ++index) {
            nalSize = (nalSize << 8) | payload[inputOffset + index];
        }
        inputOffset += lengthBytes;
        output[outputOffset++] = 0;
        output[outputOffset++] = 0;
        output[outputOffset++] = 0;
        output[outputOffset++] = 1;
        std::copy_n(payload.begin() + inputOffset, nalSize,
                    output.begin() + outputOffset);
        inputOffset += nalSize;
        outputOffset += nalSize;
    }
}

} // namespace

MediaTsFramedAccessUnit::MediaTsFramedAccessUnit(
    std::variant<std::span<const std::uint8_t>, std::vector<std::uint8_t>> storage)
    : m_storage(std::move(storage))
{
}

MediaTsFramedAccessUnit MediaTsFramedAccessUnit::borrowed(
    std::span<const std::uint8_t> bytes)
{
    return MediaTsFramedAccessUnit(bytes);
}

MediaTsFramedAccessUnit MediaTsFramedAccessUnit::owned(
    std::vector<std::uint8_t> bytes)
{
    return MediaTsFramedAccessUnit(std::move(bytes));
}

std::span<const std::uint8_t> MediaTsFramedAccessUnit::bytes() const noexcept
{
    if (const auto* borrowed =
            std::get_if<std::span<const std::uint8_t>>(&m_storage)) {
        return *borrowed;
    }
    const auto& owned = std::get<std::vector<std::uint8_t>>(m_storage);
    return owned;
}

::media::Result<MediaTsFramedAccessUnit> MediaTsH264AccessUnitFramer::frame(
    const MediaTsMuxPlan& plan,
    const MediaTsMaterializedVideoConfig& config,
    std::span<const std::uint8_t> payload,
    bool randomAccess)
{
    const auto& parameters = plan.parameters();
    if (config.layout() != parameters.h264InputLayout ||
        config.nalLengthBytes() != parameters.h264NalLengthBytes) {
        return invalid("MPEG-TS H.264 materialized config mismatches the plan");
    }
    if (payload.empty()) return invalid("MPEG-TS H.264 access unit is empty");

    const bool inject = randomAccess &&
        parameters.parameterSetPolicy ==
            MediaTsParameterSetPolicy::BeforeRandomAccess;
    std::size_t injectionSize = 0;
    if (inject) {
        if (!checkedAdd(config.spsAnnexB().size(), config.ppsAnnexB().size(),
                        injectionSize)) {
            return invalid("MPEG-TS H.264 parameter-set size overflows");
        }
    }

    if (parameters.h264InputLayout == MediaTsH264InputLayout::AnnexB) {
        auto valid = MediaH264AnnexBAccessUnitValidator::validate(payload);
        if (!valid) return invalid("MPEG-TS H.264 Annex-B access unit is malformed");
        if (!inject) {
            return ::media::Result<MediaTsFramedAccessUnit>::success(
                MediaTsFramedAccessUnit::borrowed(payload));
        }
        std::size_t totalSize = 0;
        if (!checkedAdd(injectionSize, payload.size(), totalSize)) {
            return invalid("MPEG-TS H.264 framed size overflows");
        }
        std::vector<std::uint8_t> output(totalSize);
        auto iterator = std::copy(config.spsAnnexB().begin(), config.spsAnnexB().end(),
                                  output.begin());
        iterator = std::copy(config.ppsAnnexB().begin(), config.ppsAnnexB().end(), iterator);
        std::copy(payload.begin(), payload.end(), iterator);
        return ::media::Result<MediaTsFramedAccessUnit>::success(
            MediaTsFramedAccessUnit::owned(std::move(output)));
    }
    if (parameters.h264InputLayout != MediaTsH264InputLayout::LengthPrefixed) {
        return invalid("MPEG-TS H.264 input layout is invalid");
    }

    auto converted = convertedSize(payload, parameters.h264NalLengthBytes);
    if (!converted) return invalid("MPEG-TS H.264 length-prefixed access unit is malformed");
    std::size_t totalSize = 0;
    if (!checkedAdd(injectionSize, converted.value(), totalSize)) {
        return invalid("MPEG-TS H.264 framed size overflows");
    }
    std::vector<std::uint8_t> output(totalSize);
    auto iterator = output.begin();
    if (inject) {
        iterator = std::copy(config.spsAnnexB().begin(), config.spsAnnexB().end(), iterator);
        iterator = std::copy(config.ppsAnnexB().begin(), config.ppsAnnexB().end(), iterator);
    }
    writeConverted(payload, parameters.h264NalLengthBytes,
                   std::span<std::uint8_t>(output).subspan(injectionSize));
    return ::media::Result<MediaTsFramedAccessUnit>::success(
        MediaTsFramedAccessUnit::owned(std::move(output)));
}

} // namespace media::ffmpeg::graph
