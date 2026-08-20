#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"

#include "internal/graph/nodes/mux/MediaFfmpegAacAscDialectMaterializer.h"

#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavutil/channel_layout.h>
}

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

using ByteSpan = std::span<const std::uint8_t>;

::media::ErrorInfo invalidVideoConfig(const char* reason)
{
    return ::media::ErrorInfo::invalidArgument(
        std::string("MPEG-TS video codec configuration ") + reason);
}

::media::Result<ByteSpan> extradata(const AVCodecParameters& parameters)
{
    if (!parameters.extradata || parameters.extradata_size <= 0) {
        return ::media::Result<ByteSpan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg codec parameters require non-empty extradata"));
    }
    return ::media::Result<ByteSpan>::success(ByteSpan(
        parameters.extradata,
        static_cast<std::size_t>(parameters.extradata_size)));
}

std::vector<std::uint8_t> canonicalParameterSet(ByteSpan bytes)
{
    std::vector<std::uint8_t> result{0, 0, 0, 1};
    result.insert(result.end(), bytes.begin(), bytes.end());
    return result;
}

::media::Result<ByteSpan> readAvccNal(ByteSpan bytes, std::size_t& offset)
{
    if (offset + 2 > bytes.size()) {
        return ::media::Result<ByteSpan>::failure(
            invalidVideoConfig("contains a truncated NAL length"));
    }
    const std::size_t size =
        (std::size_t{bytes[offset]} << 8) | bytes[offset + 1];
    offset += 2;
    if (size == 0 || size > bytes.size() - offset) {
        return ::media::Result<ByteSpan>::failure(
            invalidVideoConfig("contains a truncated NAL payload"));
    }
    const auto payload = bytes.subspan(offset, size);
    offset += size;
    return ::media::Result<ByteSpan>::success(payload);
}

bool avccProfileHasExtension(std::uint8_t profile) noexcept
{
    switch (profile) {
    case 44:
    case 83:
    case 86:
    case 100:
    case 110:
    case 118:
    case 122:
    case 128:
    case 134:
    case 135:
    case 138:
    case 139:
    case 144:
        return true;
    default:
        return false;
    }
}

::media::Status validateAvccExtension(ByteSpan bytes,
                                      std::size_t& offset,
                                      std::uint8_t profile)
{
    if (offset == bytes.size()) return ::media::Status::success();
    if (!avccProfileHasExtension(profile) || offset + 4 > bytes.size() ||
        (bytes[offset] & 0xFC) != 0xFC ||
        (bytes[offset + 1] & 0xF8) != 0xF8 ||
        (bytes[offset + 2] & 0xF8) != 0xF8) {
        return ::media::Status::failure(
            invalidVideoConfig("has an invalid AVC extension"));
    }
    const std::size_t extensionCount = bytes[offset + 3];
    offset += 4;
    for (std::size_t index = 0; index < extensionCount; ++index) {
        auto extension = readAvccNal(bytes, offset);
        if (!extension) return ::media::Status::failure(extension.error());
    }
    if (offset != bytes.size()) {
        return ::media::Status::failure(
            invalidVideoConfig("has trailing bytes"));
    }
    return ::media::Status::success();
}

::media::Result<MediaTsMaterializedVideoConfig> parseAvcc(
    const MediaTsMuxPlanParameters& plan,
    ByteSpan bytes)
{
    if (plan.video.codec() != MediaTsVideoCodec::H264 ||
        plan.video.layout() != MediaTsNalLayout::LengthPrefixed ||
        bytes.size() < 7 || bytes[0] != 1 ||
        (bytes[4] & 0xFC) != 0xFC || (bytes[5] & 0xE0) != 0xE0) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("is not a complete AVCDecoderConfigurationRecord"));
    }
    const auto nalLengthBytes = static_cast<std::uint8_t>((bytes[4] & 3) + 1);
    if (nalLengthBytes != plan.video.nalLengthBytes()) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("NAL length width conflicts with the plan"));
    }
    if ((bytes[5] & 0x1F) != 1) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("must contain exactly one SPS"));
    }
    std::size_t offset = 6;
    auto sps = readAvccNal(bytes, offset);
    if (!sps || sps.value().empty() || (sps.value()[0] & 0x1F) != 7) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            sps ? invalidVideoConfig("SPS has the wrong NAL type") : sps.error());
    }
    if (offset >= bytes.size() || bytes[offset++] != 1) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("must contain exactly one PPS"));
    }
    auto pps = readAvccNal(bytes, offset);
    if (!pps || pps.value().empty() || (pps.value()[0] & 0x1F) != 8) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            pps ? invalidVideoConfig("PPS has the wrong NAL type") : pps.error());
    }
    auto extension = validateAvccExtension(bytes, offset, bytes[1]);
    if (!extension) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            extension.error());
    }
    return MediaTsMaterializedVideoConfig::create(
        plan.video,
        std::vector<std::vector<std::uint8_t>>{
            canonicalParameterSet(sps.value()),
            canonicalParameterSet(pps.value())});
}

::media::Result<MediaTsMaterializedVideoConfig> parseHvcc(
    const MediaTsMuxPlanParameters& plan,
    ByteSpan bytes)
{
    if (plan.video.codec() != MediaTsVideoCodec::Hevc ||
        plan.video.layout() != MediaTsNalLayout::LengthPrefixed ||
        bytes.size() < 23 || bytes[0] != 1 ||
        (bytes[13] & 0xF0) != 0xF0 ||
        (bytes[15] & 0xFC) != 0xFC ||
        (bytes[16] & 0xFC) != 0xFC ||
        (bytes[17] & 0xF8) != 0xF8 ||
        (bytes[18] & 0xF8) != 0xF8) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig(
                "is not a complete HEVCDecoderConfigurationRecord"));
    }
    const auto nalLengthBytes =
        static_cast<std::uint8_t>((bytes[21] & 3) + 1);
    if (nalLengthBytes != plan.video.nalLengthBytes()) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("NAL length width conflicts with the plan"));
    }
    std::array<std::vector<std::uint8_t>, 3> parameterSets;
    const std::size_t arrayCount = bytes[22];
    std::size_t offset = 23;
    for (std::size_t arrayIndex = 0; arrayIndex < arrayCount; ++arrayIndex) {
        if (bytes.size() - offset < 3) {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                invalidVideoConfig("contains a truncated HEVC array"));
        }
        const std::uint8_t header = bytes[offset++];
        if ((header & 0x40) != 0) {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                invalidVideoConfig("contains a nonzero reserved HEVC array bit"));
        }
        const std::uint8_t nalType = header & 0x3F;
        const std::size_t nalCount =
            (std::size_t{bytes[offset]} << 8) | bytes[offset + 1];
        offset += 2;
        if (nalType < 32 || nalType > 34 || nalCount != 1 ||
            !parameterSets[nalType - 32].empty()) {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                invalidVideoConfig(
                    "must contain exactly one VPS, one SPS, and one PPS"));
        }
        auto nal = readAvccNal(bytes, offset);
        if (!nal || nal.value().size() < 2 ||
            (nal.value()[0] & 0x80) != 0 ||
            ((nal.value()[0] >> 1) & 0x3F) != nalType ||
            (nal.value()[1] & 0x07) == 0) {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                nal ? invalidVideoConfig("HEVC parameter set has an invalid NAL header")
                    : nal.error());
        }
        parameterSets[nalType - 32] = canonicalParameterSet(nal.value());
    }
    if (offset != bytes.size() || parameterSets[0].empty() ||
        parameterSets[1].empty() || parameterSets[2].empty()) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig(
                "must contain exactly one VPS, one SPS, and one PPS without trailing bytes"));
    }
    return MediaTsMaterializedVideoConfig::create(
        plan.video,
        std::vector<std::vector<std::uint8_t>>{
            std::move(parameterSets[0]), std::move(parameterSets[1]),
            std::move(parameterSets[2])});
}

struct StartCode final {
    std::size_t offset;
    std::size_t size;
};

std::optional<StartCode> findStartCode(ByteSpan bytes, std::size_t from) noexcept
{
    for (std::size_t index = from; index + 3 <= bytes.size(); ++index) {
        if (bytes[index] != 0 || bytes[index + 1] != 0) continue;
        if (index + 4 <= bytes.size() && bytes[index + 2] == 0 &&
            bytes[index + 3] == 1) {
            return StartCode{index, 4};
        }
        if (bytes[index + 2] == 1) return StartCode{index, 3};
    }
    return std::nullopt;
}

::media::Result<MediaTsMaterializedVideoConfig> parseAnnexB(
    const MediaTsMuxPlanParameters& plan,
    ByteSpan bytes)
{
    auto start = findStartCode(bytes, 0);
    if (!start || start->offset != 0) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("is not an Annex-B parameter-set configuration"));
    }
    const MediaTsVideoCodec codec = plan.video.codec();
    std::vector<std::vector<std::uint8_t>> parameterSets(
        codec == MediaTsVideoCodec::H264 ? 2 : 3);
    while (start) {
        const std::size_t payloadOffset = start->offset + start->size;
        const auto next = findStartCode(bytes, payloadOffset);
        const std::size_t payloadEnd = next ? next->offset : bytes.size();
        if (payloadOffset >= payloadEnd) {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                invalidVideoConfig("contains an empty Annex-B NAL"));
        }
        const ByteSpan payload = bytes.subspan(
            payloadOffset, payloadEnd - payloadOffset);
        if ((payload[0] & 0x80) != 0 ||
            (codec == MediaTsVideoCodec::Hevc &&
             (payload.size() < 2 || (payload[1] & 0x07) == 0))) {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                invalidVideoConfig("contains an invalid Annex-B NAL header"));
        }
        const std::uint8_t type = codec == MediaTsVideoCodec::H264
            ? payload[0] & 0x1F : (payload[0] >> 1) & 0x3F;
        const int parameterIndex = codec == MediaTsVideoCodec::H264
            ? (type == 7 ? 0 : type == 8 ? 1 : -1)
            : (type >= 32 && type <= 34 ? type - 32 : -1);
        if (parameterIndex < 0 ||
            !parameterSets[static_cast<std::size_t>(parameterIndex)].empty()) {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                invalidVideoConfig(
                    "contains an unexpected or duplicate Annex-B parameter set"));
        }
        parameterSets[static_cast<std::size_t>(parameterIndex)] =
            canonicalParameterSet(payload);
        start = next;
    }
    if (std::any_of(
            parameterSets.begin(), parameterSets.end(),
            [](const auto& bytes) { return bytes.empty(); })) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("is missing a required Annex-B parameter set"));
    }
    return MediaTsMaterializedVideoConfig::create(
        plan.video, std::move(parameterSets));
}

} // namespace

::media::Result<MediaTsMaterializedVideoConfig>
MediaTsFfmpegStreamConfigMaterializer::video(
    const MediaTsMuxPlan& plan,
    const AVCodecParameters& parameters)
{
    const auto& contract = plan.parameters().video;
    const AVCodecID expectedCodec = contract.codec() == MediaTsVideoCodec::H264
        ? AV_CODEC_ID_H264 : AV_CODEC_ID_HEVC;
    if (parameters.codec_type != AVMEDIA_TYPE_VIDEO ||
        parameters.codec_id != expectedCodec) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video materializer codec parameters conflict with the plan"));
    }
    auto bytes = extradata(parameters);
    if (!bytes) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            bytes.error());
    }
    switch (plan.parameters().video.layout()) {
    case MediaTsNalLayout::LengthPrefixed:
        return contract.codec() == MediaTsVideoCodec::H264
            ? parseAvcc(plan.parameters(), bytes.value())
            : parseHvcc(plan.parameters(), bytes.value());
    case MediaTsNalLayout::AnnexB:
        return parseAnnexB(plan.parameters(), bytes.value());
    default:
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS plan has an invalid H.264 input layout"));
    }
}

::media::Result<MediaTsMaterializedAudioConfig>
MediaTsFfmpegStreamConfigMaterializer::audio(
    const MediaTsMuxPlan& plan,
    const AVCodecParameters& parameters)
{
    if (parameters.codec_type != AVMEDIA_TYPE_AUDIO ||
        parameters.codec_id != AV_CODEC_ID_AAC) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS audio materializer requires AAC codec parameters"));
    }
    auto bytes = extradata(parameters);
    if (!bytes) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            bytes.error());
    }
    auto canonicalAsc = MediaFfmpegAacAscDialectMaterializer::canonicalize(
        bytes.value());
    if (!canonicalAsc) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            canonicalAsc.error());
    }
    auto asc = parseMediaAacAudioSpecificConfig(canonicalAsc.value());
    if (!asc) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            asc.error());
    }
    const auto* program = plan.audioVideoProgram();
    if (!program) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS audio materializer rejects a VideoOnly mux plan"));
    }
    const auto& planned = program->aac;
    if (planned.audioObjectType != asc.value().audioObjectType ||
        planned.samplingFrequencyIndex != asc.value().samplingFrequencyIndex ||
        planned.channelConfiguration != asc.value().channelConfiguration ||
        parameters.sample_rate != asc.value().sampleRate ||
        av_channel_layout_check(&parameters.ch_layout) != 1 ||
        parameters.ch_layout.nb_channels != asc.value().channels) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS AAC ASC, plan, sample rate, and channel layout conflict"));
    }
    return MediaTsMaterializedAudioConfig::create(
        asc.value().audioObjectType,
        asc.value().samplingFrequencyIndex,
        asc.value().channelConfiguration);
}

} // namespace media::ffmpeg::graph
