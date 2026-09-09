#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaRealtimeOutputState {
    Preparing,
    WaitingForRandomAccess,
    Running,
    Draining,
    Retired,
    Failed
};

enum class MediaRealtimeOutputFailureStage {
    Planning,
    Preparation,
    Publication,
    Runtime,
    Drain,
    Release
};

struct MediaRealtimeOutputSnapshot final {
    std::uint64_t outputId;
    MediaRealtimeOutputState state;
    MediaRealtimeOutputFailureStage stage;
    std::string detail;
    std::string descriptionPath;
    std::optional<::media::ErrorInfo> error;
};

} // namespace media::ffmpeg::graph
