#pragma once

namespace media::ffmpeg::graph {

enum class MediaStreamKind {
    Unknown,

    Video,
    Audio,
    Subtitle,
    Data,
    Attachment,

    Control,
    Metadata,

    Any
};

} // namespace media::ffmpeg::graph
