#include "internal/graph/planner/realtime/MediaCompositionSourceCapacityPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeEdgePolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncStartupPolicyPlanner.h"
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
::media::Result<std::size_t> grow(std::size_t floor, std::size_t increment)
{
    if (increment > (std::numeric_limits<std::size_t>::max)() - floor)
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::invalidArgument(
            "Composition capacity increment exceeds the quantity domain"));
    return ::media::Result<std::size_t>::success(floor + increment);
}
::media::Result<MediaPreparedRtpContainerBounds> streamBounds(
    const MediaPreparedRtpSourceSessionStreamPlan& stream,
    const MediaPreparedRtpStreamStorageSnapshot& held, std::size_t increment)
{
    using Result = ::media::Result<MediaPreparedRtpContainerBounds>;
    auto minimum = MediaPreparedRtpSourceStoragePlanner::minimumBounds(stream, held);
    if (!minimum) return Result::failure(minimum.error());
    auto acquiring = grow(minimum.value().acquiringObjects, increment);
    auto outputs = grow(minimum.value().outputObjects, increment);
    if (!acquiring || !outputs) return Result::failure(!acquiring ? acquiring.error() : outputs.error());
    // The clock binder consumes a positive signed-int capacity option.
    if (acquiring.value() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition acquiring capacity exceeds the clock binder domain"));
    return Result::success({minimum.value().prefixObjects, acquiring.value(), outputs.value()});
}
}
::media::Result<MediaCompositionSourceCapacityCandidate> MediaCompositionSourceCapacityPlanner::plan(
    const MediaVideoSourcePlan& video, const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaPreparedRtpSourceSessionPlan& session,
    const MediaPreparedRtpStorageSnapshot& held, std::size_t increment)
{
    using Result = ::media::Result<MediaCompositionSourceCapacityCandidate>;
    const bool audio = session.audio.has_value();
    if (session.canonicalRuntimeRetention || !video.decoder.outputFrame ||
        audio != held.audio.has_value() || audio != session.audioRetention.has_value())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition candidate requires decoded presentation and matching lock-held stream facts"));
    if (auto valid = session.validateAudioRetention(); !valid) return Result::failure(valid.error());
    auto videoBounds = streamBounds(session.video, held.video, increment);
    if (!videoBounds) return Result::failure(videoBounds.error());
    std::optional<MediaPreparedRtpContainerBounds> audioBounds;
    if (audio) {
        auto bounds = streamBounds(*session.audio, *held.audio, increment);
        if (!bounds) return Result::failure(bounds.error());
        audioBounds = bounds.value();
    }
    // One slot is the structural floor of a nonempty queue. Two display points
    // are required for decoded startup and aggregate current/successor storage.
    auto queue = grow(1, increment);
    auto display = grow(2, increment);
    if (!queue || !display) return Result::failure(!queue ? queue.error() : display.error());
    auto startup = MediaRealtimeMediaCapacityPlanner::planDecodedSource(*video.decoder.outputFrame,
        display.value(), audio ? &*session.audioRetention : nullptr, deployment.encode().latency);
    if (!startup) return Result::failure(startup.error());
    // Source contributions have no mux queue. Audio startup and compressed
    // prefix remain independent fixed facts; neither grows with this candidate.
    const MediaGraphQueueParameters queues{queue.value(), queue.value(), display.value(), 0};
    const auto members = audio ? MediaTranscodeStreamSet::AudioVideo : MediaTranscodeStreamSet::VideoOnly;
    auto fixed = MediaAvSyncStartupPolicyPlanner::prepare(members,
        MediaVideoStartupReadiness::DecodedPresentationSpan, deployment.encode().transportTiming.senderTransportLead);
    if (!fixed) return Result::failure(fixed.error());
    if (!fixed.value().maximumWaitNs)
        return Result::failure(::media::ErrorInfo::notInitialized("Composition acquisition deadline is absent"));
    // Metadata routes depend on the queue declarations, not startup release
    // counts. The full synchronized source policies are checked by the kernel.
    const auto policies = MediaRealtimeEdgePolicyPlanner::plan(queues);
    auto shape = MediaStartupRoutePlanner::describe(members, MediaVideoStartupReadiness::DecodedPresentationSpan);
    if (!shape) return Result::failure(shape.error());
    auto route = MediaStartupRoutePlanner::bind(shape.value(), policies.metadata, policies.atomicMetadata);
    if (!route) return Result::failure(route.error());
    return Result::success({{videoBounds.value(), audioBounds},
        {startup.value(), std::move(route).value(), queues, videoBounds.value().acquiringObjects,
            audioBounds ? std::optional{audioBounds->acquiringObjects} : std::nullopt,
            *fixed.value().maximumWaitNs}});
}
}
