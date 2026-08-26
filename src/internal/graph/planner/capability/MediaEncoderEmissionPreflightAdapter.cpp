#include "internal/graph/planner/capability/MediaEncoderEmissionPreflightAdapter.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t BitsPerKilobit = 1000;
constexpr std::int64_t BitsPerByte = 8;

::media::Result<std::int64_t> bitsFromKbits(int value, const char* field)
{
    if (value <= 0 ||
        value > (std::numeric_limits<std::int64_t>::max)() / BitsPerKilobit) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("encoder emission ") + field +
                " is outside the FFmpeg range"));
    }
    return ::media::Result<std::int64_t>::success(
        static_cast<std::int64_t>(value) * BitsPerKilobit);
}

::media::Status conflict(const char* field)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        std::string("opened encoder readback conflicts with planned ") + field));
}

} // namespace

::media::Status MediaEncoderEmissionPreflightAdapter::applyBeforeOpen(
    AVCodecContext& context,
    const MediaEncoderRateControlPlan& contract)
{
    if (!contract.targetBitrateKbps || !contract.bufferSizeKbits) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "encoder emission preflight requires target rate and VBV"));
    }
    auto target = bitsFromKbits(*contract.targetBitrateKbps, "target rate");
    auto buffer = bitsFromKbits(*contract.bufferSizeKbits, "VBV");
    auto minimum = contract.minimumBitrateKbps
        ? bitsFromKbits(*contract.minimumBitrateKbps, "minimum rate")
        : ::media::Result<std::int64_t>::success(0);
    if (!target || !buffer || !minimum) {
        return ::media::Status::failure(
            !target ? target.error() : !buffer ? buffer.error() :
            minimum.error());
    }
    auto maximum = contract.maximumBitrateKbps
        ? bitsFromKbits(*contract.maximumBitrateKbps, "maximum rate")
        : ::media::Result<std::int64_t>::success(target.value());
    if (!maximum ||
        buffer.value() > (std::numeric_limits<int>::max)()) {
        return ::media::Status::failure(
            !maximum ? maximum.error() :
            ::media::ErrorInfo::invalidArgument(
                "encoder emission VBV exceeds AVCodecContext range"));
    }
    context.bit_rate = target.value();
    context.rc_min_rate = minimum.value();
    context.rc_max_rate = maximum.value();
    context.rc_buffer_size = static_cast<int>(buffer.value());
    if (contract.privateOption) {
        if (!context.priv_data) {
            return ::media::Status::failure(::media::ErrorInfo::unsupported(
                "encoder emission private contract has no backend context"));
        }
        const int applied = av_opt_set(
            context.priv_data, contract.privateOption->name.c_str(),
            contract.privateOption->value.c_str(), 0);
        if (applied < 0) {
            return FFmpegGraphError::statusFromCode(
                applied, "apply encoder emission preflight contract");
        }
    }
    return ::media::Status::success();
}

::media::Result<MediaPreparedEncoderEmissionEnvelope>
MediaEncoderEmissionPreflightAdapter::readAfterOpen(
    const AVCodecContext& context,
    const MediaEncoderRateControlPlan& contract,
    MediaRational plannedCadence,
    MediaEncodedPacketLayout packetLayout,
    std::string authority,
    std::string backend)
{
    using Result = ::media::Result<MediaPreparedEncoderEmissionEnvelope>;
    if (!contract.targetBitrateKbps || !contract.bufferSizeKbits ||
        !plannedCadence.isKnown() || plannedCadence.num <= 0 ||
        plannedCadence.den <= 0 || authority.empty() || backend.empty()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "encoder emission readback contract is incomplete"));
    }
    auto expectedTarget = bitsFromKbits(
        *contract.targetBitrateKbps, "target rate");
    auto expectedMaximum = bitsFromKbits(
        contract.maximumBitrateKbps.value_or(*contract.targetBitrateKbps),
        "maximum rate");
    auto expectedBuffer = bitsFromKbits(
        *contract.bufferSizeKbits, "VBV");
    if (!expectedTarget || !expectedMaximum || !expectedBuffer) {
        return Result::failure(
            !expectedTarget ? expectedTarget.error() :
            !expectedMaximum ? expectedMaximum.error() :
            expectedBuffer.error());
    }
    const auto effectiveMaximum = context.rc_max_rate > 0
        ? context.rc_max_rate : context.bit_rate;
    if (context.bit_rate <= 0 || effectiveMaximum <= 0 ||
        context.rc_buffer_size <= 0) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "opened encoder did not expose effective rate/VBV readback"));
    }
    if (context.bit_rate != expectedTarget.value()) {
        return Result::failure(conflict("target rate").error());
    }
    if (effectiveMaximum != expectedMaximum.value()) {
        return Result::failure(conflict("maximum rate").error());
    }
    if (context.rc_buffer_size != expectedBuffer.value()) {
        return Result::failure(conflict("VBV").error());
    }
    if (context.framerate.num != plannedCadence.num ||
        context.framerate.den != plannedCadence.den) {
        return Result::failure(conflict("output cadence").error());
    }
    if (contract.privateOption) {
        std::int64_t effective = 0;
        const int read = av_opt_get_int(
            context.priv_data, contract.privateOption->name.c_str(), 0,
            &effective);
        if (read < 0) {
            return Result::failure(FFmpegGraphError::statusFromCode(
                read, "read encoder emission backend contract").error());
        }
        if (effective != contract.privateOption->expectedNumericValue) {
            return Result::failure(conflict("backend rate-control mode").error());
        }
    }
    const auto sustainedBytes = static_cast<std::uint64_t>(
        context.bit_rate / BitsPerByte +
        (context.bit_rate % BitsPerByte != 0 ? 1 : 0));
    const auto peakBytes = static_cast<std::uint64_t>(
        effectiveMaximum / BitsPerByte +
        (effectiveMaximum % BitsPerByte != 0 ? 1 : 0));
    const auto bufferBytes = static_cast<std::uint64_t>(
        context.rc_buffer_size / BitsPerByte +
        (context.rc_buffer_size % BitsPerByte != 0 ? 1 : 0));
    if (context.max_b_frames < 0) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "opened encoder did not expose a valid retained-frame bound"));
    }
    const auto retainedFrames =
        static_cast<std::uint64_t>(context.max_b_frames) + 1U;
    return Result::success(MediaPreparedEncoderEmissionEnvelope{
        sustainedBytes, peakBytes, bufferBytes, bufferBytes,
        static_cast<std::uint64_t>(plannedCadence.num),
        static_cast<std::uint64_t>(plannedCadence.den),
        retainedFrames,
        std::move(packetLayout),
        std::move(authority), std::move(backend)});
}

} // namespace media::ffmpeg::graph
