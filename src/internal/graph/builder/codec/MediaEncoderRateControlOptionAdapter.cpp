#include "internal/graph/builder/codec/MediaEncoderRateControlOptionAdapter.h"

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <charconv>
#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

template <typename Value>
::media::Result<std::optional<Value>> optionalInteger(
    const MediaNodeOptions& options,
    const char* key)
{
    const std::string text = options.value(key);
    if (text.empty()) {
        return ::media::Result<std::optional<Value>>::success(std::nullopt);
    }
    Value value{};
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return ::media::Result<std::optional<Value>>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("invalid planner rate-control option: ") + key));
    }
    return ::media::Result<std::optional<Value>>::success(value);
}

::media::Result<std::int64_t> bitsFromKilo(
    int value,
    const char* name)
{
    constexpr std::int64_t BitsPerKilobit = 1000;
    if (value <= 0 || value >
            std::numeric_limits<std::int64_t>::max() / BitsPerKilobit) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("invalid planned ") + name));
    }
    return ::media::Result<std::int64_t>::success(
        static_cast<std::int64_t>(value) * BitsPerKilobit);
}

} // namespace

::media::Result<MediaEncoderRateControlPlan>
MediaEncoderRateControlOptionAdapter::applyBeforeOpen(
    AVCodecContext& context,
    const MediaNodeOptions& options)
{
    const std::string modeText = options.value(
        MediaTranscodeOptionKey::PlannedVideoRateControl);
    MediaRateControlMode mode{};
    if (modeText.empty() || !parseMediaRateControlMode(modeText, mode)) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "encoder requires planner-selected rate-control mode"));
    }
    auto target = optionalInteger<int>(
        options, MediaTranscodeOptionKey::PlannedVideoTargetBitrateKbps);
    auto minimum = optionalInteger<int>(
        options, MediaTranscodeOptionKey::PlannedVideoMinBitrateKbps);
    auto maximum = optionalInteger<int>(
        options, MediaTranscodeOptionKey::PlannedVideoMaxBitrateKbps);
    auto buffer = optionalInteger<int>(
        options, MediaTranscodeOptionKey::PlannedVideoBufferSizeKbits);
    auto expected = optionalInteger<std::int64_t>(
        options, MediaTranscodeOptionKey::PlannedVideoPrivateRateControlExpected);
    if (!target || !minimum || !maximum || !buffer || !expected) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            !target ? target.error() : !minimum ? minimum.error() :
            !maximum ? maximum.error() : !buffer ? buffer.error() : expected.error());
    }

    MediaEncoderRateControlPlan plan{
        mode, target.value(), minimum.value(), maximum.value(),
        buffer.value(), std::nullopt};
    const std::string privateName = options.value(
        MediaTranscodeOptionKey::PlannedVideoPrivateRateControlName);
    const std::string privateValue = options.value(
        MediaTranscodeOptionKey::PlannedVideoPrivateRateControlValue);
    const bool anyPrivate = !privateName.empty() || !privateValue.empty() ||
                            expected.value().has_value();
    const bool completePrivate = !privateName.empty() && !privateValue.empty() &&
                                 expected.value().has_value();
    if (anyPrivate != completePrivate) {
        return ::media::Result<MediaEncoderRateControlPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "encoder private rate-control plan is incomplete"));
    }
    if (completePrivate) {
        if (!context.priv_data) {
            return ::media::Result<MediaEncoderRateControlPlan>::failure(
                ::media::ErrorInfo::unsupported(
                    "planned private rate-control option has no encoder context"));
        }
        const int applied = av_opt_set(
            context.priv_data, privateName.c_str(), privateValue.c_str(), 0);
        if (applied < 0) {
            return ::media::Result<MediaEncoderRateControlPlan>::failure(
                FFmpegGraphError::statusFromCode(
                    applied, "apply planned encoder rate-control option").error());
        }
        plan.privateOption = MediaEncoderPrivateRateControlOption{
            privateName, privateValue, *expected.value()};
    }

    if (plan.targetBitrateKbps) {
        auto bits = bitsFromKilo(*plan.targetBitrateKbps, "target bitrate");
        if (!bits) return ::media::Result<MediaEncoderRateControlPlan>::failure(bits.error());
        context.bit_rate = bits.value();
    }
    if (plan.minimumBitrateKbps) {
        auto bits = bitsFromKilo(*plan.minimumBitrateKbps, "minimum bitrate");
        if (!bits) return ::media::Result<MediaEncoderRateControlPlan>::failure(bits.error());
        context.rc_min_rate = bits.value();
    }
    if (plan.maximumBitrateKbps) {
        auto bits = bitsFromKilo(*plan.maximumBitrateKbps, "maximum bitrate");
        if (!bits) return ::media::Result<MediaEncoderRateControlPlan>::failure(bits.error());
        context.rc_max_rate = bits.value();
    }
    if (plan.bufferSizeKbits) {
        auto bits = bitsFromKilo(*plan.bufferSizeKbits, "buffer size");
        if (!bits || bits.value() > std::numeric_limits<int>::max()) {
            return ::media::Result<MediaEncoderRateControlPlan>::failure(
                bits ? ::media::ErrorInfo::invalidArgument(
                           "planned buffer size exceeds FFmpeg range")
                     : bits.error());
        }
        context.rc_buffer_size = static_cast<int>(bits.value());
    }
    return ::media::Result<MediaEncoderRateControlPlan>::success(std::move(plan));
}

::media::Status MediaEncoderRateControlOptionAdapter::verifyAfterOpen(
    const AVCodecContext& context,
    const MediaEncoderRateControlPlan& plan)
{
    if (!plan.privateOption) return ::media::Status::success();
    std::int64_t actual = 0;
    const int read = av_opt_get_int(
        context.priv_data, plan.privateOption->name.c_str(), 0, &actual);
    if (read < 0) {
        return FFmpegGraphError::statusFromCode(
            read, "read back planned encoder rate-control option");
    }
    if (actual != plan.privateOption->expectedNumericValue) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "encoder negotiated a different private rate-control mode"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
