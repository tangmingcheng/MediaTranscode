#pragma once

#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <optional>

namespace media::ffmpeg::graph {

enum class MediaNodeProcessState {
    Progress,
    Waiting,
    Finished
};

struct MediaNodeProcessResult {
    struct DeadlineWait final {
        MediaAvSyncGroupKey syncGroup;
        MediaRunningTime masterDeadline;
    };

    MediaNodeProcessState state = MediaNodeProcessState::Waiting;
    std::optional<DeadlineWait> deadlineWait;

    static constexpr MediaNodeProcessResult progress() noexcept
    {
        return { MediaNodeProcessState::Progress, std::nullopt };
    }

    static constexpr MediaNodeProcessResult waiting() noexcept
    {
        return { MediaNodeProcessState::Waiting, std::nullopt };
    }

    static MediaNodeProcessResult waitingUntil(MediaAvSyncGroupKey group,
                                               MediaRunningTime deadline)
    {
        return {MediaNodeProcessState::Waiting,
                DeadlineWait{std::move(group), deadline}};
    }

    static constexpr MediaNodeProcessResult finished() noexcept
    {
        return { MediaNodeProcessState::Finished, std::nullopt };
    }
};

} // namespace media::ffmpeg::graph
