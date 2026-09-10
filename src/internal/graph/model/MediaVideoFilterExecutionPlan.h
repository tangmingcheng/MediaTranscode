#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include <string>
#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaVideoFilterTimingAuthority { PreparedEncoderMetadata, SourceFrame };
enum class MediaVideoFilterAspectPolicy { PreserveSourceAspect };
enum class MediaVideoFilterAllocation { NegotiatedOutput, IndependentOutputPool };
enum class MediaVideoFilterCompletion { NegotiatedOutput, SynchronousOnReturn };

struct MediaVideoFilterIsolationEvidence final {
    std::string filterDescription;
    MediaVideoFilterAllocation allocation;
    MediaVideoFilterCompletion completion;
    std::string allocationAuthority;
    std::string completionAuthority;
    std::uint64_t maximumRetainedInputFrames;
    std::string inputRetentionAuthority;
};

struct MediaVideoFilterExecutionPlan final {
    MediaVideoFilterTimingAuthority timing;
    MediaVideoFilterAspectPolicy aspect;
    MediaVideoFilterIsolationEvidence output;
    MediaRational sourceFrameRate;
    MediaRational sourceSampleAspectRatio;

    bool valid() const noexcept
    {
        if (output.filterDescription.empty() || aspect != MediaVideoFilterAspectPolicy::PreserveSourceAspect) return false;
        if (timing == MediaVideoFilterTimingAuthority::PreparedEncoderMetadata)
            return output.allocation == MediaVideoFilterAllocation::NegotiatedOutput &&
                output.completion == MediaVideoFilterCompletion::NegotiatedOutput &&
                output.maximumRetainedInputFrames == 0 && output.inputRetentionAuthority.empty();
        return timing == MediaVideoFilterTimingAuthority::SourceFrame &&
            sourceFrameRate.num > 0 && sourceFrameRate.den > 0 &&
            sourceSampleAspectRatio.num >= 0 && sourceSampleAspectRatio.den > 0 &&
            output.allocation == MediaVideoFilterAllocation::IndependentOutputPool &&
            output.completion == MediaVideoFilterCompletion::SynchronousOnReturn &&
            !output.allocationAuthority.empty() && !output.completionAuthority.empty() &&
            !output.inputRetentionAuthority.empty();
    }
};

} // namespace media::ffmpeg::graph
