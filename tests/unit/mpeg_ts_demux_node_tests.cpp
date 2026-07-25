#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/demux/MpegTsDemuxNode.h"
#include "internal/graph/nodes/sync/MediaCanonicalInputNode.h"
#include "internal/graph/protocol/mpegts/MediaTsInitialAcquiringPacketBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketPayloadFootprint.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

constexpr int kVideoStream = 0;
constexpr int kAudioStream = 1;

MediaTsRuntimeBinding runtimeBinding()
{
    return MediaTsRuntimeBinding{
        MediaTsPacketOriginPolicy::PerStreamPesCarry,
        MediaTsRuntimeStreamBinding{kVideoStream, 0x201},
        MediaTsRuntimeStreamBinding{kAudioStream, 0x202}, 0x101, 32};
}

::media::ffmpeg::PacketPtr retainedPacket(int payloadBytes,
                                          std::size_t sideDataBytes)
{
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet || av_new_packet(packet.get(), payloadBytes) < 0) return {};
    if (sideDataBytes > 0 &&
        !av_packet_new_side_data(
            packet.get(), AV_PKT_DATA_NEW_EXTRADATA, sideDataBytes)) {
        return {};
    }
    packet->pts = 1;
    packet->dts = 1;
    packet->duration = 1;
    packet->time_base = AVRational{1, 90'000};
    return packet;
}

MediaTsProgramInventorySnapshot inventory()
{
    return {1, {{1, 0x100, 1, 0x101, {{0x201, 0x1b}, {0x202, 0x0f}}}}};
}

MediaTsEvidenceCheckpoint evidence(std::uint64_t offset, std::uint64_t pcr,
                                   bool discontinuity = false,
                                   std::uint64_t generation = 0)
{
    MediaTsEvidenceCheckpoint value;
    value.byteOffset = offset;
    value.inventory = inventory();
    value.pcrObservation = MediaTsRawPcrEvidence{offset, 0x101, pcr, discontinuity};
    value.discontinuity = discontinuity;
    value.generation = generation;
    if (discontinuity) {
        value.continuityEvent = MediaTsContinuityEvent{
            offset, 0x101, MediaTsContinuityEventReason::DiscontinuityIndicator};
    }
    return value;
}

MediaTsEvidenceCheckpoint continuityEvidence(std::uint64_t offset,
                                              std::uint16_t pid,
                                              std::uint64_t rawGeneration)
{
    MediaTsEvidenceCheckpoint value;
    value.byteOffset = offset;
    value.inventory = inventory();
    value.continuityEvent = MediaTsContinuityEvent{
        offset, pid, MediaTsContinuityEventReason::CounterLoss};
    value.generation = rawGeneration;
    return value;
}

struct ScriptedFrame final {
    MediaTsReadFrameState state = MediaTsReadFrameState::Waiting;
    int streamIndex = kVideoStream;
    std::int64_t position = 0;
    std::int64_t pts = AV_NOPTS_VALUE;
    std::int64_t dts = AV_NOPTS_VALUE;
    AVRational timeBase{1, 90'000};
    MediaSourceClockReadiness readiness = MediaSourceClockReadiness::Locked;
    std::int64_t duration = 3'600;
    int packetBytes = 1;
};

class ScriptedTsSession final : public MediaTsDemuxSession {
public:
    struct Stats final { int closeCalls = 0; int cancelCalls = 0; };

    explicit ScriptedTsSession(std::shared_ptr<Stats> value = std::make_shared<Stats>())
        : stats(std::move(value))
    {
        streams.emplace_back(); streams.back().index = kVideoStream;
        streams.back().streamKind = MediaStreamKind::Video;
        streams.emplace_back(); streams.back().index = kAudioStream;
        streams.back().streamKind = MediaStreamKind::Audio;
    }

    std::deque<ScriptedFrame> frames;
    std::vector<MediaTsEvidenceCheckpoint> evidenceTimeline;
    MediaTsInputRuntimeContract contract{188, 32, 4'096, 32, runtimeBinding()};
    std::vector<FFmpegInputStreamSnapshot> streams;
    std::vector<FFmpegInputProgramSnapshot> programs{
        FFmpegInputProgramSnapshot{1, 0x100, 0x101,
                                   {{kVideoStream, 0x201}, {kAudioStream, 0x202}}}};
    std::vector<std::uint64_t> observedPositions;
    std::optional<std::uint64_t> preflightLimit;
    std::shared_ptr<Stats> stats;
    bool failRead = false;
    bool cancelRead = false;
    bool failEvidence = false;
    bool failObserve = false;

    ::media::Result<MediaTsReadFrameEnvelope> readFrame() override
    {
        if (cancelRead) return ::media::Result<MediaTsReadFrameEnvelope>::failure(
            ::media::ErrorInfo::cancelled("scripted cancelled read"));
        if (failRead) return ::media::Result<MediaTsReadFrameEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument("scripted read failure"));
        if (frames.empty()) return ::media::Result<MediaTsReadFrameEnvelope>::success(
            MediaTsReadFrameEnvelope{MediaTsReadFrameState::Waiting});
        const auto frame = frames.front(); frames.pop_front();
        MediaTsReadFrameEnvelope envelope;
        envelope.state = frame.state;
        if (frame.state == MediaTsReadFrameState::Frame) {
            envelope.packet = ::media::ffmpeg::makePacket();
            if (av_new_packet(envelope.packet.get(), frame.packetBytes) < 0) {
                return ::media::Result<MediaTsReadFrameEnvelope>::failure(
                    ::media::ErrorInfo::allocationFailed(
                        "scripted packet allocation failed"));
            }
            envelope.packet->stream_index = frame.streamIndex;
            envelope.packet->pos = frame.position;
            envelope.packet->pts = frame.pts;
            envelope.packet->dts = frame.dts;
            envelope.packet->duration = frame.duration;
            envelope.packet->time_base = frame.timeBase;
            if (frame.readiness == MediaSourceClockReadiness::Locked) {
                envelope.provenance = MediaTsPacketProvenance{
                    static_cast<std::uint64_t>(frame.position),
                    static_cast<std::uint64_t>(frame.position),
                    frame.readiness};
            } else if (frame.readiness ==
                       MediaSourceClockReadiness::ReacquireRequired) {
                envelope.provenance = MediaTsPacketProvenance{
                    static_cast<std::uint64_t>(frame.position),
                    std::nullopt, frame.readiness};
            } else {
                envelope.provenance = MediaTsPacketProvenance{
                    std::nullopt, std::nullopt, frame.readiness};
            }
        }
        return ::media::Result<MediaTsReadFrameEnvelope>::success(std::move(envelope));
    }
    ::media::Status close() noexcept override { ++stats->closeCalls; return ::media::Status::success(); }
    void cancel() noexcept override { ++stats->cancelCalls; }
    const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept override { return streams; }
    const std::vector<FFmpegInputProgramSnapshot>& programSnapshots() const noexcept override { return programs; }
    ::media::Result<std::vector<FFmpegInputStreamSnapshot>> cloneStreamSnapshots() const override
    {
        std::vector<FFmpegInputStreamSnapshot> result;
        result.emplace_back(); result.back().index = kVideoStream;
        result.emplace_back(); result.back().index = kAudioStream;
        return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::success(std::move(result));
    }
    MediaTsProgramInventorySnapshot programInventory() const override { return inventory(); }
    const MediaTsInputRuntimeContract& runtimeContract() const noexcept override { return contract; }
    ::media::Result<std::vector<MediaTsEvidenceCheckpoint>> evidenceSnapshotAfter(
        std::optional<std::uint64_t> exclusiveOffset) const override
    {
        if (failEvidence) return ::media::Result<std::vector<MediaTsEvidenceCheckpoint>>::failure(
            ::media::ErrorInfo::invalidArgument("scripted evidence failure"));
        const auto visible = exclusiveOffset
            ? (observedPositions.empty() ? std::optional<std::uint64_t>{}
                                         : std::optional<std::uint64_t>{observedPositions.back()})
            : preflightLimit;
        std::vector<MediaTsEvidenceCheckpoint> result;
        for (const auto& item : evidenceTimeline) {
            if (exclusiveOffset && item.byteOffset <= *exclusiveOffset) continue;
            if (visible && item.byteOffset > *visible) continue;
            result.push_back(item);
        }
        return ::media::Result<std::vector<MediaTsEvidenceCheckpoint>>::success(std::move(result));
    }
    ::media::Status observePacketPosition(std::uint64_t position) override
    {
        if (failObserve) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("scripted observe failure"));
        observedPositions.push_back(position);
        return ::media::Status::success();
    }
};

struct NodeFixture final {
    MediaGraph graph;
    MediaNodeId source, demux, videoSink, audioSink, clockSink;
    MediaGraphExecutionContext execution;

    explicit NodeFixture(bool options = true, bool clockOutput = false)
    {
        const auto queue = MediaGraphBuildSupport::blockingQueuePolicy(16);
        source = graph.addNode(MediaNodeKind::DebugDump, "test.source");
        demux = graph.addNode(MediaNodeKind::MpegTsDemux, "test.demux");
        videoSink = graph.addNode(MediaNodeKind::DebugDump, "test.video");
        audioSink = graph.addNode(MediaNodeKind::DebugDump, "test.audio");
        clockSink = graph.addNode(MediaNodeKind::DebugDump, "test.clock");
        graph.addOutputPort(source, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
        graph.addInputPort(demux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
        graph.addOutputPort(demux, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        graph.addOutputPort(demux, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        if (clockOutput) {
            graph.addOutputPort(demux, "clock", MediaStreamKind::Metadata,
                                MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
            graph.addInputPort(clockSink, "clock", MediaStreamKind::Metadata,
                               MediaEdgeKind::Event, MediaPayloadKind::GraphEvent);
        }
        graph.addInputPort(videoSink, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        graph.addInputPort(audioSink, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        graph.connect(source, "format", demux, "format", "format", queue);
        graph.connect(demux, "video", videoSink, "video", "video", queue);
        graph.connect(demux, "audio", audioSink, "audio", "audio", queue);
        if (clockOutput) {
            graph.connect(demux, "clock", clockSink, "clock", "clock", queue);
        }
        if (options) setOptions();
    }
    void setOptions(std::size_t projectionCapacity = 32, const std::string& skipped = {})
    {
        const std::vector<std::pair<std::string, std::string>> values{
            {"mpegts.program_number", "1"}, {"mpegts.pmt_pid", "256"},
            {"mpegts.video_pid", "513"}, {"mpegts.audio_pid", "514"},
            {"mpegts.pcr_pid", "257"}, {"mpegts.pcr_interval_27mhz", "2700000"},
            {"mpegts.maximum_pcr_jitter_27mhz", "2700"}, {"mpegts.maximum_pcr_gap_27mhz", "8100000"},
            {"mpegts.packet_stride", "188"}, {"mpegts.evidence_timeline_capacity", "32"},
            {"mpegts.projection_capacity", std::to_string(projectionCapacity)},
            {"mpegts.initial_acquiring_video_packet_capacity", "8"},
            {"mpegts.initial_acquiring_audio_packet_capacity", "8"},
            {"mpegts.initial_acquiring_video_byte_capacity", "8388608"},
            {"mpegts.initial_acquiring_audio_byte_capacity", "1048576"},
            {"mpegts.maximum_acquiring_video_packet_bytes", "1048576"},
            {"mpegts.maximum_acquiring_audio_packet_bytes", "131072"},
            {"mpegts.maximum_position_regression_bytes", "4096"},
            {"mpegts.pes_provenance_capacity", "32"},
            {"mpegts.packet_origin_policy", "0"},
            {"mpegts.timestamp_time_base_num", "1"}, {"mpegts.timestamp_time_base_den", "90000"},
            {"mpegts.initial_source_generation", "0"}, {"mpegts.initial_raw_generation", "0"}};
        for (const auto& [key, value] : values) {
            if (key != skipped) graph.setNodeOption(demux, key, value);
        }
    }
    bool compile() { return static_cast<bool>(execution.compile(graph)); }
    MediaChannel* input() { return execution.findInputChannel(demux, "format"); }
    MediaChannel* video() { return execution.findOutputChannel(demux, "video"); }
    MediaChannel* audio() { return execution.findOutputChannel(demux, "audio"); }
    MediaChannel* clock() { return execution.findOutputChannel(demux, "clock"); }
};

MediaBufferRef prepared(std::unique_ptr<MediaTsDemuxSession> session)
{
    auto result = MediaTsPreparedInputBuffer::create(std::move(session));
    return result ? MediaBufferRef(std::move(result).value()) : MediaBufferRef{};
}

const FFmpegPacketBuffer* popPacket(MediaChannel& channel, MediaBufferRef& owner)
{
    return channel.tryPop(owner) ? dynamic_cast<const FFmpegPacketBuffer*>(owner.get()) : nullptr;
}

void testBindingFailures(TestContext& ctx)
{
    NodeFixture missing(false); EXPECT_TRUE(ctx, missing.compile());
    EXPECT_TRUE(ctx, missing.input()->push(prepared(std::make_unique<ScriptedTsSession>())));
    MpegTsDemuxNode missingNode(missing.demux); EXPECT_FALSE(ctx, missingNode.process(missing.execution));

    NodeFixture wrong; EXPECT_TRUE(ctx, wrong.compile());
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Metadata);
    EXPECT_TRUE(ctx, wrong.input()->push(eof.value()));
    MpegTsDemuxNode wrongNode(wrong.demux); EXPECT_FALSE(ctx, wrongNode.process(wrong.execution));

    for (const auto* missingKey : {"mpegts.packet_stride", "mpegts.evidence_timeline_capacity"}) {
        NodeFixture fixture(false); fixture.setOptions(32, missingKey);
        EXPECT_TRUE(ctx, fixture.compile());
        EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::make_unique<ScriptedTsSession>())));
        MpegTsDemuxNode node(fixture.demux); EXPECT_FALSE(ctx, node.process(fixture.execution));
    }

    for (int failure = 0; failure < 3; ++failure) {
        NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
        auto session = std::make_unique<ScriptedTsSession>();
        if (failure == 0) session->programs[0].programNumber = 2;
        if (failure == 1) session->programs[0].streamBindings.pop_back();
        if (failure == 2) session->failEvidence = true;
        EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
        MpegTsDemuxNode node(fixture.demux); EXPECT_FALSE(ctx, node.process(fixture.execution));
    }
}

void testDuplicateTransferAndRuntimeContract(TestContext& ctx)
{
    auto bufferResult = MediaTsPreparedInputBuffer::create(std::make_unique<ScriptedTsSession>());
    EXPECT_TRUE(ctx, bufferResult);
    EXPECT_TRUE(ctx, bufferResult.value()->takeSession());
    EXPECT_FALSE(ctx, bufferResult.value()->takeSession());

    NodeFixture duplicate; EXPECT_TRUE(ctx, duplicate.compile());
    auto first = std::make_unique<ScriptedTsSession>();
    first->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
    first->frames.push_back({MediaTsReadFrameState::Waiting});
    EXPECT_TRUE(ctx, duplicate.input()->push(prepared(std::move(first))));
    MpegTsDemuxNode duplicateNode(duplicate.demux);
    EXPECT_TRUE(ctx, duplicateNode.process(duplicate.execution));
    EXPECT_TRUE(ctx, duplicate.input()->push(prepared(std::make_unique<ScriptedTsSession>())));
    EXPECT_FALSE(ctx, duplicateNode.process(duplicate.execution));

    NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
    auto session = std::make_unique<ScriptedTsSession>(); session->contract.packetStride = 192;
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode node(fixture.demux); EXPECT_FALSE(ctx, node.process(fixture.execution));

    NodeFixture originMismatch; EXPECT_TRUE(ctx, originMismatch.compile());
    auto mismatchedSession = std::make_unique<ScriptedTsSession>();
    mismatchedSession->contract.originBinding->video.pid = 0x203;
    EXPECT_TRUE(ctx, originMismatch.input()->push(prepared(std::move(mismatchedSession))));
    MpegTsDemuxNode mismatchedNode(originMismatch.demux);
    EXPECT_FALSE(ctx, mismatchedNode.process(originMismatch.execution));
}

void testFramesRollbackAndLifecycle(TestContext& ctx)
{
    NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
    auto stats = std::make_shared<ScriptedTsSession::Stats>();
    auto session = std::make_unique<ScriptedTsSession>(stats); auto* observer = session.get();
    session->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
    session->frames = {{MediaTsReadFrameState::Waiting},
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 90'000, 89'000},
        {MediaTsReadFrameState::Frame, kAudioStream, 200, AV_NOPTS_VALUE, AV_NOPTS_VALUE},
        {MediaTsReadFrameState::EndOfStream}};
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode node(fixture.demux);
    auto waiting = node.process(fixture.execution); EXPECT_TRUE(ctx, waiting);
    if (waiting) EXPECT_EQ(ctx, waiting.value().state, MediaNodeProcessState::Waiting);
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef videoOwner; const auto* video = popPacket(*fixture.video(), videoOwner);
    EXPECT_TRUE(ctx, video != nullptr);
    if (video && video->sourceTiming()) {
        EXPECT_EQ(ctx, video->packet()->pts, std::int64_t{90'000});
        EXPECT_EQ(ctx, video->packet()->dts, std::int64_t{89'000});
        EXPECT_EQ(ctx, video->sourceTiming()->readiness, MediaSourceClockReadiness::Locked);
    }
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef audioOwner; const auto* audio = popPacket(*fixture.audio(), audioOwner);
    EXPECT_TRUE(ctx, audio != nullptr);
    if (audio && audio->sourceTiming()) {
        EXPECT_EQ(ctx, audio->packet()->pts, std::int64_t{AV_NOPTS_VALUE});
        EXPECT_FALSE(ctx, audio->sourceTiming()->presentationNs.has_value());
    }
    EXPECT_EQ(ctx, observer->observedPositions.size(), std::size_t{2});
    auto finished = node.process(fixture.execution); EXPECT_TRUE(ctx, finished);
    if (finished) EXPECT_EQ(ctx, finished.value().state, MediaNodeProcessState::Finished);
    MediaBufferRef vEof, aEof; EXPECT_TRUE(ctx, fixture.video()->tryPop(vEof));
    EXPECT_TRUE(ctx, fixture.audio()->tryPop(aEof));
    node.abort(fixture.execution);
    EXPECT_EQ(ctx, stats->cancelCalls, 1); EXPECT_EQ(ctx, stats->closeCalls, 1);

    NodeFixture stopped; EXPECT_TRUE(ctx, stopped.compile());
    auto stopStats = std::make_shared<ScriptedTsSession::Stats>();
    auto stopSession = std::make_unique<ScriptedTsSession>(stopStats);
    stopSession->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
    stopSession->frames.push_back({MediaTsReadFrameState::Waiting});
    EXPECT_TRUE(ctx, stopped.input()->push(prepared(std::move(stopSession))));
    MpegTsDemuxNode stoppedNode(stopped.demux);
    EXPECT_TRUE(ctx, stoppedNode.process(stopped.execution));
    EXPECT_TRUE(ctx, stoppedNode.stop(stopped.execution));
    EXPECT_EQ(ctx, stopStats->cancelCalls, 0); EXPECT_EQ(ctx, stopStats->closeCalls, 1);
}

void testPublishesSelectedProgramClockStateBeforeTimedPacket(TestContext& ctx)
{
    NodeFixture fixture(true, true);
    fixture.graph.setNodeOption(
        fixture.demux, "mpegts.initial_source_generation", "7");
    EXPECT_TRUE(ctx, fixture.compile());
    auto session = std::make_unique<ScriptedTsSession>();
    session->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
    session->frames.push_back(
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 90'000, 89'000,
         {1, 90'000}});
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode node(fixture.demux);
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef stateOwner;
    EXPECT_TRUE(ctx, fixture.clock()->tryPop(stateOwner));
    const auto* state = dynamic_cast<const MediaSourceClockStateBuffer*>(
        stateOwner.get());
    EXPECT_TRUE(ctx, state != nullptr);
    if (state) {
        EXPECT_EQ(ctx, state->readiness(), MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, state->generation(), std::uint64_t{7});
    }
    EXPECT_EQ(ctx, fixture.video()->size(), std::size_t{0});
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef packetOwner;
    const auto* packet = popPacket(*fixture.video(), packetOwner);
    EXPECT_TRUE(ctx, packet != nullptr && packet->sourceTiming());
    if (packet && packet->sourceTiming()) {
        EXPECT_EQ(ctx, packet->sourceTiming()->generation, state->generation());
    }
}

void testNegativePtsDtsMapAs33BitValues(TestContext& ctx)
{
    NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
    auto session = std::make_unique<ScriptedTsSession>();
    session->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
    session->frames.push_back({MediaTsReadFrameState::Frame, kVideoStream, 200, -1, -45'000, {1, 90'000}});
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode node(fixture.demux); EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef owner; const auto* packet = popPacket(*fixture.video(), owner);
    EXPECT_TRUE(ctx, packet != nullptr);
    if (packet && packet->sourceTiming()) {
        EXPECT_EQ(ctx, packet->packet()->pts, std::int64_t{-1});
        EXPECT_EQ(ctx, packet->packet()->dts, std::int64_t{-45'000});
        EXPECT_EQ(ctx, packet->packet()->time_base.num, 1);
        EXPECT_EQ(ctx, packet->packet()->time_base.den, 90'000);
        EXPECT_EQ(ctx, packet->sourceTiming()->presentationNs, std::optional<std::int64_t>{-11'111});
        EXPECT_EQ(ctx, packet->sourceTiming()->decodeNs, std::optional<std::int64_t>{-500'000'000});
    }

    NodeFixture dtsFixture; EXPECT_TRUE(ctx, dtsFixture.compile());
    auto dtsSession = std::make_unique<ScriptedTsSession>();
    dtsSession->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
    dtsSession->frames.push_back({MediaTsReadFrameState::Frame, kVideoStream, 200,
                                 AV_NOPTS_VALUE, 0, {1001, 90'000}});
    EXPECT_TRUE(ctx, dtsFixture.input()->push(prepared(std::move(dtsSession))));
    MpegTsDemuxNode dtsNode(dtsFixture.demux);
    EXPECT_TRUE(ctx, dtsNode.process(dtsFixture.execution));
    MediaBufferRef dtsOwner; const auto* dtsPacket = popPacket(*dtsFixture.video(), dtsOwner);
    EXPECT_TRUE(ctx, dtsPacket != nullptr);
    if (dtsPacket && dtsPacket->sourceTiming()) {
        EXPECT_FALSE(ctx, dtsPacket->sourceTiming()->presentationNs.has_value());
        EXPECT_EQ(ctx, dtsPacket->sourceTiming()->decodeNs, std::optional<std::int64_t>{0});
        EXPECT_EQ(ctx, dtsPacket->packet()->pts, std::int64_t{AV_NOPTS_VALUE});
        EXPECT_EQ(ctx, dtsPacket->packet()->dts, std::int64_t{0});
        EXPECT_EQ(ctx, dtsPacket->packet()->time_base.num, 1001);
        EXPECT_EQ(ctx, dtsPacket->packet()->time_base.den, 90'000);
    }
}

void testIncrementalReacquireAndRollback(TestContext& ctx)
{
    constexpr std::int64_t modulus = std::int64_t{1} << 33;
    NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
    auto session = std::make_unique<ScriptedTsSession>(); session->preflightLimit = 200;
    session->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000),
        evidence(300, 5'400'000, true, 1), evidence(400, 8'100'000, false, 1),
        evidence(500, 10'800'000, false, 1)};
    session->frames = {
        {MediaTsReadFrameState::Frame, kVideoStream, 200, modulus - 1, modulus - 1},
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 1, 1},
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 3'000'000'000, 3'000'000'000},
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 6'000'000'000, 6'000'000'000},
        {MediaTsReadFrameState::Frame, kVideoStream, 200, modulus - 1, modulus - 1},
        {MediaTsReadFrameState::Frame, kVideoStream, 300, 180'000, 180'000},
        {MediaTsReadFrameState::Frame, kVideoStream, 500, 90'000, 90'000},
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 1, 1}};
    session->frames[5].readiness = MediaSourceClockReadiness::ReacquireRequired;
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode node(fixture.demux);
    for (int index = 0; index < 5; ++index) {
        EXPECT_TRUE(ctx, node.process(fixture.execution));
        MediaBufferRef seededOwner; const auto* seeded = popPacket(*fixture.video(), seededOwner);
        EXPECT_TRUE(ctx, seeded != nullptr);
        if (seeded && seeded->sourceTiming()) {
            EXPECT_EQ(ctx, seeded->sourceTiming()->generation, std::uint64_t{0});
        }
    }
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef first; auto* reacquire = popPacket(*fixture.video(), first);
    EXPECT_TRUE(ctx, reacquire != nullptr);
    if (reacquire && reacquire->sourceTiming()) {
        EXPECT_EQ(ctx, reacquire->sourceTiming()->readiness, MediaSourceClockReadiness::ReacquireRequired);
        EXPECT_EQ(ctx, reacquire->sourceTiming()->generation, std::uint64_t{1});
    }
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef second; auto* locked = popPacket(*fixture.video(), second);
    EXPECT_TRUE(ctx, locked != nullptr);
    if (locked && locked->sourceTiming()) {
        EXPECT_EQ(ctx, locked->sourceTiming()->readiness, MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, locked->sourceTiming()->generation, std::uint64_t{1});
        EXPECT_EQ(ctx, locked->sourceTiming()->presentationNs, std::optional<std::int64_t>{1'000'000'000});
    }
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef third; auto* rollback = popPacket(*fixture.video(), third);
    EXPECT_TRUE(ctx, rollback != nullptr);
    if (rollback && rollback->sourceTiming()) {
        EXPECT_EQ(ctx, rollback->sourceTiming()->generation, std::uint64_t{0});
        EXPECT_EQ(ctx, rollback->sourceTiming()->presentationNs,
                  std::optional<std::int64_t>{95'443'717'700'000});
        EXPECT_EQ(ctx, rollback->sourceTiming()->decodeNs,
                  std::optional<std::int64_t>{95'443'717'700'000});
    }
}

void testProjectionIsTheOnlySourceGenerationAuthority(TestContext& ctx)
{
    NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
    auto session = std::make_unique<ScriptedTsSession>();
    session->evidenceTimeline = {
        evidence(100, 0), evidence(200, 2'700'000),
        continuityEvidence(300, 0x777, 1), evidence(400, 5'400'000, false, 1),
        continuityEvidence(500, 0x202, 2), evidence(600, 8'100'000, false, 2),
        evidence(700, 10'800'000, false, 2)};
    session->frames = {
        {MediaTsReadFrameState::Frame, kVideoStream, 400, 90'000, 90'000},
        {MediaTsReadFrameState::Frame, kAudioStream, 500, 90'000, 90'000},
        {MediaTsReadFrameState::Frame, kVideoStream, 700, 90'000, 90'000},
        {MediaTsReadFrameState::Frame, kAudioStream, 700, 90'000, 90'000}};
    session->frames[1].readiness = MediaSourceClockReadiness::ReacquireRequired;
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode node(fixture.demux);

    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef firstOwner;
    const auto* afterUnselected = popPacket(*fixture.video(), firstOwner);
    EXPECT_TRUE(ctx, afterUnselected && afterUnselected->sourceTiming());
    if (afterUnselected && afterUnselected->sourceTiming()) {
        EXPECT_EQ(ctx, afterUnselected->sourceTiming()->generation, std::uint64_t{0});
        EXPECT_EQ(ctx, afterUnselected->sourceTiming()->readiness,
                  MediaSourceClockReadiness::Locked);
    }

    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef secondOwner;
    const auto* selectedFault = popPacket(*fixture.audio(), secondOwner);
    EXPECT_TRUE(ctx, selectedFault && selectedFault->sourceTiming());
    if (selectedFault && selectedFault->sourceTiming()) {
        EXPECT_EQ(ctx, selectedFault->sourceTiming()->generation, std::uint64_t{1});
        EXPECT_EQ(ctx, selectedFault->sourceTiming()->readiness,
                  MediaSourceClockReadiness::ReacquireRequired);
    }

    EXPECT_TRUE(ctx, node.process(fixture.execution));
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef videoOwner;
    MediaBufferRef audioOwner;
    const auto* video = popPacket(*fixture.video(), videoOwner);
    const auto* audio = popPacket(*fixture.audio(), audioOwner);
    EXPECT_TRUE(ctx, video && video->sourceTiming());
    EXPECT_TRUE(ctx, audio && audio->sourceTiming());
    if (video && audio && video->sourceTiming() && audio->sourceTiming()) {
        EXPECT_EQ(ctx, video->sourceTiming()->generation, std::uint64_t{1});
        EXPECT_EQ(ctx, audio->sourceTiming()->generation, std::uint64_t{1});
        EXPECT_EQ(ctx, video->sourceTiming()->readiness,
                  MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, audio->sourceTiming()->readiness,
                  MediaSourceClockReadiness::Locked);
    }
}

void testPesInvalidityDoesNotInventSourceGeneration(TestContext& ctx)
{
    NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
    auto session = std::make_unique<ScriptedTsSession>();
    session->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
    session->frames = {
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 90'000, 90'000}};
    session->frames.front().readiness = MediaSourceClockReadiness::ReacquireRequired;
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode node(fixture.demux);
    EXPECT_TRUE(ctx, node.process(fixture.execution));
    MediaBufferRef owner;
    const auto* packet = popPacket(*fixture.video(), owner);
    EXPECT_TRUE(ctx, packet && packet->sourceTiming());
    if (packet && packet->sourceTiming()) {
        EXPECT_EQ(ctx, packet->sourceTiming()->generation, std::uint64_t{0});
        EXPECT_EQ(ctx, packet->sourceTiming()->readiness,
                  MediaSourceClockReadiness::ReacquireRequired);
    }
}

void testCancelledReadAndSessionTimelineFailurePropagation(TestContext& ctx)
{
    for (const bool timelineFailure : {false, true}) {
        NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
        auto session = std::make_unique<ScriptedTsSession>();
        session->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000)};
        if (timelineFailure) {
            session->failObserve = true;
            session->frames.push_back({MediaTsReadFrameState::Frame, kVideoStream, 200, 0, 0});
        } else {
            session->cancelRead = true;
        }
        EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
        MpegTsDemuxNode node(fixture.demux); auto result = node.process(fixture.execution);
        EXPECT_FALSE(ctx, result);
        if (!result) EXPECT_EQ(ctx, result.error().code,
            timelineFailure ? media::ErrorCode::InvalidArgument : media::ErrorCode::Cancelled);
    }
}

void testPlannerTimeBaseMaterializesWhenFfmpegPacketOmitsIt(TestContext& ctx)
{
    NodeFixture fixture;
    EXPECT_TRUE(ctx, fixture.compile());
    auto session = std::make_unique<ScriptedTsSession>();
    session->evidenceTimeline = {
        evidence(100, 0), evidence(200, 2'700'000)};
    session->frames = {{
        MediaTsReadFrameState::Frame, kVideoStream, 200,
        90'000, 89'000, {0, 1}, MediaSourceClockReadiness::Locked}};
    EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));

    MpegTsDemuxNode node(fixture.demux);
    EXPECT_TRUE(ctx, node.process(fixture.execution));

    MediaBufferRef owner;
    const auto* packet = popPacket(*fixture.video(), owner);
    EXPECT_TRUE(ctx, packet != nullptr);
    if (packet) {
        EXPECT_EQ(ctx, owner->timeDescriptor().timeBase.num, 1);
        EXPECT_EQ(ctx, owner->timeDescriptor().timeBase.den, 90'000);
        EXPECT_EQ(ctx, packet->packet()->time_base.num, 1);
        EXPECT_EQ(ctx, packet->packet()->time_base.den, 90'000);
    }
}

void testPacketAndSessionFailures(TestContext& ctx)
{
    for (int failure = 0; failure < 4; ++failure) {
        NodeFixture fixture; EXPECT_TRUE(ctx, fixture.compile());
        auto session = std::make_unique<ScriptedTsSession>();
        if (failure == 0) session->failRead = true;
        else {
            session->evidenceTimeline = failure == 3 ? std::vector<MediaTsEvidenceCheckpoint>{}
                                                     : std::vector<MediaTsEvidenceCheckpoint>{evidence(100, 0), evidence(200, 2'700'000)};
            session->frames.push_back({MediaTsReadFrameState::Frame,
                failure == 1 ? 99 : kVideoStream, 200, 0, 0});
            if (failure == 2) session->failObserve = true;
        }
        EXPECT_TRUE(ctx, fixture.input()->push(prepared(std::move(session))));
        MpegTsDemuxNode node(fixture.demux); EXPECT_FALSE(ctx, node.process(fixture.execution));
    }

    NodeFixture acquiring(true, true);
    acquiring.graph.setNodeOption(
        acquiring.demux, "mpegts.initial_source_generation", "7");
    EXPECT_TRUE(ctx, acquiring.compile());
    auto acquiringSession = std::make_unique<ScriptedTsSession>();
    acquiringSession->evidenceTimeline = {
        evidence(100, 0), evidence(200, 2'700'000)};
    acquiringSession->frames = {
        {MediaTsReadFrameState::Frame, kVideoStream, -1, 45'000, 44'000,
         {1, 90'000}, MediaSourceClockReadiness::Acquiring},
        {MediaTsReadFrameState::Frame, kAudioStream, -1, 60'000, 59'000,
         {1, 90'000}, MediaSourceClockReadiness::Acquiring},
        {MediaTsReadFrameState::Frame, kVideoStream, 200, 90'000, 89'000,
         {1, 90'000}, MediaSourceClockReadiness::Locked}};
    EXPECT_TRUE(ctx, acquiring.input()->push(prepared(std::move(acquiringSession))));
    MpegTsDemuxNode acquiringNode(acquiring.demux);
    EXPECT_TRUE(ctx, acquiringNode.process(acquiring.execution));
    MediaBufferRef acquiringStateOwner;
    EXPECT_TRUE(ctx, acquiring.clock()->tryPop(acquiringStateOwner));
    const auto* acquiringState =
        dynamic_cast<const MediaSourceClockStateBuffer*>(acquiringStateOwner.get());
    EXPECT_TRUE(ctx, acquiringState != nullptr);
    if (acquiringState) {
        EXPECT_EQ(ctx, acquiringState->readiness(),
                  MediaSourceClockReadiness::Acquiring);
    }
    EXPECT_EQ(ctx, acquiring.video()->size(), std::size_t{0});

    EXPECT_TRUE(ctx, acquiringNode.process(acquiring.execution));
    MediaBufferRef secondAcquiringStateOwner;
    EXPECT_TRUE(ctx, acquiring.clock()->tryPop(secondAcquiringStateOwner));
    EXPECT_EQ(ctx, acquiring.video()->size(), std::size_t{0});
    EXPECT_EQ(ctx, acquiring.audio()->size(), std::size_t{0});

    EXPECT_TRUE(ctx, acquiringNode.process(acquiring.execution));
    MediaBufferRef lockedStateOwner;
    EXPECT_TRUE(ctx, acquiring.clock()->tryPop(lockedStateOwner));
    const auto* lockedState =
        dynamic_cast<const MediaSourceClockStateBuffer*>(lockedStateOwner.get());
    EXPECT_TRUE(ctx, lockedState != nullptr);
    if (lockedState) {
        EXPECT_EQ(ctx, lockedState->readiness(), MediaSourceClockReadiness::Locked);
        EXPECT_EQ(ctx, lockedState->generation(), std::uint64_t{7});
    }
    EXPECT_EQ(ctx, acquiring.video()->size(), std::size_t{0});

    struct ExpectedReplay final {
        MediaChannel* channel;
        std::int64_t pts;
    };
    const std::vector<ExpectedReplay> expectedReplay{
        {acquiring.video(), 45'000},
        {acquiring.audio(), 60'000},
        {acquiring.video(), 90'000}};
    for (const auto& expected : expectedReplay) {
        EXPECT_TRUE(ctx, acquiringNode.process(acquiring.execution));
        MediaBufferRef owner;
        const auto* packet = popPacket(*expected.channel, owner);
        EXPECT_TRUE(ctx, packet != nullptr && packet->sourceTiming());
        if (packet && packet->sourceTiming()) {
            EXPECT_EQ(ctx, packet->sourceTiming()->readiness,
                      MediaSourceClockReadiness::Locked);
            EXPECT_EQ(ctx, packet->sourceTiming()->generation, std::uint64_t{7});
            EXPECT_TRUE(ctx, packet->sourceTiming()->presentationNs.has_value());
            EXPECT_EQ(ctx, packet->packet()->pts, expected.pts);
            EXPECT_TRUE(ctx, packet->packet()->duration > 0);
        }
    }

    NodeFixture overflow; overflow.setOptions(2); EXPECT_TRUE(ctx, overflow.compile());
    auto session = std::make_unique<ScriptedTsSession>();
    session->evidenceTimeline = {evidence(100, 0), evidence(200, 2'700'000), evidence(300, 5'400'000)};
    EXPECT_TRUE(ctx, overflow.input()->push(prepared(std::move(session))));
    MpegTsDemuxNode overflowNode(overflow.demux); EXPECT_FALSE(ctx, overflowNode.process(overflow.execution));
}

void testInitialAcquiringRetentionFailsClosedAndReplaysExactlyOnce(TestContext& ctx)
{
    {
        NodeFixture capacity;
        capacity.graph.setNodeOption(
            capacity.demux,
            "mpegts.initial_acquiring_video_packet_capacity", "1");
        EXPECT_TRUE(ctx, capacity.compile());
        auto session = std::make_unique<ScriptedTsSession>();
        session->frames = {
            {MediaTsReadFrameState::Frame, kVideoStream, -1, 1, 1,
             {1, 90'000}, MediaSourceClockReadiness::Acquiring},
            {MediaTsReadFrameState::Frame, kVideoStream, -1, 2, 2,
             {1, 90'000}, MediaSourceClockReadiness::Acquiring}};
        EXPECT_TRUE(ctx, capacity.input()->push(prepared(std::move(session))));
        MpegTsDemuxNode node(capacity.demux);
        EXPECT_TRUE(ctx, node.process(capacity.execution));
        EXPECT_FALSE(ctx, node.process(capacity.execution));
    }
    {
        NodeFixture bytes;
        bytes.graph.setNodeOption(
            bytes.demux, "mpegts.initial_acquiring_video_byte_capacity", "1");
        bytes.graph.setNodeOption(
            bytes.demux, "mpegts.maximum_acquiring_video_packet_bytes", "1");
        EXPECT_TRUE(ctx, bytes.compile());
        auto session = std::make_unique<ScriptedTsSession>();
        ScriptedFrame oversized{
            MediaTsReadFrameState::Frame, kVideoStream, -1, 1, 1,
            {1, 90'000}, MediaSourceClockReadiness::Acquiring};
        oversized.packetBytes = 2;
        session->frames.push_back(oversized);
        EXPECT_TRUE(ctx, bytes.input()->push(prepared(std::move(session))));
        MpegTsDemuxNode node(bytes.demux);
        EXPECT_FALSE(ctx, node.process(bytes.execution));
    }
    {
        NodeFixture eof;
        EXPECT_TRUE(ctx, eof.compile());
        auto session = std::make_unique<ScriptedTsSession>();
        session->frames = {
            {MediaTsReadFrameState::Frame, kVideoStream, -1, 1, 1,
             {1, 90'000}, MediaSourceClockReadiness::Acquiring},
            {MediaTsReadFrameState::EndOfStream}};
        EXPECT_TRUE(ctx, eof.input()->push(prepared(std::move(session))));
        MpegTsDemuxNode node(eof.demux);
        EXPECT_TRUE(ctx, node.process(eof.execution));
        EXPECT_FALSE(ctx, node.process(eof.execution));
    }
    {
        NodeFixture blocked(true, true);
        blocked.graph.setNodeOption(
            blocked.demux, "mpegts.initial_source_generation", "7");
        EXPECT_TRUE(ctx, blocked.compile());
        auto session = std::make_unique<ScriptedTsSession>();
        session->evidenceTimeline = {
            evidence(100, 0), evidence(200, 2'700'000)};
        session->frames = {
            {MediaTsReadFrameState::Frame, kVideoStream, -1, 45'000, 44'000,
             {1, 90'000}, MediaSourceClockReadiness::Acquiring},
            {MediaTsReadFrameState::Frame, kVideoStream, 200, 90'000, 89'000,
             {1, 90'000}, MediaSourceClockReadiness::Locked}};
        EXPECT_TRUE(ctx, blocked.input()->push(prepared(std::move(session))));
        MpegTsDemuxNode node(blocked.demux);
        EXPECT_TRUE(ctx, node.process(blocked.execution));
        MediaBufferRef state;
        EXPECT_TRUE(ctx, blocked.clock()->tryPop(state));
        EXPECT_TRUE(ctx, node.process(blocked.execution));
        EXPECT_TRUE(ctx, blocked.clock()->tryPop(state));

        auto raw = ::media::ffmpeg::makePacket();
        auto dummy = FFmpegBufferFactory::wrapPacket(
            std::move(raw), MediaStreamKind::Video, std::nullopt);
        EXPECT_TRUE(ctx, dummy);
        constexpr std::size_t OutputCapacity = 16;
        for (std::size_t index = 0; index < OutputCapacity; ++index) {
            EXPECT_TRUE(ctx, blocked.video()->push(dummy.value()));
        }
        auto wouldBlock = node.process(blocked.execution);
        EXPECT_TRUE(ctx, wouldBlock);
        if (wouldBlock) {
            EXPECT_EQ(ctx, wouldBlock.value().state,
                      MediaNodeProcessState::Waiting);
        }
        MediaBufferRef discarded;
        EXPECT_TRUE(ctx, blocked.video()->tryPop(discarded));
        EXPECT_TRUE(ctx, node.process(blocked.execution));
        std::size_t retainedCopies = 0;
        while (blocked.video()->tryPop(discarded)) {
            const auto* packet = dynamic_cast<const FFmpegPacketBuffer*>(discarded.get());
            if (packet && packet->packet()->pts == 45'000) ++retainedCopies;
        }
        EXPECT_EQ(ctx, retainedCopies, std::size_t{1});
        EXPECT_TRUE(ctx, node.process(blocked.execution));
        MediaBufferRef current;
        const auto* currentPacket = popPacket(*blocked.video(), current);
        EXPECT_TRUE(ctx, currentPacket != nullptr);
        if (currentPacket) {
            EXPECT_EQ(ctx, currentPacket->packet()->pts, std::int64_t{90'000});
            auto duration = MediaRunningTime::checkedFromTicks(3'600, 1, 90'000);
            EXPECT_TRUE(ctx, duration);
            if (duration && currentPacket->sourceTiming()) {
                auto canonical = MediaCanonicalInputNode::canonicalize(
                    current, *currentPacket->sourceTiming(), duration.value(),
                    MediaScheduledStream::Video,
                    MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
                    "mpeg-ts-boundary", MediaSourceAccessUnitSequence(1),
                    std::nullopt);
                EXPECT_TRUE(ctx, canonical);
                if (canonical) {
                    EXPECT_EQ(ctx, canonical.value()->generation(), std::uint64_t{7});
                }
            }
        }
    }
}

void testInitialAcquiringRetentionAccountsCompletePacketFootprint(TestContext& ctx)
{
    AVPacket missingSideDataArray{};
    missingSideDataArray.side_data_elems = 1;
    EXPECT_FALSE(ctx, ffmpegPacketPayloadFootprintBytes(
        missingSideDataArray).has_value());

    const MediaTsInitialPacketRetentionLimit audioLimit{8, 64, 32};
    {
        auto buffer = MediaTsInitialAcquiringPacketBuffer::create(
            MediaTsInitialPacketRetentionLimit{8, 64, 8}, audioLimit);
        EXPECT_TRUE(ctx, buffer);
        auto packet = retainedPacket(3, 6);
        EXPECT_TRUE(ctx, packet != nullptr);
        if (buffer && packet) {
            EXPECT_FALSE(ctx, buffer.value().retain(
                std::move(packet), MediaStreamKind::Video));
        }
    }
    {
        auto buffer = MediaTsInitialAcquiringPacketBuffer::create(
            MediaTsInitialPacketRetentionLimit{8, 9, 8}, audioLimit);
        EXPECT_TRUE(ctx, buffer);
        if (buffer) {
            EXPECT_TRUE(ctx, buffer.value().retain(
                retainedPacket(2, 3), MediaStreamKind::Video));
            EXPECT_FALSE(ctx, buffer.value().retain(
                retainedPacket(2, 3), MediaStreamKind::Video));
        }
    }
    {
        auto buffer = MediaTsInitialAcquiringPacketBuffer::create(
            MediaTsInitialPacketRetentionLimit{8, 5, 5}, audioLimit);
        EXPECT_TRUE(ctx, buffer);
        if (buffer) {
            EXPECT_TRUE(ctx, buffer.value().retain(
                retainedPacket(2, 3), MediaStreamKind::Video));
            EXPECT_FALSE(ctx, buffer.value().retain(
                retainedPacket(1, 0), MediaStreamKind::Video));
        }
    }
    {
        auto buffer = MediaTsInitialAcquiringPacketBuffer::create(
            MediaTsInitialPacketRetentionLimit{8, 64, 32}, audioLimit);
        auto packet = retainedPacket(1, 0);
        EXPECT_TRUE(ctx, buffer && packet);
        if (buffer && packet) {
            packet->side_data_elems = -1;
            EXPECT_FALSE(ctx, buffer.value().retain(
                std::move(packet), MediaStreamKind::Video));
        }
    }
    {
        auto buffer = MediaTsInitialAcquiringPacketBuffer::create(
            MediaTsInitialPacketRetentionLimit{8, 64, 32}, audioLimit);
        auto packet = retainedPacket(1, 1);
        EXPECT_TRUE(ctx, buffer && packet);
        if (buffer && packet) {
            av_freep(&packet->side_data[0].data);
            packet->side_data[0].size = 7;
            EXPECT_FALSE(ctx, buffer.value().retain(
                std::move(packet), MediaStreamKind::Video));
        }
    }
    {
        auto buffer = MediaTsInitialAcquiringPacketBuffer::create(
            MediaTsInitialPacketRetentionLimit{8, 64, 32}, audioLimit);
        auto packet = retainedPacket(1, 1);
        EXPECT_TRUE(ctx, buffer && packet);
        if (buffer && packet) {
            packet->side_data[0].size =
                std::numeric_limits<std::size_t>::max();
            EXPECT_FALSE(ctx, buffer.value().retain(
                std::move(packet), MediaStreamKind::Video));
        }
    }
}

} // namespace

void runMpegTsDemuxNodeTests(TestContext& ctx)
{
    testBindingFailures(ctx);
    testDuplicateTransferAndRuntimeContract(ctx);
    testFramesRollbackAndLifecycle(ctx);
    testPublishesSelectedProgramClockStateBeforeTimedPacket(ctx);
    testNegativePtsDtsMapAs33BitValues(ctx);
    testIncrementalReacquireAndRollback(ctx);
    testProjectionIsTheOnlySourceGenerationAuthority(ctx);
    testPesInvalidityDoesNotInventSourceGeneration(ctx);
    testPacketAndSessionFailures(ctx);
    testInitialAcquiringRetentionFailsClosedAndReplaysExactlyOnce(ctx);
    testInitialAcquiringRetentionAccountsCompletePacketFootprint(ctx);
    testCancelledReadAndSessionTimelineFailurePropagation(ctx);
    testPlannerTimeBaseMaterializesWhenFfmpegPacketOmitsIt(ctx);
}
