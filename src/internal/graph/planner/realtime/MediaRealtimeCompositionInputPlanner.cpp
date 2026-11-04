#include "internal/graph/planner/realtime/MediaRealtimeCompositionInputPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"

#include <utility>
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeCompositionPreparationPlan>
MediaRealtimeCompositionInputPlanner::plan(const MediaRealtimeCompositionConfig& composition)
{
    using Result = ::media::Result<MediaRealtimeCompositionPreparationPlan>;
    if (!composition.preparationStorageLimitBytes ||
        composition.preparationStorageLimitBytes > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition preparation storage requires a positive addressable deployment limit"));
    if (composition.inputs.size() < 2 || composition.inputs.size() > 4 ||
        composition.audioSource >= composition.inputs.size())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition requires two through four inputs and a valid selected audio source"));
    std::vector<MediaRealtimeCompositionInputPlan> plans;
    plans.reserve(composition.inputs.size());
    for (const auto& input : composition.inputs) {
        if (input.type != RealtimeInputType::RtpPort ||
            !input.openTimeoutMs || *input.openTimeoutMs <= 0 ||
            !input.inputLatencyMs || *input.inputLatencyMs <= 0)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Composition source requires explicit RTP input, positive open timeout and input latency"));
        const auto& audio = input.audioRtp;
        const bool hasAudio = !audio.url.empty() || !audio.codecName.empty() ||
            audio.payloadType || audio.clockRate || audio.channels || audio.fmtp;
        if (hasAudio) {
            auto descriptor = MediaRealtimeRtpCodecRegistry::describe(MediaStreamKind::Audio, audio);
            if (!descriptor) return Result::failure(descriptor.error());
        }
        plans.push_back({input, hasAudio ? MediaTranscodeStreamSet::AudioVideo
                                        : MediaTranscodeStreamSet::VideoOnly});
    }
    if (plans[composition.audioSource].members != MediaTranscodeStreamSet::AudioVideo)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Selected composition audio source has no actual audio stream"));
    return Result::success({std::move(plans),
        static_cast<std::size_t>(composition.preparationStorageLimitBytes)});
}

} // namespace media::ffmpeg::graph
