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

MediaAvSyncGroupRegistry::~MediaAvSyncGroupRegistry()
{
    clear();
}

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
    clear();
    std::scoped_lock lock(m_state->mutex, other.m_state->mutex);
    m_state->groups = std::move(other.m_state->groups);
    other.m_state->groups.clear();
    return *this;
}

::media::Status MediaAvSyncGroupRegistry::registerGroup(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock,
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch,
    std::shared_ptr<MediaAvEpochTransitionService> transitionService)
{
    auto runtime = MediaAvSyncGroupRuntime::create(
        std::move(key), std::move(plan), std::move(clock),
        std::move(sharedNtpEpoch), std::move(transitionService));
    if (!runtime) return ::media::Status::failure(runtime.error());
    return registerRuntime(std::move(runtime).value());
}

::media::Status MediaAvSyncGroupRegistry::registerRuntime(
    std::shared_ptr<MediaAvSyncGroupRuntime> runtime)
{
    const MediaAvSyncGroupKey key = runtime->key();
    std::lock_guard<std::mutex> lock(m_state->mutex);
    if (!m_state->groups.emplace(key, std::move(runtime)).second) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group is already registered"));
    }
    return ::media::Status::success();
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
    decltype(m_state->groups) groups;
    {
        std::lock_guard<std::mutex> lock(m_state->mutex);
        groups.swap(m_state->groups);
    }
    for (auto& [key, group] : groups) {
        (void)key;
        if (group) group->shutdown();
    }
}

} // namespace media::ffmpeg::graph
