#pragma once

#include <chrono>
#include <memory>

namespace media::ffmpeg::graph {
class MediaRealtimeVideoRunControl;
}

namespace media::ffmpeg::graph::cli {

// OS notification only; all media cancellation remains in the run controller.
class RealtimeStopSignal final {
public:
    RealtimeStopSignal(MediaRealtimeVideoRunControl& control,
                       std::chrono::milliseconds pollInterval);
    ~RealtimeStopSignal();
    RealtimeStopSignal(const RealtimeStopSignal&) = delete;
    RealtimeStopSignal& operator=(const RealtimeStopSignal&) = delete;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace media::ffmpeg::graph::cli
