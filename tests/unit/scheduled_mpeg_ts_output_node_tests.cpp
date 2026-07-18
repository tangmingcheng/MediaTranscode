#include "common/GraphRuntimeTestSupport.h"
#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h"
#include "internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaRunningTime ms(std::int64_t value)
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

MediaTsMuxPlan muxPlan()
{
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x100, 0x101, 0x102, 0x101, 0,
        ms(100), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, 4,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{ms(20), ms(100), ms(5), 1, 90'000},
        ms(100), 188, MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024}).value();
}

MediaBufferRef activation(const MediaAvSyncGroupKey& group,
                          const MediaPlaybackEpoch& epoch)
{
    return MediaPlaybackEpochActivatedBuffer::create(
        group, epoch,
        MediaAudioPlaybackOrigin{
            epoch.generation, epoch.sourceStart, epoch.masterRelease, 0,
            48'000}).value();
}

MediaBufferRef scheduled(TestContext& ctx,
                         MediaScheduledStream stream,
                         std::uint64_t generation,
                         std::uint64_t sequence,
                         MediaRunningTime presentation,
                         MediaRunningTime dispatch,
                         MediaRunningTime emit)
{
    auto nativePacket = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, nativePacket && av_new_packet(nativePacket.get(), 1) == 0);
    if (!nativePacket || !nativePacket->data) return {};
    nativePacket->data[0] = stream == MediaScheduledStream::Video ? 0x65 : 0x01;
    nativePacket->flags = stream == MediaScheduledStream::Video
        ? AV_PKT_FLAG_KEY : 0;
    nativePacket->pts = static_cast<std::int64_t>(sequence);
    nativePacket->dts = nativePacket->pts;
    nativePacket->duration = 1;
    auto packet = ::media::ffmpeg::graph::FFmpegBufferFactory::wrapPacket(
        std::move(nativePacket),
        stream == MediaScheduledStream::Video
            ? MediaStreamKind::Video : MediaStreamKind::Audio,
        std::nullopt);
    EXPECT_TRUE(ctx, packet);
    if (!packet) return {};
    auto created = MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            std::move(packet).value(), stream, presentation, dispatch,
            presentation, dispatch, emit, ms(10), generation,
            MediaSourceAccessUnitSequence(sequence), std::nullopt,
            std::nullopt,
            stream == MediaScheduledStream::Video
                ? std::optional(MediaVideoSyncDecisionKind::Display)
                : std::nullopt});
    EXPECT_TRUE(ctx, created);
    return created ? std::move(created).value() : MediaBufferRef{};
}

class BindingBuffer final : public MediaBuffer {
public:
    explicit BindingBuffer(MediaStreamKind kind,
                           MediaPayloadKind payload)
    {
        setStreamKind(kind);
        setPayloadKind(payload);
    }

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::Unknown;
    }
};

struct MuxState final {
    bool plan = false;
    bool sink = false;
    bool video = false;
    bool audio = false;
    std::size_t packetWrites = 0;
};

class BindingAwareMuxSession final : public MediaMuxSession {
public:
    explicit BindingAwareMuxSession(std::shared_ptr<MuxState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Status bindResource(MediaGraphExecutionContext&,
                                 const MediaBufferRef& buffer) override
    {
        if (dynamic_cast<const MediaTsMuxRuntimePlanBuffer*>(buffer.get())) {
            m_state->plan = true;
        } else {
            m_state->sink = true;
        }
        return ::media::Status::success();
    }

    ::media::Status bindStreamConfig(MediaGraphExecutionContext&,
                                     const MediaBufferRef& buffer) override
    {
        if (buffer->streamKind() == MediaStreamKind::Video) {
            m_state->video = true;
        } else if (buffer->streamKind() == MediaStreamKind::Audio) {
            m_state->audio = true;
        } else {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "test mux rejects unknown stream config"));
        }
        return ::media::Status::success();
    }

    ::media::Status write(MediaGraphExecutionContext&,
                          const MediaBufferRef& buffer) override
    {
        if (!bindingsReady() ||
            !dynamic_cast<const MediaTsAccessUnitBuffer*>(buffer.get())) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "test mux rejects packet before activation"));
        }
        ++m_state->packetWrites;
        return ::media::Status::success();
    }

    ::media::Result<MediaMuxSessionPollResult> poll(
        MediaGraphExecutionContext&) override
    {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }

    bool bindingsReady() const noexcept override
    {
        return m_state->plan && m_state->sink && m_state->video &&
               m_state->audio;
    }

    ::media::Status flush(MediaGraphExecutionContext&) override
    {
        return ::media::Status::success();
    }

    ::media::Status finish(MediaGraphExecutionContext&) override
    {
        return ::media::Status::success();
    }

    void abort() noexcept override {}

private:
    std::shared_ptr<MuxState> m_state;
};

class BindingAwareMuxFactory final : public MediaMuxSessionFactory {
public:
    explicit BindingAwareMuxFactory(std::shared_ptr<MuxState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Result<std::unique_ptr<MediaMuxSession>> create(
        const MediaNodeOptions&) const override
    {
        return ::media::Result<std::unique_ptr<MediaMuxSession>>::success(
            std::make_unique<BindingAwareMuxSession>(m_state));
    }

private:
    std::shared_ptr<MuxState> m_state;
};

struct AssemblyFixture final {
    MediaGraph graph;
    MediaNodeId epochSource;
    MediaNodeId sinkSource;
    MediaNodeId videoCodecSource;
    MediaNodeId audioCodecSource;
    MediaNodeId scheduledSource;
    MediaNodeId planSource;
    MediaNodeId mux;
    MediaNodeId adapter;
    MediaGraphExecutionContext execution;
    MediaAvSyncGroupKey group{"task9-group"};
    MediaPlaybackEpoch epoch{ms(0), ms(1'000), 7};
};

AssemblyFixture assemblyFixture(TestContext& ctx)
{
    AssemblyFixture f;
    f.epochSource = f.graph.addNode(MediaNodeKind::DebugDump, "epoch-source");
    f.sinkSource = f.graph.addNode(MediaNodeKind::DebugDump, "sink-source");
    f.videoCodecSource = f.graph.addNode(
        MediaNodeKind::DebugDump, "video-codec-source");
    f.audioCodecSource = f.graph.addNode(
        MediaNodeKind::DebugDump, "audio-codec-source");
    f.scheduledSource = f.graph.addNode(
        MediaNodeKind::DebugDump, "scheduled-source");
    f.planSource = f.graph.addNode(
        MediaNodeKind::ProjectMpegTsPlanSource, "plan-source");
    f.mux = f.graph.addNode(MediaNodeKind::FileMux, "project-mpeg-ts-mux");
    f.adapter = f.graph.addNode(
        MediaNodeKind::ScheduledTsAccessUnitAdapter, "scheduled-adapter");

    f.graph.addOutputPort(f.epochSource, "epoch", MediaStreamKind::Metadata,
                          MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    f.graph.addOutputPort(f.sinkSource, "resource", MediaStreamKind::Metadata,
                          MediaEdgeKind::Metadata,
                          MediaPayloadKind::OutputByteSink);
    f.graph.addOutputPort(f.videoCodecSource, "codec", MediaStreamKind::Video,
                          MediaEdgeKind::Metadata,
                          MediaPayloadKind::CodecParameters);
    f.graph.addOutputPort(f.audioCodecSource, "codec", MediaStreamKind::Audio,
                          MediaEdgeKind::Metadata,
                          MediaPayloadKind::CodecParameters);
    f.graph.addOutputPort(f.scheduledSource, "scheduled", MediaStreamKind::Any,
                          MediaEdgeKind::EncodedPacket,
                          MediaPayloadKind::Packet);

    f.graph.addInputPort(f.planSource, "epoch", MediaStreamKind::Metadata,
                         MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    f.graph.addOutputPort(f.planSource, "plan", MediaStreamKind::Metadata,
                          MediaEdgeKind::Metadata,
                          MediaPayloadKind::TsMuxRuntimePlan, true, true);
    f.graph.addInputPort(f.mux, "plan", MediaStreamKind::Metadata,
                         MediaEdgeKind::Metadata,
                         MediaPayloadKind::TsMuxRuntimePlan);
    f.graph.addInputPort(f.mux, "resource", MediaStreamKind::Metadata,
                         MediaEdgeKind::Metadata,
                         MediaPayloadKind::OutputByteSink);
    f.graph.addInputPort(f.mux, "codec", MediaStreamKind::Any,
                         MediaEdgeKind::Metadata,
                         MediaPayloadKind::CodecParameters, true, true);
    f.graph.addInputPort(f.mux, "packet", MediaStreamKind::Any,
                         MediaEdgeKind::EncodedPacket,
                         MediaPayloadKind::TsAccessUnit);
    f.graph.addInputPort(f.adapter, "plan", MediaStreamKind::Metadata,
                         MediaEdgeKind::Metadata,
                         MediaPayloadKind::TsMuxRuntimePlan);
    f.graph.addInputPort(f.adapter, "scheduled", MediaStreamKind::Any,
                         MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    f.graph.addOutputPort(f.adapter, "packet", MediaStreamKind::Any,
                          MediaEdgeKind::EncodedPacket,
                          MediaPayloadKind::TsAccessUnit);

    const auto metadata = MediaGraphBuildSupport::blockingQueuePolicy(1);
    const auto packet = MediaGraphBuildSupport::blockingQueuePolicy(4);
    f.graph.connect(f.epochSource, "epoch", f.planSource, "epoch", "epoch",
                    metadata);
    f.graph.connect(f.planSource, "plan", f.mux, "plan", "mux plan", metadata);
    f.graph.connect(f.planSource, "plan", f.adapter, "plan", "adapter plan",
                    metadata);
    f.graph.connect(f.sinkSource, "resource", f.mux, "resource", "mux sink",
                    metadata);
    f.graph.connect(f.videoCodecSource, "codec", f.mux, "codec", "video codec",
                    metadata);
    f.graph.connect(f.audioCodecSource, "codec", f.mux, "codec", "audio codec",
                    metadata);
    f.graph.connect(f.scheduledSource, "scheduled", f.adapter, "scheduled",
                    "scheduled", packet);
    f.graph.connect(f.adapter, "packet", f.mux, "packet", "mux packet", packet);
    const auto compiled = f.execution.compile(f.graph);
    if (!compiled) {
        std::cerr << "scheduled MPEG-TS graph compile failed: "
                  << compiled.error().message << '\n';
    }
    EXPECT_TRUE(ctx, compiled);
    return f;
}

void expectBlocked(TestContext& ctx,
                   AssemblyFixture& f,
                   MediaScheduledTsAccessUnitAdapterNode& adapter,
                   const std::shared_ptr<MuxState>& state)
{
    auto waiting = adapter.process(f.execution);
    EXPECT_TRUE(ctx, waiting &&
        waiting.value().state == MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, f.execution.findInputChannel(f.adapter, "scheduled")->size(),
              std::size_t{1});
    EXPECT_EQ(ctx, state->packetWrites, std::size_t{0});
}

void muxBindingsReadinessPrecedesMedia(TestContext& ctx)
{
    auto f = assemblyFixture(ctx);
    if (!f.execution.compiled()) return;
    auto state = std::make_shared<MuxState>();
    MediaProjectMpegTsPlanSourceNode source(f.planSource, f.group, muxPlan());
    FileMuxNode mux(f.mux, std::make_unique<BindingAwareMuxFactory>(state));
    MediaScheduledTsAccessUnitAdapterNode adapter(f.adapter, f.group);
    EXPECT_TRUE(ctx, source.start(f.execution));
    EXPECT_TRUE(ctx, mux.start(f.execution));
    EXPECT_TRUE(ctx, adapter.start(f.execution));

    EXPECT_TRUE(ctx, f.execution.findInputChannel(f.planSource, "epoch")->push(
        activation(f.group, f.epoch)));
    EXPECT_TRUE(ctx, source.process(f.execution));
    EXPECT_TRUE(ctx, adapter.process(f.execution));
    EXPECT_TRUE(ctx, f.execution.findInputChannel(f.adapter, "scheduled")->push(
        scheduled(ctx, MediaScheduledStream::Video, 7, 1, ms(1'140),
                  ms(1'120), ms(1'020))));
    EXPECT_TRUE(ctx, adapter.process(f.execution));
    EXPECT_EQ(ctx, f.execution.findInputChannel(f.mux, "packet")->size(),
              std::size_t{1});
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, state->plan);
    EXPECT_EQ(ctx, f.execution.findInputChannel(f.mux, "packet")->size(),
              std::size_t{1});
    EXPECT_EQ(ctx, state->packetWrites, std::size_t{0});

    EXPECT_TRUE(ctx, f.execution.findInputChannel(f.mux, "resource")->push(
        makeMediaBufferRef<BindingBuffer>(
            MediaStreamKind::Metadata, MediaPayloadKind::OutputByteSink)));
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, state->sink);
    EXPECT_EQ(ctx, f.execution.findInputChannel(f.mux, "packet")->size(),
              std::size_t{1});
    EXPECT_EQ(ctx, state->packetWrites, std::size_t{0});

    EXPECT_TRUE(ctx, f.execution.findOutputChannel(
        f.videoCodecSource, "codec")->push(
        makeMediaBufferRef<BindingBuffer>(
            MediaStreamKind::Video, MediaPayloadKind::CodecParameters)));
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, state->video);
    EXPECT_EQ(ctx, f.execution.findInputChannel(f.mux, "packet")->size(),
              std::size_t{1});
    EXPECT_EQ(ctx, state->packetWrites, std::size_t{0});

    EXPECT_TRUE(ctx, f.execution.findOutputChannel(
        f.audioCodecSource, "codec")->push(
        makeMediaBufferRef<BindingBuffer>(
            MediaStreamKind::Audio, MediaPayloadKind::CodecParameters)));
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, state->audio);
    EXPECT_EQ(ctx, f.execution.findInputChannel(f.mux, "packet")->size(),
              std::size_t{1});
    EXPECT_EQ(ctx, state->packetWrites, std::size_t{0});
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_EQ(ctx, state->packetWrites, std::size_t{1});
    EXPECT_EQ(ctx, f.execution.findInputChannel(f.adapter, "scheduled")->size(),
              std::size_t{0});

    EXPECT_TRUE(ctx, adapter.stop(f.execution));
    mux.abort(f.execution);
    EXPECT_TRUE(ctx, source.stop(f.execution));
}

void generationMismatchFailsAfterMuxReadiness(TestContext& ctx)
{
    auto f = assemblyFixture(ctx);
    if (!f.execution.compiled()) return;
    auto state = std::make_shared<MuxState>();
    MediaProjectMpegTsPlanSourceNode source(f.planSource, f.group, muxPlan());
    FileMuxNode mux(f.mux, std::make_unique<BindingAwareMuxFactory>(state));
    MediaScheduledTsAccessUnitAdapterNode adapter(f.adapter, f.group);
    EXPECT_TRUE(ctx, source.start(f.execution));
    EXPECT_TRUE(ctx, mux.start(f.execution));
    EXPECT_TRUE(ctx, adapter.start(f.execution));
    EXPECT_TRUE(ctx, f.execution.findInputChannel(f.planSource, "epoch")->push(
        activation(f.group, f.epoch)));
    EXPECT_TRUE(ctx, source.process(f.execution));
    EXPECT_TRUE(ctx, adapter.process(f.execution));
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, f.execution.findInputChannel(f.mux, "resource")->push(
        makeMediaBufferRef<BindingBuffer>(
            MediaStreamKind::Metadata, MediaPayloadKind::OutputByteSink)));
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, f.execution.findOutputChannel(
        f.videoCodecSource, "codec")->push(
        makeMediaBufferRef<BindingBuffer>(
            MediaStreamKind::Video, MediaPayloadKind::CodecParameters)));
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, f.execution.findOutputChannel(
        f.audioCodecSource, "codec")->push(
        makeMediaBufferRef<BindingBuffer>(
            MediaStreamKind::Audio, MediaPayloadKind::CodecParameters)));
    EXPECT_TRUE(ctx, mux.process(f.execution));
    EXPECT_TRUE(ctx, f.execution.findInputChannel(f.adapter, "scheduled")->push(
        scheduled(ctx, MediaScheduledStream::Audio, 8, 1, ms(1'140),
                  ms(1'120), ms(1'020))));
    EXPECT_FALSE(ctx, adapter.process(f.execution));
    EXPECT_EQ(ctx, state->packetWrites, std::size_t{0});
    adapter.abort(f.execution);
    mux.abort(f.execution);
    source.abort(f.execution);
}

void planFanoutBackpressurePublishesExactlyOnce(TestContext& ctx)
{
    auto f = assemblyFixture(ctx);
    if (!f.execution.compiled()) return;
    MediaProjectMpegTsPlanSourceNode source(f.planSource, f.group, muxPlan());
    EXPECT_TRUE(ctx, source.start(f.execution));
    auto* epoch = f.execution.findInputChannel(f.planSource, "epoch");
    auto* muxPlanChannel = f.execution.findInputChannel(f.mux, "plan");
    auto* adapterPlanChannel = f.execution.findInputChannel(f.adapter, "plan");
    EXPECT_TRUE(ctx, epoch != nullptr && muxPlanChannel != nullptr &&
                     adapterPlanChannel != nullptr);
    if (!epoch || !muxPlanChannel || !adapterPlanChannel) return;
    auto occupied = MediaTsMuxRuntimePlanBuffer::create(
        muxPlan(), f.epoch, f.group).value();
    EXPECT_TRUE(ctx, muxPlanChannel->push(occupied));
    EXPECT_TRUE(ctx, epoch->push(activation(f.group, f.epoch)));

    auto blocked = source.process(f.execution);
    EXPECT_TRUE(ctx, blocked);
    if (blocked) {
        EXPECT_EQ(ctx, blocked.value().state, MediaNodeProcessState::Waiting);
    }
    MediaBufferRef removed;
    EXPECT_TRUE(ctx, muxPlanChannel->tryPop(removed));
    EXPECT_TRUE(ctx, removed == occupied);
    auto resumed = source.process(f.execution);
    EXPECT_TRUE(ctx, resumed);
    if (resumed) {
        EXPECT_EQ(ctx, resumed.value().state, MediaNodeProcessState::Progress);
    }
    EXPECT_EQ(ctx, muxPlanChannel->size(), std::size_t{1});
    EXPECT_EQ(ctx, adapterPlanChannel->size(), std::size_t{1});
    EXPECT_FALSE(ctx, muxPlanChannel->closed());
    EXPECT_FALSE(ctx, adapterPlanChannel->closed());

    auto finished = source.process(f.execution);
    EXPECT_TRUE(ctx, finished);
    if (finished) {
        EXPECT_EQ(ctx, finished.value().state, MediaNodeProcessState::Finished);
    }
    EXPECT_TRUE(ctx, muxPlanChannel->closed());
    EXPECT_TRUE(ctx, adapterPlanChannel->closed());
    EXPECT_EQ(ctx, muxPlanChannel->size(), std::size_t{1});
    EXPECT_EQ(ctx, adapterPlanChannel->size(), std::size_t{1});

    MediaBufferRef muxPublished;
    MediaBufferRef adapterPublished;
    EXPECT_TRUE(ctx, muxPlanChannel->tryPop(muxPublished));
    EXPECT_TRUE(ctx, adapterPlanChannel->tryPop(adapterPublished));
    EXPECT_TRUE(ctx, muxPublished == adapterPublished);
    EXPECT_TRUE(ctx, muxPublished != occupied);
    EXPECT_EQ(ctx, muxPlanChannel->size(), std::size_t{0});
    EXPECT_EQ(ctx, adapterPlanChannel->size(), std::size_t{0});
    source.abort(f.execution);
}

void planSourceRejectsDuplicateActivationQueuedBeforeCommit(TestContext& ctx)
{
    auto f = assemblyFixture(ctx);
    if (!f.execution.compiled()) return;
    MediaProjectMpegTsPlanSourceNode source(f.planSource, f.group, muxPlan());
    EXPECT_TRUE(ctx, source.start(f.execution));
    auto* epoch = f.execution.findInputChannel(f.planSource, "epoch");
    auto* muxPlanChannel = f.execution.findInputChannel(f.mux, "plan");
    EXPECT_TRUE(ctx, epoch != nullptr && muxPlanChannel != nullptr);
    if (!epoch || !muxPlanChannel) return;

    auto occupied = MediaTsMuxRuntimePlanBuffer::create(
        muxPlan(), f.epoch, f.group).value();
    EXPECT_TRUE(ctx, muxPlanChannel->push(occupied));
    EXPECT_TRUE(ctx, epoch->push(activation(f.group, f.epoch)));
    EXPECT_TRUE(ctx, source.process(f.execution));
    EXPECT_TRUE(ctx, epoch->push(activation(f.group, f.epoch)));

    MediaBufferRef removed;
    EXPECT_TRUE(ctx, muxPlanChannel->tryPop(removed));
    EXPECT_TRUE(ctx, source.process(f.execution));
    auto duplicate = source.process(f.execution);
    EXPECT_FALSE(ctx, duplicate);
    if (!duplicate) {
        EXPECT_TRUE(ctx, duplicate.error().message.find(
                             "rejects duplicate activation") !=
                             std::string::npos);
    }
    EXPECT_FALSE(ctx, muxPlanChannel->closed());
    source.abort(f.execution);
}

void adapterBackpressureTransfersOneAccessUnit(TestContext& ctx)
{
    auto f = assemblyFixture(ctx);
    if (!f.execution.compiled()) return;
    MediaScheduledTsAccessUnitAdapterNode adapter(f.adapter, f.group);
    EXPECT_TRUE(ctx, adapter.start(f.execution));
    auto* plan = f.execution.findInputChannel(f.adapter, "plan");
    auto* scheduledInput = f.execution.findInputChannel(f.adapter, "scheduled");
    auto* packet = f.execution.findInputChannel(f.mux, "packet");
    EXPECT_TRUE(ctx, plan != nullptr && scheduledInput != nullptr && packet != nullptr);
    if (!plan || !scheduledInput || !packet) return;
    EXPECT_TRUE(ctx, plan->push(MediaTsMuxRuntimePlanBuffer::create(
        muxPlan(), f.epoch, f.group).value()));
    EXPECT_TRUE(ctx, adapter.process(f.execution));
    for (std::size_t index = 0; index < 4; ++index) {
        EXPECT_TRUE(ctx, packet->push(makeMediaBufferRef<BindingBuffer>(
            MediaStreamKind::Video, MediaPayloadKind::TsAccessUnit)));
    }
    EXPECT_TRUE(ctx, scheduledInput->push(scheduled(
        ctx, MediaScheduledStream::Video, 7, 99,
        ms(1'140), ms(1'120), ms(1'020))));
    auto blocked = adapter.process(f.execution);
    EXPECT_TRUE(ctx, blocked);
    if (blocked) {
        EXPECT_EQ(ctx, blocked.value().state, MediaNodeProcessState::Waiting);
    }
    EXPECT_EQ(ctx, scheduledInput->size(), std::size_t{0});
    EXPECT_EQ(ctx, packet->size(), std::size_t{4});
    MediaBufferRef filler;
    EXPECT_TRUE(ctx, packet->tryPop(filler));
    auto resumed = adapter.process(f.execution);
    EXPECT_TRUE(ctx, resumed);
    if (resumed) {
        EXPECT_EQ(ctx, resumed.value().state, MediaNodeProcessState::Progress);
    }
    EXPECT_EQ(ctx, packet->size(), std::size_t{4});
    for (std::size_t index = 0; index < 3; ++index) {
        EXPECT_TRUE(ctx, packet->tryPop(filler));
        EXPECT_TRUE(ctx,
                    dynamic_cast<MediaTsAccessUnitBuffer*>(filler.get()) == nullptr);
    }
    MediaBufferRef transferred;
    EXPECT_TRUE(ctx, packet->tryPop(transferred));
    auto* accessUnit = dynamic_cast<MediaTsAccessUnitBuffer*>(transferred.get());
    EXPECT_TRUE(ctx, accessUnit != nullptr);
    if (accessUnit) {
        auto view = accessUnit->view();
        EXPECT_TRUE(ctx, view);
        if (view) {
            EXPECT_EQ(ctx, view.value().generation, std::uint64_t{7});
            EXPECT_EQ(ctx, view.value().emitOnMaster, ms(1'020));
        }
    }
    EXPECT_EQ(ctx, packet->size(), std::size_t{0});
    adapter.abort(f.execution);
}

void segmentBuildsCompleteAcyclicTopology(TestContext& ctx)
{
    MediaGraph graph;
    const auto epoch = graph.addNode(MediaNodeKind::DebugDump, "epoch");
    const auto videoCodec = graph.addNode(MediaNodeKind::DebugDump, "video-codec");
    const auto audioCodec = graph.addNode(MediaNodeKind::DebugDump, "audio-codec");
    const auto scheduledSource = graph.addNode(
        MediaNodeKind::DebugDump, "scheduled");
    graph.addOutputPort(epoch, "epoch", MediaStreamKind::Metadata,
                        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
    graph.addOutputPort(videoCodec, "codec", MediaStreamKind::Video,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(audioCodec, "codec", MediaStreamKind::Audio,
                        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(scheduledSource, "serialized", MediaStreamKind::Any,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    MediaGraphQueueParameters queues;
    queues.metadata = 4;
    queues.packet = 4;
    queues.frame = 4;
    queues.mux = 4;
    MediaRealtimeAvSyncAssemblyPlan assembly{
        MediaMpegTsInputClockAssemblyPlan{},
        MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange,
        MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire,
        {"video", MediaPacketDurationPlan{true},
         MediaDecodeOrderMode::ReorderedRequiresDecodeTime, 4, ms(1'000)},
        {"audio", MediaPacketDurationPlan{true},
         MediaDecodeOrderMode::PresentationOrderNoReorder, 4, ms(1'000)},
        ms(20)};
    MediaRealtimeAvSyncRuntimePlan plan{
        MediaAvSyncGroupKey("segment-group"), {}, std::move(assembly),
        MediaAvSyncOutputAdapterKind::ProjectMpegTs,
        MediaProjectMpegTsRuntimeOutputPlan{
            "udp://127.0.0.1:7000", MediaOutputResourceKind::ByteSink,
            MediaMuxSessionKind::ProjectMpegTs,
            MediaProjectMpegTsOutputPlan::accept(48'000, muxPlan()).value()},
        queues, MediaGraphBuildSupport::blockingEdgePolicySet(queues),
        {}, {{}, ms(1'000), ms(500)}, {}, {}};
    auto built = MediaScheduledMpegTsOutputSegmentBuilder::build(
        graph,
        MediaScheduledMpegTsOutputSegmentOptions{
            "scheduled.ts", {epoch, "epoch"}, {videoCodec, "codec"},
            {audioCodec, "codec"}, {scheduledSource, "serialized"}},
        plan);
    EXPECT_TRUE(ctx, built);
    if (!built) return;
    EXPECT_TRUE(ctx, MediaGraphValidation::validate(graph).ok());
    EXPECT_TRUE(ctx, graph.findNode(built.value().mux)->outputPorts.empty());
    const auto planFanout = std::count_if(
        graph.edges().begin(), graph.edges().end(),
        [&](const MediaEdge& edge) {
            return edge.from.nodeId == built.value().planSource;
        });
    EXPECT_EQ(ctx, planFanout, std::size_t{2});
    const MediaNode* planNode = graph.findNode(built.value().planSource);
    const MediaNode* adapterNode = graph.findNode(built.value().adapter);
    EXPECT_TRUE(ctx, planNode != nullptr && adapterNode != nullptr);
    if (!planNode || !adapterNode) return;
    auto decoded = MediaProjectMpegTsPlanSourceNodePlanCodec::decode(*planNode);
    EXPECT_TRUE(ctx, decoded);
    if (decoded) {
        EXPECT_EQ(ctx, decoded.value().groupKey,
                  MediaAvSyncGroupKey("segment-group"));
        EXPECT_EQ(ctx, decoded.value().muxPlan.parameters(),
                  muxPlan().parameters());
    }
    EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::create(*planNode));
    EXPECT_TRUE(ctx, MediaRuntimeNodeFactory::create(*adapterNode));
}

} // namespace

int main()
{
    TestContext ctx;
    muxBindingsReadinessPrecedesMedia(ctx);
    generationMismatchFailsAfterMuxReadiness(ctx);
    planFanoutBackpressurePublishesExactlyOnce(ctx);
    planSourceRejectsDuplicateActivationQueuedBeforeCommit(ctx);
    adapterBackpressureTransfersOneAccessUnit(ctx);
    segmentBuildsCompleteAcyclicTopology(ctx);
    if (ctx.failures != 0) return 1;
    std::cout << "Scheduled MPEG-TS output node tests passed\n";
    return 0;
}
