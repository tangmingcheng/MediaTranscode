#pragma once

#include "application/realtime/MediaRealtimeVideoRunController.h"
#include <exception>

namespace media::ffmpeg::graph {

struct MediaRealtimeOutputObserverFailure final { std::exception_ptr exception; };

class MediaRealtimeOutputPublisher final {
public:
    static void publish(MediaRealtimeVideoRunControl& control,
                        const MediaRealtimeVideoRunObserver& observer,
                        const MediaRealtimeOutputSnapshot& snapshot)
    {
        control.publishOutputSnapshot(snapshot);
        if (observer.outputChanged) {
            try { observer.outputChanged(snapshot); }
            catch (...) { throw MediaRealtimeOutputObserverFailure{std::current_exception()}; }
        }
    }
};

} // namespace media::ffmpeg::graph
