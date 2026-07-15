#include "common/TestAssert.h"
#include "common/AvSyncRuntimeTestSupport.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/channel_layout.h>
}

#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaRunningTime ms(std::int64_t value)
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class ManualClock final : public MediaMasterClock {
public:
    explicit ManualClock(MediaRunningTime value) : value(value) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(value);
    }

    MediaRunningTime value;
};

struct SinkState final {
    std::vector<std::uint8_t> bytes;
    std::size_t flushes = 0;
    std::size_t closes = 0;
};

class RecordingSink final : public MediaOutputByteSink {
public:
    explicit RecordingSink(std::shared_ptr<SinkState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override
    {
        m_state->bytes.insert(m_state->bytes.end(), bytes.begin(), bytes.end());
        return ::media::Result<std::size_t>::success(bytes.size());
    }

    ::media::Status flush() override
    {
        ++m_state->flushes;
        return ::media::Status::success();
    }

    ::media::Status close() override
    {
        ++m_state->closes;
        return ::media::Status::success();
    }

private:
    std::shared_ptr<SinkState> m_state;
};

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

MediaAvSyncPlan avSyncPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "project-ts-file-mux-node";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 64;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = ms(40);
    auto plan = MediaAvSyncPlanner::plan(request).value();
    plan.audioServo.commandLeadNs = ms(1'500);
    plan.audioServo.compensationWindowNs = ms(2'000);
    plan.audioServo.frequencyFilterTimeConstantNs = ms(5'000);
    return plan;
}

MediaBufferRef codecParameters(MediaStreamKind kind)
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

MediaBufferRef accessUnit(MediaScheduledStream stream,
                          std::uint64_t generation,
                          MediaRunningTime emission,
                          MediaRunningTime lead = ms(100))
{
    auto packet = ::media::ffmpeg::makePacket();
    av_new_packet(packet.get(), stream == MediaScheduledStream::Video ? 7 : 3);
    if (stream == MediaScheduledStream::Video) {
        const std::array<std::uint8_t, 7> payload{0, 0, 0, 3, 0x65, 1, 2};
        std::memcpy(packet->data, payload.data(), payload.size());
        packet->flags |= AV_PKT_FLAG_KEY;
    } else {
        const std::array<std::uint8_t, 3> payload{1, 2, 3};
        std::memcpy(packet->data, payload.data(), payload.size());
    }
    auto outer = std::make_shared<FFmpegPacketBuffer>(
        std::move(packet), std::nullopt);
    outer->setStreamKind(stream == MediaScheduledStream::Video
                             ? MediaStreamKind::Video
                             : MediaStreamKind::Audio);
    const auto dispatch = emission.checkedAdd(lead).value();
    return MediaTsAccessUnitBuffer::create(
        outer, stream, generation, dispatch, dispatch, emission, lead).value();
}

struct ProjectFileMuxHarness final {
    MediaGraph graph;
    MediaGraphExecutionContext execution;
    MediaNodeId mux;
    MediaNodeId binder;
    MediaNodeId planSource;
    MediaNodeId sinkSource;
    MediaNodeId videoConfigSource;
    MediaNodeId audioConfigSource;
    MediaNodeId videoPacketSource;
    MediaNodeId audioPacketSource;
    MediaAvSyncGroupKey group{"project-file-mux-group"};
    MediaPlaybackEpoch epoch{ms(0), ms(1'000), 7};
    std::shared_ptr<ManualClock> clock =
        std::make_shared<ManualClock>(epoch.masterRelease);
    std::shared_ptr<SinkState> sink = std::make_shared<SinkState>();
    std::unique_ptr<FileMuxNode> runtime;
    std::unique_ptr<MediaGraphRuntime> graphRuntime;

    bool initialize(TestContext& ctx)
    {
        planSource = addSource("plan.source", "plan", MediaStreamKind::Metadata,
                               MediaEdgeKind::Metadata,
                               MediaPayloadKind::TsMuxRuntimePlan);
        sinkSource = addSource("sink.source", "resource", MediaStreamKind::Metadata,
                               MediaEdgeKind::Metadata,
                               MediaPayloadKind::OutputByteSink);
        videoConfigSource = addSource("video.config.source", "codec",
                                      MediaStreamKind::Video,
                                      MediaEdgeKind::Metadata,
                                      MediaPayloadKind::CodecParameters);
        audioConfigSource = addSource("audio.config.source", "codec",
                                      MediaStreamKind::Audio,
                                      MediaEdgeKind::Metadata,
                                      MediaPayloadKind::CodecParameters);
        videoPacketSource = addSource("video.packet.source", "packet",
                                      MediaStreamKind::Video,
                                      MediaEdgeKind::EncodedPacket,
                                      MediaPayloadKind::TsAccessUnit);
        audioPacketSource = addSource("audio.packet.source", "packet",
                                      MediaStreamKind::Audio,
                                      MediaEdgeKind::EncodedPacket,
                                      MediaPayloadKind::TsAccessUnit);
        const auto scheduler = graph.addNode(
            MediaNodeKind::AvOutputScheduler, "project.av.scheduler");
        binder = graph.addNode(
            MediaNodeKind::PlaybackEpochBinder, "project.epoch.binder");
        graph.setNodeOption(scheduler, "av_scheduler.sync_group",
                            group.value());
        graph.setNodeOption(binder, "playback_epoch_binder.sync_group",
                            group.value());
        mux = graph.addNode(MediaNodeKind::FileMux, "project.file.mux");
        graph.setNodeOption(mux, MediaTranscodeOptionKey::MuxSessionKind,
                            "project_mpegts");
        graph.setNodeOption(mux, MediaTranscodeOptionKey::MuxExpectVideo, "1");
        graph.setNodeOption(mux, MediaTranscodeOptionKey::MuxExpectAudio, "1");
        graph.addInputPort(mux, "resource", MediaStreamKind::Metadata,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::OutputByteSink);
        graph.addInputPort(mux, "plan", MediaStreamKind::Metadata,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::TsMuxRuntimePlan);
        graph.addInputPort(mux, "codec", MediaStreamKind::Any,
                           MediaEdgeKind::Metadata,
                           MediaPayloadKind::CodecParameters, true, true);
        graph.addInputPort(mux, "packet", MediaStreamKind::Any,
                           MediaEdgeKind::EncodedPacket,
                           MediaPayloadKind::TsAccessUnit, true, true);
        connect(planSource, "plan", "plan", "plan.edge");
        connect(sinkSource, "resource", "resource", "sink.edge");
        connect(videoConfigSource, "codec", "codec", "video.config.edge");
        connect(audioConfigSource, "codec", "codec", "audio.config.edge");
        connect(videoPacketSource, "packet", "packet", "video.packet.edge");
        connect(audioPacketSource, "packet", "packet", "audio.packet.edge");

        EXPECT_TRUE(ctx, media_transcode::test::compileAndActivateAvSyncRuntime(
            std::move(graph),
            MediaAvSyncRuntimeBinding{
                group, avSyncPlan(),
                MediaAvGenerationTransitionPlanner::plan(
                    MediaAvSyncOutputAdapterKind::ProjectMpegTs,
                    ms(1'000), ms(500))},
            clock, epoch, binder, execution, graphRuntime));
        EXPECT_EQ(ctx, channel(planSource)->binding().payloadKind,
                  MediaPayloadKind::TsMuxRuntimePlan);
        EXPECT_EQ(ctx, channel(sinkSource)->binding().payloadKind,
                  MediaPayloadKind::OutputByteSink);
        EXPECT_EQ(ctx, channel(videoConfigSource)->binding().payloadKind,
                  MediaPayloadKind::CodecParameters);
        EXPECT_EQ(ctx, channel(audioConfigSource)->binding().payloadKind,
                  MediaPayloadKind::CodecParameters);
        EXPECT_EQ(ctx, channel(videoPacketSource)->binding().payloadKind,
                  MediaPayloadKind::TsAccessUnit);
        EXPECT_EQ(ctx, channel(audioPacketSource)->binding().payloadKind,
                  MediaPayloadKind::TsAccessUnit);
        runtime = std::make_unique<FileMuxNode>(mux);
        EXPECT_TRUE(ctx, runtime->start(execution));
        return execution.compiled();
    }

    MediaNodeId addSource(const char* name,
                          const char* port,
                          MediaStreamKind stream,
                          MediaEdgeKind edge,
                          MediaPayloadKind payload)
    {
        const auto node = graph.addNode(MediaNodeKind::DebugDump, name);
        graph.addOutputPort(node, port, stream, edge, payload);
        return node;
    }

    void connect(MediaNodeId source,
                 const char* sourcePort,
                 const char* targetPort,
                 const char* edgeName)
    {
        MediaEdgePolicy policy;
        policy.queuePolicy.bounded = true;
        policy.queuePolicy.capacity = 8;
        policy.queuePolicy.allowFlushControlBypass = true;
        graph.connect(source, sourcePort, mux, targetPort, edgeName, policy);
    }

    MediaChannel* channel(MediaNodeId source)
    {
        for (MediaChannel* input : execution.inputChannels(mux)) {
            if (input && input->binding().from.nodeId == source) return input;
        }
        return nullptr;
    }

    ::media::Status push(MediaNodeId source, MediaBufferRef buffer)
    {
        MediaChannel* input = channel(source);
        return input
            ? input->push(std::move(buffer))
            : ::media::Status::failure(
                  ::media::ErrorInfo::notInitialized("test input channel is missing"));
    }

    void closeMetadata()
    {
        channel(planSource)->close();
        channel(sinkSource)->close();
        channel(videoConfigSource)->close();
        channel(audioConfigSource)->close();
    }

    MediaBufferRef planBuffer() const
    {
        return MediaTsMuxRuntimePlanBuffer::create(
            muxPlan(), epoch, group).value();
    }

    MediaBufferRef sinkBuffer() const
    {
        return MediaOutputByteSinkBuffer::create(
            std::make_unique<RecordingSink>(sink)).value();
    }
};

bool processOne(ProjectFileMuxHarness& harness, TestContext& ctx)
{
    auto result = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, result);
    return static_cast<bool>(result);
}

void bindInAsynchronousOrder(ProjectFileMuxHarness& harness, TestContext& ctx)
{
    EXPECT_TRUE(ctx, harness.push(
                         harness.audioConfigSource,
                         codecParameters(MediaStreamKind::Audio)));
    processOne(harness, ctx);
    EXPECT_TRUE(ctx, harness.push(harness.sinkSource, harness.sinkBuffer()));
    processOne(harness, ctx);
    EXPECT_TRUE(ctx, harness.push(
                         harness.videoConfigSource,
                         codecParameters(MediaStreamKind::Video)));
    processOne(harness, ctx);
    EXPECT_TRUE(ctx, harness.push(harness.planSource, harness.planBuffer()));
    processOne(harness, ctx);
}

std::uint8_t hexNibble(char value)
{
    return static_cast<std::uint8_t>(
        value <= '9' ? value - '0' : value - 'a' + 10);
}

const std::vector<std::uint8_t>& expectedMuxBytes()
{
    static constexpr std::string_view fixedHex =
        "47400030a600ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff0000b00d0001c100000001e100e8f95e7d"
        "474100309c00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff0002b0170001c10000e101f0001be101f0000fe102f0009e28c6dd"
        "4701012fb710000000007e00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "4701012fb710000003847e00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "474101309440ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff000001e000008080052100015461000000016742001e0000000168ce00000001650102"
        "47010120b710000007087e00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
        "474102309f00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff000001c000128080052100016271fff14c80015ffc010203";
    static const std::vector<std::uint8_t> bytes = [] {
        std::vector<std::uint8_t> decoded;
        decoded.reserve(fixedHex.size() / 2);
        for (std::size_t index = 0; index < fixedHex.size(); index += 2) {
            decoded.push_back(static_cast<std::uint8_t>(
                (hexNibble(fixedHex[index]) << 4) |
                hexNibble(fixedHex[index + 1])));
        }
        return decoded;
    }();
    return bytes;
}

void realNodeProducesExactBytesAndLifecycle(TestContext& ctx)
{
    ProjectFileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    bindInAsynchronousOrder(harness, ctx);
    EXPECT_EQ(ctx, harness.sink->bytes.size(), std::size_t{376});

    auto due = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, due);
    if (due) EXPECT_EQ(ctx, due.value().state, MediaNodeProcessState::Progress);
    harness.clock->value = ms(1'010);
    auto waiting = harness.runtime->process(harness.execution);
    EXPECT_TRUE(ctx, waiting);
    if (waiting) {
        EXPECT_EQ(ctx, waiting.value().state, MediaNodeProcessState::Waiting);
        EXPECT_TRUE(ctx, waiting.value().deadlineWait.has_value());
        if (waiting.value().deadlineWait) {
            EXPECT_EQ(ctx, waiting.value().deadlineWait->syncGroup, harness.group);
            EXPECT_EQ(ctx, waiting.value().deadlineWait->masterDeadline, ms(1'020));
        }
    }

    EXPECT_TRUE(ctx, harness.push(
                         harness.videoPacketSource,
                         accessUnit(MediaScheduledStream::Video, 7, ms(1'020))));
    processOne(harness, ctx);
    EXPECT_TRUE(ctx, harness.push(
                         harness.audioPacketSource,
                         accessUnit(MediaScheduledStream::Audio, 7, ms(1'040))));
    processOne(harness, ctx);
    harness.clock->value = ms(1'050);
    auto flush = FFmpegBufferFactory::makeFlush(MediaStreamKind::Control);
    EXPECT_TRUE(ctx, flush);
    if (flush) {
        EXPECT_TRUE(ctx, harness.push(
                             harness.videoPacketSource,
                             std::move(flush).value()));
        processOne(harness, ctx);
    }

    harness.closeMetadata();
    auto videoEof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    auto audioEof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    EXPECT_TRUE(ctx, videoEof);
    EXPECT_TRUE(ctx, audioEof);
    if (videoEof) {
        EXPECT_TRUE(ctx, harness.push(
                             harness.videoPacketSource,
                             std::move(videoEof).value()));
        processOne(harness, ctx);
    }
    if (audioEof) {
        EXPECT_TRUE(ctx, harness.push(
                             harness.audioPacketSource,
                             std::move(audioEof).value()));
        auto finished = harness.runtime->process(harness.execution);
        EXPECT_TRUE(ctx, finished);
        if (finished) {
            EXPECT_EQ(ctx, finished.value().state, MediaNodeProcessState::Finished);
        }
    }
    EXPECT_EQ(ctx, harness.sink->flushes, std::size_t{1});
    EXPECT_EQ(ctx, harness.sink->closes, std::size_t{1});
    harness.runtime->abort(harness.execution);
    EXPECT_EQ(ctx, harness.sink->closes, std::size_t{1});
    EXPECT_EQ(ctx, harness.sink->bytes, expectedMuxBytes());
}

void packetBeforeCompleteBindingIsTerminal(TestContext& ctx)
{
    ProjectFileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    EXPECT_TRUE(ctx, harness.push(harness.planSource, harness.planBuffer()));
    processOne(harness, ctx);
    EXPECT_TRUE(ctx, harness.push(harness.sinkSource, harness.sinkBuffer()));
    processOne(harness, ctx);
    EXPECT_TRUE(ctx, harness.push(
                         harness.videoConfigSource,
                         codecParameters(MediaStreamKind::Video)));
    processOne(harness, ctx);
    const auto before = harness.sink->bytes;
    EXPECT_TRUE(ctx, harness.push(
                         harness.videoPacketSource,
                         accessUnit(MediaScheduledStream::Video, 7, ms(1'020))));
    const auto failure = harness.runtime->process(harness.execution);
    EXPECT_FALSE(ctx, failure);
    EXPECT_EQ(ctx, harness.sink->bytes, before);
    EXPECT_TRUE(ctx, harness.sink->bytes.empty());
    harness.runtime->abort(harness.execution);
    EXPECT_EQ(ctx, harness.sink->closes, std::size_t{1});
}

void expectPacketFailureWithoutPes(TestContext& ctx,
                                   MediaBufferRef packet)
{
    ProjectFileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    bindInAsynchronousOrder(harness, ctx);
    const auto before = harness.sink->bytes;
    EXPECT_EQ(ctx, before.size(), std::size_t{376});
    EXPECT_TRUE(ctx, harness.push(harness.videoPacketSource, std::move(packet)));
    const auto failure = harness.runtime->process(harness.execution);
    EXPECT_FALSE(ctx, failure);
    EXPECT_EQ(ctx, harness.sink->bytes, before);
    harness.runtime->abort(harness.execution);
    EXPECT_EQ(ctx, harness.sink->closes, std::size_t{1});
}

void invalidPacketContractsDoNotWritePes(TestContext& ctx)
{
    expectPacketFailureWithoutPes(
        ctx, codecParameters(MediaStreamKind::Video));
    expectPacketFailureWithoutPes(
        ctx, accessUnit(MediaScheduledStream::Video, 8, ms(1'020)));
    expectPacketFailureWithoutPes(
        ctx, accessUnit(MediaScheduledStream::Video, 7, ms(1'020), ms(90)));
}

void duplicateAndWrongBindingsDoNotWritePes(TestContext& ctx)
{
    ProjectFileMuxHarness duplicate;
    if (!duplicate.initialize(ctx)) return;
    bindInAsynchronousOrder(duplicate, ctx);
    const auto beforeDuplicate = duplicate.sink->bytes;
    EXPECT_EQ(ctx, beforeDuplicate.size(), std::size_t{376});
    EXPECT_TRUE(ctx, duplicate.push(duplicate.planSource, duplicate.planBuffer()));
    EXPECT_FALSE(ctx, duplicate.runtime->process(duplicate.execution));
    EXPECT_EQ(ctx, duplicate.sink->bytes, beforeDuplicate);
    EXPECT_EQ(ctx, duplicate.sink->closes, std::size_t{1});

    ProjectFileMuxHarness wrong;
    if (!wrong.initialize(ctx)) return;
    bindInAsynchronousOrder(wrong, ctx);
    const auto beforeWrong = wrong.sink->bytes;
    EXPECT_EQ(ctx, beforeWrong.size(), std::size_t{376});
    EXPECT_TRUE(ctx, wrong.push(wrong.sinkSource, wrong.planBuffer()));
    EXPECT_FALSE(ctx, wrong.runtime->process(wrong.execution));
    EXPECT_EQ(ctx, wrong.sink->bytes, beforeWrong);
    wrong.runtime->abort(wrong.execution);
    EXPECT_EQ(ctx, wrong.sink->closes, std::size_t{1});
}

void abortClosesRealProjectSinkOnce(TestContext& ctx)
{
    ProjectFileMuxHarness harness;
    if (!harness.initialize(ctx)) return;
    bindInAsynchronousOrder(harness, ctx);
    EXPECT_EQ(ctx, harness.sink->bytes.size(), std::size_t{376});
    harness.runtime->abort(harness.execution);
    harness.runtime->abort(harness.execution);
    EXPECT_EQ(ctx, harness.sink->flushes, std::size_t{0});
    EXPECT_EQ(ctx, harness.sink->closes, std::size_t{1});
}

} // namespace

void runProjectMpegTsFileMuxNodeTests(TestContext& ctx)
{
    realNodeProducesExactBytesAndLifecycle(ctx);
    packetBeforeCompleteBindingIsTerminal(ctx);
    invalidPacketContractsDoNotWritePes(ctx);
    duplicateAndWrongBindingsDoNotWritePes(ctx);
    abortClosesRealProjectSinkOnce(ctx);
}
