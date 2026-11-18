#include "internal/graph/protocol/mpegts/MediaTsVideoAccessUnitFramer.h"

#include "internal/graph/protocol/codec/MediaAnnexBAccessUnitValidator.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

using FramedResult = ::media::Result<std::span<const std::uint8_t>>;
constexpr std::size_t AnnexBOutputPrefixBytes = 4;

std::size_t nalHeaderBytes(MediaTsVideoCodec codec) noexcept
{
    return codec == MediaTsVideoCodec::H264 ? 1U : 2U;
}

FramedResult invalid(const char* message)
{
    return FramedResult::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

bool checkedAdd(std::size_t left,
                std::size_t right,
                std::size_t& output) noexcept
{
    if (right > std::numeric_limits<std::size_t>::max() - left) return false;
    output = left + right;
    return true;
}

bool validNalHeader(std::span<const std::uint8_t> nal,
                    MediaTsVideoCodec codec) noexcept
{
    if (nal.size() < nalHeaderBytes(codec)) return false;
    if (codec == MediaTsVideoCodec::H264) {
        const std::uint8_t type = nal[0] & 0x1F;
        return (nal[0] & 0x80) == 0 && type != 0 && type <= 23;
    }
    const std::uint8_t type = (nal[0] >> 1) & 0x3F;
    return (nal[0] & 0x80) == 0 && type <= 47 &&
        (nal[1] & 0x07) != 0;
}

::media::Result<std::size_t> convertedSize(
    std::span<const std::uint8_t> payload,
    std::uint8_t lengthBytes,
    MediaTsVideoCodec codec)
{
    std::size_t offset = 0;
    std::size_t outputSize = 0;
    while (offset < payload.size()) {
        if (payload.size() - offset < lengthBytes) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS video NAL length field is truncated"));
        }
        std::size_t nalSize = 0;
        for (std::size_t index = 0; index < lengthBytes; ++index) {
            nalSize = (nalSize << 8) | payload[offset + index];
        }
        offset += lengthBytes;
        if (nalSize == 0 || nalSize > payload.size() - offset ||
            !validNalHeader(payload.subspan(offset, nalSize), codec)) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS video NAL length or header is invalid"));
        }
        if (!checkedAdd(outputSize, AnnexBOutputPrefixBytes, outputSize) ||
            !checkedAdd(outputSize, nalSize, outputSize)) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS video converted size overflows"));
        }
        offset += nalSize;
    }
    if (outputSize == 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video access unit is empty"));
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

std::size_t writeParameterSets(
    const MediaTsMaterializedVideoConfig& config,
    std::span<std::uint8_t> output)
{
    std::size_t offset = 0;
    for (const auto& parameterSet : config.parameterSetsAnnexB()) {
        std::copy(parameterSet.begin(), parameterSet.end(),
                  output.begin() + offset);
        offset += parameterSet.size();
    }
    return offset;
}

::media::Result<std::size_t> parameterSetBytes(
    const MediaTsMaterializedVideoConfig& config, bool inject)
{
    std::size_t size = 0;
    if (inject) {
        for (const auto& parameterSet : config.parameterSetsAnnexB()) {
            if (!checkedAdd(size, parameterSet.size(), size)) {
                return ::media::Result<std::size_t>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MPEG-TS video parameter-set size overflows"));
            }
        }
    }
    return ::media::Result<std::size_t>::success(size);
}

} // namespace

::media::Result<std::size_t>
MediaTsVideoAccessUnitFramer::maximumFramedPayloadBytes(
    const MediaTsMaterializedVideoConfig& config,
    std::size_t maximumInputPayloadBytes,
    bool injectParameterSets)
{
    using Result = ::media::Result<std::size_t>;
    if (maximumInputPayloadBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS video payload bound must be positive"));
    }
    std::size_t size = maximumInputPayloadBytes;
    const auto& contract = config.contract();
    if (contract.layout() == MediaTsNalLayout::LengthPrefixed) {
        const std::size_t lengthBytes = contract.nalLengthBytes();
        // Every accepted NAL consumes its length field and at least its header.
        const std::size_t maximumNals = size /
            (lengthBytes + nalHeaderBytes(contract.codec()));
        const std::size_t growthPerNal = AnnexBOutputPrefixBytes - lengthBytes;
        if (growthPerNal != 0 && maximumNals >
                (std::numeric_limits<std::size_t>::max() - size) / growthPerNal) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS converted video payload bound overflows"));
        }
        size += maximumNals * growthPerNal;
    }
    auto injection = parameterSetBytes(config, injectParameterSets);
    if (!injection) return Result::failure(injection.error());
    if (!checkedAdd(size, injection.value(), size)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS framed video payload bound overflows"));
    }
    return Result::success(size);
}

FramedResult MediaTsVideoAccessUnitFramer::frame(
    const MediaTsMuxPlan& plan,
    const MediaTsMaterializedVideoConfig& config,
    std::span<const std::uint8_t> payload,
    bool randomAccess,
    std::vector<std::uint8_t>& workspace)
{
    const auto& parameters = plan.parameters();
    if (config.contract() != parameters.video) {
        return invalid("MPEG-TS materialized video config mismatches the plan");
    }
    if (payload.empty()) return invalid("MPEG-TS video access unit is empty");

    const bool inject = randomAccess &&
        parameters.parameterSetPolicy ==
            MediaTsParameterSetPolicy::BeforeRandomAccess;
    auto injection = parameterSetBytes(config, inject);
    if (!injection) return FramedResult::failure(injection.error());
    const std::size_t injectionSize = injection.value();

    if (parameters.video.layout() == MediaTsNalLayout::AnnexB) {
        const auto validatorCodec =
            parameters.video.codec() == MediaTsVideoCodec::H264
                ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc;
        if (!MediaAnnexBAccessUnitValidator::validate(
                payload, validatorCodec)) {
            return invalid("MPEG-TS Annex-B video access unit is malformed");
        }
        if (!inject) {
            workspace.clear();
            return FramedResult::success(payload);
        }
        std::size_t totalSize = 0;
        if (!checkedAdd(injectionSize, payload.size(), totalSize)) {
            return invalid("MPEG-TS framed video size overflows");
        }
        workspace.resize(totalSize);
        const std::size_t offset = writeParameterSets(config, workspace);
        std::copy(payload.begin(), payload.end(), workspace.begin() + offset);
        return FramedResult::success(workspace);
    }
    if (parameters.video.layout() != MediaTsNalLayout::LengthPrefixed) {
        return invalid("MPEG-TS video input layout is invalid");
    }

    auto converted = convertedSize(
        payload, parameters.video.nalLengthBytes(),
        parameters.video.codec());
    if (!converted) {
        return invalid("MPEG-TS length-prefixed video access unit is malformed");
    }
    std::size_t totalSize = 0;
    if (!checkedAdd(injectionSize, converted.value(), totalSize)) {
        return invalid("MPEG-TS framed video size overflows");
    }
    workspace.resize(totalSize);
    if (inject) {
        writeParameterSets(config, workspace);
    }
    writeConverted(
        payload, parameters.video.nalLengthBytes(),
        std::span<std::uint8_t>(workspace).subspan(injectionSize));
    return FramedResult::success(workspace);
}

} // namespace media::ffmpeg::graph
