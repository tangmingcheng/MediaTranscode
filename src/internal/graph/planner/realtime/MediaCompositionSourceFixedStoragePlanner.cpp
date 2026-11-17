#include "internal/graph/planner/realtime/MediaCompositionSourceFixedStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaCompositionSourcePlanningKernel.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaRtpInputIdentitySuffix.h"
#include "internal/graph/runtime/resource/MediaStandardContainerStorageReadback.h"
#include "internal/graph/nodes/audio/AudioResampleLineageState.h"
#include "internal/graph/nodes/audio/MediaAudioStartupTrimNode.h"
#include "internal/graph/sync/MediaAudioDriftControllerState.h"
#include <limits>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {
using Geometry = MediaStandardContainerStorageReadback;
class StorageSum final {
public:
    void add(::media::Result<std::size_t> value)
    {
        if (error) return;
        if (!value) { error = value.error(); return; }
        if (value.value() > (std::numeric_limits<std::size_t>::max)() - bytes) {
            error = ::media::ErrorInfo::allocationFailed("Fixed source metadata storage overflows"); return;
        }
        bytes += value.value();
    }
    void copy(std::string_view text) { add(Geometry::stringCopy(text.size())); }
    void input(const MediaRealtimeRtpTranscodeRequest& request, std::string_view url, std::string_view sdp,
        const MediaRealtimeRtpTransportPlan* transport, const MediaRtpDepacketizerConfig* depacketizer,
        const MediaPreparedRtpAccessUnitEnvelope* envelope)
    {
        for (const auto text : {url, sdp, std::string_view{request.input.rtspTransport}, std::string_view{request.mediaId}})
            add(Geometry::stringAssignEmpty(text.size()));
        if (transport) copy(transport->bindAddress);
        if (depacketizer) { copy(depacketizer->codecName); copy(depacketizer->fmtp); }
        if (envelope) { copy(envelope->codecName); copy(envelope->sizeAuthority); copy(envelope->completionAuthority); }
    }
    // Protocol declaration, startup, clock timing and assembly each retain
    // identity. Only the first uses concatenation; the other three copy it.
    void identity(std::string_view group, std::string_view suffix)
    {
        add(Geometry::stringConcatenation(group.size(), suffix.size()));
        if (error) return;
        const auto length = group.size() + suffix.size(); // concatenation checked above
        add(Geometry::stringCopy(length));
        add(Geometry::stringCopy(length));
        add(Geometry::stringCopy(length));
    }
    std::size_t bytes = 0;
    std::optional<::media::ErrorInfo> error;
};
}

::media::Result<std::shared_ptr<const MediaPreparationStorageLease>>
MediaCompositionSourceFixedStoragePlanner::reserve(const MediaCompositionSourceFixedRequest& request,
    const std::shared_ptr<MediaPreparationMemoryResource>& resource)
try {
    using Result = ::media::Result<std::shared_ptr<const MediaPreparationStorageLease>>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("Fixed storage requires its planning resource"));
    if (auto status = Geometry::validatePreparationMoves(); !status) return Result::failure(status.error());
    const auto& raw = request.raw;
    const bool audio = request.sourceRequest.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo;
    StorageSum sum;
    sum.input(request.sourceRequest, raw.videoUrl, raw.videoSdp,
        &raw.videoTransport, &raw.videoDepacketizer, &raw.videoAccessUnitEnvelope);
    if (raw.audio) {
        sum.input(request.sourceRequest, raw.audioUrl, raw.audioSdp,
            raw.audioTransport ? &*raw.audioTransport : nullptr,
            raw.audioDepacketizer ? &*raw.audioDepacketizer : nullptr,
            raw.audioAccessUnitEnvelope ? &*raw.audioAccessUnitEnvelope : nullptr);
    }
    if (audio && raw.audio) {
        // One persistent resampler lineage owner and correction owner per audio source.
        sum.add(::media::Result<std::size_t>::success(sizeof(AudioResampleLineageState)));
        sum.add(::media::Result<std::size_t>::success(sizeof(MediaAudioDriftControllerState)));
        sum.add(::media::Result<std::size_t>::success(sizeof(MediaAudioStartupTrimLineageState)));
        // Canonicalization transforms its input copy in place. All replacement
        // aliases in canonicalCodecName fit both audited inline string buffers.
        sum.copy(raw.audio->codecName);
        const auto& output = request.outputAudio;
        for (const auto* text : {&output.codecName(), &output.profile().canonicalName(),
                &output.channelLayout(), &output.sampleFormat(), &output.encoderName(), &output.preset()})
            sum.copy(*text);
        if (raw.audio->selectedDecoder) {
            const auto& decoder = *raw.audio->selectedDecoder;
            for (const auto* text : {&decoder.name, &decoder.outputSampleFormat, &decoder.outputChannelLayout}) sum.copy(*text);
            if (decoder.preparedInputRetention) {
                sum.copy(decoder.preparedInputRetention->authority); // audio pipeline
                sum.copy(decoder.preparedInputRetention->authority); // producer holder
            }
        }
    }
    sum.copy(request.sourceRequest.mediaId); // runtime request group
    sum.copy(request.sourceRequest.mediaId); // fixed product group
    sum.identity(request.sourceRequest.mediaId, MediaRtpVideoInputIdentitySuffix);
    if (audio) sum.identity(request.sourceRequest.mediaId, MediaRtpAudioInputIdentitySuffix);
    if (request.video.decoder.preparedInputRetention) sum.copy(request.video.decoder.preparedInputRetention->authority);
    sum.add(MediaAvGenerationTransitionPlanner::sourceContributionStorageBytes(
        MediaAvSyncSourceClockMode::RtpSenderReports, audio ? MediaBranchMode::TranscodeFrame : MediaBranchMode::Drop,
        request.video.filterActive, std::holds_alternative<MediaObservedVideoCadence>(request.timing)
            ? MediaVideoStartupReadiness::IntervalCoverage : MediaVideoStartupReadiness::DecodedPresentationSpan,
        request.selectedAudio));
    if (sum.error) return Result::failure(*sum.error);
    auto lease = resource->reserveStorage(sum.bytes, MediaStorageAdmission::RequiredAllocation);
    if (!lease) return Result::failure(lease.error());
    return Result::success(std::allocate_shared<MediaPreparationStorageLease>(
        std::pmr::polymorphic_allocator<MediaPreparationStorageLease>{resource.get()}, std::move(lease).value()));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<const MediaPreparationStorageLease>>::failure(
        ::media::ErrorInfo::allocationFailed("Fixed source metadata reservation failed"));
}
}
