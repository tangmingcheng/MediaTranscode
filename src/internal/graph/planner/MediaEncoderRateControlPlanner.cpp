#include "internal/graph/planner/MediaEncoderRateControlPlanner.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cctype>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

::media::Status validatePositive(
    const std::optional<int>& value,
    const char* name)
{
    return !value || *value > 0
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::invalidArgument(
              std::string("MediaEncoderRateControlPlanner requires positive ") + name));
}

bool equalAsciiInsensitive(std::string_view left, std::string_view right)
{
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(),
               [](char a, char b) {
                   return std::tolower(static_cast<unsigned char>(a)) ==
                          std::tolower(static_cast<unsigned char>(b));
               });
}

::media::Result<MediaEncoderPrivateRateControlOption> resolveNamedMode(
    const AVCodec& encoder,
    std::string_view requestedMode)
{
    auto context = ::media::ffmpeg::makeCodecContext(&encoder);
    if (!context || !context->priv_data) {
        return ::media::Result<MediaEncoderPrivateRateControlOption>::failure(
            ::media::ErrorInfo::unsupported(
                "planned encoder has no private rate-control option context"));
    }
    const AVOption* mode = av_opt_find(
        context->priv_data, "rc_mode", nullptr, 0, 0);
    if (!mode || !mode->unit) {
        return ::media::Result<MediaEncoderPrivateRateControlOption>::failure(
            ::media::ErrorInfo::unsupported(
                "planned encoder does not expose a named rc_mode option"));
    }
    const AVOption* option = nullptr;
    while ((option = av_opt_next(context->priv_data, option)) != nullptr) {
        if (option->type == AV_OPT_TYPE_CONST && option->unit &&
            std::string_view(option->unit) == mode->unit &&
            equalAsciiInsensitive(option->name, requestedMode)) {
            return ::media::Result<MediaEncoderPrivateRateControlOption>::success(
                MediaEncoderPrivateRateControlOption{
                    mode->name, option->name, option->default_val.i64});
        }
    }
    return ::media::Result<MediaEncoderPrivateRateControlOption>::failure(
        ::media::ErrorInfo::unsupported(
            "planned encoder does not advertise requested rc_mode=" +
            std::string(requestedMode)));
}

} // namespace

::media::Result<MediaEncoderRateControlPlan>
MediaEncoderRateControlPlanner::plan(
    const std::string& encoderName,
    MediaHardwareDeviceKind deviceKind,
    const MediaEncoderRateControlRequest& request)
{
    if (encoderName.empty()) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaEncoderRateControlPlanner requires a planner-selected encoder"));
    }
    if (auto status = validatePositive(request.targetBitrateKbps, "target bitrate"); !status) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(status.error());
    }
    if (auto status = validatePositive(request.minimumBitrateKbps, "minimum bitrate"); !status) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(status.error());
    }
    if (auto status = validatePositive(request.maximumBitrateKbps, "maximum bitrate"); !status) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(status.error());
    }
    if (auto status = validatePositive(request.bufferSizeKbits, "buffer size"); !status) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(status.error());
    }
    if (request.minimumBitrateKbps && request.maximumBitrateKbps &&
        *request.minimumBitrateKbps > *request.maximumBitrateKbps) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaEncoderRateControlPlanner requires minimum bitrate <= maximum bitrate"));
    }
    if (request.mode == MediaRateControlMode::Cbr &&
        (!request.targetBitrateKbps || !request.bufferSizeKbits)) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "CBR requires target bitrate and buffer-size facts"));
    }
    if (request.mode == MediaRateControlMode::Cbr &&
        ((request.minimumBitrateKbps &&
             *request.targetBitrateKbps != *request.minimumBitrateKbps) ||
         (request.maximumBitrateKbps &&
             *request.targetBitrateKbps != *request.maximumBitrateKbps))) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "CBR minimum or maximum bitrate conflicts with target bitrate"));
    }
    if (request.mode == MediaRateControlMode::Vbr &&
        (!request.targetBitrateKbps || !request.minimumBitrateKbps ||
         !request.maximumBitrateKbps)) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VBR requires target, minimum, and maximum bitrate facts"));
    }
    if (request.mode == MediaRateControlMode::Vbr &&
        (*request.targetBitrateKbps < *request.minimumBitrateKbps ||
         *request.targetBitrateKbps > *request.maximumBitrateKbps)) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VBR requires minimum bitrate <= target bitrate <= maximum bitrate"));
    }

    MediaEncoderRateControlPlan result{
        request.mode, request.targetBitrateKbps,
        request.minimumBitrateKbps, request.maximumBitrateKbps,
        request.bufferSizeKbits, std::nullopt};
    if (request.mode == MediaRateControlMode::Cbr) {
        result.minimumBitrateKbps = request.targetBitrateKbps;
        result.maximumBitrateKbps = request.targetBitrateKbps;
    }
    if (deviceKind == MediaHardwareDeviceKind::RKMPP) {
        if (request.mode == MediaRateControlMode::Crf ||
            request.mode == MediaRateControlMode::Cvbr) {
            return ::media::Result<MediaEncoderRateControlPlan>::failure(
                ::media::ErrorInfo::unsupported(
                    "RKMPP encoder does not expose the requested rate-control mode"));
        }
        if (request.mode == MediaRateControlMode::Cbr ||
            request.mode == MediaRateControlMode::Vbr) {
            const AVCodec* encoder = avcodec_find_encoder_by_name(encoderName.c_str());
            if (!encoder) {
                return ::media::Result<MediaEncoderRateControlPlan>::failure(
                    ::media::ErrorInfo::unsupported(
                        "planner-selected encoder is unavailable: " + encoderName));
            }
            auto option = resolveNamedMode(
                *encoder, mediaRateControlModeName(request.mode));
            if (!option) {
                return ::media::Result<MediaEncoderRateControlPlan>::failure(option.error());
            }
            result.privateOption = std::move(option).value();
        }
    }
    return ::media::Result<MediaEncoderRateControlPlan>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
