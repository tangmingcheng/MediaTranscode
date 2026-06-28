#pragma once

#include "internal/graph/runtime/lifecycle/MediaRuntimeLifecycleEvent.h"

namespace media::ffmpeg::graph {

class MediaRuntimeLifecycleObserver {
public:
    virtual ~MediaRuntimeLifecycleObserver() = default;
    virtual void onLifecycleEvent(const MediaRuntimeLifecycleEvent& event) = 0;

protected:
    MediaRuntimeLifecycleObserver() = default;
};

} // namespace media::ffmpeg::graph
