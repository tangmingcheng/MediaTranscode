#pragma once

#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <chrono>
#include <memory>

namespace media_transcode::test {

inline void addPlaybackEpochReleaseBoundary(
    ::media::ffmpeg::graph::MediaGraph& graph,
    ::media::ffmpeg::graph::MediaNodeId binderId)
{
    using namespace ::media::ffmpeg::graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump,
                                      "test.epoch.release.source");
    const auto sequencer = graph.addNode(
        MediaNodeKind::ActivatedStartupReleaseSequencer,
        "test.epoch.release.sequencer");
    const MediaNode* binder = graph.findNode(binderId);
    const std::string group = binder
        ? binder->options.value("playback_epoch_binder.sync_group")
        : std::string{};
    graph.setNodeOption(
        sequencer, "activated_startup_release_sequencer.sync_group", group);
    graph.setNodeOption(
        sequencer, "activated_startup_release_sequencer.output_lead_ns",
        "100000000");
    const auto activatedSink = graph.addNode(
        MediaNodeKind::DebugDump, "test.epoch.activated.sink");
    const auto releaseSink = graph.addNode(
        MediaNodeKind::DebugDump, "test.epoch.bound_release.sink");
    graph.addOutputPort(source, "release", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(binderId, "release", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(binderId, "transaction", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(sequencer, "transaction", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(sequencer, "activated", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(sequencer, "bound_release", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(activatedSink, "activated", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addInputPort(releaseSink, "release", MediaStreamKind::Metadata,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(2);
    graph.connect(source, "release", binderId, "release",
                  "test epoch release", policy);
    graph.connect(binderId, "transaction", sequencer, "transaction",
                  "test release transaction", policy);
    graph.connect(sequencer, "activated", activatedSink, "activated",
                  "test epoch activated", policy);
    graph.connect(sequencer, "bound_release", releaseSink, "release",
                  "test bound release", policy);
}

inline bool activateInitialThroughRelease(
    ::media::ffmpeg::graph::MediaGraphRuntime& runtime,
    ::media::ffmpeg::graph::MediaNodeId binderId,
    ::media::ffmpeg::graph::MediaAvSyncGroupKey groupKey,
    ::media::ffmpeg::graph::MediaPlaybackEpoch epoch,
    int audioSampleRate = 48'000)
{
    using namespace ::media::ffmpeg::graph;
    auto* binder = dynamic_cast<MediaPlaybackEpochBinderNode*>(
        runtime.scheduler().findNode(binderId));
    auto* input = runtime.context().findInputChannel(binderId, "release");
    MediaActivatedStartupReleaseSequencerNode* sequencer = nullptr;
    if (const MediaGraph* graph = runtime.graph()) {
        for (const MediaNode& node : graph->nodes()) {
            if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer) {
                sequencer = dynamic_cast<
                    MediaActivatedStartupReleaseSequencerNode*>(
                        runtime.scheduler().findNode(node.id));
                break;
            }
        }
    }
    if (!binder || !sequencer || !input) return false;
    const auto payload = [] {
        return makeMediaBufferRef<MediaAvStartupClockBuffer>(
            MediaRunningTime::fromNanoseconds(0));
    };
    auto release = MediaAvStartupReleaseBuffer::create(
        std::move(groupKey), MediaAvStartupReleaseKind::InitialAtomicRelease,
        epoch,
        {epoch.generation, epoch.sourceStart, epoch.masterRelease, 0,
         audioSampleRate},
        {{payload(), 0}}, {{payload(), 0}});
    if (!release || !input->push(release.value())) return false;
    const auto processed = binder->process(runtime.context());
    if (!processed ||
        processed.value().state != MediaNodeProcessState::Progress) {
        return false;
    }
    const auto sequenced = sequencer->process(runtime.context());
    return sequenced &&
           sequenced.value().state == MediaNodeProcessState::Progress;
}

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
    if (binding.assemblyMode !=
        MediaAvSyncBindingAssemblyMode::ComponentCore) {
        return false;
    }
    auto runtime = std::make_unique<MediaGraphRuntime>(
        std::make_shared<FixedAvSyncClockSource>(std::move(clock)));
    MediaRealtimeExecutableGraph executable;
    const auto groupKey = binding.groupKey;
    executable.graph = std::move(graph);
    addPlaybackEpochReleaseBoundary(executable.graph, binderId);
    executable.avSyncBinding.emplace(std::move(binding));
    if (!runtime->compile(std::move(executable)) ||
        !runtime->registerDefaultRuntimeNodes()) {
        return false;
    }
    if (!activateInitialThroughRelease(
            *runtime, binderId, groupKey, epoch, audioSampleRate)) {
        return false;
    }
    context = std::move(runtime->context());
    runtimeOwner = std::move(runtime);
    return true;
}

} // namespace media_transcode::test
