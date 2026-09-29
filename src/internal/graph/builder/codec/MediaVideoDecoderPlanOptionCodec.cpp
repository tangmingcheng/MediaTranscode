#include "internal/graph/builder/codec/MediaVideoDecoderPlanOptionCodec.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <limits>
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
        options.set("video.lineage.capacity", std::to_string(lineage->capacity));
        options.set("video.lineage.decoder_copy_opaque",
            boolean(lineage->propagation == MediaVideoLineagePropagation::CodecCopyOpaque));
    }
    if (decoder.preparedInputRetention) {
        const auto& retention = *decoder.preparedInputRetention;
        const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (retention.threadCount <= 0 || retention.threadType < 0 || retention.authority.empty() ||
            retention.mainHandoffPackets > maximum ||
            retention.frameWorkerPackets > maximum - retention.mainHandoffPackets ||
            retention.serialPrivatePackets > maximum - retention.mainHandoffPackets - retention.frameWorkerPackets ||
            retention.maximumInternalPackets() == 0) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Decoder options require valid prepared input retention facts"));
        }
        options.set("decoder.pipeline.input_retention.thread_count", std::to_string(retention.threadCount));
        options.set("decoder.pipeline.input_retention.thread_type", std::to_string(retention.threadType));
        options.set("decoder.pipeline.input_retention.main_handoff_packets", std::to_string(retention.mainHandoffPackets));
        options.set("decoder.pipeline.input_retention.frame_worker_packets", std::to_string(retention.frameWorkerPackets));
        options.set("decoder.pipeline.input_retention.serial_private_packets", std::to_string(retention.serialPrivatePackets));
        options.set("decoder.pipeline.input_retention.maximum_internal_packets", std::to_string(retention.maximumInternalPackets()));
        options.set("decoder.pipeline.input_retention.authority", retention.authority);
    }
    return Result::success(std::move(options));
}

} // namespace media::ffmpeg::graph
