#pragma once

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/runtime/factory/MediaAvRuntimeRegistrationPlan.h"

#include <vector>
#include "internal/graph/builder/segments/MediaAvSyncInputBindings.h"
#include "internal/graph/builder/segments/MediaBranchConnectionBindings.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeAvSyncInputEndpoints final {
    MediaEndpoint releasedVideo;
    MediaEndpoint releasedAudio;
    MediaEndpoint activatedRelease;
    MediaAvRuntimeInputRegistration registration;
    std::vector<MediaNodeId> sourceMembers;
    std::optional<MediaVideoBranchBindings> decodedVideoBindings;
    MediaAvSyncInputBindings bindings;
};

} // namespace media::ffmpeg::graph
