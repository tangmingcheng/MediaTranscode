#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h"

#include <limits>
#include <utility>

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

::media::Result<MediaRealtimeAvSyncAssemblyPlan> planAssembly(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSourceTimingFacts& facts)
{
    if (!request.audio.enabled || !synchronization.sourceClockMode ||
        !synchronization.startup.videoIdentity ||
        synchronization.startup.videoIdentity->empty() ||
        !synchronization.startup.audioIdentity ||
        synchronization.startup.audioIdentity->empty() ||
        !synchronization.startup.videoCapacity ||
        *synchronization.startup.videoCapacity == 0 ||
        !synchronization.startup.audioCapacity ||
        *synchronization.startup.audioCapacity == 0 ||
        !synchronization.startup.maximumWaitNs ||
        *synchronization.startup.maximumWaitNs <=
            MediaRunningTime::fromNanoseconds(0) ||
        facts.inputVideoIdentity != synchronization.startup.videoIdentity ||
        facts.inputAudioIdentity != synchronization.startup.audioIdentity) {
        return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V production assembly requires complete startup and source facts"));
    }

    MediaAvSyncInputClockPlan inputClock;
    MediaCanonicalVideoDurationPlan videoDuration;
    MediaCanonicalAudioDurationPlan audioDuration;
    std::uint64_t initialGeneration = MediaFirstLockedSourceGeneration;
    if (*synchronization.sourceClockMode ==
        MediaAvSyncSourceClockMode::RtpSenderReports) {
        if (request.inputType != RealtimeInputType::RtpPort ||
            request.inputLayout != RealtimeInputStreamLayout::SeparateStreams ||
            !synchronization.rtpInput || !facts.inputVideoClockRate ||
            *facts.inputVideoClockRate <= 0 || !facts.inputAudioSampleRate ||
            *facts.inputAudioSampleRate <= 0 ||
            !facts.inputAudioSamplesPerAccessUnit ||
            *facts.inputAudioSamplesPerAccessUnit == 0 ||
            (request.videoCodecName != "h264" &&
             request.videoCodecName != "hevc")) {
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaRtpInputClockAssemblyPlan>(
            synchronization.rtpInput->input.commonEpochPolicy);
        videoDuration.emplace<MediaRtpTimestampDeltaDurationPlan>(
            *facts.inputVideoClockRate,
            MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::MpegTsPcr) {
        if (request.inputType != RealtimeInputType::MpegTsUdp ||
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
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS production assembly facts are incomplete"));
        }
        inputClock.emplace<MediaMpegTsInputClockAssemblyPlan>();
        videoDuration.emplace<MediaPacketDurationPlan>(true);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else if (*synchronization.sourceClockMode ==
               MediaAvSyncSourceClockMode::DemuxTimestamps) {
        if (request.inputType != RealtimeInputType::Url ||
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
            return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "demux timestamp production assembly facts are incomplete"));
        }
        const auto& demux = *synchronization.demuxTimestampInput;
        initialGeneration = *demux.initialGeneration;
        inputClock.emplace<MediaDemuxTimestampInputClockAssemblyPlan>(
            MediaDemuxTimestampInputClockAssemblyPlan{
                demux.videoTimeBase,
                demux.audioTimeBase,
                *demux.firstWindowMaximumSkewNs,
                *demux.discontinuityThresholdNs,
                initialGeneration,
                *synchronization.startup.videoIdentity,
                *synchronization.startup.audioIdentity,
                *demux.canonicalTargetEpochNs,
                *demux.preparedInput,
                *demux.preparedEvidence});
        videoDuration.emplace<MediaPacketDurationPlan>(true);
        audioDuration.emplace<MediaPlannedAudioSamplesDurationPlan>(
            *facts.inputAudioSampleRate,
            *facts.inputAudioSamplesPerAccessUnit);
    } else {
        return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "A/V production input clock mode is unsupported"));
    }

    return ::media::Result<MediaRealtimeAvSyncAssemblyPlan>::success(
        MediaRealtimeAvSyncAssemblyPlan{
            std::move(inputClock),
            MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange,
            initialGeneration,
            MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire,
            MediaCanonicalVideoAssemblyPlan{
                *synchronization.startup.videoIdentity,
                std::move(videoDuration),
                MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
                *synchronization.startup.videoCapacity,
                *synchronization.startup.maximumWaitNs},
            MediaCanonicalAudioAssemblyPlan{
                *synchronization.startup.audioIdentity,
                std::move(audioDuration),
                MediaDecodeOrderMode::PresentationOrderNoReorder,
                *synchronization.startup.audioCapacity,
                *synchronization.startup.maximumWaitNs}});
}

} // namespace

::media::Result<MediaRealtimeAvSourceClockPlan> MediaRealtimeAvSourceClockPlanner::plan(
    const MediaRealtimeAvSourceClockRequest& request,
    const MediaAvSyncPlan& synchronization)
{
    if (auto status = MediaAvSyncPlanValidator::validateSourceClock(synchronization); !status)
        return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(status.error());
    const auto& audio = request.audio;
    const auto* isolatedAudioInput = request.isolatedAudioInput;
    const bool audioCopy = audio.branchMode == MediaBranchMode::CopyPacket;
    if (!audio.enabled || !audio.resolvedOutput ||
        (audioCopy && (!audio.maximumAccessUnitSamples || *audio.maximumAccessUnitSamples <= 0 ||
                        audio.selectedDecoder || audio.selectedResampler)) ||
        (!audioCopy && (audio.branchMode != MediaBranchMode::TranscodeFrame || !audio.selectedDecoder)))
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
            !synchronization.rtpInput->audioInput.clockRate ||
            !isolatedAudioInput || !isolatedAudioInput->rtpDepacketizer ||
            isolatedAudioInput->rtpDepacketizer->accessUnitDurationRtpTicks <= 0) {
            return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "RTP input clock does not publish complete duration facts"));
        }
        facts.inputVideoClockRate =
            *synchronization.rtpInput->videoInput.clockRate;
        facts.inputAudioSampleRate =
            *synchronization.rtpInput->audioInput.clockRate;
        facts.inputAudioSamplesPerAccessUnit = static_cast<std::uint32_t>(
            isolatedAudioInput->rtpDepacketizer->accessUnitDurationRtpTicks);
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

    auto assembly = planAssembly(request, synchronization, facts);
    if (!assembly) return ::media::Result<MediaRealtimeAvSourceClockPlan>::failure(assembly.error());
    return ::media::Result<MediaRealtimeAvSourceClockPlan>::success(
        {std::move(facts), std::move(assembly).value()});
}

} // namespace media::ffmpeg::graph
