#pragma once

#include "common/AvSyncRuntimeTestSupport.h"
#include "common/TestAssert.h"
#include "unit/fixtures/MediaRtpUdpSenderFakePort.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNodePlanCodec.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace media_transcode::test::scheduled_rtp_output {

using namespace media::ffmpeg::graph;

constexpr MediaRunningTime milliseconds(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class TestMasterClock final : public MediaMasterClock {
public:
    explicit TestMasterClock(MediaRunningTime now);

    [[nodiscard]] ::media::Result<MediaRunningTime> now() const noexcept override;
    void set(MediaRunningTime now) noexcept;

private:
    std::atomic<std::int64_t> m_now;
};

[[nodiscard]] MediaRealtimeRtpTranscodeRequest completeRequest();

struct ActiveGroupFixture final {
    std::shared_ptr<TestMasterClock> clock;
    std::unique_ptr<MediaGraphRuntime> runtime;
    std::shared_ptr<MediaAvSyncGroupRuntime> group;
};

[[nodiscard]] ActiveGroupFixture activeGroup(
    TestContext& ctx,
    const MediaRealtimeAvSyncRuntimePlan& plan);

struct PacketizerState final {
    int createCalls = 0;
    int openCalls = 0;
    int writeCalls = 0;
    bool failOpen = false;
    MediaStreamKind stream = MediaStreamKind::Unknown;
    MediaScheduledRtpPacketizationMode mode =
        MediaScheduledRtpPacketizationMode::H264AnnexB;
    int payloadType = 0;
    std::uint32_t ssrc = 0;
    std::vector<MediaRtpTimestamp> timestamps;
};

class FakePacketizerFactory final : public ScheduledRtpPacketizerFactory {
public:
    explicit FakePacketizerFactory(std::shared_ptr<PacketizerState> state);

    [[nodiscard]] ::media::Result<
        std::unique_ptr<ScheduledRtpPacketizerSession>> create(
        ScheduledRtpMuxStreamConfig config,
        ScheduledRtpRewrittenDatagramSink sink) override;

private:
    std::shared_ptr<PacketizerState> m_state;
};

[[nodiscard]] ::media::ffmpeg::CodecContextPtr codecContext(
    MediaScheduledStream stream);

[[nodiscard]] ::media::Result<MediaBufferRef> description(
    MediaScheduledStream stream,
    std::uint64_t generation = 1,
    std::uint64_t sessionVersion = 1);

[[nodiscard]] ::media::Result<MediaBufferRef> scheduledUnit(
    MediaScheduledStream stream,
    MediaRunningTime senderLead,
    std::uint64_t sequence = 1,
    std::uint64_t generation = 1);

struct SenderGraphFixture final {
    MediaGraph graph;
    MediaNodeId epochSource;
    MediaNodeId codecSource;
    MediaNodeId scheduledSource;
    MediaNodeId sender;
    MediaNodeId descriptionSink;
    MediaGraphExecutionContext execution;
};

[[nodiscard]] SenderGraphFixture senderGraph(
    TestContext& ctx,
    MediaScheduledStream stream,
    std::size_t descriptionCapacity = 8);

[[nodiscard]] ::media::Result<MediaDecodedScheduledRtpSenderNodePlan>
cloneSenderPlan(
    const MediaRealtimeAvSyncRuntimePlan& plan,
    MediaScheduledStream stream);

struct SenderCase final {
    SenderGraphFixture graph;
    std::shared_ptr<rtp_udp::FakePortState> rtp;
    std::shared_ptr<rtp_udp::FakePortState> rtcp;
    std::shared_ptr<PacketizerState> packetizer;
    std::unique_ptr<MediaScheduledRtpSenderNode> node;
    MediaScheduledStream stream = MediaScheduledStream::Video;
    MediaRunningTime senderLead = milliseconds(0);
};

[[nodiscard]] std::unique_ptr<SenderCase> senderCase(
    TestContext& ctx,
    const MediaRealtimeAvSyncRuntimePlan& plan,
    const std::shared_ptr<MediaAvSyncGroupRuntime>& group,
    MediaScheduledStream stream,
    bool failPacketizerOpen = false,
    std::uint16_t localBase = 43'000,
    std::size_t descriptionCapacity = 8);

[[nodiscard]] bool pushActivationAndCodec(
    TestContext& ctx,
    SenderCase& fixture,
    const MediaAvSyncGroupKey& groupKey,
    bool pushCodec);

} // namespace media_transcode::test::scheduled_rtp_output
