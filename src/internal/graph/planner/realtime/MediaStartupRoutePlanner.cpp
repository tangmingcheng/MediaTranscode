#include "internal/graph/planner/realtime/MediaStartupRoutePlanner.h"
#include "internal/graph/model/MediaAtomicOutputPolicyContract.h"

namespace media::ffmpeg::graph {
const MediaStartupRouteShape::Connections& MediaStartupRouteShape::connections() noexcept
{
    using Role = MediaStartupRouteRole;
    using Policy = MediaStartupRoutePolicy;
    using Form = MediaStartupEventMediaForm;
    static constexpr Connections edges{{
        {Role::VideoInput, "out", Role::Coordinator, "video", Form::VideoEnvelope, Policy::Metadata,
            "canonical video -> startup coordinator"},
        {Role::AudioInput, "out", Role::Coordinator, "audio", Form::AudioEnvelope, Policy::Metadata,
            "canonical audio -> startup coordinator"},
        {Role::Coordinator, "release", Role::Binder, "release", Form::Release, Policy::Metadata,
            "startup release -> epoch binder"},
        {Role::Binder, "transaction", Role::Sequencer, "transaction", Form::Transaction, Policy::AtomicMetadata,
            "epoch transaction -> activation sequencer"},
        {Role::Binder, "preparation", Role::Extractor, "preparation", Form::Transaction, Policy::AtomicMetadata,
            "epoch transaction -> video preparation extractor"},
        {Role::Sequencer, "bound_release", Role::Extractor, "bound_release", Form::Transaction, Policy::AtomicMetadata,
            "activated release -> atomic extractor"}}};
    return edges;
}
bool MediaStartupRouteShape::includes(const MediaStartupRouteConnection& connection) const noexcept
{ return connection.from != MediaStartupRouteRole::AudioInput || members == MediaTranscodeStreamSet::AudioVideo; }
MediaNodeKind MediaStartupRouteShape::nodeKind(MediaStartupRouteRole role) const noexcept
{
    switch (role) {
    case MediaStartupRouteRole::VideoInput: return readiness == MediaVideoStartupReadiness::DecodedPresentationSpan
        ? MediaNodeKind::DecodedVideoStartup : MediaNodeKind::CanonicalInput;
    case MediaStartupRouteRole::AudioInput: return MediaNodeKind::CanonicalInput;
    case MediaStartupRouteRole::Coordinator: return MediaNodeKind::AvStartupCoordinator;
    case MediaStartupRouteRole::Binder: return MediaNodeKind::PlaybackEpochBinder;
    case MediaStartupRouteRole::Sequencer: return MediaNodeKind::ActivatedStartupReleaseSequencer;
    case MediaStartupRouteRole::Extractor: return MediaNodeKind::AvBoundReleaseExtractor;
    }
    return MediaNodeKind::Unknown;
}
const MediaStartupRouteConnection* MediaStartupRouteShape::find(MediaNodeKind from,
    std::string_view output, MediaNodeKind to, std::string_view input) const noexcept
{
    for (const auto& connection : connections())
        if (includes(connection) && nodeKind(connection.from) == from && connection.output == output &&
            nodeKind(connection.to) == to && connection.input == input) return &connection;
    return nullptr;
}
const MediaEdgePolicy& MediaStartupRoutePlan::policy(const MediaStartupRouteConnection& connection) const noexcept
{ return connection.policy == MediaStartupRoutePolicy::Metadata ? metadata : atomicMetadata; }
::media::Result<MediaStartupRouteShape> MediaStartupRoutePlanner::describe(
    MediaTranscodeStreamSet members, MediaVideoStartupReadiness readiness)
{
    using R = ::media::Result<MediaStartupRouteShape>;
    if ((members != MediaTranscodeStreamSet::VideoOnly && members != MediaTranscodeStreamSet::AudioVideo) ||
        !encodeMediaVideoStartupReadiness(readiness))
        return R::failure(::media::ErrorInfo::invalidArgument("Startup route requires planned members and readiness"));
    return R::success({members, readiness});
}
::media::Result<MediaStartupRoutePlan> MediaStartupRoutePlanner::bind(MediaStartupRouteShape shape,
    const MediaEdgePolicy& metadata, const MediaEdgePolicy& atomicMetadata)
{
    using R = ::media::Result<MediaStartupRoutePlan>;
    auto valid = describe(shape.members, shape.readiness);
    if (!valid) return R::failure(valid.error());
    const auto& queue = metadata.queuePolicy;
    if (!queue.bounded || !queue.capacity || !MediaAtomicOutputPolicyContract::accepts(atomicMetadata))
        return R::failure(::media::ErrorInfo::invalidArgument("Startup route requires bounded metadata and atomic metadata policies"));
    return R::success({shape, metadata, atomicMetadata});
}
} // namespace media::ffmpeg::graph
