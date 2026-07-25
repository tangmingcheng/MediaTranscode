#include "internal/graph/sync/lineage/MediaAudioLineageStagePreparation.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"

#include <charconv>
#include <string>

namespace media::ffmpeg::graph {

::media::Result<MediaAudioLineageStagePreparation>
prepareMediaAudioLineageStage(
    const MediaNode& node,
    std::string_view expectedIdentity)
{
    const bool synchronized = node.options.value(
        std::string(MediaAudioLineageModeOptionKey)) ==
        mediaAudioLineageExecutionModeName(
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio);
    const bool hasIdentity = node.options.has("audio.lineage.identity");
    const bool hasCapacity = node.options.has("audio.lineage.capacity");
    if (!synchronized) {
        if (hasIdentity || hasCapacity) {
            return ::media::Result<MediaAudioLineageStagePreparation>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Legacy audio lineage stage rejects synchronized options"));
        }
        return ::media::Result<MediaAudioLineageStagePreparation>::success(
            {false, 0});
    }
    const std::string capacityText = node.options.value("audio.lineage.capacity");
    std::size_t capacity = 0;
    const auto parsed = std::from_chars(
        capacityText.data(), capacityText.data() + capacityText.size(), capacity);
    if (!hasIdentity || !hasCapacity || expectedIdentity.empty() || capacity == 0 ||
        parsed.ec != std::errc{} || parsed.ptr != capacityText.data() + capacityText.size() ||
        node.options.value("audio.lineage.identity") != expectedIdentity) {
        return ::media::Result<MediaAudioLineageStagePreparation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized audio stage requires exact planner identity and capacity"));
    }
    return ::media::Result<MediaAudioLineageStagePreparation>::success(
        {true, capacity});
}

} // namespace media::ffmpeg::graph
