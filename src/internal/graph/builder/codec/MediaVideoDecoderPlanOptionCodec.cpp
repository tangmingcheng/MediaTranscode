#include "internal/graph/builder/codec/MediaDecoderOpaqueRetentionOptionCodec.h"
#include "internal/graph/builder/codec/MediaDecoderInputRetentionOptionCodec.h"
#include "internal/graph/builder/codec/MediaVideoDecoderPlanOptionCodec.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaNodeOptions> MediaVideoDecoderPlanOptionCodec::encode(
    const MediaPipelineStagePlan& decoder,
    std::optional<MediaVideoDecoderLineageOptions> lineage)
{
    using Result = ::media::Result<MediaNodeOptions>;
    if (decoder.role != MediaPipelineStageRole::Decoder ||
        decoder.ffmpegName.empty() || decoder.ffmpegName == "auto" || !decoder.outputFrame) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoder options require a selected decoder and output frame contract"));
    }
    if (lineage && (lineage->capacity == 0 ||
        mediaVideoDecodeLineageProductName(lineage->product).empty() ||
        (lineage->product == MediaVideoDecodeLineageProduct::SourceAccessUnit &&
         lineage->propagation != MediaVideoLineagePropagation::CodecCopyOpaque) ||
        (lineage->propagation != MediaVideoLineagePropagation::CodecCopyOpaque &&
         lineage->propagation != MediaVideoLineagePropagation::SubmissionOrder))) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoder options require an explicit planner lineage propagation product"));
    }
    MediaNodeOptions options;
    const auto boolean = [](bool value) { return value ? "1" : "0"; };
    const auto& output = *decoder.outputFrame;
    options.set(MediaTranscodeOptionKey::PlannedDecoder, decoder.ffmpegName);
    options.set("pipeline.hardware", boolean(output.isHardwareBacked()));
    options.set("pipeline.hwaccel", decoder.hwaccelName);
    options.set("pipeline.device", mediaHardwareDeviceKindName(output.deviceKind));
    options.set("pipeline.frame_kind", mediaHardwareFrameKindName(output.frameKind));
    options.set("decoder.output.pixel_format", output.pixelFormat);
    options.set("decoder.output.surface_pixel_format", output.surfacePixelFormat);
    options.set("decoder.output.requires_hw_device_ctx", boolean(output.requiresHardwareDeviceContext));
    options.set("decoder.output.requires_hw_frames_ctx", boolean(output.requiresHardwareFramesContext));
    if (lineage) {
        options.set("video.lineage.decoder_product",
            std::string(mediaVideoDecodeLineageProductName(lineage->product)));
        options.set("video.lineage.capacity", std::to_string(lineage->capacity));
        options.set("video.lineage.decoder_copy_opaque",
            boolean(lineage->propagation == MediaVideoLineagePropagation::CodecCopyOpaque));
    }
    if (decoder.preparedInputRetention) {
        auto retained = MediaDecoderInputRetentionOptionCodec::encode(*decoder.preparedInputRetention);
        if (!retained) return Result::failure(retained.error());
        for (const auto& [key, value] : retained.value().values()) options.set(key, value);
    }
    if (decoder.preparedOpaqueRetention) {
        auto opaque = MediaDecoderOpaqueRetentionOptionCodec::encode(*decoder.preparedOpaqueRetention);
        if (!opaque) return Result::failure(opaque.error());
        for (const auto& [key, value] : opaque.value().values()) options.set(key, value);
    }
    return Result::success(std::move(options));
}

} // namespace media::ffmpeg::graph
