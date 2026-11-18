#include "internal/graph/planner/realtime/MediaStartupBatchPayloadPlanner.h"

#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/model/MediaVideoStartupReadiness.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

namespace media::ffmpeg::graph {

::media::Result<MediaStartupBatchPayloadPlan> MediaStartupBatchPayloadPlanner::plan(
    const MediaNode& coordinator)
{
    using Result = ::media::Result<MediaStartupBatchPayloadPlan>;
    if (coordinator.kind != MediaNodeKind::AvStartupCoordinator)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Startup batch payload requires its coordinator"));
    const auto& options = coordinator.options;
    auto members = MediaTranscodeStreamSetCodec::decode(options.value("av_startup.members"));
    if (!members) return Result::failure(members.error());
    auto readiness = decodeMediaVideoStartupReadiness(options.value("av_startup.video_readiness"));
    if (!readiness) return Result::failure(readiness.error());
    // Match the runtime coordinator's positive-int capacity contract exactly.
    auto video = requiredPositiveIntNodeOption(&options, "Startup batch payload", "av_startup.video_capacity");
    if (!video) return Result::failure(video.error());
    std::optional<std::uint64_t> audio;
    if (members.value() == MediaTranscodeStreamSet::AudioVideo) {
        auto capacity = requiredPositiveIntNodeOption(&options, "Startup batch payload", "av_startup.audio_capacity");
        if (!capacity) return Result::failure(capacity.error());
        audio = static_cast<std::uint64_t>(capacity.value());
    } else if (members.value() != MediaTranscodeStreamSet::VideoOnly || options.has("av_startup.audio_capacity")) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Startup batch payload differs from planned stream members"));
    }
    return Result::success({coordinator.id,
        readiness.value() == MediaVideoStartupReadiness::DecodedPresentationSpan
            ? MediaPayloadKind::Frame : MediaPayloadKind::Packet,
        static_cast<std::uint64_t>(video.value()), audio});
}

} // namespace media::ffmpeg::graph
