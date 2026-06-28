#pragma once

#include "internal/graph/runtime/lifecycle/MediaRuntimeLifecycleObserver.h"

#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaRuntimeLifecycleController final {
public:
    void addObserver(MediaRuntimeLifecycleObserver* observer);
    void clearObservers();

    void transition(MediaRuntimeLifecycleStage stage, std::string message = {});
    MediaRuntimeLifecycleStage stage() const noexcept;
    const std::vector<MediaRuntimeLifecycleEvent>& events() const noexcept;

private:
    MediaRuntimeLifecycleStage m_stage = MediaRuntimeLifecycleStage::Created;
    std::vector<MediaRuntimeLifecycleEvent> m_events;
    std::vector<MediaRuntimeLifecycleObserver*> m_observers;
};

} // namespace media::ffmpeg::graph
