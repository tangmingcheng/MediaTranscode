#pragma once

#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <chrono>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

enum class MediaNodeProcessState {
    Progress,
    Waiting,
    Finished
};

struct MediaNodeProcessResult {
    struct DeadlineWait final {
        struct AvSyncMaster final {
            MediaAvSyncGroupKey syncGroup;
            MediaRunningTime deadline;
        };
        struct Steady final {
            std::chrono::steady_clock::time_point deadline;
        };

        std::variant<AvSyncMaster, Steady> deadline;

        DeadlineWait(MediaAvSyncGroupKey group,
            MediaRunningTime deadline)
            : deadline(AvSyncMaster{std::move(group), deadline})
        {
        }

        explicit DeadlineWait(
            std::chrono::steady_clock::time_point deadline)
            : deadline(Steady{deadline})
        {
        }
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

    static MediaNodeProcessResult waitingUntil(
        std::chrono::steady_clock::time_point deadline)
    {
        return {MediaNodeProcessState::Waiting, DeadlineWait{deadline}};
    }

    static constexpr MediaNodeProcessResult finished() noexcept
    {
        return { MediaNodeProcessState::Finished, std::nullopt };
    }
};

} // namespace media::ffmpeg::graph
