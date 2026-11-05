#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceStoragePlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include <limits>
#include <algorithm>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaPreparedRtpSourceSessionStreamPlan> streamPlan(
    const MediaRtpDepacketizerConfig& depacketizer,
    const MediaPreparedRtpAccessUnitEnvelope& envelope,
    MediaRealtimeRtpTransportPlan transport,
    const MediaPreparedRtpIngressCandidate& candidate,
    const MediaPreparedRtpAdmissionStream& retention,
    const MediaRtpVideoTimingPlan& videoTiming,
    const MediaAvSyncRtpInputPlan& clockPolicy,
    MediaTranscodeStreamSet members, MediaRunningTime acquisitionTimeout)
{
    using Result = ::media::Result<MediaPreparedRtpSourceSessionStreamPlan>;
    if (candidate.snapshot.source.expired() ||
        candidate.snapshot.maximumDatagramBytes != candidate.plan.maximumDatagramBytes() ||
        candidate.snapshot.socketReceiveCapacityBytes != candidate.plan.socketReceiveCapacityBytes())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared protocol candidate lost its source or capacity facts"));
    if (auto bound = MediaPreparedRtpIngressPlanner::bind(candidate.plan, transport); !bound)
        return Result::failure(bound.error());
    if (!transport.rtcpCompositionMode || transport.senderReportTimeoutMs <= 0 ||
        transport.cnameTimeoutMs <= 0 || transport.maximumExtrapolationMs <= 0 ||
        transport.clockRate != depacketizer.clockRate || transport.payloadType != depacketizer.payloadType)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared protocol clock differs from source transport facts"));
    MediaRtpReorderConfig reorder{candidate.plan.reorderWindowPackets(),
        std::chrono::nanoseconds(candidate.plan.maximumReorderDelayNanoseconds()), transport.payloadType};
    auto workspace = MediaSourcePreparationStoragePlanner::protocolWorkingSet(
        candidate.plan.maximumDatagramBytes(), reorder, depacketizer, envelope);
    if (!workspace) return Result::failure(workspace.error());
    constexpr std::int64_t NanosecondsPerMillisecond = 1'000'000;
    MediaRtpInputClockSessionConfig clock{{transport.requireSenderReports, transport.requireCname,
        transport.senderReportTimeoutMs * NanosecondsPerMillisecond,
        transport.cnameTimeoutMs * NanosecondsPerMillisecond},
        transport.maximumExtrapolationMs * NanosecondsPerMillisecond,
        transport.clockLossPolicy, *transport.rtcpCompositionMode};
    const auto& expected = depacketizer.streamKind == MediaStreamKind::Video
        ? clockPolicy.videoInput : clockPolicy.audioInput;
    const auto expectedLoss = depacketizer.streamKind == MediaStreamKind::Video
        ? clockPolicy.input.clockLossPolicy : clockPolicy.input.secondaryClockLossPolicy;
    if (!expected.clockRate || !expected.payloadType ||
        *expected.clockRate != transport.clockRate || *expected.payloadType != transport.payloadType ||
        clock.tracker.requireSenderReports != *clockPolicy.input.requireSenderReports || clock.tracker.requireCname ||
        clock.tracker.senderReportTimeoutNs != clockPolicy.input.senderReportTimeoutNs->nanoseconds() ||
        clock.tracker.cnameTimeoutNs != clockPolicy.input.identityEvidenceTimeoutNs->nanoseconds() ||
        clock.maximumExtrapolationNs != clockPolicy.input.maximumExtrapolationNs->nanoseconds() ||
        clock.compositionMode != *clockPolicy.input.rtcpCompositionMode || !expectedLoss ||
        clock.lossPolicy != *expectedLoss)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared protocol transport conflicts with the authoritative clock policy"));
    auto validClock = MediaRtpInputClockSession::create(clock, MediaRuntimeLineageAllocation{});
    if (!validClock) return Result::failure(validClock.error());
    MediaRtpPacketClockSessionConfig binder{depacketizer.streamKind, members,
        retention.maximumUnits, acquisitionTimeout,
        depacketizer.streamKind == MediaStreamKind::Video
            ? std::optional<int>{transport.clockRate} : std::nullopt,
        depacketizer.streamKind == MediaStreamKind::Video
            ? std::optional<MediaRtpVideoTimingPlan>{videoTiming}
            : std::nullopt};
    auto validBinder = MediaRtpPacketClockSession::create(binder,
        MediaRuntimeLineageAllocation{});
    if (!validBinder) return Result::failure(validBinder.error());
    // Finite admission for one batch and one expiry pass. Each released RTP
    // packet can emit one discontinuity and one clock evidence. This is not a
    // promise that a subsequent batch waits for the consumer to drain it.
    auto passes = MediaCheckedArithmetic::add(candidate.plan.descriptorCapacity(), 1,
        "prepared protocol batch and expiry pass");
    if (!passes) return Result::failure(passes.error());
    auto released = MediaCheckedArithmetic::multiply(passes.value(),
        workspace.value().maximumReorderedPackets, "prepared protocol released packets");
    if (!released) return Result::failure(released.error());
    auto events = MediaCheckedArithmetic::multiply(released.value(), 2,
        "prepared protocol discontinuities and evidence");
    if (!events) return Result::failure(events.error());
    auto control = MediaCheckedArithmetic::add(events.value(), 1,
        "prepared protocol clock expiry");
    if (!control) return Result::failure(control.error());
    auto objects = MediaCheckedArithmetic::add(control.value(), retention.maximumUnits,
        "prepared protocol output objects");
    if (!objects) return Result::failure(objects.error());
    // Actual queue allocations and AU payloads share this deployment budget.
    // Do not pre-reserve count times a maximum access-unit or node size.
    const auto bytes = retention.maximumBytes;
    if (objects.value() > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Prepared protocol output exceeds addressable storage"));
    return Result::success({depacketizer, envelope, reorder, clock, std::move(workspace).value(), binder,
        static_cast<std::size_t>(objects.value()), bytes});
}

} // namespace

::media::Status MediaPreparedRtpSourceSessionPlan::validateAudioRetention() const
{
    if (audio.has_value() != audioRetention.has_value() || audio.has_value() != admission.audio.has_value() ||
        (canonicalRuntimeRetention && canonicalRuntimeRetention->audio != audioRetention) ||
        (sealedAdmission && sealedAdmission->audio.has_value() != audio.has_value()))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared audio retention differs from source members or canonical retention"));
    if (sealedAdmission) {
        const auto& sealed = *sealedAdmission;
        if (sealed.sharedCapacityBytes != admission.sharedCapacityBytes ||
            sealed.startup.audioUnits.has_value() != audio.has_value() ||
            sealed.startup.audioBytes.has_value() != audio.has_value() ||
            sealed.startup.audioUnitBytes.has_value() != audio.has_value())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Sealed audio retention differs from source budget or startup members"));
        if (audio && (!audioRetention->valid() ||
            !sealed.audio->compressed.maximumUnits || !sealed.audio->bounds.acquiringObjects ||
            admission.audio->maximumUnits != sealed.audio->compressed.maximumUnits ||
            admission.audio->maximumBytes != sealed.audio->compressed.maximumBytes ||
            admission.audio->maximumBytes != admission.sharedCapacityBytes ||
            audio->binder.acquiringCapacity != sealed.audio->bounds.acquiringObjects ||
            sealed.startup.audioUnits != audioRetention->maximumUnits ||
            sealed.startup.audioBytes != audioRetention->maximumBytes ||
            sealed.startup.audioUnitBytes != audioRetention->maximumUnitBytes))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Prepared audio differs from sealed compressed and independent startup retention"));
        return ::media::Status::success();
    }
    if (audioRetention && (!audioRetention->valid() ||
        admission.audio->maximumUnits != (std::min)(admission.sharedCapacityBytes / sizeof(MediaBufferRef), audioRetention->maximumUnits) ||
        !admission.audio->maximumUnits || admission.audio->maximumBytes != admission.sharedCapacityBytes ||
        audio->binder.acquiringCapacity != admission.audio->maximumUnits))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Prepared audio admission differs from its independent retention product"));
    return ::media::Status::success();
}

::media::Result<MediaPreparedRtpSourceSessionPlan> MediaPreparedRtpSourceSessionPlanner::plan(
    const MediaRealtimeCompositionSourceFacts& facts,
    const MediaPreparedRtpIngressCandidate& videoCandidate,
    const MediaPreparedRtpIngressCandidate* audioCandidate,
    std::uint64_t videoPrefixAccessUnitBound,
    std::optional<std::uint64_t> audioPrefixAccessUnitBound,
    std::chrono::steady_clock::time_point deadline,
    std::chrono::steady_clock::time_point observedNow,
    std::size_t sharedCapacityBytes)
{
    using Result = ::media::Result<MediaPreparedRtpSourceSessionPlan>;
    const auto& request = facts.request;
    const auto& raw = facts.raw;
    const bool hasAudio = request.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo;
    if (!sharedCapacityBytes || (request.parameters.execution.streamSet != MediaTranscodeStreamSet::VideoOnly && !hasAudio) ||
        request.input.type != RealtimeInputType::RtpPort || !request.input.openTimeoutMs ||
        *request.input.openTimeoutMs <= 0 || observedNow >= deadline ||
        deadline - observedNow > std::chrono::milliseconds(*request.input.openTimeoutMs) ||
        hasAudio != (audioCandidate != nullptr) || hasAudio != audioPrefixAccessUnitBound.has_value() ||
        hasAudio != raw.audio.has_value() || hasAudio != raw.audioTransport.has_value() ||
        hasAudio != raw.audioDepacketizer.has_value() || hasAudio != raw.audioAccessUnitEnvelope.has_value())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Prepared source session requires current deadline and exact source members"));
    const auto members = *request.parameters.execution.streamSet;
    const auto lifecycleMode = MediaAvSourceLifecycleMode::PreserveActivatedOutput;
    auto policy = MediaAvSyncPlanner::planRtpInputClock(request, lifecycleMode);
    if (!policy) return Result::failure(policy.error());
    auto startup = MediaAvSyncStartupPolicyPlanner::planInputPreflight(request);
    if (!startup) return Result::failure(startup.error());
    if (!startup.value().maximumWaitNs || startup.value().maximumWaitNs->nanoseconds() <= 0)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Prepared source session lacks its original acquisition timeout"));
    const auto& clock = policy.value().input;
    if (!clock.requireSenderReports || !clock.rtcpCompositionMode || !clock.secondaryClockLossPolicy ||
        !clock.senderReportTimeoutNs || !clock.maximumExtrapolationNs ||
        !clock.identityEvidenceTimeoutNs || !clock.maximumSenderClockResidualNs ||
        !clock.maximumSenderClockRateErrorPpm || !clock.clockLossPolicy ||
        (hasAudio && !clock.maximumInterStreamClockOffsetSkewNs) ||
        clock.streamAssociationMode != (hasAudio ? MediaAvSyncRtpStreamAssociationMode::PlannedStreamPair
                                                : MediaAvSyncRtpStreamAssociationMode::PlannedSingleStream))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Prepared source session lacks authoritative clock-group policy"));
    // Capacity covers the remaining preparation interval and the unchanged
    // runtime acquisition policy. The policy timeout is not rebased or extended.
    const auto window = MediaRunningTime::fromNanoseconds((std::max)(
        std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - observedNow).count(),
        startup.value().maximumWaitNs->nanoseconds()));
    std::optional<MediaPreparedInputRetentionStream> audioRetention;
    if (hasAudio) {
        auto plannedAudio = MediaPreparedInputRetentionPlanner::planStream(window,
            {raw.audioDepacketizer->clockRate, raw.audioDepacketizer->accessUnitDurationRtpTicks},
            *audioPrefixAccessUnitBound, {MediaStreamKind::Audio,
                raw.audioAccessUnitEnvelope->maximumAccessUnitBytes, raw.audioAccessUnitEnvelope->sizeAuthority},
            "audio-source-AU-cadence+max-open-remaining-and-startup-wait-finite-admission");
        if (!plannedAudio) return Result::failure(plannedAudio.error());
        audioRetention.emplace(std::move(plannedAudio).value());
    }
    std::optional<MediaPreparedInputRetentionPlan> retention;
    MediaRtpVideoTimingPlan videoTiming{MediaRtpVideoTimingProduct::CanonicalInterval, std::nullopt};
    if (const auto* cadence = std::get_if<MediaObservedVideoCadence>(&facts.timing)) {
        auto videoRetention = MediaPreparedInputRetentionPlanner::planStream(window, cadence->rate,
            videoPrefixAccessUnitBound, {MediaStreamKind::Video, raw.videoAccessUnitEnvelope.maximumAccessUnitBytes,
                raw.videoAccessUnitEnvelope.sizeAuthority}, "prepared-source-cadence+max-open-remaining-and-startup-wait-finite-admission");
        if (!videoRetention) return Result::failure(videoRetention.error());
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::PlannerCapability,
            "composition_prepared_video_admission media_id=" + request.mediaId +
            " rate=" + std::to_string(cadence->rate.num) + "/" +
            std::to_string(cadence->rate.den) + " window_ns=" +
            std::to_string(window.nanoseconds()) + " replay_units=" +
            std::to_string(videoPrefixAccessUnitBound) + " maximum_units=" +
            std::to_string(videoRetention.value().maximumUnits));
        retention.emplace(MediaPreparedInputRetentionPlan{*startup.value().maximumWaitNs,
            std::move(videoRetention).value(), std::nullopt});
        retention->audio = audioRetention;
        if (auto valid = retention->validate(); !valid) return Result::failure(valid.error());
    } else {
        auto identity = MediaCanonicalIdentity::copy(request.mediaId);
        if (!identity || identity.value().empty()) return Result::failure(identity
            ? ::media::ErrorInfo::invalidArgument("Prepared presentation requires source identity") : identity.error());
        videoTiming = {MediaRtpVideoTimingProduct::SourcePresentation, std::move(identity).value()};
    }
    const auto count = sharedCapacityBytes / sizeof(MediaBufferRef);
    if (!count) return Result::failure(::media::ErrorInfo::allocationFailed(
        "Prepared RTP deployment capacity cannot hold one access-unit reference"));
    MediaPreparedRtpAdmissionPlan admission{sharedCapacityBytes,
        {retention ? (std::min)(count, retention->video.maximumUnits) : count, sharedCapacityBytes}, std::nullopt};
    if (hasAudio) admission.audio.emplace(MediaPreparedRtpAdmissionStream{
        (std::min)(count, audioRetention->maximumUnits), sharedCapacityBytes});
    auto video = streamPlan(raw.videoDepacketizer, raw.videoAccessUnitEnvelope, raw.videoTransport,
        videoCandidate, admission.video, videoTiming, policy.value(), members, *startup.value().maximumWaitNs);
    if (!video) return Result::failure(video.error());
    std::optional<MediaPreparedRtpSourceSessionStreamPlan> audio;
    if (hasAudio) {
        auto planned = streamPlan(*raw.audioDepacketizer, *raw.audioAccessUnitEnvelope, *raw.audioTransport,
            *audioCandidate, *admission.audio, videoTiming, policy.value(), members, *startup.value().maximumWaitNs);
        if (!planned) return Result::failure(planned.error());
        audio.emplace(std::move(planned).value());
    }
    // Stream association is an explicit planned pair/single stream, not CNAME
    // equality. This matches the existing production clock-group builder.
    const bool requireMatchingCname = false;
    const auto mapper = [&](int rate) {
        return MediaRtpSourceClockMapperConfig{rate, requireMatchingCname,
            clock.senderReportTimeoutNs->nanoseconds(), clock.maximumExtrapolationNs->nanoseconds(),
            clock.maximumSenderClockResidualNs->nanoseconds(), *clock.maximumSenderClockRateErrorPpm};
    };
    MediaRtpClockGroupSessionConfig group{mapper(raw.videoTransport.clockRate),
        hasAudio ? std::optional{mapper(raw.audioTransport->clockRate)} : std::nullopt,
        {members, clock.senderReportTimeoutNs->nanoseconds(), clock.maximumExtrapolationNs->nanoseconds(),
         clock.identityEvidenceTimeoutNs->nanoseconds(), hasAudio
            ? std::optional{MediaRtpClockGroupAudioConfig{clock.identityEvidenceTimeoutNs->nanoseconds(),
                clock.maximumInterStreamClockOffsetSkewNs->nanoseconds()}} : std::nullopt,
         requireMatchingCname, clock.commonEpochPolicy,
         *clock.clockLossPolicy == MediaRtpClockLossPolicy::InvalidateAndWait}};
    auto validGroup = MediaRtpClockGroupSession::create(group);
    if (!validGroup) return Result::failure(validGroup.error());
    MediaPreparedRtpSourceSessionPlan product{std::move(video).value(), std::move(audio), std::move(group),
        std::move(admission), std::move(retention), std::move(audioRetention), lifecycleMode, MediaFirstLockedSourceGeneration, deadline};
    if (auto storage = MediaPreparedRtpSourceStoragePlanner::plan(product); !storage)
        return Result::failure(storage.error());
    return Result::success(std::move(product));
}

} // namespace media::ffmpeg::graph
