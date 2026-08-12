#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"

#include "internal/graph/builder/codec/CodecResolverEncoderFormatPlanner.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/metadata/CodecResolverHardwareFrames.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/MediaFfmpegCopyOpaqueCapability.h"
#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

namespace media::ffmpeg::graph {
namespace {

std::string optionValue(const MediaNodeOptions* options, const std::string& key, std::string missingValue = {})
{
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
}

::media::Result<std::optional<int>> intOption(const MediaNodeOptions* options, const std::string& key)
{
    if (!options) {
        return ::media::Result<std::optional<int>>::success(std::nullopt);
    }

    const std::string value = options->value(key);
    if (value.empty()) {
        return ::media::Result<std::optional<int>>::success(std::nullopt);
    }

    int parsed = 0;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        return ::media::Result<std::optional<int>>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder invalid integer option: " + key));
    }

    return ::media::Result<std::optional<int>>::success(parsed);
}

::media::Result<std::optional<bool>> boolOption(const MediaNodeOptions* options, const std::string& key)
{
    if (!options) {
        return ::media::Result<std::optional<bool>>::success(std::nullopt);
    }

    const std::string value = options->value(key);
    if (value.empty()) {
        return ::media::Result<std::optional<bool>>::success(std::nullopt);
    }

    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return ::media::Result<std::optional<bool>>::success(true);
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return ::media::Result<std::optional<bool>>::success(false);
    }

    return ::media::Result<std::optional<bool>>::failure(
        ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder invalid boolean option: " + key));
}

void setPrivateOption(AVCodecContext* context, const std::string& key, const std::string& value)
{
    if (!context || !context->priv_data || key.empty() || value.empty()) {
        return;
    }

    av_opt_set(context->priv_data, key.c_str(), value.c_str(), 0);
}

::media::Result<int64_t> bitsFromKbps(int kbps, const std::string& name)
{
    if (kbps <= 0) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(name + " must be positive"));
    }

    constexpr int64_t kBitsPerKbps = 1000;
    if (kbps > std::numeric_limits<int64_t>::max() / kBitsPerKbps) {
        return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(name + " is too large"));
    }

    return ::media::Result<int64_t>::success(static_cast<int64_t>(kbps) * kBitsPerKbps);
}

::media::Result<int> bitsFromKbits(int kbits, const std::string& name)
{
    if (kbits <= 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(name + " must be positive"));
    }

    constexpr int kBitsPerKbit = 1000;
    if (kbits > std::numeric_limits<int>::max() / kBitsPerKbit) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument(name + " is too large"));
    }

    return ::media::Result<int>::success(kbits * kBitsPerKbit);
}

::media::Result<AVRational> resolveFrameRate(const MediaTimeDescriptor& sourceTime,
                                             const MediaNodeOptions* options)
{
    auto fpsNumResult = intOption(options, MediaTranscodeOptionKey::VideoFpsNum);
    if (!fpsNumResult) {
        return ::media::Result<AVRational>::failure(fpsNumResult.error());
    }
    auto fpsDenResult = intOption(options, MediaTranscodeOptionKey::VideoFpsDen);
    if (!fpsDenResult) {
        return ::media::Result<AVRational>::failure(fpsDenResult.error());
    }

    const std::optional<int> fpsNum = fpsNumResult.value();
    const std::optional<int> fpsDen = fpsDenResult.value();
    if (fpsNum || fpsDen) {
        if (!fpsNum || !fpsDen || *fpsNum <= 0 || *fpsDen <= 0) {
            return ::media::Result<AVRational>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires valid video fps numerator/denominator"));
        }
        return ::media::Result<AVRational>::success(AVRational{ *fpsNum, *fpsDen });
    }

    if (sourceTime.frameRate.num > 0 && sourceTime.frameRate.den > 0) {
        return ::media::Result<AVRational>::success(AVRational{ sourceTime.frameRate.num, sourceTime.frameRate.den });
    }

    return ::media::Result<AVRational>::failure(
        ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cannot resolve input frame rate; specify fps explicitly"));
}

::media::Status validateRequest(const CodecResolverEncoderContextBuildRequest& request)
{
    if (!request.codecParameters) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires source stream"));
    }

    return ::media::Status::success();
}

::media::Status requireBitrate(const AVCodecContext* encoderContext, const std::string& rcMode)
{
    if (!encoderContext || encoderContext->bit_rate <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder " + rcMode + " mode requires video bitrate"));
    }
    return ::media::Status::success();
}

::media::Status applyQualityByRateControlMode(AVCodecContext* encoderContext,
                                              const std::string& rcMode,
                                              const std::optional<int>& quality)
{
    if (!quality) {
        return ::media::Status::success();
    }

    if (*quality < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative quality"));
    }

    const std::string value = std::to_string(*quality);
    if (rcMode == "crf") {
        setPrivateOption(encoderContext, "crf", value);
        return ::media::Status::success();
    }

    if (rcMode == "auto") {
        setPrivateOption(encoderContext, "quality", value);
        setPrivateOption(encoderContext, "q", value);
        return ::media::Status::success();
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects quality without crf/auto rate control"));
}

} // namespace

::media::Result<CodecResolverEncoderContextBuildResult> CodecResolverEncoderContextBuilder::build(
    const CodecResolverEncoderContextBuildRequest& request)
{
    auto validation = validateRequest(request);
    if (!validation) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(validation.error());
    }

    const MediaNodeOptions* options = request.options;
    const AVCodecParameters* params = request.codecParameters;

    const std::string plannedEncoder = optionValue(options, MediaTranscodeOptionKey::PlannedEncoder);
    if (plannedEncoder.empty() || plannedEncoder == "auto") {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires planner-selected encoder"));
    }

    const AVCodec* encoder = avcodec_find_encoder_by_name(plannedEncoder.c_str());
    if (!encoder) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::unsupported("CodecResolverEncoderContextBuilder encoder not found: " + plannedEncoder));
    }

    auto formatPlanResult = CodecResolverEncoderFormatPlanner::build(
        CodecResolverEncoderFormatPlanRequest{ encoder, options });
    if (!formatPlanResult) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(formatPlanResult.error());
    }
    const CodecResolverEncoderFormatPlan formatPlan = formatPlanResult.value();

    auto frameRateResult = resolveFrameRate(request.sourceTime, options);
    if (!frameRateResult) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(frameRateResult.error());
    }
    const AVRational frameRate = std::move(frameRateResult).value();

    auto widthOption = intOption(options, MediaTranscodeOptionKey::VideoWidth);
    if (!widthOption) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(widthOption.error());
    }
    auto heightOption = intOption(options, MediaTranscodeOptionKey::VideoHeight);
    if (!heightOption) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(heightOption.error());
    }
    const int targetWidth = widthOption.value().value_or(params->width);
    const int targetHeight = heightOption.value().value_or(params->height);
    if (targetWidth <= 0 || targetHeight <= 0) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires valid target dimensions"));
    }

    CodecResolverEncoderContextBuildResult result;
    result.hardwareFramesFormat = formatPlan.hardwareFramesFormat;
    result.surfaceSoftwareFormat = formatPlan.surfaceSoftwareFormat;

    auto encoderContext = ::media::ffmpeg::makeCodecContext(encoder);
    if (!encoderContext) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::allocationFailed("CodecResolverEncoderContextBuilder failed: avcodec_alloc_context3 returned null"));
    }

    encoderContext->width = targetWidth;
    encoderContext->height = targetHeight;
    encoderContext->pix_fmt = formatPlan.encoderPixelFormat;
    encoderContext->sw_pix_fmt = formatPlan.surfaceSoftwareFormat;
    encoderContext->time_base = AVRational{ frameRate.den, frameRate.num };
    encoderContext->framerate = frameRate;
    encoderContext->sample_aspect_ratio = AVRational{ request.sourceFormat.video.sampleAspectRatio.num,
                                                      request.sourceFormat.video.sampleAspectRatio.den };
    encoderContext->color_range = params->color_range;
    encoderContext->color_primaries = params->color_primaries;
    encoderContext->color_trc = params->color_trc;
    encoderContext->colorspace = params->color_space;
    auto copyOpaque = parseMediaVideoLineageCopyOpaqueOption(options);
    if (!copyOpaque) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            copyOpaque.error());
    }
    if (copyOpaque.value()) {
#if defined(AV_CODEC_FLAG_COPY_OPAQUE)
        if (auto status = requireMediaFfmpegCopyOpaqueCapability(); !status) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                status.error());
        }
        encoderContext->flags |= AV_CODEC_FLAG_COPY_OPAQUE;
#else
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            requireMediaFfmpegCopyOpaqueCapability().error());
#endif
    }

    auto globalHeader = boolOption(options, MediaTranscodeOptionKey::VideoGlobalHeader);
    if (!globalHeader) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(globalHeader.error());
    }
    if (globalHeader.value().value_or(false)) {
        encoderContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    auto bitrateKbps = intOption(options, MediaTranscodeOptionKey::VideoBitrateKbps);
    if (!bitrateKbps) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bitrateKbps.error());
    }
    if (bitrateKbps.value()) {
        if (*bitrateKbps.value() < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative video bitrate"));
        }
        if (*bitrateKbps.value() > 0) {
            auto bits = bitsFromKbps(*bitrateKbps.value(), "video bitrate");
            if (!bits) {
                return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bits.error());
            }
            encoderContext->bit_rate = bits.value();
        }
    } else if (params->bit_rate > 0) {
        encoderContext->bit_rate = params->bit_rate;
    }

    auto minBitrateKbps = intOption(options, MediaTranscodeOptionKey::VideoMinBitrateKbps);
    if (!minBitrateKbps) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(minBitrateKbps.error());
    }
    if (minBitrateKbps.value()) {
        if (*minBitrateKbps.value() < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative video min bitrate"));
        }
        if (*minBitrateKbps.value() > 0) {
            auto bits = bitsFromKbps(*minBitrateKbps.value(), "video min bitrate");
            if (!bits) {
                return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bits.error());
            }
            encoderContext->rc_min_rate = bits.value();
        }
    }

    auto maxBitrateKbps = intOption(options, MediaTranscodeOptionKey::VideoMaxBitrateKbps);
    if (!maxBitrateKbps) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(maxBitrateKbps.error());
    }
    if (maxBitrateKbps.value()) {
        if (*maxBitrateKbps.value() < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative video max bitrate"));
        }
        if (*maxBitrateKbps.value() > 0) {
            auto bits = bitsFromKbps(*maxBitrateKbps.value(), "video max bitrate");
            if (!bits) {
                return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bits.error());
            }
            encoderContext->rc_max_rate = bits.value();
        }
    }

    if (encoderContext->rc_min_rate > 0 && encoderContext->rc_max_rate > 0 && encoderContext->rc_min_rate > encoderContext->rc_max_rate) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder requires video min bitrate <= max bitrate"));
    }

    auto bufferSizeKbits = intOption(options, MediaTranscodeOptionKey::VideoBufferSizeKbits);
    if (!bufferSizeKbits) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bufferSizeKbits.error());
    }
    if (bufferSizeKbits.value()) {
        auto bufferSizeBits = bitsFromKbits(*bufferSizeKbits.value(), "video buffer size");
        if (!bufferSizeBits) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bufferSizeBits.error());
        }
        encoderContext->rc_buffer_size = bufferSizeBits.value();
    }

    auto gop = intOption(options, MediaTranscodeOptionKey::VideoGop);
    if (!gop) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(gop.error());
    }
    if (gop.value()) {
        if (*gop.value() < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative gop"));
        }
        encoderContext->gop_size = *gop.value();
    }

    auto bframes = intOption(options, MediaTranscodeOptionKey::VideoBFrames);
    if (!bframes) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bframes.error());
    }
    if (bframes.value()) {
        if (*bframes.value() < 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder rejects negative bframes"));
        }
        encoderContext->max_b_frames = *bframes.value();
    }

    if (formatPlan.requiresHardwareDeviceContext && !request.hardwareDevice) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "CodecResolverEncoderContextBuilder requires the planner-selected hardware device context"));
    }

    if (formatPlan.requiresHardwareFramesContext) {
        auto framesStatus = configureEncoderHardwareFrames(encoderContext.get(),
                                                           request.hardwareDevice,
                                                           result.hardwareFramesFormat,
                                                           result.surfaceSoftwareFormat,
                                                           targetWidth,
                                                           targetHeight,
                                                           32);
        if (!framesStatus) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(framesStatus.error());
        }
    } else if (formatPlan.requiresHardwareDeviceContext) {
        encoderContext->hw_device_ctx = av_buffer_ref(request.hardwareDevice);
        if (!encoderContext->hw_device_ctx) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "CodecResolverEncoderContextBuilder failed to reference hardware device"));
        }
    }

    const std::string rcMode = optionValue(options, MediaTranscodeOptionKey::VideoRateControl);
    if (rcMode == "cbr") {
        auto bitrateStatus = requireBitrate(encoderContext.get(), rcMode);
        if (!bitrateStatus) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bitrateStatus.error());
        }
        if (encoderContext->rc_min_rate <= 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cbr mode requires explicit video min bitrate"));
        }
        if (encoderContext->rc_max_rate <= 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cbr mode requires explicit video max bitrate"));
        }
        if (encoderContext->rc_buffer_size <= 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cbr mode requires explicit video buffer size"));
        }
    } else if (rcMode == "cvbr") {
        auto bitrateStatus = requireBitrate(encoderContext.get(), rcMode);
        if (!bitrateStatus) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(bitrateStatus.error());
        }
        if (encoderContext->rc_max_rate <= 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cvbr mode requires video max bitrate"));
        }
        if (encoderContext->rc_buffer_size <= 0) {
            return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
                ::media::ErrorInfo::invalidArgument("CodecResolverEncoderContextBuilder cvbr mode requires explicit video buffer size"));
        }
    }

    setPrivateOption(encoderContext.get(), "preset", optionValue(options, MediaTranscodeOptionKey::VideoPreset));
    setPrivateOption(encoderContext.get(), "profile", optionValue(options, MediaTranscodeOptionKey::VideoProfile));
    setPrivateOption(encoderContext.get(), "tune", optionValue(options, MediaTranscodeOptionKey::VideoTune));
    setPrivateOption(encoderContext.get(), "level", optionValue(options, MediaTranscodeOptionKey::VideoLevel));

    auto quality = intOption(options, MediaTranscodeOptionKey::VideoQuality);
    if (!quality) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(quality.error());
    }
    auto qualityStatus = applyQualityByRateControlMode(encoderContext.get(),
                                                       rcMode,
                                                       quality.value());
    if (!qualityStatus) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(qualityStatus.error());
    }

    const int openRet = avcodec_open2(encoderContext.get(), encoder, nullptr);
    if (openRet < 0) {
        return ::media::Result<CodecResolverEncoderContextBuildResult>::failure(
            FFmpegGraphError::statusFromCode(openRet, "avcodec_open2(video encoder " + plannedEncoder + ")").error());
    }

    result.context = std::move(encoderContext);
    return ::media::Result<CodecResolverEncoderContextBuildResult>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
