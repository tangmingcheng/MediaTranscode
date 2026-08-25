#include "internal/graph/planner/capability/MediaEncoderOpenContractAdapter.h"

#include "internal/graph/planner/capability/MediaEncoderEmissionPreflightAdapter.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

#include <array>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(std::string message)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(std::move(message)));
}

::media::Status applyPrivateString(
    AVCodecContext& context,
    const char* name,
    const std::string& value)
{
    if (value.empty()) return ::media::Status::success();
    if (!context.priv_data ||
        !av_opt_find(context.priv_data, name, nullptr, 0, 0)) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            std::string("opened encoder does not expose planned ") + name));
    }
    const int applied = av_opt_set(context.priv_data, name, value.c_str(), 0);
    return applied < 0
        ? FFmpegGraphError::statusFromCode(
              applied, std::string("apply planned encoder ") + name)
        : ::media::Status::success();
}

::media::Result<std::string> privateString(
    const AVCodecContext& context,
    const char* name)
{
    if (!context.priv_data ||
        !av_opt_find(context.priv_data, name, nullptr, 0, 0)) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::unsupported(
                std::string("opened encoder does not expose planned ") + name));
    }
    std::uint8_t* raw = nullptr;
    const int read = av_opt_get(context.priv_data, name, 0, &raw);
    if (read < 0) {
        return ::media::Result<std::string>::failure(
            FFmpegGraphError::statusFromCode(
                read, std::string("read planned encoder ") + name).error());
    }
    std::string value = raw ? reinterpret_cast<const char*>(raw) : std::string();
    av_free(raw);
    return ::media::Result<std::string>::success(std::move(value));
}

::media::Status comparePrivateString(
    const AVCodecContext& left,
    const AVCodecContext& right,
    const char* name,
    const std::string& requested)
{
    if (requested.empty()) return ::media::Status::success();
    auto leftValue = privateString(left, name);
    auto rightValue = privateString(right, name);
    if (!leftValue || !rightValue) {
        return ::media::Status::failure(
            !leftValue ? leftValue.error() : rightValue.error());
    }
    return leftValue.value() == rightValue.value()
        ? ::media::Status::success()
        : invalid(std::string("packet-layout probe readback differs for encoder ") + name);
}

} // namespace

::media::Status MediaEncoderOpenContractAdapter::applyBeforeOpen(
    AVCodecContext& context,
    const MediaEncoderOpenContract& contract)
{
    if (!context.codec || !context.codec->name ||
        contract.codecName != context.codec->name ||
        contract.width <= 0 || contract.height <= 0 ||
        !contract.frameRate.isKnown()) {
        return invalid("encoder open contract identity or geometry is incomplete");
    }
    if (contract.lowLatency && contract.bFrames.value_or(0) != 0) {
        return invalid("low-latency encoder open contract rejects B-frames");
    }

    context.width = contract.width;
    context.height = contract.height;
    context.time_base = AVRational{
        contract.frameRate.den, contract.frameRate.num};
    context.framerate = AVRational{
        contract.frameRate.num, contract.frameRate.den};
    if (contract.gop) context.gop_size = *contract.gop;
    if (contract.bFrames) context.max_b_frames = *contract.bFrames;
    else if (contract.lowLatency) context.max_b_frames = 0;
    if (contract.globalHeader) {
        if (*contract.globalHeader) context.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        else context.flags &= ~AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (auto status = MediaEncoderEmissionPreflightAdapter::applyBeforeOpen(
            context, contract.rateControl); !status) return status;
    if (auto status = applyPrivateString(context, "preset", contract.preset); !status) return status;
    if (auto status = applyPrivateString(context, "profile", contract.profile); !status) return status;
    if (auto status = applyPrivateString(context, "tune", contract.tune); !status) return status;
    if (auto status = applyPrivateString(context, "level", contract.level); !status) return status;

    if (contract.quality) {
        const char* name = contract.rateControl.mode == MediaRateControlMode::Crf
            ? "crf" : contract.rateControl.mode == MediaRateControlMode::Auto
            ? "quality" : nullptr;
        if (!name) {
            return invalid("encoder quality conflicts with planned rate-control mode");
        }
        const std::string value = std::to_string(*contract.quality);
        if (auto status = applyPrivateString(context, name, value); !status) return status;
    }
    return ::media::Status::success();
}

::media::Status MediaEncoderOpenContractAdapter::validateEquivalentReadback(
    const AVCodecContext& productionProbe,
    const AVCodecContext& packetLayoutProbe,
    const MediaEncoderOpenContract& contract)
{
    const bool publicFieldsMatch =
        productionProbe.codec_id == packetLayoutProbe.codec_id &&
        productionProbe.width == packetLayoutProbe.width &&
        productionProbe.height == packetLayoutProbe.height &&
        av_cmp_q(productionProbe.time_base, packetLayoutProbe.time_base) == 0 &&
        av_cmp_q(productionProbe.framerate, packetLayoutProbe.framerate) == 0 &&
        productionProbe.bit_rate == packetLayoutProbe.bit_rate &&
        productionProbe.rc_min_rate == packetLayoutProbe.rc_min_rate &&
        productionProbe.rc_max_rate == packetLayoutProbe.rc_max_rate &&
        productionProbe.rc_buffer_size == packetLayoutProbe.rc_buffer_size &&
        productionProbe.gop_size == packetLayoutProbe.gop_size &&
        productionProbe.max_b_frames == packetLayoutProbe.max_b_frames &&
        productionProbe.profile == packetLayoutProbe.profile &&
        productionProbe.level == packetLayoutProbe.level &&
        ((productionProbe.flags ^ packetLayoutProbe.flags) &
         AV_CODEC_FLAG_GLOBAL_HEADER) == 0;
    if (!publicFieldsMatch) {
        return invalid(
            "packet-layout probe readback differs from production encoder open contract");
    }

    if (auto status = comparePrivateString(
            productionProbe, packetLayoutProbe, "preset", contract.preset); !status) return status;
    if (auto status = comparePrivateString(
            productionProbe, packetLayoutProbe, "profile", contract.profile); !status) return status;
    if (auto status = comparePrivateString(
            productionProbe, packetLayoutProbe, "tune", contract.tune); !status) return status;
    if (auto status = comparePrivateString(
            productionProbe, packetLayoutProbe, "level", contract.level); !status) return status;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
