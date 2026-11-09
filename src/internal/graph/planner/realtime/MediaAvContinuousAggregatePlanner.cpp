#include "internal/graph/nodes/sync/MediaContinuousAudioAggregateNode.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanValidator.h"
#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"
#include "internal/graph/runtime/storage/MediaAvAggregateStorage.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"

#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include <array>
#include <charconv>
#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
struct PortName final {
    static constexpr std::string_view video = "source_video_", audio = "source_audio_";
    std::array<char, video.size() + std::numeric_limits<std::size_t>::digits10 + 1> text{};
    std::size_t length;
    PortName(std::string_view prefix, std::size_t index) {
        std::copy(prefix.begin(), prefix.end(), text.begin());
        const auto result = std::to_chars(text.data() + prefix.size(), text.data() + text.size(), index);
        length = static_cast<std::size_t>(result.ptr - text.data());
    }
    std::string owned() const { return std::string(text.data(), length); }
};
class Size final {
public:
    bool add(std::uint64_t count, std::uint64_t bytes = 1) noexcept
    {
        if (bytes && count > (std::numeric_limits<std::uint64_t>::max() - value) / bytes) return false;
        value += count * bytes; return true;
    }
    std::uint64_t value = 0;
};
// Only the first pass owns identity/container construction. Capacity refresh
// runs on the same immutable source declarations and retains every backing.
// Any failure invalidates the workspace; callers must not publish it.
template<bool Updating> ::media::Status compileAggregate(
    std::span<const MediaAvAggregateSourceFacts> sources, MediaAvContinuousAggregateTopology& plan)
{
    using Result = ::media::Status;
    const auto fail = [](const char* message) { return Result::failure(::media::ErrorInfo::invalidArgument(message)); };
    if (sources.empty() || plan.audioSource >= sources.size())
        return fail("Aggregate capacity requires its prepared source catalog");
    if constexpr (Updating) {
        if (sources.size() != plan.sources.size())
            return fail("Aggregate capacity refresh changed its source catalog");
    }
    const auto& selected = sources[plan.audioSource];
    if (!selected.audioIdentity || !selected.audioCorrection ||
        selected.audioCorrection->maximumResamplerOutputBlockSamples <= 0 ||
        selected.audioCorrection->epochOutputSampleIndex != plan.initialAudioSample ||
        selected.frameQueueCapacity == 0)
        return fail("Aggregate selected audio requires prepared correction and queue facts");
    for (const auto& source : sources) {
        if (source.activationOutputLead.nanoseconds() <= 0 || source.activationOutputLead > plan.preparationLead)
            return fail("Aggregate capacity refresh differs from its prepared lead");
    }
    plan.maximumAudioCandidates = selected.frameQueueCapacity;
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
        !videoPending.add(plan.outputGroupKey.value().size()) ||
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
        auto candidateCapacity = MediaAvContinuousAggregatePlanner::videoCandidateCapacity(source.frameQueueCapacity, display, source.frameRate, plan.preparationLead);
        if (!candidateCapacity) return Result::failure(candidateCapacity.error());
        const auto candidates = candidateCapacity.value();
        const auto product = display ? MediaVideoFrameLineageProduct::SourceDisplayPoint
                                     : MediaVideoFrameLineageProduct::CanonicalInterval;
        if constexpr (Updating) {
            auto& owned = plan.sources[i];
            if (owned.groupKey != source.groupKey || owned.videoIdentity != source.videoIdentity ||
                owned.audioIdentity.has_value() != hasAudio ||
                (hasAudio && *owned.audioIdentity != *source.audioIdentity) || owned.videoProduct != product)
                return fail("Aggregate capacity refresh changed its source identity or lineage product");
            owned.maximumVideoCandidates = candidates;
        } else {
            plan.sources.push_back({source.groupKey, PortName(PortName::video, i).owned(), candidates,
                i == plan.audioSource || !hasAudio ? std::nullopt : std::optional<std::string>(PortName(PortName::audio, i).owned()),
                std::string(source.videoIdentity),
                hasAudio ? std::optional<std::string>(std::string(*source.audioIdentity)) : std::nullopt,
                display ? MediaVideoFrameLineageProduct::SourceDisplayPoint : MediaVideoFrameLineageProduct::CanonicalInterval});
        }
        Size lineage;
        const auto lineageSize = plan.sources[i].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint
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
        !outputFragment.add(plan.outputGroupKey.value().size()) || !outputFragment.add(audioIdentityBytes) ||
        !audioPending.add(sizeof(MediaBoundCanonicalAudioBuffer) + sizeof(MediaCanonicalAudioSamplesBuffer)) ||
        !audioPending.add(plan.maximumAudioContributions, outputFragment.value) ||
        !retained.add(videoPending.value) || !retained.add(audioPending.value))
        return fail("Aggregate pending audio metadata overflow");
    Size total;
    if (!total.add(fixed.value) || !total.add(retained.value))
        return fail("Aggregate metadata total overflow");
    plan.storage = {fixed.value, retained.value, objects.value};
    plan.maximumMetadataBytes = total.value;
    return MediaAvContinuousAggregatePlanValidator::validate(plan);
}
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
    const MediaRealtimeAvOutputRuntimePlan& output, const MediaVideoCanvasGeometry& canvas,
    MediaRational outputFrameRate, std::size_t audioSource, MediaPreparationMemoryResource& resource)
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
    using Geometry = MediaStandardContainerStorageReadback;
    if (auto status = Geometry::validatePreparationMoves(); !status) return Result::failure(status.error());
    Size backing;
    std::optional<::media::ErrorInfo> storageError;
    const auto add = [&](::media::Result<std::size_t> value) {
        if (storageError) return;
        if (!value) { storageError = value.error(); return; }
        if (!backing.add(value.value())) storageError = ::media::ErrorInfo::allocationFailed("Aggregate preparation storage overflow");
    };
    const auto copy = [&](std::string_view text) { add(Geometry::stringCopy(text.size())); };
    copy(output.groupKey.value());
    const auto& audio = output.audioOutput;
    for (const auto* text : {&audio.codecName(), &audio.profile().canonicalName(), &audio.channelLayout(),
            &audio.sampleFormat(), &audio.encoderName(), &audio.preset()}) copy(*text);
    add(Geometry::vectorReserveEmpty<MediaVideoCanvasRectangle>(canvas.tiles.size()));
    add(Geometry::vectorReserveEmpty<MediaAvAggregateSourcePlan>(sources.size()));
    add(Geometry::stringCopy(PortName(PortName::audio, audioSource).length));
    for (std::size_t i = 0; i < sources.size(); ++i) {
        copy(sources[i].groupKey.value()); copy(sources[i].videoIdentity);
        add(Geometry::stringCopy(PortName(PortName::video, i).length));
        if (sources[i].audioIdentity) {
            copy(*sources[i].audioIdentity);
            if (i != audioSource) add(Geometry::stringCopy(PortName(PortName::audio, i).length));
        }
    }
    if (storageError) return Result::failure(*storageError);
    if (backing.value > (std::numeric_limits<std::size_t>::max)()) return fail("Aggregate preparation exceeds address space");
    auto lease = resource.reserveStorage(static_cast<std::size_t>(backing.value), MediaStorageAdmission::RequiredAllocation);
    if (!lease) return Result::failure(lease.error());
    MediaAvContinuousAggregateTopology plan{
        MediaAvAggregatePreparationOwner{std::move(lease).value()},
        output.groupKey, {}, audioSource, PortName(PortName::audio, audioSource).owned(),
        canvas, outputFrameRate, output.audioOutput, lead,
        selected.initialGeneration, selected.audioCorrection->epochOutputSampleIndex,
        selected.frameQueueCapacity, 0, static_cast<std::size_t>(output.audioOutput.codecFrameSamples()), 0, {0, 0, 0}};
    plan.sources.reserve(sources.size());
    if (auto status = compileAggregate<false>(sources, plan); !status)
        return Result::failure(status.error());
    return Result::success(std::move(plan));
}
::media::Status MediaAvContinuousAggregatePlanner::rebindCapacity(
    std::span<const MediaAvAggregateSourceFacts> sources, MediaAvContinuousAggregateTopology& plan)
{
    if (auto status = MediaAvContinuousAggregatePlanValidator::validate(plan); !status) return status;
    return compileAggregate<true>(sources, plan);
}
} // namespace media::ffmpeg::graph
