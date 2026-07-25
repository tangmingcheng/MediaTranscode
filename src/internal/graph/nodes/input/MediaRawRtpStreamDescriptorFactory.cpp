#include "internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h"

#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <cstring>

namespace media::ffmpeg::graph {
namespace {

::media::Result<void> setExtradata(AVCodecParameters& parameters, const std::vector<uint8_t>& bytes)
{
    if (bytes.empty()) return ::media::Result<void>::success();
    parameters.extradata = static_cast<uint8_t*>(av_mallocz(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!parameters.extradata) return ::media::Result<void>::failure(
        ::media::ErrorInfo::allocationFailed("raw RTP codec extradata allocation failed"));
    std::memcpy(parameters.extradata, bytes.data(), bytes.size());
    parameters.extradata_size = static_cast<int>(bytes.size());
    return ::media::Result<void>::success();
}

::media::Result<std::vector<uint8_t>> annexBParameterSets(const std::string& text)
{
    std::vector<uint8_t> result;
    std::size_t offset = 0;
    while (offset <= text.size()) {
        const std::size_t comma = text.find(',', offset);
        auto decoded = decodeRtpFmtpBase64(text.substr(offset, comma == std::string::npos ? std::string::npos : comma - offset));
        if (!decoded || decoded.value().empty()) return ::media::Result<std::vector<uint8_t>>::failure(
            decoded ? ::media::ErrorInfo::invalidArgument("RTP parameter set is empty") : decoded.error());
        result.insert(result.end(), {0, 0, 0, 1});
        result.insert(result.end(), decoded.value().begin(), decoded.value().end());
        if (comma == std::string::npos) break;
        offset = comma + 1;
    }
    return ::media::Result<std::vector<uint8_t>>::success(std::move(result));
}

::media::Result<std::vector<uint8_t>> codecExtradata(const MediaRtpDepacketizerConfig& config)
{
    auto fmtp = parseRtpFmtp(config.fmtp);
    if (!fmtp) return ::media::Result<std::vector<uint8_t>>::failure(fmtp.error());
    if (config.codecName == "aac") {
        const auto found = fmtp.value().find("config");
        if (found == fmtp.value().end()) return ::media::Result<std::vector<uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument("AAC RTP requires config extradata"));
        return decodeRtpFmtpHex(found->second);
    }
    if (config.codecName == "h264") {
        const auto found = fmtp.value().find("sprop-parameter-sets");
        if (found == fmtp.value().end()) return ::media::Result<std::vector<uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument("H264 RTP requires parameter sets"));
        return annexBParameterSets(found->second);
    }
    if (config.codecName == "hevc") {
        std::vector<uint8_t> result;
        for (const char* key : {"sprop-vps", "sprop-sps", "sprop-pps"}) {
            const auto found = fmtp.value().find(key);
            if (found == fmtp.value().end()) return ::media::Result<std::vector<uint8_t>>::failure(
                ::media::ErrorInfo::invalidArgument(std::string("HEVC RTP requires ") + key));
            auto set = annexBParameterSets(found->second);
            if (!set) return ::media::Result<std::vector<uint8_t>>::failure(set.error());
            result.insert(result.end(), set.value().begin(), set.value().end());
        }
        return ::media::Result<std::vector<uint8_t>>::success(std::move(result));
    }
    if (config.codecName == "opus") {
        std::vector<uint8_t> head{'O','p','u','s','H','e','a','d',1,static_cast<uint8_t>(config.channels),0,0,0x80,0xbb,0,0,0,0,0};
        return ::media::Result<std::vector<uint8_t>>::success(std::move(head));
    }
    return ::media::Result<std::vector<uint8_t>>::failure(
        ::media::ErrorInfo::unsupported("raw RTP descriptor codec unsupported"));
}

AVCodecID codecId(const std::string& codec) noexcept
{
    if (codec == "h264") return AV_CODEC_ID_H264;
    if (codec == "hevc") return AV_CODEC_ID_HEVC;
    if (codec == "aac") return AV_CODEC_ID_AAC;
    if (codec == "opus") return AV_CODEC_ID_OPUS;
    return AV_CODEC_ID_NONE;
}

} // namespace

::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>> MediaRawRtpStreamDescriptorFactory::create(
    const MediaRtpDepacketizerConfig& config)
{
    auto validator = MediaRtpDepacketizerFactory::create(config);
    if (!validator) return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(validator.error());
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(
        ::media::ErrorInfo::allocationFailed("raw RTP codec parameters allocation failed"));
    parameters->codec_type = config.streamKind == MediaStreamKind::Video ? AVMEDIA_TYPE_VIDEO : AVMEDIA_TYPE_AUDIO;
    parameters->codec_id = codecId(config.codecName);
    if (config.streamKind == MediaStreamKind::Audio) {
        parameters->sample_rate = config.clockRate;
        av_channel_layout_default(&parameters->ch_layout, config.channels);
    }
    auto extradata = codecExtradata(config);
    if (!extradata) return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(extradata.error());
    if (auto status = setExtradata(*parameters, extradata.value()); !status) return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(status.error());

    FFmpegInputStreamSnapshot stream;
    stream.index = 0;
    stream.streamKind = config.streamKind;
    auto ownedSnapshot = FFmpegCodecParametersSnapshot::takeOwnership(std::move(parameters));
    if (!ownedSnapshot) {
        return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(ownedSnapshot.error());
    }
    stream.codec = std::move(ownedSnapshot).value();
    stream.time.timeBase = MediaRational{1, config.clockRate};
    stream.time.realtimeClockDriven = true;
    stream.format.streamKind = config.streamKind;
    stream.format.streamIndex = 0;
    stream.format.codec.codecName = config.codecName;
    stream.format.codec.domain = config.streamKind == MediaStreamKind::Video ? MediaCodecDomain::Video : MediaCodecDomain::Audio;
    stream.format.codec.operation = MediaCodecOperation::Decode;
    stream.format.isInput = true;
    stream.format.isRealtime = true;
    stream.format.time = stream.time;
    if (config.streamKind == MediaStreamKind::Audio) {
        stream.format.audio.sampleRate = config.clockRate;
        stream.format.audio.channels = config.channels;
    }
    std::vector<FFmpegInputStreamSnapshot> streams;
    streams.push_back(std::move(stream));
    return FFmpegFormatContextBuffer::createSnapshot(std::move(streams));
}

} // namespace media::ffmpeg::graph
