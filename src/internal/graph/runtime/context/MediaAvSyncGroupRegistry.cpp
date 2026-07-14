#include "internal/graph/runtime/context/MediaAvSyncGroupRegistry.h"

#include <mutex>
#include <unordered_map>

namespace media::ffmpeg::graph {

struct MediaAvSyncGroupRegistry::State final {
    mutable std::mutex mutex;
    std::unordered_map<MediaAvSyncGroupKey,
                       std::shared_ptr<MediaAvSyncGroupRuntime>,
                       MediaAvSyncGroupKeyHash> groups;
};

MediaAvSyncGroupRegistry::MediaAvSyncGroupRegistry()
    : m_state(std::make_unique<State>())
{
}

MediaAvSyncGroupRegistry::~MediaAvSyncGroupRegistry() = default;

MediaAvSyncGroupRegistry::MediaAvSyncGroupRegistry(
    MediaAvSyncGroupRegistry&& other)
    : m_state(std::make_unique<State>())
{
    std::lock_guard<std::mutex> lock(other.m_state->mutex);
    m_state->groups = std::move(other.m_state->groups);
    other.m_state->groups.clear();
}

MediaAvSyncGroupRegistry& MediaAvSyncGroupRegistry::operator=(
    MediaAvSyncGroupRegistry&& other)
{
    if (this == &other) return *this;
    std::scoped_lock lock(m_state->mutex, other.m_state->mutex);
    m_state->groups = std::move(other.m_state->groups);
    other.m_state->groups.clear();
    return *this;
}
::media::Status MediaAvSyncGroupRegistry::registerGroup(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock)
{
    auto runtime = MediaAvSyncGroupRuntime::create(
        key, std::move(plan), std::move(clock));
    if (!runtime) return ::media::Status::failure(runtime.error());
    auto shared = std::move(runtime).value();
    std::lock_guard<std::mutex> lock(m_state->mutex);
    if (!m_state->groups.emplace(shared->key(), std::move(shared)).second) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group is already registered"));
    }
    return ::media::Status::success();
}

::media::Status MediaAvSyncGroupRegistry::activatePlaybackEpoch(
    const MediaAvSyncGroupKey& key,
    MediaPlaybackEpoch epoch)
{
    auto group = find(key);
    return group ? group->activatePlaybackEpoch(epoch)
                 : ::media::Status::failure(::media::ErrorInfo::notInitialized(
                       "A/V sync group is not registered"));
}

::media::Status MediaAvSyncGroupRegistry::activateNextPlaybackEpoch(
    const MediaAvSyncGroupKey& key,
    MediaPlaybackEpoch epoch)
{
    auto group = find(key);
    return group ? group->activateNextPlaybackEpoch(epoch)
                 : ::media::Status::failure(::media::ErrorInfo::notInitialized(
                       "A/V sync group is not registered"));
}

std::shared_ptr<MediaAvSyncGroupRuntime> MediaAvSyncGroupRegistry::find(
    const MediaAvSyncGroupKey& key) const noexcept
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    auto found = m_state->groups.find(key);
    return found == m_state->groups.end() ? nullptr : found->second;
}

void MediaAvSyncGroupRegistry::clear() noexcept
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->groups.clear();
}

} // namespace media::ffmpeg::graph
