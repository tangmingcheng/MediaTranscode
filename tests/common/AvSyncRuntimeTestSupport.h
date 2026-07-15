#pragma once

#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"

#include <chrono>
#include <memory>

namespace media_transcode::test {

class FixedAvSyncClockSource final
    : public ::media::ffmpeg::graph::MediaAvSyncClockSource {
public:
    explicit FixedAvSyncClockSource(
        std::shared_ptr<::media::ffmpeg::graph::MediaMasterClock> clock)
        : m_clock(std::move(clock))
    {
    }

    ::media::Result<::media::ffmpeg::graph::MediaAvSyncClockBundle> capture(
        bool requireSharedNtpEpoch) override
    {
        using namespace ::media::ffmpeg::graph;
        std::shared_ptr<const MediaSharedNtpEpoch> sharedEpoch;
        if (requireSharedNtpEpoch) {
            auto created = MediaSharedNtpEpoch::create(
                MediaRunningTime::fromNanoseconds(0),
                std::chrono::nanoseconds(0));
            if (!created) {
                return ::media::Result<MediaAvSyncClockBundle>::failure(
                    created.error());
            }
            sharedEpoch = std::make_shared<const MediaSharedNtpEpoch>(
                std::move(created).value());
        }
        return ::media::Result<MediaAvSyncClockBundle>::success(
            MediaAvSyncClockBundle{m_clock, std::move(sharedEpoch)});
    }

private:
    std::shared_ptr<::media::ffmpeg::graph::MediaMasterClock> m_clock;
};

inline bool compileAndActivateAvSyncRuntime(
    ::media::ffmpeg::graph::MediaGraph graph,
    ::media::ffmpeg::graph::MediaAvSyncRuntimeBinding binding,
    std::shared_ptr<::media::ffmpeg::graph::MediaMasterClock> clock,
    ::media::ffmpeg::graph::MediaPlaybackEpoch epoch,
    ::media::ffmpeg::graph::MediaNodeId binderId,
    ::media::ffmpeg::graph::MediaGraphExecutionContext& context,
    std::unique_ptr<::media::ffmpeg::graph::MediaGraphRuntime>& runtimeOwner,
    int audioSampleRate = 48'000)
{
    using namespace ::media::ffmpeg::graph;
    auto runtime = std::make_unique<MediaGraphRuntime>(
        std::make_shared<FixedAvSyncClockSource>(std::move(clock)));
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graph);
    executable.avSyncBinding.emplace(std::move(binding));
    if (!runtime->compile(std::move(executable)) ||
        !runtime->registerDefaultRuntimeNodes()) {
        return false;
    }
    auto* binder = dynamic_cast<MediaPlaybackEpochBinderNode*>(
        runtime->scheduler().findNode(binderId));
    if (!binder || !binder->publishInitial(
            epoch, {epoch.generation, epoch.sourceStart,
                    epoch.masterRelease, 0, audioSampleRate})) {
        return false;
    }
    context = std::move(runtime->context());
    runtimeOwner = std::move(runtime);
    return true;
}

} // namespace media_transcode::test
