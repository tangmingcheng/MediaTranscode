#include "internal/graph/runtime/compilation/MediaDemuxClockRuntimeAssembler.h"

#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/time/MediaDemuxTimestampClockMapper.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<std::vector<std::unique_ptr<MediaRuntimeNode>>>
MediaDemuxClockRuntimeAssembler::create(
    MediaGraphExecutionContext& context,
    const MediaAvSyncGroupKey& groupKey,
    const MediaAvDemuxClockRegistration& registration)
{
    using Result = ::media::Result<std::vector<std::unique_ptr<MediaRuntimeNode>>>;
    const auto* graph = context.graph();
    const auto* videoNode = graph ? graph->findNode(registration.videoBinder) : nullptr;
    const auto* audioNode = graph ? graph->findNode(registration.audioBinder) : nullptr;
    auto group = context.findAvSyncGroup(groupKey);
    if (!videoNode || !audioNode || !group || group->key() != groupKey)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Demux clock assembly requires its exact nodes and registered domain"));
    auto video = MediaDemuxPacketClockBinderNodePlanCodec::decode(*videoNode);
    auto audio = MediaDemuxPacketClockBinderNodePlanCodec::decode(*audioNode);
    if (!video) return Result::failure(video.error());
    if (!audio) return Result::failure(audio.error());
    for (const auto* plan : {&video.value(), &audio.value()}) {
        if (auto exact = MediaDemuxPacketClockBinderNodePlanCodec::validateAgainstPlanner(
                *plan, groupKey, group->plan()); !exact)
            return Result::failure(exact.error());
    }
    if (video.value().stream != MediaScheduledStream::Video ||
        audio.value().stream != MediaScheduledStream::Audio ||
        video.value().mapper != audio.value().mapper)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Demux clock assembly conflicts with the typed stream roles"));
    auto created = MediaDemuxTimestampClockMapper::create(video.value().mapper);
    if (!created) return Result::failure(created.error());
    auto mapper = std::move(created).value();
    const std::weak_ptr<MediaNodeWakeup> videoWakeup = context.sharedNodeWakeup(videoNode->id);
    const std::weak_ptr<MediaNodeWakeup> audioWakeup = context.sharedNodeWakeup(audioNode->id);
    if (auto bound = mapper->bindStateChangeNotifiers(
            [videoWakeup]() noexcept {
                if (auto wakeup = videoWakeup.lock()) wakeup->notify();
            },
            [audioWakeup]() noexcept {
                if (auto wakeup = audioWakeup.lock()) wakeup->notify();
            }); !bound) return Result::failure(bound.error());
    auto videoRuntime = MediaRuntimeNodeFactory::createDemuxPacketClockBinder(
        *videoNode, video.value(), mapper, group);
    if (!videoRuntime) return Result::failure(videoRuntime.error());
    auto audioRuntime = MediaRuntimeNodeFactory::createDemuxPacketClockBinder(
        *audioNode, audio.value(), mapper, group);
    if (!audioRuntime) return Result::failure(audioRuntime.error());
    std::vector<std::unique_ptr<MediaRuntimeNode>> nodes;
    nodes.push_back(std::move(videoRuntime).value());
    nodes.push_back(std::move(audioRuntime).value());
    return Result::success(std::move(nodes));
}

} // namespace media::ffmpeg::graph
