#include "internal/graph/protocol/rtp/MediaRtpVideoParameterSetValidator.h"

#include "internal/graph/protocol/rtp/MediaH264SpsCodedSizeParser.h"
#include "internal/graph/protocol/rtp/MediaHevcSpsCodedSizeParser.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
}

#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

struct CodecContextDeleter final {
    void operator()(AVCodecContext* context) const noexcept
    {
        avcodec_free_context(&context);
    }
};

using CodecContextOwner =
    std::unique_ptr<AVCodecContext, CodecContextDeleter>;

void appendAnnexBNal(std::vector<std::uint8_t>& output,
                     std::span<const std::uint8_t> nal)
{
    output.insert(output.end(), {0, 0, 0, 1});
    output.insert(output.end(), nal.begin(), nal.end());
}

::media::Result<std::vector<std::uint8_t>> annexBParameterSets(
    const std::string& codecName,
    const MediaRtpVideoSignalingFacts& facts)
{
    std::vector<std::uint8_t> output;
    if (codecName == "h264") {
        const auto* h264 = std::get_if<MediaH264SignalingFacts>(&facts);
        if (!h264) {
            return ::media::Result<std::vector<std::uint8_t>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "H264 parameter-set facts conflict with codec identity"));
        }
        appendAnnexBNal(output, h264->sps);
        appendAnnexBNal(output, h264->pps);
        return ::media::Result<std::vector<std::uint8_t>>::success(
            std::move(output));
    }
    if (codecName == "hevc") {
        const auto* hevc = std::get_if<MediaHevcSignalingFacts>(&facts);
        if (!hevc) {
            return ::media::Result<std::vector<std::uint8_t>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "HEVC parameter-set facts conflict with codec identity"));
        }
        appendAnnexBNal(output, hevc->vps);
        appendAnnexBNal(output, hevc->sps);
        appendAnnexBNal(output, hevc->pps);
        return ::media::Result<std::vector<std::uint8_t>>::success(
            std::move(output));
    }
    return ::media::Result<std::vector<std::uint8_t>>::failure(
        ::media::ErrorInfo::unsupported(
            "RTP video parameter-set validation supports only H264 and HEVC"));
}

} // namespace

::media::Result<MediaRtpVideoParameterSetInfo>
MediaRtpVideoParameterSetValidator::inspect(
    const std::string& requestedCodecName,
    const MediaRtpVideoSignalingFacts& facts)
{
    const std::string codecName = canonicalCodecName(requestedCodecName);
    const AVCodecID codecId = codecName == "h264"
        ? AV_CODEC_ID_H264
        : (codecName == "hevc" ? AV_CODEC_ID_HEVC : AV_CODEC_ID_NONE);
    if (codecId == AV_CODEC_ID_NONE) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            ::media::ErrorInfo::unsupported(
            "RTP video parameter-set validation codec is unsupported"));
    }
    auto annexB = annexBParameterSets(codecName, facts);
    if (!annexB) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            annexB.error());
    }

    const AVCodec* decoder = avcodec_find_decoder(codecId);
    if (!decoder) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            ::media::ErrorInfo::unsupported(
            "RTP video parameter-set validation decoder is unavailable"));
    }
    CodecContextOwner context(avcodec_alloc_context3(decoder));
    if (!context) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            ::media::ErrorInfo::allocationFailed(
            "RTP video parameter-set validation context allocation failed"));
    }
    const std::size_t paddedSize = annexB.value().size() +
        AV_INPUT_BUFFER_PADDING_SIZE;
    context->extradata = static_cast<std::uint8_t*>(av_mallocz(paddedSize));
    if (!context->extradata) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            ::media::ErrorInfo::allocationFailed(
            "RTP video parameter-set validation extradata allocation failed"));
    }
    std::memcpy(context->extradata, annexB.value().data(),
                annexB.value().size());
    context->extradata_size = static_cast<int>(annexB.value().size());
    const int opened = avcodec_open2(context.get(), decoder, nullptr);
    if (opened < 0) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            FFmpegGraphError::fromCode(
                opened, "RTP video parameter-set validation"));
    }
    if (codecName == "h264") {
        const auto& h264 = std::get<MediaH264SignalingFacts>(facts);
        auto codedSize = MediaH264SpsCodedSizeParser::parse(h264.sps);
        if (!codedSize) {
            return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
                codedSize.error());
        }
        return ::media::Result<MediaRtpVideoParameterSetInfo>::success(
            MediaRtpVideoParameterSetInfo{codedSize.value()});
    }
    const auto& hevc = std::get<MediaHevcSignalingFacts>(facts);
    auto codedSize = MediaHevcSpsCodedSizeParser::parse(hevc.sps);
    if (!codedSize) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            codedSize.error());
    }
    return ::media::Result<MediaRtpVideoParameterSetInfo>::success(
        MediaRtpVideoParameterSetInfo{codedSize.value()});
}

::media::Status MediaRtpVideoParameterSetValidator::validate(
    const std::string& requestedCodecName,
    const MediaRtpVideoSignalingFacts& facts)
{
    auto inspected = inspect(requestedCodecName, facts);
    return inspected ? ::media::Status::success()
                     : ::media::Status::failure(inspected.error());
}

} // namespace media::ffmpeg::graph
