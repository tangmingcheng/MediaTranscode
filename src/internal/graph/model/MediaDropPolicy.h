#pragma once

#include <cstddef>

namespace media::ffmpeg::graph {

enum class MediaDropMode {
    None,
    DropNewest,
    DropOldest,
    DropNonKeyFrame,
    DropUntilKeyFrame,
    DropLateFrame,
    Custom
};

enum class MediaDropTarget {
    Any,
    Packet,
    Frame,
    VideoOnly,
    AudioOnly,
    NonKeyVideoOnly
};

struct MediaDropPolicy {
    MediaDropMode mode = MediaDropMode::None;
    MediaDropTarget target = MediaDropTarget::Any;

    std::size_t maxDropCountPerBurst = 0;
    bool preserveKeyFrames = true;
    bool preserveAudioContinuity = true;
    bool emitDropDiagnostics = true;

    constexpr bool operator==(const MediaDropPolicy&) const noexcept = default;

    constexpr bool enabled() const noexcept
    {
        return mode != MediaDropMode::None;
    }
};

} // namespace media::ffmpeg::graph
