#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"

#include <limits>
#include <utility>
#include <type_traits>

namespace media::ffmpeg::graph {
namespace {

bool validPacketDurationEvidence(
    const MediaTsPacketDurationEvidence& evidence,
    int streamIndex,
    int elementaryPid) noexcept
{
    return evidence.streamIndex == streamIndex &&
        evidence.elementaryPid == elementaryPid &&
        evidence.packetDuration > 0 && evidence.timeBase.num > 0 &&
        evidence.timeBase.den > 0;
}

template<bool Updating> ::media::Status compileAssembly(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlanView& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts,
    std::conditional_t<Updating, MediaRealtimeAvSyncAssemblyView, std::optional<MediaRealtimeAvSyncAssemblyPlan>>& result)
{
    const bool hasAudio = synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    if (request.audio.enabled != hasAudio || !synchronization.sourceClockMode ||
        !synchronization.startup.videoIdentity ||
        synchronization.startup.videoIdentity->empty() ||
        !synchronization.startup.videoCapacity ||
        *synchronization.startup.videoCapacity == 0 ||
        (hasAudio && (!synchronization.startup.audioIdentity ||
                      synchronization.startup.audioIdentity->empty() ||
                      !synchronization.startup.audioCapacity ||
                      *synchronization.startup.audioCapacity == 0)) ||
        (!hasAudio && (synchronization.startup.audioIdentity || synchronization.startup.audioCapacity)) ||
        !synchronization.startup.maximumWaitNs ||
        *synchronization.startup.maximumWaitNs <=
            MediaRunningTime::fromNanoseconds(0) ||
        facts.inputVideoIdentity != synchronization.startup.videoIdentity ||
        facts.inputAudioIdentity != synchronization.startup.audioIdentity) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V production assembly requires complete startup and source facts"));
    }

    MediaAvSyncInputClockPlan inputClock;
    const bool decoded = synchronization.startup.videoReadiness ==
        MediaVideoStartupReadiness::DecodedPresentationSpan;
    const auto* capacity = request.sourceCapacity;
    if (decoded != (capacity != nullptr) ||
        (capacity && (capacity->audioAcquiringCapacity.has_value() != hasAudio ||
            capacity->route.shape.members != synchronization.members ||
            capacity->route.shape.readiness != synchronization.startup.videoReadiness ||
            capacity->acquisitionWindow != synchronization.startup.maximumWaitNs ||
            capacity->startup.videoUnits != synchronization.startup.videoCapacity ||
            capacity->startup.videoBytes != synchronization.startup.videoByteCapacity ||
            capacity->startup.videoUnitBytes != synchronization.startup.maximumVideoUnitBytes ||
            capacity->startup.audioUnits != synchronization.startup.audioCapacity ||
            capacity->startup.audioBytes != synchronization.startup.audioByteCapacity ||
            capacity->startup.audioUnitBytes != synchronization.startup.maximumAudioUnitBytes ||
            !capacity->videoAcquiringCapacity ||
            (hasAudio && !*capacity->audioAcquiringCapacity))))
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Source clock acquiring capacity requires matching capacity facts"));
    const auto videoAcquiringCapacity = capacity
        ? capacity->videoAcquiringCapacity : *synchronization.startup.videoCapacity;
    const auto audioAcquiringCapacity = hasAudio
        ? std::optional<std::size_t>{capacity ? *capacity->audioAcquiringCapacity
                                            : *synchronization.startup.audioCapacity}
        : std::nullopt;
    if ((!decoded && synchronization.startup.videoReadiness !=
            MediaVideoStartupReadiness::IntervalCoverage) ||
        (decoded && (synchronization.domainRole != MediaAvSyncDomainRole::SourceContribution ||
                     *synchronization.sourceClockMode != MediaAvSyncSourceClockMode::RtpSenderReports)))
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("Decoded startup requires an RTP source contribution"));
    MediaVideoInputAssemblyTiming videoTiming = MediaDecodedVideoAssemblyTiming{};
    MediaCanonicalAudioDurationPlan audioDuration;
    std::uint64_t initialGeneration = MediaFirstLockedSourceGeneration;
    if (*synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::RtpSenderReports) {
        if (request.inputType != RealtimeInputType::RtpPort ||
            request.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
            !synchronization.rtpInput || !facts.inputVideoClockRate ||
            *facts.inputVideoClockRate <= 0 ||
            (hasAudio && (!facts.inputAudioSampleRate || *facts.inputAudioSampleRate <= 0 ||
                          !facts.inputAudioSamplesPerAccessUnit || *facts.inputAudioSamplesPerAccessUnit == 0)) ||
            (!hasAudio && (facts.inputAudioSampleRate || facts.inputAudioSamplesPerAccessUnit)) ||
            (request.videoCodecName != "h264" &&
             request.videoCodecName != "hevc")) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaRtpInputClockAssemblyPlan>(
            synchronization.rtpInput->input.commonEpochPolicy);
        if (!decoded) videoTiming = MediaCanonicalVideoAssemblyTiming{
            MediaRtpTimestampDeltaDurationPlan{*facts.inputVideoClockRate,
                MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta},
            MediaDecodeOrderMode::ReorderedRequiresDecodeTime};
        if (hasAudio) audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate, *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        if (!hasAudio || request.inputType != RealtimeInputType::MpegTsUdp ||
            request.inputLayout !=
                RealtimeInputStreamLayout::MuxedTransportStream ||
            !synchronization.mpegTsInput || !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0 ||
            !facts.inputVideoPacketDuration ||
            facts.inputVideoPacketDuration->packetDuration <= 0 ||
            facts.inputVideoPacketDuration->timeBase.num <= 0 ||
            facts.inputVideoPacketDuration->timeBase.den <= 0 ||
            !facts.inputAudioPacketDuration ||
            facts.inputAudioPacketDuration->packetDuration <= 0 ||
            facts.inputAudioPacketDuration->timeBase.num <= 0 ||
            facts.inputAudioPacketDuration->timeBase.den <= 0) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaMpegTsInputClockAssemblyPlan>();
        videoTiming = MediaCanonicalVideoAssemblyTiming{MediaPacketDurationPlan{true},
            MediaDecodeOrderMode::ReorderedRequiresDecodeTime};
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        if (!hasAudio || request.inputType != RealtimeInputType::Url ||
            request.inputLayout !=
                RealtimeInputStreamLayout::SessionDescribed ||
            !synchronization.demuxTimestampInput ||
            !synchronization.demuxTimestampInput->firstWindowMaximumSkewNs ||
            !synchronization.demuxTimestampInput->discontinuityThresholdNs ||
            !synchronization.demuxTimestampInput->initialGeneration ||
            !synchronization.demuxTimestampInput->canonicalTargetEpochNs ||
            !synchronization.demuxTimestampInput->preparedInput ||
            !synchronization.demuxTimestampInput->preparedEvidence ||
            !synchronization.demuxTimestampInput->videoTimeBase.isKnown() ||
            synchronization.demuxTimestampInput->videoTimeBase.num <= 0 ||
            synchronization.demuxTimestampInput->videoTimeBase.den <= 0 ||
            !synchronization.demuxTimestampInput->audioTimeBase.isKnown() ||
            synchronization.demuxTimestampInput->audioTimeBase.num <= 0 ||
            synchronization.demuxTimestampInput->audioTimeBase.den <= 0 ||
            !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp production assembly facts are incomplete"));
        }
        const auto& demux = *synchronization.demuxTimestampInput;
        initialGeneration = *demux.initialGeneration;
        if constexpr (Updating) {
            const auto* original = std::get_if<MediaDemuxTimestampInputClockAssemblyPlan>(&result.inputClock);
            if (!original || original->videoTimeBase != demux.videoTimeBase ||
                original->audioTimeBase != demux.audioTimeBase ||
                original->firstWindowMaximumSkew != *demux.firstWindowMaximumSkewNs ||
                original->discontinuityThreshold != *demux.discontinuityThresholdNs ||
                original->initialGeneration != initialGeneration ||
                original->videoSourceIdentity != *synchronization.startup.videoIdentity ||
                original->audioSourceIdentity != *synchronization.startup.audioIdentity ||
                original->canonicalTargetEpoch != *demux.canonicalTargetEpochNs ||
                original->preparedInput != *demux.preparedInput ||
                original->preparedEvidence != *demux.preparedEvidence)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Source assembly demux clock identity changed"));
        } else {
            inputClock.emplace<MediaDemuxTimestampInputClockAssemblyPlan>(
                MediaDemuxTimestampInputClockAssemblyPlan{
                    demux.videoTimeBase, demux.audioTimeBase, *demux.firstWindowMaximumSkewNs,
                    *demux.discontinuityThresholdNs, initialGeneration,
                    *synchronization.startup.videoIdentity, *synchronization.startup.audioIdentity,
                    *demux.canonicalTargetEpochNs, *demux.preparedInput, *demux.preparedEvidence});
        }
        videoTiming = MediaCanonicalVideoAssemblyTiming{MediaPacketDurationPlan{true},
            MediaDecodeOrderMode::ReorderedRequiresDecodeTime};
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "A/V production input clock mode is unsupported"));
    }

    if constexpr (Updating) {
        if (result.audio.has_value() != hasAudio ||
            result.video.sourceIdentity != *synchronization.startup.videoIdentity ||
            result.video.timing != videoTiming ||
            result.generationPolicy != MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange ||
            result.initialGeneration != initialGeneration ||
            result.evidencePolicy != MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire ||
            (*synchronization.sourceClockMode != MediaAvSyncSourceClockMode::DemuxTimestamps && result.inputClock != inputClock) ||
            (hasAudio && (result.audio->sourceIdentity != *synchronization.startup.audioIdentity ||
                result.audio->duration != audioDuration ||
                result.audio->decodeOrder != MediaDecodeOrderMode::PresentationOrderNoReorder)))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Source assembly fixed identity changed"));
        result.video.acquiringCapacity = videoAcquiringCapacity;
        result.video.acquiringTimeout = *synchronization.startup.maximumWaitNs;
        if (hasAudio) {
            result.audio->acquiringCapacity = *audioAcquiringCapacity;
            result.audio->acquiringTimeout = *synchronization.startup.maximumWaitNs;
        }
    } else {
        std::optional<MediaCanonicalAudioAssemblyPlan> audioAssembly;
        if (hasAudio) audioAssembly.emplace(MediaCanonicalAudioAssemblyPlan{
            *synchronization.startup.audioIdentity, std::move(audioDuration),
            MediaDecodeOrderMode::PresentationOrderNoReorder,
            *audioAcquiringCapacity, *synchronization.startup.maximumWaitNs});
        result.emplace(MediaRealtimeAvSyncAssemblyPlan{std::move(inputClock), MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange,
            initialGeneration, MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire,
            {*synchronization.startup.videoIdentity, std::move(videoTiming), videoAcquiringCapacity,
                *synchronization.startup.maximumWaitNs}, std::move(audioAssembly)});
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaRealtimeAvSourceClockPlan> MediaRealtimeAvSourceClockPlanner::plan(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlanView& synchronization)
{
    if (auto status = MediaAvSyncPlanValidator::validateSourceClock(synchronization); !status)
        return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(status.error());
    const auto& audio = request.audio;
    const auto* isolatedAudioInput = request.isolatedAudioInput;
    const bool hasAudio = synchronization.members == MediaTranscodeStreamSet::AudioVideo;
    const bool audioCopy = audio.branchMode == MediaBranchMode::CopyPacket;
    if (audio.enabled != hasAudio ||
        (!hasAudio && (audio.branchMode != MediaBranchMode::Drop || audio.resolvedOutput ||
                       audio.selectedDecoder || audio.selectedResampler || isolatedAudioInput)) ||
        (hasAudio && (!audio.resolvedOutput ||
        (audioCopy && (!audio.maximumAccessUnitSamples || *audio.maximumAccessUnitSamples <= 0 ||
                        audio.selectedDecoder || audio.selectedResampler)) ||
        (!audioCopy && (audio.branchMode != MediaBranchMode::TranscodeFrame || !audio.selectedDecoder)))))
        return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
            ::media::ErrorInfo::notInitialized("Source clock requires resolved audio source timing"));
    MediaRealtimeAvSourceTimingFacts facts;
    facts.inputVideoIdentity = synchronization.startup.videoIdentity;
    facts.inputAudioIdentity = synchronization.startup.audioIdentity;
    if (!synchronization.sourceClockMode) {
        return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "synchronized input clock mode is missing"));
    }
    if (*synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::RtpSenderReports) {
        if (!synchronization.rtpInput ||
            !synchronization.rtpInput->videoInput.clockRate ||
            (hasAudio && (!synchronization.rtpInput->audioInput.clockRate ||
             !isolatedAudioInput || !isolatedAudioInput->rtpDepacketizer ||
             isolatedAudioInput->rtpDepacketizer->accessUnitDurationRtpTicks <= 0))) {
            return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "RTP input clock does not publish complete duration facts"));
        }
        const auto boundedReceive = [&](const MediaRealtimeRtpInputNodePlan& input) {
            return synchronization.recovery.reacquisitionTimeoutNs &&
                input.rtpTransport && input.rtpTransport->cancellableReadTimeoutMs > 0 &&
                static_cast<std::int64_t>(input.rtpTransport->cancellableReadTimeoutMs) *
                    1'000'000 < synchronization.recovery.reacquisitionTimeoutNs->nanoseconds();
        };
        if (!boundedReceive(request.input) ||
            (hasAudio && !boundedReceive(*isolatedAudioInput))) {
            return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP receive wait must be shorter than source purge acknowledgement timeout"));
        }
        facts.inputVideoClockRate =
            *synchronization.rtpInput->videoInput.clockRate;
        if (hasAudio) {
            facts.inputAudioSampleRate = *synchronization.rtpInput->audioInput.clockRate;
            facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
                isolatedAudioInput->rtpDepacketizer->accessUnitDurationRtpTicks);
        }
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        const auto* selectedProgram = request.input.mpegTs
            ? std::get_if<MediaTsAudioVideoSelectedProgramPlan>(
                  &request.input.mpegTs->selectedProgram)
            : nullptr;
        const int inputSampleRate = audioCopy
            ? audio.resolvedOutput->sampleRate()
            : audio.selectedDecoder
                ? audio.selectedDecoder->inputSampleRate
                : 0;
        const std::int64_t inputAccessUnitSamples = audioCopy
            ? *audio.maximumAccessUnitSamples
            : audio.selectedDecoder
                ? audio.selectedDecoder->maximumOutputBlockInputSamples
                : 0;
        if (inputSampleRate <= 0 || inputAccessUnitSamples <= 0 ||
            inputAccessUnitSamples >
                std::numeric_limits<std::uint32_t>::max() ||
            !synchronization.mpegTsInput ||
            !synchronization.mpegTsInput->videoPid ||
            !synchronization.mpegTsInput->audioPid ||
            !selectedProgram ||
            !validPacketDurationEvidence(
                selectedProgram->videoPacketDuration,
                request.videoStreamIndex,
                *synchronization.mpegTsInput->videoPid) ||
            !validPacketDurationEvidence(
                selectedProgram->audioPacketDuration,
                audio.sourceStreamIndex,
                *synchronization.mpegTsInput->audioPid)) {
            return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS input clock does not publish complete duration facts"));
        }
        facts.inputAudioSampleRate = inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            inputAccessUnitSamples);
        facts.inputVideoPacketDuration =
            selectedProgram->videoPacketDuration;
        facts.inputAudioPacketDuration =
            selectedProgram->audioPacketDuration;
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        const int inputSampleRate = audioCopy
            ? audio.resolvedOutput->sampleRate()
            : audio.selectedDecoder
                ? audio.selectedDecoder->inputSampleRate
                : 0;
        const std::int64_t inputAccessUnitSamples = audioCopy
            ? *audio.maximumAccessUnitSamples
            : audio.selectedDecoder
                ? audio.selectedDecoder->maximumOutputBlockInputSamples
                : 0;
        if (!synchronization.demuxTimestampInput ||
            inputSampleRate <= 0 || inputAccessUnitSamples <= 0 ||
            inputAccessUnitSamples >
                std::numeric_limits<std::uint32_t>::max()) {
            return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp input does not publish complete duration facts"));
        }
        facts.inputAudioSampleRate = inputSampleRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            inputAccessUnitSamples);
    } else {
        return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "synchronized input clock mode is unsupported"));
    }

    std::optional<MediaRealtimeAvSyncAssemblyPlan> assembly;
    if (auto status = compileAssembly<false>(request, synchronization, facts, assembly); !status)
        return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(status.error());
    return ::media::Result<MediaRealtimeAvSourceClockPlan>::success({std::move(facts), std::move(*assembly)});
}

::media::Status MediaRealtimeAvSourceClockPlanner::rebindAssembly(
    const MediaRealtimeAvSourceClockRequest& request, const MediaAvSyncPlanView& synchronization,
    const MediaRealtimeAvSourceTimingFacts& timing, MediaRealtimeAvSyncAssemblyView& assembly)
{
    if (auto status = MediaAvSyncPlanValidator::validateSourceClock(synchronization); !status) return status;
    return compileAssembly<true>(request, synchronization, timing, assembly);
}

} // namespace media::ffmpeg::graph
