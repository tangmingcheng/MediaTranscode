#pragma once

#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/runtime/threading/MediaNodeDeadlineWakePolicy.h"

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
        MediaNodeDeadlineWakePolicy wakePolicy;

        DeadlineWait(MediaAvSyncGroupKey group,
            MediaRunningTime deadline,
            MediaNodeDeadlineWakePolicy selectedWakePolicy)
            : deadline(AvSyncMaster{std::move(group), deadline}),
              wakePolicy(selectedWakePolicy)
        {
        }

        explicit DeadlineWait(
            std::chrono::steady_clock::time_point deadline,
            MediaNodeDeadlineWakePolicy selectedWakePolicy)
            : deadline(Steady{deadline}),
              wakePolicy(selectedWakePolicy)
        {
        }
    };

    MediaNodeProcessState state = MediaNodeProcessState::Waiting;
    std::optional<DeadlineWait> deadlineWait;

    static MediaNodeProcessResult progress() noexcept
    {
        return { MediaNodeProcessState::Progress, std::nullopt };
    }

    static MediaNodeProcessResult waiting() noexcept
    {
        return { MediaNodeProcessState::Waiting, std::nullopt };
    }

    static MediaNodeProcessResult waitingUntilInputOrDeadline(
        MediaAvSyncGroupKey group,
        MediaRunningTime deadline)
    {
        return {MediaNodeProcessState::Waiting,
                DeadlineWait{
                    std::move(group), deadline,
                    MediaNodeDeadlineWakePolicy::InputOrDeadline}};
    }

    static MediaNodeProcessResult waitingUntilInputOrDeadline(
        std::chrono::steady_clock::time_point deadline)
    {
        return {MediaNodeProcessState::Waiting,
                DeadlineWait{
                    deadline,
                    MediaNodeDeadlineWakePolicy::InputOrDeadline}};
    }

    static MediaNodeProcessResult finished() noexcept
    {
        return { MediaNodeProcessState::Finished, std::nullopt };
    }
};

} // namespace media::ffmpeg::graph
