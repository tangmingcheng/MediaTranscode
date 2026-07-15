#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"

#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavutil/channel_layout.h>
}

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
        std::string("MPEG-TS H.264 codec configuration ") + reason);
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
    if (bytes.size() < 7 || bytes[0] != 1 ||
        (bytes[4] & 0xFC) != 0xFC || (bytes[5] & 0xE0) != 0xE0) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("is not a complete AVCDecoderConfigurationRecord"));
    }
    const auto nalLengthBytes = static_cast<std::uint8_t>((bytes[4] & 3) + 1);
    if (nalLengthBytes != plan.h264NalLengthBytes) {
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
        MediaTsH264InputLayout::LengthPrefixed,
        nalLengthBytes,
        canonicalParameterSet(sps.value()),
        canonicalParameterSet(pps.value()));
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
    std::vector<std::uint8_t> sps;
    std::vector<std::uint8_t> pps;
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
        const std::uint8_t type = payload[0] & 0x1F;
        if (type == 7 && sps.empty()) {
            sps = canonicalParameterSet(payload);
        } else if (type == 8 && pps.empty()) {
            pps = canonicalParameterSet(payload);
        } else {
            return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
                invalidVideoConfig("must contain exactly one SPS and one PPS"));
        }
        start = next;
    }
    if (sps.empty() || pps.empty()) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            invalidVideoConfig("must contain exactly one SPS and one PPS"));
    }
    return MediaTsMaterializedVideoConfig::create(
        MediaTsH264InputLayout::AnnexB,
        plan.h264NalLengthBytes,
        std::move(sps),
        std::move(pps));
}

} // namespace

::media::Result<MediaTsMaterializedVideoConfig>
MediaTsFfmpegStreamConfigMaterializer::video(
    const MediaTsMuxPlan& plan,
    const AVCodecParameters& parameters)
{
    if (parameters.codec_type != AVMEDIA_TYPE_VIDEO ||
        parameters.codec_id != AV_CODEC_ID_H264) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS video materializer requires H.264 codec parameters"));
    }
    auto bytes = extradata(parameters);
    if (!bytes) {
        return ::media::Result<MediaTsMaterializedVideoConfig>::failure(
            bytes.error());
    }
    switch (plan.parameters().h264InputLayout) {
    case MediaTsH264InputLayout::LengthPrefixed:
        return parseAvcc(plan.parameters(), bytes.value());
    case MediaTsH264InputLayout::AnnexB:
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
    auto asc = parseMediaAacAudioSpecificConfig(bytes.value());
    if (!asc) {
        return ::media::Result<MediaTsMaterializedAudioConfig>::failure(
            asc.error());
    }
    const auto& planned = plan.parameters().aac;
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
