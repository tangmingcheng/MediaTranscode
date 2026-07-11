#pragma once
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <string>
#include <mutex>
#include <unordered_map>
namespace media::ffmpeg::graph {
struct MediaHardwareCapability {
    bool available = false;
    std::string reason;
};
class MediaHardwareCapabilityProbe final {
public:
    static bool decoderExists(const std::string& name) noexcept;
    static bool encoderExists(const std::string& name) noexcept;
    static bool filterExists(const std::string& name) noexcept;
    void apply(MediaPipelineStagePlan& stage,
               const MediaPipelinePlannerOptions& options);

private:
    std::unordered_map<std::string, MediaHardwareCapability> m_cache;
    std::mutex m_mutex;
};
}
