#pragma once

#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "media_transcode/Result.h"

#include <memory>
#include <new>
#include <utility>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

// Frozen before publication. Weak targets cannot keep workers or their storage
// alive; notification never re-enters protocol/global state or executes work.
class MediaStorageReleaseNotification final {
public:
    static ::media::Result<std::shared_ptr<const MediaStorageReleaseNotification>> create(
        std::vector<std::weak_ptr<MediaNodeWakeup>> targets)
    {
        using Result = ::media::Result<std::shared_ptr<const MediaStorageReleaseNotification>>;
        if (targets.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
            "Storage release notification requires admitted producer targets"));
        for (const auto& target : targets) {
            if (target.expired()) return Result::failure(::media::ErrorInfo::invalidArgument(
                "Storage release notification producer is missing"));
        }
        try {
            return Result::success(std::shared_ptr<const MediaStorageReleaseNotification>(
                new MediaStorageReleaseNotification(std::move(targets))));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "Storage release notification allocation failed"));
        }
    }

    void notify() const noexcept
    {
        for (const auto& target : m_targets) {
            if (auto wakeup = target.lock()) wakeup->notify();
        }
    }

private:
    explicit MediaStorageReleaseNotification(
        std::vector<std::weak_ptr<MediaNodeWakeup>> targets) noexcept
        : m_targets(std::move(targets)) {}
    const std::vector<std::weak_ptr<MediaNodeWakeup>> m_targets;
};

struct MediaStorageReleaseUnobserved final {};
using MediaStorageReleaseObservation = std::variant<
    MediaStorageReleaseUnobserved,
    std::shared_ptr<const MediaStorageReleaseNotification>>;

} // namespace media::ffmpeg::graph
