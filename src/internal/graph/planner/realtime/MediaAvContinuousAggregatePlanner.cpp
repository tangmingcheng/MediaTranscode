#include "internal/graph/nodes/sync/MediaContinuousAudioAggregateNode.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanValidator.h"
#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"
#include "internal/graph/runtime/storage/MediaAvAggregateStorage.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
class Size final {
public:
    bool add(std::uint64_t count, std::uint64_t bytes = 1) noexcept
    {
        if (bytes && count > (std::numeric_limits<std::uint64_t>::max() - value) / bytes) return false;
        value += count * bytes; return true;
    }
    std::uint64_t value = 0;
};
}

::media::Result<std::size_t> MediaAvContinuousAggregatePlanner::videoCandidateCapacity(
    std::size_t frameQueueCapacity, bool sourceDisplayPoints,
    MediaRational sourceFrameRate, MediaRunningTime outputLead)
{
    using Result = ::media::Result<std::size_t>;
    if (!frameQueueCapacity || outputLead.nanoseconds() < 0)
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate candidate capacity requires queue and nonnegative lead"));
    if (sourceDisplayPoints) return Result::success(frameQueueCapacity);
    if (sourceFrameRate.num <= 0 || sourceFrameRate.den <= 0)
        return Result::failure(::media::ErrorInfo::invalidArgument("Canonical aggregate capacity requires observed cadence"));
    Size ticks;
    if (!ticks.add(static_cast<std::uint64_t>(outputLead.nanoseconds()), static_cast<std::uint64_t>(sourceFrameRate.num)))
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate candidate lead conversion overflow"));
    const auto denominator = static_cast<std::uint64_t>(sourceFrameRate.den) * 1000000000ULL;
    const auto leadFrames = ticks.value / denominator + (ticks.value % denominator != 0);
    if (leadFrames >= std::numeric_limits<std::size_t>::max())
        return Result::failure(::media::ErrorInfo::invalidArgument("Aggregate candidate count overflow"));
    return Result::success(std::max(frameQueueCapacity, static_cast<std::size_t>(leadFrames + 1)));
}

::media::Result<MediaAvContinuousAggregateTopology> MediaAvContinuousAggregatePlanner::plan(
    std::span<const MediaAvAggregateSourceFacts> sources,
    const MediaRealtimeAvOutputRuntimePlan& output, MediaVideoCanvasGeometry canvas,
    MediaRational outputFrameRate, std::size_t audioSource)
{
    using Result = ::media::Result<MediaAvContinuousAggregateTopology>;
    const auto fail = [](const char* message) { return Result::failure(::media::ErrorInfo::invalidArgument(message)); };
    if (sources.empty() || audioSource >= sources.size() || !output.groupKey.valid() ||
        output.activationOutputLead.nanoseconds() <= 0 || output.audioOutput.sampleRate() <= 0 ||
        output.audioOutput.codecFrameSamples() <= 0 ||
        output.synchronization.domainRole != MediaAvSyncDomainRole::ContinuousOutput)
        return fail("Aggregate planning requires prepared source and output contracts");
    const auto& selected = sources[audioSource];
    if (!selected.audioIdentity || !selected.audioCorrection || selected.audioCorrection->maximumResamplerOutputBlockSamples <= 0 ||
        selected.audioCorrection->epochOutputSampleIndex < 0 || selected.frameQueueCapacity == 0)
        return fail("Aggregate selected audio requires prepared correction and queue facts");
    MediaRunningTime lead = output.activationOutputLead;
    for (const auto& source : sources) {
        if (source.activationOutputLead.nanoseconds() <= 0)
            return fail("Aggregate source preparation lead is missing");
        lead = std::max(lead, source.activationOutputLead);
    }
    MediaAvContinuousAggregateTopology plan{
        output.groupKey, {}, audioSource, "source_audio_" + std::to_string(audioSource),
        std::move(canvas), outputFrameRate, output.audioOutput, lead,
        selected.initialGeneration, selected.audioCorrection->epochOutputSampleIndex,
        selected.frameQueueCapacity, 0, static_cast<std::size_t>(output.audioOutput.codecFrameSamples()), 0, {0, 0, 0}};
    Size samples;
    if (!samples.add(plan.maximumAudioCandidates,
            static_cast<std::uint64_t>(selected.audioCorrection->maximumResamplerOutputBlockSamples)) ||
        samples.value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return fail("Aggregate candidate audio sample bound overflow");
    plan.maximumAudioCandidateSamples = static_cast<std::int64_t>(samples.value);
    Size fixed, retained, objects;
    if (!fixed.add(sizeof(MediaAvContinuousAggregateNode)) ||
        !fixed.add(sizeof(MediaContinuousAudioAggregateNode) + sizeof(MediaOwnerThreadGenerationPurge)) ||
        !fixed.add(sizeof(MediaAvAggregateCoordination)) ||
        !fixed.add(sources.size(), sizeof(MediaAvAggregateSourceCompletion)) ||
        !fixed.add(2, sizeof(MediaAvAggregateCoordinationSnapshot)) ||
        !fixed.add(sources.size(), sizeof(std::uint64_t)) ||
        !fixed.add(2, sizeof(MediaAvOutputPermitCommitReservation)) ||
        !fixed.add(sources.size(), sizeof(MediaAvAggregateSourceStorage) + sizeof(MediaOwnerThreadGenerationPurge)) ||
        !fixed.add(plan.maximumAudioCandidates, sizeof(std::shared_ptr<MediaBoundCanonicalAudioBuffer>)) ||
        !fixed.add(sources.size(), sizeof(std::uint64_t)) ||
        !fixed.add(sources.size(), sizeof(MediaAvOutputPermitCommitReservation)) ||
        !fixed.add(sizeof(MediaAvOutputPermitCommitReservation)) ||
        !fixed.add(sources.size(), sizeof(MediaVideoCanvasTile) + sizeof(MediaCanonicalVideoContribution)) ||
        !fixed.add(plan.maximumAudioContributions, sizeof(MediaAudioIntervalFragment)) ||
        !objects.add(plan.maximumAudioCandidates) || !objects.add(2))
        return fail("Aggregate fixed backing storage overflow");
    Size videoPending;
    if (!videoPending.add(sizeof(MediaCanonicalVideoFrameBuffer) + sizeof(MediaCanonicalLineage)) ||
        !videoPending.add(output.groupKey.value().size()) ||
        !videoPending.add(sources.size(), sizeof(MediaCanonicalVideoContribution)))
        return fail("Aggregate pending video metadata overflow");
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const auto& source = sources[i];
        const bool hasAudio = source.members == MediaTranscodeStreamSet::AudioVideo;
        const bool display = source.sourceDisplayPoints;
        if ((!hasAudio && source.members != MediaTranscodeStreamSet::VideoOnly) ||
            source.audioIdentity.has_value() != hasAudio || source.audioEnabled != hasAudio ||
            (source.audioCorrection != nullptr) != hasAudio ||
            (source.audioOutput != nullptr) != hasAudio ||
            (!display && (source.frameRate.num <= 0 || source.frameRate.den <= 0)) ||
            source.frameQueueCapacity == 0 ||
            (display && source.frameQueueCapacity < 2) ||
            source.videoReadiness != (display
                ? MediaVideoStartupReadiness::DecodedPresentationSpan : MediaVideoStartupReadiness::IntervalCoverage) ||
            source.videoIdentity.empty() ||
            source.initialGeneration != plan.initialGeneration ||
            source.domainRole != MediaAvSyncDomainRole::SourceContribution)
            return fail("Aggregate source normalization, identity or clock contract differs");
        if (hasAudio && (source.audioIdentity->empty() ||
            source.audioOutput->sampleRate() != plan.audio.sampleRate() ||
            source.audioOutput->sampleFormat() != plan.audio.sampleFormat() ||
            source.audioOutput->channelLayout() != plan.audio.channelLayout()))
            return fail("Aggregate source audio normalization differs from its output contract");
        // Display points use the admitted count bound. It does not promise a
        // buffered time horizon; one slot retains the current frame, another
        // allows the next point or EOF to make progress.
        auto candidateCapacity = videoCandidateCapacity(source.frameQueueCapacity, display, source.frameRate, lead);
        if (!candidateCapacity) return Result::failure(candidateCapacity.error());
        const auto candidates = candidateCapacity.value();
        plan.sources.push_back({source.groupKey, "source_video_" + std::to_string(i), candidates,
            i == audioSource || !hasAudio ? std::nullopt : std::optional<std::string>("source_audio_" + std::to_string(i)),
            std::string(source.videoIdentity),
            hasAudio ? std::optional<std::string>(std::string(*source.audioIdentity)) : std::nullopt,
            display ? MediaVideoFrameLineageProduct::SourceDisplayPoint : MediaVideoFrameLineageProduct::CanonicalInterval});
        Size lineage;
        const auto lineageSize = plan.sources.back().videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint
            ? sizeof(MediaVideoDecodeBuffer) + sizeof(MediaVideoDisplayLineage) + sizeof(MediaVideoDecodeLineage)
            : sizeof(MediaCanonicalVideoFrameBuffer) + sizeof(MediaCanonicalLineage);
        if (!lineage.add(lineageSize) ||
            !lineage.add(source.videoIdentity.size()) ||
            !retained.add(candidates, lineage.value) ||
            !fixed.add(candidates, sizeof(MediaBufferRef)) || !objects.add(candidates) ||
            !videoPending.add(source.videoIdentity.size()))
            return fail("Aggregate source video retention overflow");
    }
    const auto audioIdentityBytes = selected.audioIdentity->size();
    Size sourceFragment;
    if (!sourceFragment.add(sizeof(MediaAudioIntervalFragment) + sizeof(MediaCanonicalLineage)) ||
        !sourceFragment.add(audioIdentityBytes, 2) ||
        !retained.add(plan.maximumAudioCandidates, sizeof(MediaBoundCanonicalAudioBuffer) + sizeof(MediaCanonicalAudioSamplesBuffer)) ||
        !retained.add(samples.value, sourceFragment.value))
        return fail("Aggregate source audio metadata overflow");
    Size outputFragment, audioPending;
    // Each positive, contiguous fragment contains at least one sample. Count
    // shared lineage/identity per fragment conservatively, as the runtime guard.
    if (!outputFragment.add(sizeof(MediaAudioIntervalFragment) + sizeof(MediaCanonicalLineage)) ||
        !outputFragment.add(output.groupKey.value().size()) || !outputFragment.add(audioIdentityBytes) ||
        !audioPending.add(sizeof(MediaBoundCanonicalAudioBuffer) + sizeof(MediaCanonicalAudioSamplesBuffer)) ||
        !audioPending.add(plan.maximumAudioContributions, outputFragment.value) ||
        !retained.add(videoPending.value) || !retained.add(audioPending.value))
        return fail("Aggregate pending audio metadata overflow");
    Size total;
    if (!total.add(fixed.value) || !total.add(retained.value))
        return fail("Aggregate metadata total overflow");
    plan.storage = {fixed.value, retained.value, objects.value};
    plan.maximumMetadataBytes = total.value;
    if (auto status = MediaAvContinuousAggregatePlanValidator::validate(plan); !status)
        return Result::failure(status.error());
    return Result::success(std::move(plan));
}
} // namespace media::ffmpeg::graph
