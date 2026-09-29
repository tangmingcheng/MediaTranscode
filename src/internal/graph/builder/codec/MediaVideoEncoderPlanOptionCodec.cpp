#include "internal/graph/builder/codec/MediaVideoEncoderPlanOptionCodec.h"

#include "internal/graph/builder/codec/MediaEncoderRateControlOptionAdapter.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaNodeOptions> MediaVideoEncoderPlanOptionCodec::encode(
    const MediaPipelineStagePlan& encoder)
{
    using Result = ::media::Result<MediaNodeOptions>;
    if (!encoder.inputFrame || !encoder.encoderRateControl || !encoder.encoderOpenContract) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Encoder options require planner frame, rate-control and open products"));
    }
    auto options = MediaEncoderRateControlOptionAdapter::encode(*encoder.encoderRateControl);
    const auto boolean = [](bool value) { return value ? "1" : "0"; };
    const auto& input = *encoder.inputFrame;
    options.set(MediaTranscodeOptionKey::PlannedEncoder, encoder.ffmpegName);
    options.set("encoder.pixel_format", input.pixelFormat);
    options.set("encoder.hw_frames_format", input.requiresHardwareFramesContext ? input.pixelFormat : std::string());
    options.set("encoder.surface_pixel_format", input.surfacePixelFormat);
    options.set("encoder.requires_hw_device_ctx", boolean(input.requiresHardwareDeviceContext));
    options.set("encoder.requires_hw_frames_ctx", boolean(input.requiresHardwareFramesContext));
    const auto& open = *encoder.encoderOpenContract;
    options.set("encoder.low_latency", boolean(open.lowLatency));
    options.set(MediaTranscodeOptionKey::VideoWidth, std::to_string(open.width));
    options.set(MediaTranscodeOptionKey::VideoHeight, std::to_string(open.height));
    options.set(MediaTranscodeOptionKey::VideoFpsNum, std::to_string(open.frameRate.num));
    options.set(MediaTranscodeOptionKey::VideoFpsDen, std::to_string(open.frameRate.den));
    options.set(MediaTranscodeOptionKey::VideoPreset, open.preset);
    options.set(MediaTranscodeOptionKey::VideoProfile, open.profile);
    options.set(MediaTranscodeOptionKey::VideoTune, open.tune);
    options.set(MediaTranscodeOptionKey::VideoLevel, open.level);
    const auto optional = [&](const char* key, const std::optional<int>& value) {
        if (value) options.set(key, std::to_string(*value));
    };
    optional(MediaTranscodeOptionKey::VideoQuality, open.quality);
    optional(MediaTranscodeOptionKey::VideoGop, open.gop);
    optional(MediaTranscodeOptionKey::VideoBFrames, open.bFrames);
    if (open.globalHeader) options.set(MediaTranscodeOptionKey::VideoGlobalHeader, boolean(*open.globalHeader));
    return Result::success(std::move(options));
}

} // namespace media::ffmpeg::graph
