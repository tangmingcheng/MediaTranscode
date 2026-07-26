#include "common/GraphRuntimeTestSupport.h"
#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/builder/segments/MediaScheduledMpegTsOutputSegmentBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h"
#include "internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace media::ffmpeg::graph {

struct MediaAvEpochTransitionServiceTestAccess final {
    static ::media::Status activateInitial(
        const std::shared_ptr<MediaAvEpochTransitionService>& service,
        const MediaPlaybackEpoch& epoch)
    {
        return service->activateInitial(
            epoch,
            MediaAudioPlaybackOrigin{
                epoch.generation, epoch.sourceStart, epoch.masterRelease,
                0, 48'000});
    }

    static ::media::Status activateNext(
        const std::shared_ptr<MediaAvEpochTransitionService>& service,
        std::uint64_t transitionSequence,
        const MediaPlaybackEpoch& epoch)
    {
        return service->activateNextAfter(
            transitionSequence, epoch,
            MediaAudioPlaybackOrigin{
                epoch.generation, epoch.sourceStart, epoch.masterRelease,
                0, 48'000});
    }
};

} // namespace media::ffmpeg::graph

namespace {

class FixedMasterClock final : public MediaMasterClock {
public:
    explicit FixedMasterClock(MediaRunningTime now) : m_now(now) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(m_now);
    }

private:
    MediaRunningTime m_now;
};

void protocolOutputStatePreservesAuthorityAcrossRollover(TestContext& ctx)
{
    MediaProtocolOutputGenerationState state(
        "project_mpegts_output_generation_state");
    const auto* stableAuthority = &state;
    EXPECT_TRUE(ctx, state.permitActivatedGeneration(1, 0));
    EXPECT_TRUE(ctx, state.validateCommitGeneration(1));
    EXPECT_TRUE(ctx, state.purge(MediaAvGenerationPurge{1, 2, 1}));
    EXPECT_EQ(ctx, &state, stableAuthority);
    EXPECT_FALSE(ctx, state.validateCommitGeneration(1));
    EXPECT_FALSE(ctx, state.validateCommitGeneration(2));
    EXPECT_TRUE(ctx, state.permitActivatedGeneration(2, 1));
    EXPECT_TRUE(ctx, state.validateCommitGeneration(2));
}

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

MediaRealtimeRtpTranscodeRequest avSyncTsRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "scheduled-mpeg-ts-output-tests";
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout =
        RealtimeInputStreamLayout::MuxedTransportStream;
    request.output.streamLayout =
        RealtimeOutputStreamLayout::MuxedTransportStream;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 64;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = ms(40);
    return request;
}

MediaTsSelectedProgramPlan selectedTsProgram()
{
    return MediaTsSelectedProgramPlan{7, 777, 703, 705, 701};
}

MediaProjectMpegTsResolvedPipelineFacts validTsResolvedFacts()
{
    MediaResolvedAudioSource source{
        "aac", MediaAudioProfile::knownAacLow(), 48'000, 2,
        "stereo", "fltp", 128'000};
    MediaResolvedAudioRequest request;
    auto target = MediaResolvedAudioTargetDecision::create(source, request, {});
    auto audio = MediaResolvedAudioOutputPlan::create(
        target.value(), std::nullopt, 1'024);
    return MediaProjectMpegTsResolvedPipelineFacts{
        "h264", MediaEncodedPacketLayout::lengthPrefixed(4).value(),
        std::move(audio).value()};
}

MediaAvSyncPlan avSyncPlan()
{
    const auto selected = selectedTsProgram();
    const auto resolved = validTsResolvedFacts();
    auto plan = MediaAvSyncPlanner::plan(
        avSyncTsRequest(), &selected, &resolved).value();
    MediaRealtimeAvSyncPlanningFacts facts;
    facts.outputSampleRate = 48'000;
    facts.decoderDelaySamples = 0;
    facts.encoderLookaheadSamples = 0;
    facts.decodeQueueSamples = 1'024;
    facts.resampleQueueSamples = 1'024;
    facts.encodeQueueSamples = 1'024;
    facts.schedulerQueueSamples = 1'024;
    facts.protocolBatchSamples = 1'024;
    facts.mailboxDeliveryMarginSamples = 1'024;
    facts.maximumResamplerOutputBlockSamples = 1'024;
    facts.mailboxCapacity = 1;
    const auto correction =
        MediaAudioCorrectionReachabilityPlanner::plan(plan, facts).value();
    plan.audioServo.commandLeadNs = correction.commandLead;
    plan.audioServo.compensationWindowNs = correction.compensationWindow;
    plan.audioServo.frequencyFilterTimeConstantNs =
        correction.frequencyFilterTimeConstant;
    return plan;
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

struct RecordingTsSinkState final {
    std::vector<std::uint8_t> bytes;
    std::size_t closes = 0;
};

class RecordingTsSink final : public MediaOutputByteSink {
public:
    explicit RecordingTsSink(std::shared_ptr<RecordingTsSinkState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override
    {
        m_state->bytes.insert(m_state->bytes.end(), bytes.begin(), bytes.end());
        return ::media::Result<std::size_t>::success(bytes.size());
    }

    ::media::Status flush() override { return ::media::Status::success(); }
    ::media::Status close() override
    {
        ++m_state->closes;
        return ::media::Status::success();
    }

private:
    std::shared_ptr<RecordingTsSinkState> m_state;
};

MediaBufferRef muxCodecParameters(MediaStreamKind kind)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (kind == MediaStreamKind::Video) {
        static constexpr std::array<std::uint8_t, 17> avcc{
            1, 0x42, 0, 0x1E, 0xFF, 0xE1, 0, 4, 0x67, 0x42, 0, 0x1E,
            1, 0, 2, 0x68, 0xCE};
        parameters->codec_type = AVMEDIA_TYPE_VIDEO;
        parameters->codec_id = AV_CODEC_ID_H264;
        parameters->extradata = static_cast<std::uint8_t*>(
            av_mallocz(avcc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        parameters->extradata_size = static_cast<int>(avcc.size());
        std::memcpy(parameters->extradata, avcc.data(), avcc.size());
    } else {
        static constexpr std::array<std::uint8_t, 2> asc{0x11, 0x90};
        parameters->codec_type = AVMEDIA_TYPE_AUDIO;
        parameters->codec_id = AV_CODEC_ID_AAC;
        parameters->sample_rate = 48'000;
        parameters->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        parameters->extradata = static_cast<std::uint8_t*>(
            av_mallocz(asc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        parameters->extradata_size = static_cast<int>(asc.size());
        std::memcpy(parameters->extradata, asc.data(), asc.size());
    }
    auto buffer = makeMediaBufferRef<FFmpegCodecParametersBuffer>(
        std::move(parameters));
    buffer->setStreamKind(kind);
    return buffer;
}

MediaBufferRef muxAccessUnit(MediaScheduledStream stream,
                             std::uint64_t generation,
                             MediaRunningTime emission)
{
    auto packet = ::media::ffmpeg::makePacket();
    av_new_packet(packet.get(),
                  stream == MediaScheduledStream::Video ? 7 : 3);
    if (stream == MediaScheduledStream::Video) {
        const std::array<std::uint8_t, 7> payload{
            0, 0, 0, 3, 0x65, 1, 2};
        std::memcpy(packet->data, payload.data(), payload.size());
        packet->flags |= AV_PKT_FLAG_KEY;
    } else {
        const std::array<std::uint8_t, 3> payload{1, 2, 3};
        std::memcpy(packet->data, payload.data(), payload.size());
    }
    auto media = std::make_shared<FFmpegPacketBuffer>(
        std::move(packet), std::nullopt);
    media->setStreamKind(stream == MediaScheduledStream::Video
                             ? MediaStreamKind::Video
                             : MediaStreamKind::Audio);
    const auto dispatch = emission.checkedAdd(ms(100)).value();
    return MediaTsAccessUnitBuffer::create(
        media, stream, generation, dispatch, dispatch, emission,
        ms(100)).value();
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
    std::shared_ptr<MediaAvEpochTransitionService> transitionService;
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

    const auto metadata = MediaBlockingEdgePolicyPlanner::planQueue(1);
    const auto packet = MediaBlockingEdgePolicyPlanner::planQueue(4);
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
    if (!compiled) return f;
    auto transitionPlan = MediaAvGenerationTransitionPlanner::plan(
        MediaAvSyncOutputAdapterKind::ProjectMpegTs, ms(1'000), ms(1'000));
    auto transition =
        MediaAvEpochTransitionService::create(transitionPlan);
    EXPECT_TRUE(ctx, transition);
    if (!transition) return f;
    f.transitionService = transition.value();
    EXPECT_TRUE(ctx, f.execution.registerAvSyncGroup(
        f.group, avSyncPlan(), std::make_shared<FixedMasterClock>(ms(0)),
        std::shared_ptr<const MediaSharedNtpEpoch>{},
        f.transitionService));
    EXPECT_TRUE(ctx, MediaAvEpochTransitionServiceTestAccess::activateInitial(
        f.transitionService, f.epoch));
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

    auto awaitingNextGeneration = source.process(f.execution);
    EXPECT_TRUE(ctx, awaitingNextGeneration);
    if (awaitingNextGeneration) {
        EXPECT_EQ(ctx, awaitingNextGeneration.value().state,
                  MediaNodeProcessState::Waiting);
    }
    EXPECT_FALSE(ctx, muxPlanChannel->closed());
    EXPECT_FALSE(ctx, adapterPlanChannel->closed());
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

    const MediaPlaybackEpoch nextEpoch{ms(2'000), ms(3'000), 8};
    auto purge = f.transitionService->beginReacquisition(
        f.epoch.generation, nextEpoch.generation);
    EXPECT_TRUE(ctx, purge);
    if (!purge) {
        source.abort(f.execution);
        return;
    }
    EXPECT_TRUE(ctx, source.generationPurgeTarget()->purge(purge.value()));
    for (const auto& participant :
         f.transitionService->transitionPlan().participants) {
        EXPECT_TRUE(ctx, f.transitionService->acknowledge(
            MediaAvGenerationAcknowledgement{
                participant.participant, purge.value().transitionSequence,
                ::media::Status::success()}));
    }
    EXPECT_TRUE(ctx, MediaAvEpochTransitionServiceTestAccess::activateNext(
        f.transitionService, purge.value().transitionSequence, nextEpoch));
    EXPECT_TRUE(ctx, epoch->push(activation(f.group, nextEpoch)));
    auto republished = source.process(f.execution);
    EXPECT_TRUE(ctx, republished);
    if (republished) {
        EXPECT_EQ(ctx, republished.value().state,
                  MediaNodeProcessState::Progress);
    }
    EXPECT_EQ(ctx, muxPlanChannel->size(), std::size_t{1});
    EXPECT_EQ(ctx, adapterPlanChannel->size(), std::size_t{1});
    EXPECT_FALSE(ctx, muxPlanChannel->closed());
    EXPECT_FALSE(ctx, adapterPlanChannel->closed());
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

void muxGenerationRolloverPreservesSinkAndResetsTransportState(
    TestContext& ctx)
{
    auto f = assemblyFixture(ctx);
    if (!f.execution.compiled()) return;
    auto sinkState = std::make_shared<RecordingTsSinkState>();
    ProjectMpegTsMuxSessionAdapter adapter;
    EXPECT_TRUE(ctx, adapter.bindResource(
        f.execution,
        MediaTsMuxRuntimePlanBuffer::create(
            muxPlan(), f.epoch, f.group).value()));
    EXPECT_TRUE(ctx, adapter.bindStreamConfig(
        f.execution, muxCodecParameters(MediaStreamKind::Video)));
    EXPECT_TRUE(ctx, adapter.bindStreamConfig(
        f.execution, muxCodecParameters(MediaStreamKind::Audio)));
    auto sinkBuffer = MediaOutputByteSinkBuffer::create(
        std::make_unique<RecordingTsSink>(sinkState));
    EXPECT_TRUE(ctx, sinkBuffer);
    if (!sinkBuffer) return;
    EXPECT_TRUE(ctx, adapter.bindResource(
        f.execution, MediaBufferRef(std::move(sinkBuffer).value())));
    EXPECT_TRUE(ctx, adapter.write(
        f.execution,
        muxAccessUnit(MediaScheduledStream::Video, 7, ms(900))));
    EXPECT_TRUE(ctx, !sinkState->bytes.empty());
    if (sinkState->bytes.size() < 188) return;
    const auto firstGenerationPatContinuity =
        sinkState->bytes[3] & std::uint8_t{0x0F};

    const MediaPlaybackEpoch nextEpoch{ms(0), ms(1'000), 8};
    auto purge = f.transitionService->beginReacquisition(7, 8);
    EXPECT_TRUE(ctx, purge);
    if (!purge) return;
    EXPECT_TRUE(ctx,
                adapter.generationPurgeTarget()->purge(purge.value()));
    const auto bytesBeforeClosedPermitWrite = sinkState->bytes.size();
    EXPECT_TRUE(ctx, adapter.write(
        f.execution,
        muxAccessUnit(MediaScheduledStream::Video, 7, ms(920))));
    EXPECT_EQ(ctx, sinkState->bytes.size(), bytesBeforeClosedPermitWrite);
    for (const auto& participant :
         f.transitionService->transitionPlan().participants) {
        EXPECT_TRUE(ctx, f.transitionService->acknowledge(
            MediaAvGenerationAcknowledgement{
                participant.participant, purge.value().transitionSequence,
                ::media::Status::success()}));
    }
    EXPECT_TRUE(ctx, MediaAvEpochTransitionServiceTestAccess::activateNext(
        f.transitionService, purge.value().transitionSequence, nextEpoch));

    const auto secondGenerationOffset = sinkState->bytes.size();
    EXPECT_TRUE(ctx, adapter.bindResource(
        f.execution,
        MediaTsMuxRuntimePlanBuffer::create(
            muxPlan(), nextEpoch, f.group).value()));
    EXPECT_TRUE(ctx, adapter.write(
        f.execution,
        muxAccessUnit(MediaScheduledStream::Video, 8, ms(900))));
    EXPECT_TRUE(ctx, sinkState->bytes.size() >
                         secondGenerationOffset + 188);
    EXPECT_EQ(ctx, sinkState->closes, std::size_t{0});
    if (sinkState->bytes.size() > secondGenerationOffset + 3) {
        EXPECT_EQ(ctx, sinkState->bytes[secondGenerationOffset],
                  std::uint8_t{0x47});
        EXPECT_EQ(ctx,
                  sinkState->bytes[secondGenerationOffset + 3] &
                      std::uint8_t{0x0F},
                  firstGenerationPatContinuity);
    }
    EXPECT_TRUE(ctx, adapter.finish(f.execution));
    EXPECT_EQ(ctx, sinkState->closes, std::size_t{1});
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
        queues, MediaBlockingEdgePolicyPlanner::plan(queues),
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
    protocolOutputStatePreservesAuthorityAcrossRollover(ctx);
    muxBindingsReadinessPrecedesMedia(ctx);
    generationMismatchFailsAfterMuxReadiness(ctx);
    planFanoutBackpressurePublishesExactlyOnce(ctx);
    planSourceRejectsDuplicateActivationQueuedBeforeCommit(ctx);
    muxGenerationRolloverPreservesSinkAndResetsTransportState(ctx);
    adapterBackpressureTransfersOneAccessUnit(ctx);
    segmentBuildsCompleteAcyclicTopology(ctx);
    if (ctx.failures != 0) return 1;
    std::cout << "Scheduled MPEG-TS output node tests passed\n";
    return 0;
}
