#include "internal/graph/runtime/validation/MediaAvSyncGraphShapeValidator.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/runtime/validation/MediaAvCommonCoreShapeValidator.h"
#include "internal/graph/runtime/validation/MediaOutputAuthorityShapeValidator.h"
#include "internal/graph/runtime/validation/MediaSourceClockShapeValidator.h"

namespace media::ffmpeg::graph {
namespace {

bool isSynchronizedNode(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::SourceClockStateFanout:
    case MediaNodeKind::LockedPacketGate:
    case MediaNodeKind::CanonicalInput:
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvStartupClock:
    case MediaNodeKind::PlaybackEpochBinder:
    case MediaNodeKind::ActivatedStartupReleaseSequencer:
    case MediaNodeKind::AvBoundReleaseExtractor:
    case MediaNodeKind::AudioDriftController:
    case MediaNodeKind::AvOutputScheduler:
    case MediaNodeKind::ScheduledOutputRouter:
    case MediaNodeKind::RtpClockGroup:
    case MediaNodeKind::RtpClockSnapshotFanout:
    case MediaNodeKind::RtpSourceClockStateAdapter:
    case MediaNodeKind::RtpPacketClockBinder:
    case MediaNodeKind::DemuxPacketClockBinder:
        return true;
    default:
        return false;
    }
}

} // namespace

::media::Status MediaAvSyncGraphShapeValidator::validate(
    const MediaGraph& graph,
    const MediaAvSyncRuntimeBinding& binding)
{
    if (!binding.groupKey.valid()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized graph shape requires a valid runtime group"));
    }
    if (auto plan =
            MediaAvSyncPlanValidator::validateRuntime(binding.plan);
        !plan) {
        return plan;
    }
    if (auto source =
            MediaSourceClockShapeValidator::validate(graph, binding);
        !source) {
        return source;
    }
    if (auto common =
            MediaAvCommonCoreShapeValidator::validate(graph, binding);
        !common) {
        return common;
    }
    return MediaOutputAuthorityShapeValidator::validate(
        graph, binding);
}

::media::Status MediaAvSyncGraphShapeValidator::validateAbsent(
    const MediaGraph& graph)
{
    for (const MediaNode& node : graph.nodes()) {
        if (isSynchronizedNode(node.kind)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "Synchronized runtime nodes require an A/V sync binding"));
        }
        for (const auto& [key, value] : node.options.values()) {
            if (key.ends_with(".sync_group")) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Synchronized runtime options require an A/V sync binding"));
            }
        }
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
