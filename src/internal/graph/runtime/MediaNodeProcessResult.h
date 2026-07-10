#pragma once

namespace media::ffmpeg::graph {

enum class MediaNodeProcessState {
    Progress,
    Waiting,
    Finished
};

struct MediaNodeProcessResult {
    MediaNodeProcessState state = MediaNodeProcessState::Waiting;

    static constexpr MediaNodeProcessResult progress() noexcept
    {
        return { MediaNodeProcessState::Progress };
    }

    static constexpr MediaNodeProcessResult waiting() noexcept
    {
        return { MediaNodeProcessState::Waiting };
    }

    static constexpr MediaNodeProcessResult finished() noexcept
    {
        return { MediaNodeProcessState::Finished };
    }
};

} // namespace media::ffmpeg::graph
