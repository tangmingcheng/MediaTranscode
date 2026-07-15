#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanner.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFactsResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::int64_t> checkedAdd(std::int64_t left,
                                        std::int64_t right,
                                        const char* owner)
{
    if (left < 0 || right < 0 ||
        left > std::numeric_limits<std::int64_t>::max() - right) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(owner) + " sample bound overflow"));
    }
    return ::media::Result<std::int64_t>::success(left + right);
}

::media::Result<std::int64_t> runningTimeToSamples(MediaRunningTime time,
                                                   int sampleRate)
{
    constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
    if (time <= MediaRunningTime::fromNanoseconds(0) || sampleRate <= 0 ||
        time.nanoseconds() >
            (std::numeric_limits<std::int64_t>::max() -
             (NanosecondsPerSecond - 1)) / sampleRate) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio command lead cannot be converted to output samples"));
    }
    return ::media::Result<std::int64_t>::success(
        (time.nanoseconds() * sampleRate + NanosecondsPerSecond - 1) /
        NanosecondsPerSecond);
}

::media::Result<MediaRunningTime> samplesToRunningTime(std::int64_t samples,
                                                       int sampleRate)
{
    constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
    if (samples <= 0 || sampleRate <= 0 ||
        samples > (std::numeric_limits<std::int64_t>::max() - sampleRate + 1) /
                      NanosecondsPerSecond) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio sample bound cannot be converted to running time"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            (samples * NanosecondsPerSecond + sampleRate - 1) / sampleRate));
}

::media::Result<MediaScheduledRtpOutputPlan> scheduledRtpOutput(
    MediaScheduledStream stream,
    MediaRealtimeRtpOutputNodePlan& legacyOutput,
    const MediaAvSyncRtpOutputStreamPlan& synchronization,
    const std::string& codecName,
    std::optional<int> maximumAccessUnitSamples,
    MediaRunningTime senderLead,
    MediaRunningTime senderReportInterval)
{
    if (!synchronization.payloadType || !synchronization.ssrc ||
        !synchronization.baseTimestamp || !synchronization.clockRate ||
        !synchronization.cname || synchronization.cname->empty() ||
        legacyOutput.packetSize <= 0 || !legacyOutput.scheduledTransport) {
        return ::media::Result<MediaScheduledRtpOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "scheduled RTP output requires complete protocol planning facts"));
    }
    const auto streamKind = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    auto packetization = MediaScheduledRtpPacketizationPlan::create(
        streamKind, codecName, 1, *synchronization.clockRate,
        *synchronization.payloadType,
        static_cast<std::size_t>(legacyOutput.packetSize),
        maximumAccessUnitSamples);
    if (!packetization) {
        return ::media::Result<MediaScheduledRtpOutputPlan>::failure(
            packetization.error());
    }
    return ::media::Result<MediaScheduledRtpOutputPlan>::success(
        MediaScheduledRtpOutputPlan{
            stream,
            std::move(*legacyOutput.scheduledTransport),
            std::move(packetization).value(),
            *synchronization.ssrc,
            *synchronization.baseTimestamp,
            *synchronization.clockRate,
            *synchronization.cname,
            senderLead,
            senderReportInterval});
}

::media::Result<MediaAudioCorrectionReachabilityPlan> reachabilityPlan(
    MediaAvSyncPlan& synchronization,
    const MediaRealtimeAvSyncPlanningFacts& facts)
{
    if (!facts.outputSampleRate || !facts.decoderDelaySamples ||
        !facts.encoderLookaheadSamples || !facts.decodeQueueSamples ||
        !facts.resampleQueueSamples || !facts.encodeQueueSamples ||
        !facts.schedulerQueueSamples || !facts.protocolBatchSamples ||
        !facts.mailboxDeliveryMarginSamples ||
        !facts.maximumResamplerOutputBlockSamples || !facts.mailboxCapacity) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V synchronization planning facts are incomplete"));
    }
    std::int64_t worst = 0;
    for (const auto value : {
             *facts.decoderDelaySamples,
             *facts.encoderLookaheadSamples,
             *facts.decodeQueueSamples,
             *facts.resampleQueueSamples,
             *facts.encodeQueueSamples,
             *facts.schedulerQueueSamples,
             *facts.protocolBatchSamples}) {
        auto sum = checkedAdd(worst, value, "audio in-flight");
        if (!sum) {
            return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
                sum.error());
        }
        worst = sum.value();
    }
    auto required = checkedAdd(
        worst, *facts.mailboxDeliveryMarginSamples, "audio command lead");
    if (!required) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            required.error());
    }
    required = checkedAdd(required.value(),
                          *facts.maximumResamplerOutputBlockSamples,
                          "audio command lead");
    if (!required || required.value() == std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            required ? ::media::ErrorInfo::invalidArgument(
                           "audio command lead has no strict representable margin")
                     : required.error());
    }
    if (!synchronization.audioServo.maximumMeasurementGapNs ||
        !synchronization.audioServo.recoveryCorrectionLimitPpm) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "audio measurement gap fact is missing"));
    }
    auto measurementGapSamples = runningTimeToSamples(
        *synchronization.audioServo.maximumMeasurementGapNs,
        *facts.outputSampleRate);
    if (!measurementGapSamples ||
        measurementGapSamples.value() ==
            std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            measurementGapSamples
                ? ::media::ErrorInfo::invalidArgument(
                      "audio measurement gap has no strict representable margin")
                : measurementGapSamples.error());
    }
    const auto correctionPpm = static_cast<std::int64_t>(
        *synchronization.audioServo.recoveryCorrectionLimitPpm);
    if (correctionPpm <= 0 || measurementGapSamples.value() >
        (std::numeric_limits<std::int64_t>::max() - 999'999) /
            correctionPpm) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio measurement correction headroom overflow"));
    }
    const std::int64_t correctionHeadroomSamples =
        (measurementGapSamples.value() * correctionPpm + 999'999) / 1'000'000;
    auto measurementLead = checkedAdd(
        measurementGapSamples.value(), correctionHeadroomSamples,
        "audio measurement command lead");
    if (!measurementLead ||
        measurementLead.value() == std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            measurementLead
                ? ::media::ErrorInfo::invalidArgument(
                      "audio measurement command lead has no strict margin")
                : measurementLead.error());
    }
    const std::int64_t commandLeadSamples = std::max(
        required.value() + 1, measurementLead.value() + 1);
    auto commandLead = samplesToRunningTime(commandLeadSamples, *facts.outputSampleRate);
    auto compensationSamples = checkedAdd(
        commandLeadSamples,
        *facts.maximumResamplerOutputBlockSamples,
        "audio compensation window");
    auto compensation = compensationSamples
        ? samplesToRunningTime(compensationSamples.value(),
                               *facts.outputSampleRate)
        : ::media::Result<MediaRunningTime>::failure(
              compensationSamples.error());
    auto frequency = compensation
        ? checkedAdd(compensation.value().nanoseconds(), 1'000'000'000,
                     "audio frequency filter")
        : ::media::Result<std::int64_t>::failure(compensation.error());
    if (!commandLead || !compensation || !frequency ||
        frequency.value() > 60'000'000'000) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "bounded audio queues exceed the synchronization policy duration"));
    }
    synchronization.audioServo.commandLeadNs = commandLead.value();
    synchronization.audioServo.compensationWindowNs = compensation.value();
    synchronization.audioServo.frequencyFilterTimeConstantNs =
        MediaRunningTime::fromNanoseconds(frequency.value());
    if (auto status = MediaAvSyncPlanValidator::validate(synchronization); !status) {
        return ::media::Result<MediaAudioCorrectionReachabilityPlan>::failure(
            status.error());
    }
    return ::media::Result<MediaAudioCorrectionReachabilityPlan>::success(
        MediaAudioCorrectionReachabilityPlan{
            *facts.outputSampleRate,
            0,
            worst,
            *facts.mailboxDeliveryMarginSamples,
            *facts.maximumResamplerOutputBlockSamples,
            commandLeadSamples,
            *facts.mailboxCapacity});
}

} // namespace

::media::Result<MediaRealtimeAvSyncRuntimePlan>
MediaRealtimeAvSyncRuntimePlanner::plan(
    MediaRealtimeRtpTranscodePlan& outer,
    MediaAvSyncPlan synchronization)
{
    auto facts = MediaRealtimeAvSyncPlanningFactsResolver::resolve(
        outer, synchronization);
    if (!facts) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            facts.error());
    }
    auto correction = reachabilityPlan(synchronization, facts.value());
    if (!correction || !facts.value().acknowledgementTimeout ||
        !facts.value().terminalDrainWindow || !synchronization.topology) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            correction ? ::media::ErrorInfo::notInitialized(
                             "A/V generation transition timing facts are incomplete")
                       : correction.error());
    }

    MediaAvSyncOutputAdapterKind adapter;
    std::optional<std::variant<MediaSeparateRtpOutputRuntimePlan,
                               MediaProjectMpegTsRuntimeOutputPlan>> protocolOutput;
    if (*synchronization.topology ==
        MediaAvSyncTopology::SeparateRtpToSeparateRtp) {
        if (!synchronization.rtp || synchronization.ts ||
            !synchronization.startup.outputLeadNs ||
            !synchronization.rtp->output.senderReportIntervalNs ||
            outer.sdp.path.empty() || !outer.audioPlan.resolvedOutput) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "separate RTP synchronization output facts are incomplete"));
        }
        auto video = scheduledRtpOutput(
            MediaScheduledStream::Video,
            outer.videoOutput,
            synchronization.rtp->videoOutput,
            outer.videoPlan.outputCodecName,
            std::nullopt,
            *synchronization.startup.outputLeadNs,
            *synchronization.rtp->output.senderReportIntervalNs);
        auto audio = scheduledRtpOutput(
            MediaScheduledStream::Audio,
            outer.audioOutput,
            synchronization.rtp->audioOutput,
            outer.audioPlan.resolvedOutput->codecName(),
            outer.audioPlan.resolvedOutput->codecFrameSamples(),
            *synchronization.startup.outputLeadNs,
            *synchronization.rtp->output.senderReportIntervalNs);
        if (!video || !audio) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                video ? audio.error() : video.error());
        }
        adapter = MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp;
        protocolOutput.emplace(std::in_place_type<MediaSeparateRtpOutputRuntimePlan>,
            MediaSeparateRtpOutputRuntimePlan{
                std::move(video).value(),
                std::move(audio).value(),
                outer.sdp.path});
    } else if (*synchronization.topology ==
               MediaAvSyncTopology::MpegTsToMpegTs) {
        if (synchronization.rtp || !synchronization.ts ||
            !synchronization.ts->outputMux ||
            !facts.value().outputSampleRate || outer.muxedOutput.url.empty()) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS synchronization output facts are incomplete"));
        }
        auto accepted = MediaProjectMpegTsOutputPlan::accept(
            *facts.value().outputSampleRate,
            *synchronization.ts->outputMux);
        if (!accepted) {
            return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
                accepted.error());
        }
        adapter = MediaAvSyncOutputAdapterKind::ProjectMpegTs;
        protocolOutput.emplace(std::in_place_type<MediaProjectMpegTsRuntimeOutputPlan>,
            MediaProjectMpegTsRuntimeOutputPlan{
                outer.muxedOutput.url,
                MediaOutputResourceKind::ByteSink,
                MediaMuxSessionKind::ProjectMpegTs,
                std::move(accepted).value()});
    } else {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::unsupported(
                "A/V runtime plan topology is unsupported"));
    }

    auto transition = MediaAvGenerationTransitionPlanner::plan(
        adapter,
        *facts.value().acknowledgementTimeout,
        *facts.value().terminalDrainWindow);
    return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::success(
        MediaRealtimeAvSyncRuntimePlan{
            MediaAvSyncGroupKey("realtime.av"),
            std::move(synchronization),
            adapter,
            std::move(*protocolOutput),
            outer.queues,
            outer.edgePolicies,
            outer.threadingPolicy,
            std::move(transition),
            std::move(correction).value()});
}

} // namespace media::ffmpeg::graph
