#include "internal/graph/runtime/lifecycle/MediaRuntimeLifecycleController.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

void MediaRuntimeLifecycleController::addObserver(MediaRuntimeLifecycleObserver* observer)
{
    if (!observer) {
        return;
    }

    if (std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end()) {
        m_observers.push_back(observer);
    }
}

void MediaRuntimeLifecycleController::clearObservers()
{
    m_observers.clear();
}

void MediaRuntimeLifecycleController::transition(MediaRuntimeLifecycleStage stage, std::string message)
{
    m_stage = stage;

    MediaRuntimeLifecycleEvent event;
    event.stage = stage;
    event.timestamp = std::chrono::steady_clock::now();
    event.message = std::move(message);
    m_events.push_back(event);

    for (MediaRuntimeLifecycleObserver* observer : m_observers) {
        if (observer) {
            observer->onLifecycleEvent(event);
        }
    }
}

MediaRuntimeLifecycleStage MediaRuntimeLifecycleController::stage() const noexcept
{
    return m_stage;
}

const std::vector<MediaRuntimeLifecycleEvent>& MediaRuntimeLifecycleController::events() const noexcept
{
    return m_events;
}

} // namespace media::ffmpeg::graph
