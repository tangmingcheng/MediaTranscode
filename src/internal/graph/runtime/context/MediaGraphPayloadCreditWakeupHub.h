#pragma once

#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <memory>
#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

class MediaNodeWakeup;

class MediaGraphPayloadCreditWakeupHub final
    : public MediaGraphPayloadCreditReleaseObserver {
public:
    void add(std::weak_ptr<MediaNodeWakeup> wakeup);
    void onGraphPayloadCreditReleased() noexcept override;
    void interrupt() noexcept;

private:
    std::mutex m_mutex;
    std::vector<std::weak_ptr<MediaNodeWakeup>> m_wakeups;
    bool m_interrupted = false;
};

} // namespace media::ffmpeg::graph
