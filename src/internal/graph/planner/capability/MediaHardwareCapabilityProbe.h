#pragma once
#include "internal/graph/planner/MediaPipelinePlanner.h"

#include <functional>
#include <string>
namespace media::ffmpeg::graph {
struct MediaHardwareCapability {
    bool available = false;
    std::string reason;
};
class MediaHardwareCapabilityProbe final {
public:
    using ChainValidator = std::function<MediaHardwareCapability(
        MediaPipelineChainPlan&, const MediaPipelinePlannerOptions&)>;

    MediaHardwareCapabilityProbe();
    explicit MediaHardwareCapabilityProbe(ChainValidator chainValidator);

    static bool decoderExists(const std::string& name) noexcept;
    static bool encoderExists(const std::string& name) noexcept;
    static bool filterExists(const std::string& name) noexcept;

    ::media::Status validate(MediaPipelineChainPlan& chain,
                             const MediaPipelinePlannerOptions& options) const;

private:
    ChainValidator m_chainValidator;
};
}
