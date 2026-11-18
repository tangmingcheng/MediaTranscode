#include "internal/graph/protocol/rtp/MediaRtpVideoParameterSetValidator.h"

#include "internal/graph/protocol/codec/MediaH264SpsCodedSizeParser.h"
#include "internal/graph/protocol/codec/MediaVideoNalUnitScanner.h"
#include "internal/graph/protocol/codec/MediaHevcSpsCodedSizeParser.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

struct ParameterBinding final {
    const MediaRtpVideoSignalingFacts& expected;
    bool parameterSetsOnly;
    bool vps = false;
    bool sps = false;
    bool pps = false;
    bool conflict = false;

    static void visit(void* opaque, std::span<const std::uint8_t> nal)
    {
        auto& self = *static_cast<ParameterBinding*>(opaque);
        while (!nal.empty() && nal.back() == 0) nal = nal.first(nal.size() - 1);
        if (nal.empty()) { self.conflict = true; return; }
        const std::vector<std::uint8_t>* expected = nullptr;
        if (const auto* h264 = std::get_if<MediaH264SignalingFacts>(&self.expected)) {
            switch (nal.front() & 0x1f) {
            case 7: expected = &h264->sps; self.sps = true; break;
            case 8: expected = &h264->pps; self.pps = true; break;
            default: self.conflict = self.conflict || self.parameterSetsOnly; return;
            }
        } else {
            const auto& hevc = std::get<MediaHevcSignalingFacts>(self.expected);
            switch ((nal.front() >> 1) & 0x3f) {
            case 32: expected = &hevc.vps; self.vps = true; break;
            case 33: expected = &hevc.sps; self.sps = true; break;
            case 34: expected = &hevc.pps; self.pps = true; break;
            default: self.conflict = self.conflict || self.parameterSetsOnly; return;
            }
        }
        self.conflict = self.conflict ||
            !std::equal(nal.begin(), nal.end(), expected->begin(), expected->end());
    }
};

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

::media::Status MediaRtpVideoParameterSetValidator::validateBinding(
    std::span<const std::uint8_t> bytes, const MediaRtpVideoSignalingFacts& facts,
    bool parameterSetsOnly)
{
    const bool h264 = std::holds_alternative<MediaH264SignalingFacts>(facts);
    ParameterBinding binding{facts, parameterSetsOnly};
    auto scanned = MediaVideoNalUnitScanner::visit(bytes,
        h264 ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc,
        MediaEncodedPacketLayout::startCodeDelimited(), &binding, ParameterBinding::visit);
    if (!scanned) return scanned;
    if (binding.conflict || (parameterSetsOnly &&
        (!binding.sps || !binding.pps || (!h264 && !binding.vps))))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP parameter sets differ from the authoritative source signaling"));
    return ::media::Status::success();
}

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
        std::optional<MediaRational> sampleAspectRatio;
        auto codedSize = MediaH264SpsCodedSizeParser::parse(
            h264.sps, nullptr, &sampleAspectRatio);
        if (!codedSize) {
            return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
                codedSize.error());
        }
        return ::media::Result<MediaRtpVideoParameterSetInfo>::success(
            MediaRtpVideoParameterSetInfo{codedSize.value(),
                sampleAspectRatio ? *sampleAspectRatio : MediaRational{}});
    }
    const auto& hevc = std::get<MediaHevcSignalingFacts>(facts);
    std::optional<MediaRational> sampleAspectRatio;
    auto codedSize = MediaHevcSpsCodedSizeParser::parse(
        hevc.sps, nullptr, &sampleAspectRatio);
    if (!codedSize) {
        return ::media::Result<MediaRtpVideoParameterSetInfo>::failure(
            codedSize.error());
    }
    return ::media::Result<MediaRtpVideoParameterSetInfo>::success(
        MediaRtpVideoParameterSetInfo{codedSize.value(),
            sampleAspectRatio ? *sampleAspectRatio : MediaRational{}});
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
