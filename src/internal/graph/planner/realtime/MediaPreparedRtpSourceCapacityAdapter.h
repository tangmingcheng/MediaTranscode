#pragma once
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
namespace media::ffmpeg::graph {
struct MediaPreparedRtpStorageAdmission;
// The final production boundary validates real ownership before exposing the
// same capacity facts that pure candidate evaluation can consume directly.
class MediaPreparedRtpSourceCapacityAdapter final {
public:
    static ::media::Result<MediaAvSourceCapacityFacts> read(
        const MediaPreparedRtpStorageAdmission& admission,const MediaPreparationMemoryResource& owner,
        MediaTranscodeStreamSet members);
};
}
