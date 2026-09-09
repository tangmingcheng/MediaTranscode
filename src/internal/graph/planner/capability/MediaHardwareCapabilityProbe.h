#pragma once
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/capability/MediaDecoderRuntimeFacts.h"

#include <functional>
#include <string>
struct AVBufferRef;
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
    bool hasSuppliedValidator() const noexcept { return m_suppliedValidator; }
    static MediaHardwareCapability validateOutputBranch(
        MediaPipelineChainPlan& chain,
        const MediaPipelinePlannerOptions& options,
        AVBufferRef* runningFrames,
        const MediaDecoderRuntimeFacts& decoderFacts);

private:
    ChainValidator m_chainValidator;
    bool m_suppliedValidator = false;
};
}
