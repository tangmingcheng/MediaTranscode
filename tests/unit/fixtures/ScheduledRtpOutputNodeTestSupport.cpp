#include "unit/fixtures/ScheduledRtpOutputNodeTestSupport.h"

#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"
#include "internal/graph/protocol/sdp/MediaAacLatmSdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaH264SdpCodecDescriptionFactory.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <optional>
#include <utility>

namespace media_transcode::test::scheduled_rtp_output {

using namespace rtp_udp;

namespace {

class FakePacketizerSession final : public ScheduledRtpPacketizerSession {
public:
    FakePacketizerSession(
        std::shared_ptr<PacketizerState> state,
        ScheduledRtpRewrittenDatagramSink sink)
        : m_state(std::move(state)), m_sink(std::move(sink))
    {
    }

    ::media::Status open() override
    {
        ++m_state->openCalls;
        return m_state->failOpen
            ? ::media::Status::failure(
                  ::media::ErrorInfo::ioFailure(
                      "scripted packetizer open", -1))
            : ::media::Status::success();
    }

    ::media::Status writeAccessUnit(
        const AVPacket&,
        MediaRtpTimestamp timestamp) override
    {
        ++m_state->writeCalls;
        m_state->timestamps.push_back(timestamp);
        const std::vector<std::uint8_t> datagram(16, 0x11);
        return m_sink(datagram, 4);
    }

private:
    std::shared_ptr<PacketizerState> m_state;
    ScheduledRtpRewrittenDatagramSink m_sink;
};

} // namespace

TestMasterClock::TestMasterClock(MediaRunningTime now)
    : m_now(now.nanoseconds())
{
}

::media::Result<MediaRunningTime> TestMasterClock::now() const noexcept
{
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(m_now.load()));
}

void TestMasterClock::set(MediaRunningTime now) noexcept
{
    m_now.store(now.nanoseconds());
}

MediaRealtimeRtpTranscodeRequest completeRequest()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "task8-production-output";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.openTimeoutMs = 5'000;
    request.input.readTimeoutMs = 5'000;
    request.input.analyzeDurationUs = 500'000;
    request.input.probeSizeBytes = 512 * 1024;
    request.input.lowLatency = true;
    request.input.videoRtp.url = "rtp://127.0.0.1:5004";
    request.input.videoRtp.codecName = "h264";
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032";
    request.input.audioRtp.url = "rtp://127.0.0.1:5006";
    request.input.audioRtp.codecName = "aac";
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.input.audioRtp.channels = 2;
    request.input.audioRtp.bitrateKbps = 320;
    request.input.audioRtp.fmtp =
        "profile-level-id=1;mode=AAC-hbr;config=1190;sizelength=13;indexlength=3;indexdeltalength=3";
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.output.host = "127.0.0.1";
    request.output.basePort = 6000;
    request.output.sdpPath = "task8.sdp";
    request.output.packetSize = 1200;
    request.parameters.execution.includeAudio = true;
    request.parameters.execution.disableHardware = true;
    request.parameters.video.codecName = "h264";
    request.parameters.video.bitrateKbps = 8'000;
    request.parameters.audio.codecName = "aac";
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.audio.bitrateKbps = 320;
    request.parameters.audio.channels = 2;
    request.parameters.queues.metadata = 8;
    request.parameters.queues.packet = 8;
    request.parameters.queues.frame = 8;
    request.parameters.queues.mux = 8;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = milliseconds(40);
    return request;
}

ActiveGroupFixture activeGroup(
    TestContext& ctx,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    ActiveGroupFixture fixture;
    fixture.clock = std::make_shared<TestMasterClock>(milliseconds(0));
    auto runtime = std::make_unique<MediaGraphRuntime>(
        std::make_shared<FixedAvSyncClockSource>(fixture.clock));
    MediaGraph graph;
    const MediaNodeId video = graph.addNode(MediaNodeKind::DebugDump, "video");
    const MediaNodeId audio = graph.addNode(MediaNodeKind::DebugDump, "audio");
    const MediaNodeId scheduler = graph.addNode(
        MediaNodeKind::AvOutputScheduler, "scheduler");
    const MediaNodeId binder = graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "binder");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "sink");
    graph.setNodeOption(
        scheduler, "av_scheduler.sync_group", plan.groupKey.value());
    graph.setNodeOption(
        binder, "playback_epoch_binder.sync_group", plan.groupKey.value());
    graph.addOutputPort(
        video, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(
        audio, "packet", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(
        scheduler, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(
        scheduler, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(
        scheduler, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    graph.addInputPort(
        sink, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
    graph.connect(video, "packet", scheduler, "video", "video", policy);
    graph.connect(audio, "packet", scheduler, "audio", "audio", policy);
    graph.connect(
        scheduler, "scheduled", sink, "scheduled", "scheduled", policy);
    addPlaybackEpochReleaseBoundary(graph, binder);

    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graph);
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        plan.groupKey, plan.synchronization, plan.transition,
        MediaAvSyncBindingAssemblyMode::ComponentCore});
    EXPECT_TRUE(ctx, runtime->compile(std::move(executable)));
    EXPECT_TRUE(ctx, runtime->registerDefaultRuntimeNodes());
    EXPECT_TRUE(ctx, activateInitialThroughRelease(
                         *runtime, binder, plan.groupKey,
                         {milliseconds(0), milliseconds(0), 1}));
    fixture.group = runtime->context().findAvSyncGroup(plan.groupKey);
    EXPECT_TRUE(ctx, fixture.group != nullptr);
    fixture.runtime = std::move(runtime);
    return fixture;
}

FakePacketizerFactory::FakePacketizerFactory(
    std::shared_ptr<PacketizerState> state)
    : m_state(std::move(state))
{
}

::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>
FakePacketizerFactory::create(
    ScheduledRtpMuxStreamConfig config,
    ScheduledRtpRewrittenDatagramSink sink)
{
    ++m_state->createCalls;
    m_state->stream = config.streamKind();
    m_state->mode = config.packetizationMode();
    m_state->payloadType = config.identity().payloadType();
    m_state->ssrc = config.identity().ssrc();
    std::unique_ptr<ScheduledRtpPacketizerSession> session =
        std::make_unique<FakePacketizerSession>(m_state, std::move(sink));
    return ::media::Result<
        std::unique_ptr<ScheduledRtpPacketizerSession>>::success(
        std::move(session));
}

::media::ffmpeg::CodecContextPtr codecContext(MediaScheduledStream stream)
{
    auto context = ::media::ffmpeg::makeCodecContext(nullptr);
    if (!context) return {};
    const bool video = stream == MediaScheduledStream::Video;
    context->codec_type = video ? AVMEDIA_TYPE_VIDEO : AVMEDIA_TYPE_AUDIO;
    context->codec_id = video ? AV_CODEC_ID_H264 : AV_CODEC_ID_AAC;
    context->time_base = video
        ? AVRational{1, 90'000}
        : AVRational{1, 48'000};
    const std::vector<std::uint8_t> extradata = video
        ? std::vector<std::uint8_t>{
              1, 0x64, 0x00, 0x1f, 0xff, 0xe1,
              0x00, 0x08, 0x67, 0x64, 0x00, 0x1f, 0xac, 0xd9, 0x40, 0x50,
              0x01, 0x00, 0x04, 0x68, 0xee, 0x3c, 0x80,
              0xfd, 0xf8, 0xf8, 0x00}
        : std::vector<std::uint8_t>{0x11, 0x90};
    context->extradata = static_cast<std::uint8_t*>(
        av_mallocz(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!context->extradata) return {};
    std::copy(extradata.begin(), extradata.end(), context->extradata);
    context->extradata_size = static_cast<int>(extradata.size());
    if (video) {
        context->width = 1920;
        context->height = 1080;
    } else {
        context->sample_rate = 48'000;
        context->frame_size = 1024;
        av_channel_layout_default(&context->ch_layout, 2);
    }
    return context;
}

::media::Result<MediaBufferRef> description(
    MediaScheduledStream stream,
    std::uint64_t generation,
    std::uint64_t sessionVersion)
{
    auto session = MediaSdpSessionIdentity::create(
        "task8", 0x1020304050607080ULL, sessionVersion, "Task8 Session",
        MediaIpAddressFamily::Ipv4, "127.0.0.1", "task8@example");
    if (!session) {
        return ::media::Result<MediaBufferRef>::failure(session.error());
    }
    const bool video = stream == MediaScheduledStream::Video;
    auto context = codecContext(stream);
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!context || !parameters) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed(
                "Task8 description codec"));
    }
    const int copied = avcodec_parameters_from_context(
        parameters.get(), context.get());
    if (copied < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::ffmpegFailure(
                "Task8 description codec", copied));
    }
    auto identity = MediaRtpSdpMediaIdentity::create(
        video ? MediaSdpMediaKind::Video : MediaSdpMediaKind::Audio,
        MediaIpAddressFamily::Ipv4, "127.0.0.1", "127.0.0.1",
        video ? 6'000 : 6'002, video ? 6'001 : 6'003,
        static_cast<std::uint8_t>(video ? 96 : 97),
        video ? 0x11223344u : 0x55667788u,
        video ? 90'000 : 48'000, video ? 0 : 2);
    if (!identity) {
        return ::media::Result<MediaBufferRef>::failure(identity.error());
    }
    std::optional<MediaSdpCodecDescription> codec;
    if (video) {
        auto created = MediaH264SdpCodecDescriptionFactory::create(*parameters);
        if (!created) {
            return ::media::Result<MediaBufferRef>::failure(created.error());
        }
        codec.emplace(std::move(created).value());
    } else {
        auto created = MediaAacLatmSdpCodecDescriptionFactory::create(
            *parameters);
        if (!created) {
            return ::media::Result<MediaBufferRef>::failure(created.error());
        }
        codec.emplace(std::move(created).value());
    }
    auto media = MediaRtpSdpMediaDescription::create(
        std::move(identity).value(), std::move(*codec));
    if (!media) {
        return ::media::Result<MediaBufferRef>::failure(media.error());
    }
    return MediaRtpSenderDescriptionBuffer::create(
        stream, generation, std::move(session).value(),
        std::move(media).value());
}

::media::Result<MediaBufferRef> scheduledUnit(
    MediaScheduledStream stream,
    MediaRunningTime senderLead,
    std::uint64_t sequence)
{
    const MediaStreamKind streamKind = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    auto packet = makePacketBuffer(
        stream == MediaScheduledStream::Video,
        static_cast<std::int64_t>(sequence), streamKind);
    if (!packet) {
        return ::media::Result<MediaBufferRef>::failure(packet.error());
    }
    const MediaRunningTime presentation = milliseconds(100);
    auto dispatch = presentation.checkedSubtract(senderLead);
    if (!dispatch) {
        return ::media::Result<MediaBufferRef>::failure(dispatch.error());
    }
    return MediaScheduledAccessUnit::create(
        MediaScheduledAccessUnitParameters{
            std::move(packet).value(), stream, presentation, presentation,
            presentation, presentation, dispatch.value(), milliseconds(10), 1,
            MediaSourceAccessUnitSequence(sequence), std::nullopt,
            std::nullopt,
            stream == MediaScheduledStream::Video
                ? std::optional(MediaVideoSyncDecisionKind::Display)
                : std::nullopt});
}

SenderGraphFixture senderGraph(
    TestContext& ctx,
    MediaScheduledStream stream,
    std::size_t descriptionCapacity)
{
    SenderGraphFixture fixture;
    const MediaStreamKind streamKind = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    fixture.epochSource = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "epoch");
    fixture.codecSource = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "codec");
    fixture.scheduledSource = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "scheduled");
    fixture.sender = fixture.graph.addNode(
        MediaNodeKind::ScheduledRtpSender, "sender");
    fixture.descriptionSink = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "description");
    fixture.graph.addOutputPort(
        fixture.epochSource, "epoch", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    fixture.graph.addInputPort(
        fixture.sender, "epoch", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    fixture.graph.addOutputPort(
        fixture.codecSource, "codec", streamKind,
        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    fixture.graph.addInputPort(
        fixture.sender, "codec", streamKind,
        MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    fixture.graph.addOutputPort(
        fixture.scheduledSource, "scheduled", streamKind,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, false);
    fixture.graph.addInputPort(
        fixture.sender, "scheduled", streamKind,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, false);
    fixture.graph.addOutputPort(
        fixture.sender, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    fixture.graph.addInputPort(
        fixture.descriptionSink, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
    const auto descriptionPolicy =
        MediaGraphBuildSupport::blockingQueuePolicy(descriptionCapacity);
    fixture.graph.connect(
        fixture.epochSource, "epoch", fixture.sender, "epoch", "epoch",
        policy);
    fixture.graph.connect(
        fixture.codecSource, "codec", fixture.sender, "codec", "codec",
        policy);
    fixture.graph.connect(
        fixture.scheduledSource, "scheduled", fixture.sender, "scheduled",
        "scheduled", policy);
    fixture.graph.connect(
        fixture.sender, "description", fixture.descriptionSink,
        "description", "description", descriptionPolicy);
    EXPECT_TRUE(ctx, fixture.execution.compile(fixture.graph));
    return fixture;
}

::media::Result<MediaDecodedScheduledRtpSenderNodePlan> cloneSenderPlan(
    const MediaRealtimeAvSyncRuntimePlan& plan,
    MediaScheduledStream stream)
{
    const auto& separate = std::get<MediaSeparateRtpOutputRuntimePlan>(
        plan.protocolOutput);
    const MediaScheduledRtpOutputPlan& output =
        stream == MediaScheduledStream::Video
        ? separate.video
        : separate.audio;
    MediaGraph graph;
    const MediaNodeId node = graph.addNode(
        MediaNodeKind::ScheduledRtpSender, "clone");
    auto applied = MediaScheduledRtpSenderNodePlanCodec::apply(
        graph, node, plan.groupKey, output, separate.sdp);
    if (!applied) {
        return ::media::Result<
            MediaDecodedScheduledRtpSenderNodePlan>::failure(applied.error());
    }
    return MediaScheduledRtpSenderNodePlanCodec::decode(
        *graph.findNode(node));
}

std::unique_ptr<SenderCase> senderCase(
    TestContext& ctx,
    const MediaRealtimeAvSyncRuntimePlan& plan,
    const std::shared_ptr<MediaAvSyncGroupRuntime>& group,
    MediaScheduledStream stream,
    bool failPacketizerOpen,
    std::uint16_t localBase,
    std::size_t descriptionCapacity)
{
    auto decoded = cloneSenderPlan(plan, stream);
    EXPECT_TRUE(ctx, decoded);
    if (!decoded) return {};
    auto fixture = std::make_unique<SenderCase>();
    fixture->stream = stream;
    fixture->senderLead = decoded.value().output.senderLead;
    fixture->graph = senderGraph(ctx, stream, descriptionCapacity);
    fixture->graph.execution.rebindCompiledGraph(fixture->graph.graph);
    fixture->rtp = std::make_shared<FakePortState>();
    fixture->rtcp = std::make_shared<FakePortState>();
    auto rtpLocal = MediaUdpDatagramEndpoint::create(
        decoded.value().output.transport.addressFamily(),
        decoded.value().output.transport.localNumericAddress(), localBase);
    auto rtcpLocal = MediaUdpDatagramEndpoint::create(
        decoded.value().output.transport.addressFamily(),
        decoded.value().output.transport.localNumericAddress(),
        static_cast<std::uint16_t>(localBase + 2));
    EXPECT_TRUE(ctx, rtpLocal && rtcpLocal);
    if (!rtpLocal || !rtcpLocal) return {};
    fixture->rtp->scriptedBoundEndpoint = std::move(rtpLocal).value();
    fixture->rtcp->scriptedBoundEndpoint = std::move(rtcpLocal).value();
    releasePort(fixture->rtp);
    releasePort(fixture->rtcp);
    fixture->packetizer = std::make_shared<PacketizerState>();
    fixture->packetizer->failOpen = failPacketizerOpen;
    auto created = MediaScheduledRtpSenderNode::create(
        fixture->graph.sender, decoded.value().groupKey,
        std::move(decoded.value().output), std::move(decoded.value().sdp),
        MediaScheduledRtpSenderNodeDependencies{
            group,
            std::make_unique<FakeSenderPortFactory>(
                fixture->rtp, fixture->rtcp),
            std::make_unique<FakePacketizerFactory>(fixture->packetizer)});
    EXPECT_TRUE(ctx, created);
    if (!created) return {};
    fixture->node = std::move(created).value();
    EXPECT_TRUE(ctx, fixture->node->start(fixture->graph.execution));
    return fixture;
}

bool pushActivationAndCodec(
    TestContext& ctx,
    SenderCase& fixture,
    const MediaAvSyncGroupKey& groupKey,
    bool pushCodec)
{
    auto activation = MediaPlaybackEpochActivatedBuffer::create(
        groupKey,
        {milliseconds(0), milliseconds(0), 1},
        {1, milliseconds(0), milliseconds(0), 0, 48'000});
    EXPECT_TRUE(ctx, activation);
    if (!activation) return false;
    EXPECT_TRUE(
        ctx,
        fixture.graph.execution.findInputChannel(
            fixture.graph.sender, "epoch")->push(std::move(activation).value()));
    if (pushCodec) {
        auto codec = makeMediaBufferRef<FFmpegCodecContextBuffer>(
            codecContext(fixture.stream));
        EXPECT_TRUE(ctx, codec != nullptr);
        if (!codec) return false;
        EXPECT_TRUE(
            ctx,
            fixture.graph.execution.findInputChannel(
                fixture.graph.sender, "codec")->push(codec));
    }
    return true;
}

} // namespace media_transcode::test::scheduled_rtp_output
