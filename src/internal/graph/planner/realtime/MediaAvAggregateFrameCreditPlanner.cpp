#include "internal/graph/planner/realtime/MediaAvAggregateFrameCreditPlanner.h"
#include "internal/graph/planner/audio/MediaAudioFrameFootprintPlanner.h"
#include "internal/graph/planner/video/MediaVideoCanvasGeometryValidator.h"
#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"
extern "C" {
#include <libavutil/pixdesc.h>
}
#include <charconv>

namespace media::ffmpeg::graph {
::media::Result<MediaNodeOptions> MediaAvAggregateFrameCreditPlanner::encode(
    const MediaAvContinuousAggregateTopology& topology)
{
    using Result = ::media::Result<MediaNodeOptions>;
    if (auto status = MediaVideoCanvasGeometryValidator::validate(topology.canvas); !status)
        return Result::failure(status.error());
    const auto* format = av_get_pix_fmt_name(topology.canvas.softwareFormat);
    if (!format) return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate canvas format is unknown"));
    auto video = MediaVideoSurfaceFootprint::logicalBytes(topology.canvas.width, topology.canvas.height, format);
    auto audio = MediaAudioFrameFootprintPlanner::logicalBytes(topology.audio.codecFrameSamples(),
        topology.audio.channels(), topology.audio.sampleFormat(), "aggregate prepared audio frame");
    if (!video || !audio) return Result::failure(!video ? video.error() : audio.error());
    MediaNodeOptions options;
    options.set("aggregate.credit.video_bytes", std::to_string(video.value()));
    options.set("aggregate.credit.audio_bytes", std::to_string(audio.value()));
    options.set("aggregate.credit.video_scope", "prepared_hardware_canvas");
    options.set("aggregate.credit.audio_scope", "prepared_output_audio_samples");
    return Result::success(std::move(options));
}

::media::Result<MediaFrameCreditContract> MediaAvAggregateFrameCreditPlanner::decode(
    const MediaNodeOptions& options, MediaStreamKind stream)
{
    using Result = ::media::Result<MediaFrameCreditContract>;
    if (stream != MediaStreamKind::Video && stream != MediaStreamKind::Audio)
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate credit requires its output stream"));
    const bool video = stream == MediaStreamKind::Video;
    const auto bytes = options.value(video ? "aggregate.credit.video_bytes" : "aggregate.credit.audio_bytes");
    const auto scope = options.value(video ? "aggregate.credit.video_scope" : "aggregate.credit.audio_scope");
    std::uint64_t bound = 0;
    const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), bound);
    if (bytes.empty() || parsed.ec != std::errc{} || parsed.ptr != bytes.data() + bytes.size() || !bound ||
        scope != (video ? "prepared_hardware_canvas" : "prepared_output_audio_samples"))
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate credit options are incomplete"));
    return Result::success({video ? MediaFrameCreditAllocationScope::ExternalDeviceObservedOnly
                                 : MediaFrameCreditAllocationScope::EngineLogicalBytes,
        bound, 1, video ? "prepared-canvas-geometry+device-bytes-observed-only"
                       : "prepared-output-audio-frame+avframe-software-samples"});
}
} // namespace media::ffmpeg::graph
