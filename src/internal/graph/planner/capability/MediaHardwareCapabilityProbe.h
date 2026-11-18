#pragma once
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/capability/MediaDecoderRuntimeFacts.h"

#include <functional>
#include <string>
struct AVBufferRef;
struct AVFrame;
namespace media::ffmpeg::graph {
struct MediaHardwareCapability {
    bool available = false;
    std::string reason;
};
// Graph configuration/readback only; neither decode nor transfer execution is proven.
struct MediaVideoSourceFilterNegotiation {
    MediaSize outputSize;
    std::string pixelFormat;
    MediaRational sampleAspectRatio;
};
class MediaHardwareCapabilityProbe final {
public:
    using ChainValidator = std::function<MediaHardwareCapability(
        MediaPipelineChainPlan&, const MediaPipelinePlannerOptions&)>;

    MediaHardwareCapabilityProbe();
    explicit MediaHardwareCapabilityProbe(ChainValidator chainValidator);

    static ::media::Result<MediaVideoSourceFilterNegotiation> negotiateSourceFilter(
        const MediaVideoSourcePlan& source, const AVFrame& firstFrame,
        const MediaRational& inputTimeBase, const MediaRational& inputFrameRate,
        const MediaRational& sampleAspectRatio, const MediaHardwareDescriptor& target);

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
        const MediaDecoderRuntimeFacts& decoderFacts,
        const MediaVideoSharedSourcePlan& sourceAllocation);

private:
    ChainValidator m_chainValidator;
    bool m_suppliedValidator = false;
};
}
