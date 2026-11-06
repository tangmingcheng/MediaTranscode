#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"
#include "internal/graph/sync/startup/MediaAvStartupLimits.h"
#include "internal/graph/planner/realtime/MediaAvSourceCapacityFacts.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"

#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t Millisecond = 1'000'000;
constexpr std::int64_t Second = 1'000'000'000;

constexpr MediaRunningTime runningTime(std::int64_t nanoseconds) noexcept
{
    return MediaRunningTime::fromNanoseconds(nanoseconds);
}

::media::Result<MediaAvSyncStartupPolicy> makePolicy(
    const MediaRealtimeMediaCapacityPlan& capacity,
    std::optional<MediaRunningTime> outputLead,
    MediaTranscodeStreamSet members)
{
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((members != MediaTranscodeStreamSet::VideoOnly && !hasAudio) ||
        hasAudio != capacity.audioUnits.has_value() ||
        hasAudio != capacity.audioUnitBytes.has_value() ||
        hasAudio != capacity.audioBytes.has_value() ||
        (hasAudio && (*capacity.audioUnits == 0 || *capacity.audioUnitBytes == 0 || *capacity.audioBytes == 0)) ||
        capacity.videoUnits == 0 || capacity.videoUnitBytes == 0 ||
        capacity.videoBytes == 0) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup deployment budget is incomplete"));
    }

    auto fixed = MediaAvSyncStartupPolicyPlanner::prepare(members,
        MediaVideoStartupReadiness::IntervalCoverage, outputLead);
    if (!fixed) return ::media::Result<MediaAvSyncStartupPolicy>::failure(fixed.error());
    MediaAvSyncStartupPolicy startup;
    static_cast<MediaAvSyncStartupFixedPolicy&>(startup) = std::move(fixed).value();
    startup.maximumGapNs = capacity.maximumGap;
    startup.videoCapacity = capacity.videoUnits;
    startup.audioCapacity = capacity.audioUnits;
    startup.videoByteCapacity = capacity.videoBytes;
    startup.audioByteCapacity = capacity.audioBytes;
    startup.maximumVideoUnitBytes = capacity.videoUnitBytes;
    startup.maximumAudioUnitBytes = capacity.audioUnitBytes;
    const auto maximumSerialized = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max());
    if (*startup.maximumVideoUnitBytes > maximumSerialized ||
        (startup.maximumAudioUnitBytes && *startup.maximumAudioUnitBytes > maximumSerialized) ||
        *startup.videoByteCapacity > maximumSerialized ||
        (startup.audioByteCapacity && *startup.audioByteCapacity > maximumSerialized)) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup capacity exceeds the runtime option range"));
    }
    return ::media::Result<MediaAvSyncStartupPolicy>::success(
        std::move(startup));
}

} // namespace

::media::Result<MediaAvSyncStartupFixedPolicy> MediaAvSyncStartupPolicyPlanner::prepare(
    MediaTranscodeStreamSet members, MediaVideoStartupReadiness readiness,
    std::optional<MediaRunningTime> outputLead)
{
    using Result = ::media::Result<MediaAvSyncStartupFixedPolicy>;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && members != MediaTranscodeStreamSet::VideoOnly) ||
        (readiness != MediaVideoStartupReadiness::IntervalCoverage &&
         readiness != MediaVideoStartupReadiness::DecodedPresentationSpan))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Startup fixed policy requires explicit members and readiness"));
    MediaAvSyncStartupFixedPolicy startup;
    startup.videoReadiness = readiness;
    startup.requireVideoKeyFrame = true;
    startup.trimAudioToCommonStart = hasAudio;
    startup.maximumWaitNs = runningTime(10 * Second);
    startup.prerollNs = runningTime(500 * Millisecond);
    startup.keyFrameWaitNs = runningTime(5 * Second);
    if (hasAudio) startup.maximumAudioTrimNs = runningTime(250 * Millisecond);
    startup.maximumInitialSkewNs = runningTime(40 * Millisecond);
    startup.outputLeadNs = outputLead;
    startup.allowDegradedClock = false;
    return Result::success(std::move(startup));
}

::media::Result<MediaAvSyncStartupPolicy> MediaAvSyncStartupPolicyPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaAvSourceCapacityFacts& capacity,
    const MediaRealtimeDeploymentEnvelope& deployment)
{
    using Result = ::media::Result<MediaAvSyncStartupPolicy>;
    const auto members = request.parameters.execution.streamSet;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && members != MediaTranscodeStreamSet::VideoOnly) ||
        request.input.type != RealtimeInputType::RtpPort || capacity.audioAcquiringCapacity.has_value()!=hasAudio ||
        !capacity.videoAcquiringCapacity || (hasAudio && !*capacity.audioAcquiringCapacity) ||
        capacity.route.shape.members != members ||
        capacity.route.shape.readiness != MediaVideoStartupReadiness::DecodedPresentationSpan)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoded source startup requires matching pure capacity and route facts"));
    auto route=MediaStartupRoutePlanner::bind(capacity.route.shape,capacity.route.metadata,capacity.route.atomicMetadata);
    if(!route)return Result::failure(route.error());
    auto policy = makePolicy(capacity.startup,
        deployment.encode().transportTiming.senderTransportLead, *members);
    if (!policy) return policy;
    if (policy.value().maximumWaitNs != capacity.acquisitionWindow)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Decoded source capacity differs from the startup acquisition window"));
    policy.value().videoReadiness = MediaVideoStartupReadiness::DecodedPresentationSpan;
    return policy;
}

::media::Result<MediaAvSyncStartupPolicy>
MediaAvSyncStartupPolicyPlanner::plan(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeGraphResourceLedgerPlan& ledger,
    const MediaRealtimeDeploymentEnvelope& deployment)
{
    const auto members = request.parameters.execution.streamSet;
    if (members != MediaTranscodeStreamSet::AudioVideo &&
        !(members == MediaTranscodeStreamSet::VideoOnly && request.input.type == RealtimeInputType::RtpPort)) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::notInitialized(
                "Startup requires AudioVideo or a video-only RTP source"));
    }
    auto capacity = MediaRealtimeMediaCapacityPlanner::plan(ledger);
    if (!capacity) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            capacity.error());
    }
    if (!ledger.inputRetention) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V startup requires a prepared input retention product"));
    }
    const auto& retention = *ledger.inputRetention;
    if (auto status = retention.validate(); !status) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(status.error());
    }
    if (retention.audio.has_value() != (members == MediaTranscodeStreamSet::AudioVideo))
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument("Startup retention differs from planned source members"));
    auto policy = makePolicy(MediaRealtimeMediaCapacityPlan{
        retention.video.maximumUnits, retention.video.maximumUnitBytes,
        retention.video.maximumBytes,
        retention.audio ? std::optional{retention.audio->maximumUnits} : std::nullopt,
        retention.audio ? std::optional{retention.audio->maximumUnitBytes} : std::nullopt,
        retention.audio ? std::optional{retention.audio->maximumBytes} : std::nullopt,
        capacity.value().maximumGap},
        deployment.encode().transportTiming.senderTransportLead, *members);
    if (policy && policy.value().maximumWaitNs != retention.acquisitionWindow) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "input retention acquisition window differs from startup policy"));
    }
    return policy;
}

::media::Result<MediaAvSyncStartupPolicy>
MediaAvSyncStartupPolicyPlanner::finalizePrepared(
    MediaAvSyncStartupPolicy prepared,
    const MediaRealtimeGraphResourceLedgerPlan& ledger,
    const MediaRealtimeDeploymentEnvelope& deployment)
{
    auto capacity = MediaRealtimeMediaCapacityPlanner::plan(ledger);
    if (!capacity) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            capacity.error());
    }
    if (!prepared.videoCapacity || !prepared.audioCapacity ||
        !prepared.videoByteCapacity || !prepared.audioByteCapacity ||
        !prepared.maximumVideoUnitBytes ||
        !prepared.maximumAudioUnitBytes) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::notInitialized(
                "prepared A/V startup is missing its input replay bounds"));
    }
    prepared.maximumGapNs = capacity.value().maximumGap;
    prepared.outputLeadNs =
        deployment.encode().transportTiming.senderTransportLead;
    return ::media::Result<MediaAvSyncStartupPolicy>::success(
        std::move(prepared));
}

::media::Result<MediaAvSyncStartupPolicy>
MediaAvSyncStartupPolicyPlanner::planInputPreflight(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    const auto members = request.parameters.execution.streamSet;
    const bool hasAudio = members == MediaTranscodeStreamSet::AudioVideo;
    if ((!hasAudio && !(members == MediaTranscodeStreamSet::VideoOnly &&
                       request.input.type == RealtimeInputType::RtpPort)) ||
        !request.input.probeSizeBytes || *request.input.probeSizeBytes <= 0 ||
        !request.input.readTimeoutMs || *request.input.readTimeoutMs <= 0) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V input observation requires stream-set and positive probe/read limits"));
    }
    const auto perStreamBudget = static_cast<std::uint64_t>(
        *request.input.probeSizeBytes) / (hasAudio ? 2U : 1U);
    if (perStreamBudget == 0) {
        return ::media::Result<MediaAvSyncStartupPolicy>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V input observation graph budget cannot admit both streams"));
    }
    const auto maximumGap = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(*request.input.readTimeoutMs) * Millisecond);
    return makePolicy(MediaRealtimeMediaCapacityPlan{
        MediaAvStartupPreflightUnitCapacity, perStreamBudget,
        perStreamBudget,
        hasAudio ? std::optional<std::size_t>{MediaAvStartupPreflightUnitCapacity} : std::nullopt,
        hasAudio ? std::optional{perStreamBudget} : std::nullopt,
        hasAudio ? std::optional{perStreamBudget} : std::nullopt,
        maximumGap}, std::nullopt, *members);
}

} // namespace media::ffmpeg::graph
