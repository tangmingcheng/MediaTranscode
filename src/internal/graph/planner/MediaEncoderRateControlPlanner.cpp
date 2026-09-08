#include "internal/graph/planner/MediaEncoderRateControlPlanner.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cctype>
#include <limits>
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

::media::Status applyRkmppCbrEmissionBounds(
    MediaEncoderRateControlPlan& plan)
{
    // Rockchip MPP's CBR contract uses the same narrow bounds in
    // utils/mpi_enc_utils.c: target * 15 / 16 through target * 17 / 16.
    // The opened encoder readback is validated before this becomes an
    // emission fact; these values are not a sender-side headroom heuristic.
    constexpr std::int64_t RateScale = 16;
    constexpr std::int64_t MinimumRateNumerator = 15;
    constexpr std::int64_t MaximumRateNumerator = 17;
    if (plan.mode != MediaRateControlMode::Cbr ||
        !plan.targetBitrateKbps || *plan.targetBitrateKbps <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RKMPP CBR emission bounds require a positive target rate"));
    }
    const auto target = static_cast<std::int64_t>(*plan.targetBitrateKbps);
    if (target >
        ((std::numeric_limits<std::int64_t>::max)() - RateScale + 1) /
            MaximumRateNumerator) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RKMPP CBR emission bounds exceed the planner range"));
    }
    const auto minimum = target * MinimumRateNumerator / RateScale;
    const auto maximum =
        (target * MaximumRateNumerator + RateScale - 1) / RateScale;
    if (minimum <= 0 || maximum > (std::numeric_limits<int>::max)()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RKMPP CBR emission bounds exceed the encoder range"));
    }
    plan.minimumBitrateKbps = static_cast<int>(minimum);
    plan.maximumBitrateKbps = static_cast<int>(maximum);
    return ::media::Status::success();
}

::media::Result<int> lowLatencyVbvBufferSizeKbits(
    const MediaEncoderRateControlPlan& plan,
    MediaRational encoderFrameRate)
{
    const std::optional<int> emissionCeilingKbps =
        plan.mode == MediaRateControlMode::Cbr
            ? plan.targetBitrateKbps
            : plan.mode == MediaRateControlMode::Vbr
                ? plan.maximumBitrateKbps
                : std::optional<int>{};
    if (!emissionCeilingKbps) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::unsupported(
                "low-latency one-frame VBV requires CBR or VBR emission bounds"));
    }
    if (!encoderFrameRate.isKnown() || encoderFrameRate.num <= 0 ||
        encoderFrameRate.den <= 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::notInitialized(
                "low-latency one-frame VBV requires authoritative output cadence"));
    }
    auto buffer = MediaCheckedArithmetic::ceilScale(
        static_cast<std::uint64_t>(*emissionCeilingKbps),
        static_cast<std::uint64_t>(encoderFrameRate.den),
        static_cast<std::uint64_t>(encoderFrameRate.num),
        "low-latency one-frame VBV kilobits");
    if (!buffer || buffer.value() == 0 ||
        buffer.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<int>::max)())) {
        return ::media::Result<int>::failure(
            !buffer ? buffer.error() :
            ::media::ErrorInfo::invalidArgument(
                "low-latency one-frame VBV exceeds the encoder range"));
    }
    return ::media::Result<int>::success(
        static_cast<int>(buffer.value()));
}

} // namespace

::media::Result<MediaEncoderRateControlPlan>
MediaEncoderRateControlPlanner::plan(
    const std::string& encoderName,
    MediaHardwareDeviceKind deviceKind,
    const MediaEncoderRateControlRequest& request,
    MediaRational encoderFrameRate,
    bool lowLatency)
{
    if (encoderName.empty()) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaEncoderRateControlPlanner requires a planner-selected encoder"));
    }
    const auto target = request.targetBitrateKbps();
    const auto minimum = request.minimumBitrateKbps();
    const auto maximum = request.maximumBitrateKbps();
    const auto mode = request.mode();
    if (auto status = validatePositive(target, "target bitrate"); !status) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(status.error());
    }
    if (auto status = validatePositive(minimum, "minimum bitrate"); !status) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(status.error());
    }
    if (auto status = validatePositive(maximum, "maximum bitrate"); !status) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(status.error());
    }
    if (minimum && maximum && *minimum > *maximum) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaEncoderRateControlPlanner requires minimum bitrate <= maximum bitrate"));
    }
    if (mode == MediaRateControlMode::Cbr && !target) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "CBR requires target bitrate"));
    }
    if (mode == MediaRateControlMode::Cbr &&
        (minimum || maximum)) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "CBR request accepts only caller-supplied target bitrate"));
    }
    if (mode == MediaRateControlMode::Vbr && (!target || !minimum || !maximum)) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VBR requires target, minimum, and maximum bitrate facts"));
    }
    if (mode == MediaRateControlMode::Vbr &&
        (*target < *minimum || *target > *maximum)) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VBR requires minimum bitrate <= target bitrate <= maximum bitrate"));
    }

    MediaEncoderRateControlPlan result{
        mode, target, minimum, maximum, std::nullopt, std::nullopt};
    if (mode == MediaRateControlMode::Cbr) {
        result.minimumBitrateKbps = target;
        result.maximumBitrateKbps = target;
    }
    if (deviceKind == MediaHardwareDeviceKind::RKMPP) {
        if (mode == MediaRateControlMode::Crf || mode == MediaRateControlMode::Cvbr) {
            return ::media::Result<MediaEncoderRateControlPlan>::failure(
                ::media::ErrorInfo::unsupported(
                    "RKMPP encoder does not expose the requested rate-control mode"));
        }
        if (mode == MediaRateControlMode::Cbr || mode == MediaRateControlMode::Vbr) {
            const AVCodec* encoder = avcodec_find_encoder_by_name(encoderName.c_str());
            if (!encoder) {
                return ::media::Result<MediaEncoderRateControlPlan>::failure(
                    ::media::ErrorInfo::unsupported(
                        "planner-selected encoder is unavailable: " + encoderName));
            }
            auto option = resolveNamedMode(
                *encoder, mediaRateControlModeName(mode));
            if (!option) {
                return ::media::Result<MediaEncoderRateControlPlan>::failure(option.error());
            }
            result.privateOption = std::move(option).value();
        }
        if (mode == MediaRateControlMode::Cbr) {
            if (auto status = applyRkmppCbrEmissionBounds(result); !status) {
                return ::media::Result<MediaEncoderRateControlPlan>::failure(
                    status.error());
            }
        }
    }
    if (lowLatency &&
        (mode == MediaRateControlMode::Cbr ||
         mode == MediaRateControlMode::Vbr)) {
        auto buffer = lowLatencyVbvBufferSizeKbits(
            result, encoderFrameRate);
        if (!buffer) {
            return ::media::Result<MediaEncoderRateControlPlan>::failure(
                buffer.error());
        }
        result.bufferSizeKbits = buffer.value();
    }
    return ::media::Result<MediaEncoderRateControlPlan>::success(std::move(result));
}

} // namespace media::ffmpeg::graph
