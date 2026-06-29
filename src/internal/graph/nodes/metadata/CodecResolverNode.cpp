#include "internal/graph/nodes/metadata/CodecResolverNode.h"

#include "internal/FFmpegRAII.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/metadata/CodecResolverHardwareFrames.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {
namespace {

int parseIntOption(const MediaNodeOptions* options, const std::string& key, int fallback)
{
    if (!options) {
        return fallback;
    }

    const std::string value = options->value(key);
    if (value.empty()) {
        return fallback;
    }

    return std::atoi(value.c_str());
}

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string fallback = {})
{
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

bool truthyOption(const MediaNodeOptions* options, const std::string& key)
{
    const std::string value = optionValue(options, key);
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

AVCodecID codecIdFromName(const std::string& codecName)
{
    if (codecName == "h264" || codecName == "avc") {
        return AV_CODEC_ID_H264;
    }
    if (codecName == "h265" || codecName == "hevc") {
        return AV_CODEC_ID_HEVC;
    }
    if (codecName == "mpeg4") {
        return AV_CODEC_ID_MPEG4;
    }
    if (codecName == "vp9") {
        return AV_CODEC_ID_VP9;
    }
    return AV_CODEC_ID_H264;
}

void addEncoderCandidate(std::vector<const AVCodec*>& candidates, const AVCodec* codec)
{
    if (!codec) {
        return;
    }

    if (std::find(candidates.begin(), candidates.end(), codec) == candidates.end()) {
        candidates.push_back(codec);
    }
}

std::vector<const AVCodec*> encoderCandidates(const MediaNodeOptions* options)
{
    std::vector<const AVCodec*> candidates;
    const std::string explicitEncoder = optionValue(options, "encoder");
    if (!explicitEncoder.empty() && explicitEncoder != "auto") {
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name(explicitEncoder.c_str()));
        return candidates;
    }

    const AVCodecID codecId = codecIdFromName(optionValue(options, "video_codec", "h264"));
    if (codecId == AV_CODEC_ID_H264) {
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("libx264"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_mf"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_nvenc"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_qsv"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_amf"));
    } else if (codecId == AV_CODEC_ID_HEVC) {
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("libx265"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_mf"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_nvenc"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_qsv"));
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name("hevc_amf"));
    }

    addEncoderCandidate(candidates, avcodec_find_encoder(codecId));
    return candidates;
}

bool pixelFormatSupported(const AVCodec* codec, AVPixelFormat format)
{
    if (!codec || !codec->pix_fmts || format == AV_PIX_FMT_NONE) {
        return true;
    }

    for (const AVPixelFormat* current = codec->pix_fmts; *current != AV_PIX_FMT_NONE; ++current) {
        if (*current == format) {
            return true;
        }
    }

    return false;
}

std::string pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? std::string(name) : std::string("unknown");
}

AVPixelFormat plannedEncoderHardwarePixelFormat(const MediaNodeOptions* options)
{
    if (optionValue(options, "encoder.pipeline.frame_kind") != "hardware") {
        return AV_PIX_FMT_NONE;
    }

    const std::string hwaccel = optionValue(options, "encoder.pipeline.hwaccel");
    const std::string device = optionValue(options, "encoder.pipeline.device");
    if (hwaccel == "cuda" || device == "cuda") {
        return AV_PIX_FMT_CUDA;
    }
    if (hwaccel == "qsv" || device == "qsv") {
        return AV_PIX_FMT_QSV;
    }
    if (hwaccel == "vaapi" || device == "vaapi") {
        return AV_PIX_FMT_VAAPI;
    }
    if (hwaccel == "d3d11va" || device == "d3d11va") {
        return AV_PIX_FMT_D3D11;
    }

    return AV_PIX_FMT_NONE;
}

AVPixelFormat plannedEncoderSoftwarePixelFormat(const MediaNodeOptions* options, AVPixelFormat sourceFormat)
{
    const AVPixelFormat hardwareFormat = plannedEncoderHardwarePixelFormat(options);
    if (hardwareFormat == AV_PIX_FMT_CUDA &&
        (sourceFormat == AV_PIX_FMT_YUV420P || sourceFormat == AV_PIX_FMT_NONE)) {
        return AV_PIX_FMT_NV12;
    }

    if (sourceFormat != AV_PIX_FMT_NONE) {
        return sourceFormat;
    }

    if (hardwareFormat == AV_PIX_FMT_CUDA || hardwareFormat == AV_PIX_FMT_D3D11) {
        return AV_PIX_FMT_NV12;
    }

    return AV_PIX_FMT_YUV420P;
}

AVPixelFormat chooseEncoderPixelFormat(const AVCodec* encoder,
                                       AVPixelFormat sourceFormat,
                                       const MediaNodeOptions* options)
{
    const AVPixelFormat plannedHardwareFormat = plannedEncoderHardwarePixelFormat(options);
    if (plannedHardwareFormat != AV_PIX_FMT_NONE) {
        return pixelFormatSupported(encoder, plannedHardwareFormat) ? plannedHardwareFormat : AV_PIX_FMT_NONE;
    }

    if (sourceFormat != AV_PIX_FMT_NONE && pixelFormatSupported(encoder, sourceFormat)) {
        return sourceFormat;
    }

    if (pixelFormatSupported(encoder, AV_PIX_FMT_YUV420P)) {
        return AV_PIX_FMT_YUV420P;
    }

    if (encoder && encoder->pix_fmts && encoder->pix_fmts[0] != AV_PIX_FMT_NONE) {
        return encoder->pix_fmts[0];
    }

    return sourceFormat != AV_PIX_FMT_NONE ? sourceFormat : AV_PIX_FMT_YUV420P;
}

void setPrivateOption(AVCodecContext* context, const std::string& key, const std::string& value)
{
    if (!context || !context->priv_data || key.empty() || value.empty()) {
        return;
    }

    av_opt_set(context->priv_data, key.c_str(), value.c_str(), 0);
}

AVRational resolveFrameRate(AVFormatContext* formatContext, AVStream* stream, const MediaNodeOptions* options)
{
    const int fpsNum = parseIntOption(options, "fps_num", 0);
    const int fpsDen = parseIntOption(options, "fps_den", 1);
    if (fpsNum > 0) {
        return AVRational{ fpsNum, fpsDen > 0 ? fpsDen : 1 };
    }

    AVRational frameRate = av_guess_frame_rate(formatContext, stream, nullptr);
    if (frameRate.num > 0 && frameRate.den > 0) {
        return frameRate;
    }

    return AVRational{ 30, 1 };
}

AVHWDeviceType deviceTypeFromHwaccelName(const std::string& hwaccel)
{
    if (hwaccel.empty()) {
        return AV_HWDEVICE_TYPE_NONE;
    }
    if (hwaccel == "rkmpp") {
        return AV_HWDEVICE_TYPE_DRM;
    }
    return av_hwdevice_find_type_by_name(hwaccel.c_str());
}

AVPixelFormat selectHardwarePixelFormat(const AVCodec* decoder, AVHWDeviceType deviceType)
{
    if (!decoder || deviceType == AV_HWDEVICE_TYPE_NONE) {
        return AV_PIX_FMT_NONE;
    }

    for (int index = 0;; ++index) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, index);
        if (!config) {
            break;
        }

        const bool canUseDevice = (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;
        const bool internalOnly = (config->methods & AV_CODEC_HW_CONFIG_METHOD_INTERNAL) != 0;
        if (canUseDevice && config->device_type == deviceType) {
            return config->pix_fmt;
        }
        if (internalOnly && config->pix_fmt != AV_PIX_FMT_NONE) {
            return config->pix_fmt;
        }
    }

    return AV_PIX_FMT_NONE;
}

AVPixelFormat plannedHardwareGetFormat(AVCodecContext* context, const AVPixelFormat* formats)
{
    const auto* desired = static_cast<const AVPixelFormat*>(context ? context->opaque : nullptr);
    if (desired && *desired != AV_PIX_FMT_NONE) {
        for (const AVPixelFormat* current = formats; current && *current != AV_PIX_FMT_NONE; ++current) {
            if (*current == *desired) {
                return *current;
            }
        }
    }

    return formats ? formats[0] : AV_PIX_FMT_NONE;
}

void codecResolverLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("codec_resolver.") + message);
}

} // namespace

CodecResolverNode::CodecResolverNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "CodecResolverNode")
{
}

MediaNodeKind CodecResolverNode::staticKind() noexcept
{
    return MediaNodeKind::CodecResolver;
}

::media::Status CodecResolverNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return ::media::Status::success();
    }

    auto input = tryPopFirstInput(context);
    if (!input) {
        return ::media::Status::success();
    }

    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(input.value().get());
    if (!formatBuffer || !formatBuffer->context()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverNode expected FFmpegFormatContextBuffer"));
    }

    AVFormatContext* formatContext = formatBuffer->context();

    auto decoderStatus = resolveDecoder(context, formatContext);
    if (!decoderStatus) {
        return decoderStatus;
    }

    auto encoderStatus = resolveEncoder(context, formatContext);
    if (!encoderStatus) {
        return encoderStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

::media::Status CodecResolverNode::resolveDecoder(MediaGraphExecutionContext& context, AVFormatContext* formatContext)
{
    const int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return FFmpegGraphError::statusFromCode(streamIndex, "av_find_best_stream(video decoder)");
    }

    AVStream* stream = formatContext->streams[streamIndex];
    const MediaNodeOptions* options = nodeOptions(context);
    const std::string plannedDecoder = optionValue(options, "decoder");
    const bool hardwarePlanned = truthyOption(options, "pipeline.hardware");
    const std::string hwaccelName = optionValue(options, "pipeline.hwaccel");

    const AVCodec* decoder = nullptr;
    if (!plannedDecoder.empty() && plannedDecoder != "auto") {
        decoder = avcodec_find_decoder_by_name(plannedDecoder.c_str());
    } else {
        decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    }

    if (!decoder) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("CodecResolverNode failed: video decoder not found: " +
                                           (!plannedDecoder.empty() ? plannedDecoder : std::string(avcodec_get_name(stream->codecpar->codec_id)))));
    }

    auto decoderContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!decoderContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("CodecResolverNode failed: avcodec_alloc_context3(decoder) returned null"));
    }

    const int copyRet = avcodec_parameters_to_context(decoderContext.get(), stream->codecpar);
    if (copyRet < 0) {
        return FFmpegGraphError::statusFromCode(copyRet, "avcodec_parameters_to_context(video decoder)");
    }

    decoderContext->pkt_timebase = stream->time_base;

    m_decoderHardwareDevice.reset();
    m_decoderHardwarePixelFormat = AV_PIX_FMT_NONE;
    if (hardwarePlanned) {
        const AVHWDeviceType deviceType = deviceTypeFromHwaccelName(hwaccelName);
        m_decoderHardwarePixelFormat = selectHardwarePixelFormat(decoder, deviceType);

        if (deviceType != AV_HWDEVICE_TYPE_NONE) {
            AVBufferRef* rawDevice = nullptr;
            const int deviceRet = av_hwdevice_ctx_create(&rawDevice, deviceType, nullptr, nullptr, 0);
            if (deviceRet < 0) {
                return FFmpegGraphError::statusFromCode(deviceRet, "av_hwdevice_ctx_create(" + hwaccelName + ")");
            }
            m_decoderHardwareDevice = ::media::ffmpeg::BufferRefPtr(rawDevice);
            decoderContext->hw_device_ctx = av_buffer_ref(m_decoderHardwareDevice.get());
            if (!decoderContext->hw_device_ctx) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::allocationFailed("CodecResolverNode failed: av_buffer_ref(hw_device_ctx)"));
            }
        }

        if (m_decoderHardwarePixelFormat != AV_PIX_FMT_NONE) {
            decoderContext->opaque = &m_decoderHardwarePixelFormat;
            decoderContext->get_format = plannedHardwareGetFormat;
        }
    }

    std::ostringstream out;
    out << "decoder.open name=" << (decoder->name ? decoder->name : "unknown")
        << " planned=" << (plannedDecoder.empty() ? "auto" : plannedDecoder)
        << " hardware=" << (hardwarePlanned ? "true" : "false")
        << " hwaccel=" << (hwaccelName.empty() ? "none" : hwaccelName)
        << " hw_pix_fmt=" << pixelFormatName(m_decoderHardwarePixelFormat)
        << " pkt_tb=" << stream->time_base.num << "/" << stream->time_base.den;
    codecResolverLog(MediaGraphDiagnosticLevel::State, out.str());

    const int openRet = avcodec_open2(decoderContext.get(), decoder, nullptr);
    if (openRet < 0) {
        return FFmpegGraphError::statusFromCode(openRet, "avcodec_open2(video decoder)");
    }

    auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(decoderContext));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    MediaBufferRef decoderConfig = std::move(buffer).value();
    auto decoderStatus = pushOutput(context, "decoder", decoderConfig);
    if (!decoderStatus) {
        return decoderStatus;
    }

    if (context.findOutputChannel(nodeId(), "timestamp_source")) {
        auto timestampStatus = pushOutput(context, "timestamp_source", decoderConfig);
        if (!timestampStatus) {
            return timestampStatus;
        }
    }

    return ::media::Status::success();
}

::media::Status CodecResolverNode::resolveEncoder(MediaGraphExecutionContext& context, AVFormatContext* formatContext)
{
    const int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return FFmpegGraphError::statusFromCode(streamIndex, "av_find_best_stream(video encoder source)");
    }

    AVStream* stream = formatContext->streams[streamIndex];
    AVCodecParameters* params = stream->codecpar;
    const MediaNodeOptions* options = nodeOptions(context);

    const int rawWidth = parseIntOption(options, "width", params->width);
    const int rawHeight = parseIntOption(options, "height", params->height);

    const int targetWidth = rawWidth > 0 ? rawWidth : params->width;
    const int targetHeight = rawHeight > 0 ? rawHeight : params->height;

    const int bitrateKbps = parseIntOption(options, "bitrate_kbps", 0);
    const int64_t bitRate = bitrateKbps > 0 ? static_cast<int64_t>(bitrateKbps) * 1000 : 4'000'000;
    const int gop = parseIntOption(options, "gop", 0);
    const int bframes = parseIntOption(options, "bframes", 0);
    const int crf = parseIntOption(options, "crf", -1);
    const int quality = parseIntOption(options, "quality", -1);
    const std::string rcMode = optionValue(options, "rc", "auto");
    const AVRational frameRate = resolveFrameRate(formatContext, stream, options);

    std::string lastError = "no encoder candidates available";
    for (const AVCodec* encoder : encoderCandidates(options)) {
        if (!encoder) {
            continue;
        }

        auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
        if (!encoderContext) {
            lastError = std::string(encoder->name ? encoder->name : "unknown") + ": avcodec_alloc_context3 returned null";
            continue;
        }

        const AVPixelFormat encoderPixelFormat = chooseEncoderPixelFormat(
            encoder,
            static_cast<AVPixelFormat>(params->format),
            options);
        if (encoderPixelFormat == AV_PIX_FMT_NONE) {
            lastError = std::string(encoder->name ? encoder->name : "unknown") +
                        ": planned encoder pixel format unsupported";
            continue;
        }

        const AVPixelFormat plannedHardwareFormat = plannedEncoderHardwarePixelFormat(options);
        const AVPixelFormat plannedSoftwareFormat = plannedEncoderSoftwarePixelFormat(
            options,
            static_cast<AVPixelFormat>(params->format));

        encoderContext->width = targetWidth;
        encoderContext->height = targetHeight;
        encoderContext->pix_fmt = encoderPixelFormat;
        encoderContext->time_base = AVRational{ frameRate.den, frameRate.num };
        encoderContext->framerate = frameRate;
        encoderContext->bit_rate = bitRate;
        encoderContext->gop_size = gop > 0 ? gop : 60;
        encoderContext->max_b_frames = bframes;
        encoderContext->sample_aspect_ratio = stream->sample_aspect_ratio;
        encoderContext->color_range = params->color_range;
        encoderContext->color_primaries = params->color_primaries;
        encoderContext->color_trc = params->color_trc;
        encoderContext->colorspace = params->color_space;

        if (plannedHardwareFormat != AV_PIX_FMT_NONE) {
            auto framesStatus = configureEncoderHardwareFrames(encoderContext.get(),
                                                               m_decoderHardwareDevice.get(),
                                                               plannedHardwareFormat,
                                                               plannedSoftwareFormat,
                                                               targetWidth,
                                                               targetHeight,
                                                               32);
            if (!framesStatus) {
                return framesStatus;
            }
        }

        if (rcMode == "cbr" && bitRate > 0) {
            encoderContext->rc_min_rate = bitRate;
            encoderContext->rc_max_rate = bitRate;
            encoderContext->rc_buffer_size = static_cast<int>(bitRate * 2);
        } else if (rcMode == "vbr" && bitRate > 0) {
            encoderContext->rc_max_rate = bitRate;
            encoderContext->rc_buffer_size = static_cast<int>(bitRate * 2);
        }

        setPrivateOption(encoderContext.get(), "preset", optionValue(options, "preset"));
        setPrivateOption(encoderContext.get(), "profile", optionValue(options, "profile"));
        setPrivateOption(encoderContext.get(), "tune", optionValue(options, "tune"));
        setPrivateOption(encoderContext.get(), "level", optionValue(options, "level"));
        if (crf >= 0) {
            setPrivateOption(encoderContext.get(), "crf", std::to_string(crf));
        }
        if (quality >= 0) {
            setPrivateOption(encoderContext.get(), "quality", std::to_string(quality));
            setPrivateOption(encoderContext.get(), "q", std::to_string(quality));
        }

        codecResolverLog(MediaGraphDiagnosticLevel::State,
                         std::string("encoder.open name=") + (encoder->name ? encoder->name : "unknown") +
                             " pix_fmt=" + pixelFormatName(encoderContext->pix_fmt) +
                             " sw_format=" + pixelFormatName(plannedSoftwareFormat) +
                             " frame_kind=" + optionValue(options, "encoder.pipeline.frame_kind", "software") +
                             " hwaccel=" + optionValue(options, "encoder.pipeline.hwaccel", "none") +
                             " hw_frames_ctx=" + (encoderContext->hw_frames_ctx ? "set" : "none"));

        const int openRet = avcodec_open2(encoderContext.get(), encoder, nullptr);
        if (openRet < 0) {
            lastError = std::string(encoder->name ? encoder->name : "unknown") + ": encoder open failed";
            continue;
        }

        auto buffer = FFmpegBufferFactory::wrapCodecContext(std::move(encoderContext));
        if (!buffer) {
            return ::media::Status::failure(buffer.error());
        }

        return pushOutput(context, "encoder", std::move(buffer).value());
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported("CodecResolverNode failed to create video encoder: " + lastError));
}

} // namespace media::ffmpeg::graph
