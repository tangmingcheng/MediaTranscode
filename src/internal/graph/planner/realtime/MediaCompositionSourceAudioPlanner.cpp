#include "internal/graph/planner/realtime/MediaCompositionSourceAudioPlanner.h"
#include <new>

namespace media::ffmpeg::graph {
::media::Result<MediaFixedCapacityStorage<std::optional<MediaSelectedAudioResampler>>>
MediaCompositionSourceAudioPlanner::prepare(
    std::span<const MediaRealtimeCompositionSourceFacts> sources, std::size_t selected,
    const MediaAudioPipelinePlan& selectedAudio, MediaPreparationMemoryResource& resource,
    const MediaPreparationControl& control)
try {
    using Result = ::media::Result<MediaFixedCapacityStorage<std::optional<MediaSelectedAudioResampler>>>;
    if (selected >= sources.size() || !sources[selected].raw.audio ||
        !sources[selected].raw.audio->selectedDecoder || !selectedAudio.enabled ||
        selectedAudio.branchMode != MediaBranchMode::TranscodeFrame || !selectedAudio.resolvedOutput ||
        !selectedAudio.selectedDecoder || !selectedAudio.selectedResampler ||
        selectedAudio.sourceStreamIndex != sources[selected].raw.audio->streamIndex ||
        *selectedAudio.selectedDecoder != *sources[selected].raw.audio->selectedDecoder)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition audio preparation requires the selected source's actual decoder and output"));
    MediaFixedCapacityStorage<std::optional<MediaSelectedAudioResampler>> result(sources.size(), resource);
    for (std::size_t index = 0; index < sources.size(); ++index) {
        if (auto status = control.check("composition source audio preparation"); !status)
            return Result::failure(status.error());
        const auto& input = sources[index].raw.audio;
        if (!input) { result.push_back(std::nullopt); continue; }
        if (!input->selectedDecoder)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Composition source audio has no prepared decoder"));
        if (index == selected) result.push_back(selectedAudio.selectedResampler);
        else {
            auto verified = MediaAudioResamplerCapabilityProvider::verify(
                *input->selectedDecoder, *selectedAudio.resolvedOutput);
            if (!verified) return Result::failure(verified.error());
            result.push_back(std::move(verified).value());
        }
        if (auto status = control.check("composition source audio readback"); !status)
            return Result::failure(status.error());
    }
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaFixedCapacityStorage<std::optional<MediaSelectedAudioResampler>>>::failure(
        ::media::ErrorInfo::allocationFailed("composition source audio facts"));
}
} // namespace media::ffmpeg::graph
