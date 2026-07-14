#include "common/TestAssert.h"
#include "unit/fixtures/MediaRtpUdpSenderFakePort.h"

#include "internal/graph/nodes/mux/ScheduledRtpSenderConfig.h"
#include "internal/graph/nodes/mux/ScheduledRtpSenderSession.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
}

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

using namespace media::ffmpeg::graph;
using namespace media_transcode::test::rtp_udp;
using media_transcode::test::TestContext;

namespace {

class ScriptedPacketizerSession final : public ScheduledRtpPacketizerSession {
public:
    ScriptedPacketizerSession(
        ScheduledRtpRewrittenDatagramSink sink,
        std::vector<std::pair<std::vector<std::uint8_t>, std::size_t>> datagrams)
        : m_sink(std::move(sink)), m_datagrams(std::move(datagrams))
    {
    }

    ::media::Status open() override { return ::media::Status::success(); }

    ::media::Status writeAccessUnit(const AVPacket&, MediaRtpTimestamp) override
    {
        for (const auto& [datagram, payloadOctets] : m_datagrams) {
            auto sent = m_sink(datagram, payloadOctets);
            if (!sent) return sent;
        }
        return ::media::Status::success();
    }

private:
    ScheduledRtpRewrittenDatagramSink m_sink;
    std::vector<std::pair<std::vector<std::uint8_t>, std::size_t>> m_datagrams;
};

class ScriptedPacketizerFactory final : public ScheduledRtpPacketizerFactory {
public:
    explicit ScriptedPacketizerFactory(
        std::vector<std::pair<std::vector<std::uint8_t>, std::size_t>> datagrams)
        : m_datagrams(std::move(datagrams))
    {
    }

    ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>> create(
        ScheduledRtpMuxStreamConfig,
        ScheduledRtpRewrittenDatagramSink sink) override
    {
        std::unique_ptr<ScheduledRtpPacketizerSession> session =
            std::make_unique<ScriptedPacketizerSession>(
                std::move(sink), m_datagrams);
        return ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>::success(
            std::move(session));
    }

private:
    std::vector<std::pair<std::vector<std::uint8_t>, std::size_t>> m_datagrams;
};

::media::Result<ScheduledRtpSenderConfig> makeScheduledSenderConfig()
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            ::media::ErrorInfo::allocationFailed("test codec parameters"));
    }
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->codec_id = AV_CODEC_ID_H264;
    parameters->width = 1920;
    parameters->height = 1080;
    auto stream = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *parameters, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x11223344u, 1200);
    auto epoch = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::seconds(1'700'000'000));
    auto mapper = MediaRtpOutputClockMapper::create(
        90'000, 1'000, MediaRunningTime::fromNanoseconds(0));
    auto schedule = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(5'000'000'000), 7);
    auto counters = ScheduledRtpSenderCounters::create(0, 0);
    if (!stream) return ::media::Result<ScheduledRtpSenderConfig>::failure(stream.error());
    if (!epoch) return ::media::Result<ScheduledRtpSenderConfig>::failure(epoch.error());
    if (!mapper) return ::media::Result<ScheduledRtpSenderConfig>::failure(mapper.error());
    if (!schedule) return ::media::Result<ScheduledRtpSenderConfig>::failure(schedule.error());
    if (!counters) return ::media::Result<ScheduledRtpSenderConfig>::failure(counters.error());
    return ScheduledRtpSenderConfig::create(
        std::move(stream.value()), epoch.value(), mapper.value(), schedule.value(),
        "udp-sync@test", 7, counters.value());
}

std::shared_ptr<MediaRtpUdpSenderTransport> makeOpenTransport(
    TestContext& ctx,
    const std::shared_ptr<FakePortState>& rtp,
    const std::shared_ptr<FakePortState>& rtcp)
{
    releasePort(rtp);
    releasePort(rtcp);
    FakeSenderPortFactory portFactory(rtp, rtcp);
    auto config = makeConfig();
    EXPECT_TRUE(ctx, config);
    if (!config) return {};
    auto created = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), portFactory);
    EXPECT_TRUE(ctx, created);
    if (!created) return {};
    std::shared_ptr<MediaRtpUdpSenderTransport> transport(
        std::move(created.value()));
    EXPECT_TRUE(ctx, transport->open());
    return transport;
}

::media::Result<std::unique_ptr<ScheduledRtpSenderSession>> makeSender(
    std::shared_ptr<MediaRtpUdpSenderTransport> transport,
    ScriptedPacketizerFactory& packetizer)
{
    auto config = makeScheduledSenderConfig();
    if (!config) {
        return ::media::Result<std::unique_ptr<ScheduledRtpSenderSession>>::failure(
            config.error());
    }
    return ScheduledRtpSenderSession::create(
        std::move(config.value()),
        [transport](std::span<const std::uint8_t> datagram, std::size_t) {
            return transport->sendRtp(datagram);
        },
        [transport](std::span<const std::uint8_t> datagram) {
            return transport->sendRtcp(datagram);
        },
        packetizer);
}

void testRtcpNotAcceptedPreservesTransaction(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    rtcp->outcomes.push_back(MediaUdpDatagramSendOutcome::notAccepted(
        ::media::ErrorInfo::wouldBlock("scripted RTCP pressure")));
    auto transport = makeOpenTransport(ctx, rtp, rtcp);
    if (!transport) return;
    ScriptedPacketizerFactory packetizer({{{1, 2, 3}, 3}});
    auto sender = makeSender(transport, packetizer);
    EXPECT_TRUE(ctx, sender);
    if (!sender || !sender.value()->open()) return;
    EXPECT_FALSE(ctx, sender.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000)));
    AVPacket packet{};
    EXPECT_TRUE(ctx, sender.value()->sendAccessUnit(
        packet, MediaRunningTime::fromNanoseconds(1'050'000'000)));
    auto retried = sender.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'100'000'000));
    EXPECT_TRUE(ctx, retried);
    EXPECT_EQ(ctx, rtcp->sendCalls, 2);
    if (retried && retried.value().sentDiagnostics()) {
        const auto& diagnostics = *retried.value().sentDiagnostics();
        EXPECT_EQ(ctx, diagnostics.scheduledDeadline(),
                  MediaRunningTime::fromNanoseconds(1'000'000'000));
        EXPECT_EQ(ctx, diagnostics.reportInstant(),
                  MediaRunningTime::fromNanoseconds(1'100'000'000));
        EXPECT_EQ(ctx, diagnostics.senderCounters().packetCount(), std::uint64_t{1});
        EXPECT_EQ(ctx, diagnostics.senderCounters().octetCount(), std::uint64_t{3});
    }
    auto notDue = sender.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'500'000'000));
    EXPECT_TRUE(ctx, notDue);
    if (notDue) EXPECT_EQ(ctx, notDue.value().kind(), ScheduledRtcpDispatchKind::NotDue);
    EXPECT_EQ(ctx, rtcp->sendCalls, 2);
}

void testAmbiguousRtcpPoisonsWithoutSecondSyscall(TestContext& ctx)
{
    const auto run = [&ctx](bool throwFromPort) {
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        rtcp->throwOnSend = throwFromPort;
        if (!throwFromPort) {
            rtcp->outcomes.push_back(MediaUdpDatagramSendOutcome::ambiguousPartial(
                ::media::ErrorInfo::ioFailure("RTCP short send", 3), 4));
        }
        auto transport = makeOpenTransport(ctx, rtp, rtcp);
        if (!transport) return;
        ScriptedPacketizerFactory packetizer({{{1}, 1}});
        auto sender = makeSender(transport, packetizer);
        EXPECT_TRUE(ctx, sender);
        if (!sender || !sender.value()->open()) return;
        EXPECT_FALSE(ctx, sender.value()->dispatchSenderReport(
            MediaRunningTime::fromNanoseconds(1'000'000'000)));
        EXPECT_EQ(ctx, transport->state(), MediaRtpUdpSenderTransportState::Poisoned);
        EXPECT_FALSE(ctx, sender.value()->dispatchSenderReport(
            MediaRunningTime::fromNanoseconds(2'000'000'000)));
        EXPECT_EQ(ctx, rtcp->sendCalls, 1);
    };
    run(false);
    run(true);
}

void testFuAMidFailureCountsOnlyAcceptedDatagrams(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    rtp->outcomes.push_back(MediaUdpDatagramSendOutcome::accepted(17));
    rtp->outcomes.push_back(MediaUdpDatagramSendOutcome::notAccepted(
        ::media::ErrorInfo::wouldBlock("second FU-A fragment not accepted")));
    auto transport = makeOpenTransport(ctx, rtp, rtcp);
    if (!transport) return;
    ScriptedPacketizerFactory packetizer({
        {std::vector<std::uint8_t>(17, 1), 5},
        {std::vector<std::uint8_t>(19, 2), 7}});
    auto sender = makeSender(transport, packetizer);
    EXPECT_TRUE(ctx, sender);
    if (!sender || !sender.value()->open()) return;
    AVPacket packet{};
    EXPECT_FALSE(ctx, sender.value()->sendAccessUnit(
        packet, MediaRunningTime::fromNanoseconds(0)));
    EXPECT_EQ(ctx, sender.value()->counters().packetCount(), std::uint64_t{1});
    EXPECT_EQ(ctx, sender.value()->counters().octetCount(), std::uint64_t{5});
    EXPECT_EQ(ctx, transport->state(), MediaRtpUdpSenderTransportState::Open);
    EXPECT_FALSE(ctx, sender.value()->sendAccessUnit(
        packet, MediaRunningTime::fromNanoseconds(1'000'000'000)));
    EXPECT_EQ(ctx, rtp->sendCalls, 2);
}

void testSinkLifetimeAndSenderGuardReentry(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    auto transport = makeOpenTransport(ctx, rtp, rtcp);
    if (!transport) return;
    std::weak_ptr<MediaRtpUdpSenderTransport> lifetime = transport;
    ScriptedPacketizerFactory packetizer({{{1, 2, 3}, 3}});
    auto sender = makeSender(transport, packetizer);
    EXPECT_TRUE(ctx, sender);
    if (!sender || !sender.value()->open()) return;
    ScheduledRtpSenderSession* rawSender = sender.value().get();
    bool nestedRejected = false;
    rtp->onSend = [&] {
        nestedRejected = !rawSender->dispatchSenderReport(
            MediaRunningTime::fromNanoseconds(1'000'000'000));
    };
    AVPacket packet{};
    EXPECT_TRUE(ctx, rawSender->sendAccessUnit(
        packet, MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, nestedRejected);
    EXPECT_EQ(ctx, rtcp->sendCalls, 0);
    transport.reset();
    EXPECT_FALSE(ctx, lifetime.expired());
    sender.value().reset();
    EXPECT_TRUE(ctx, lifetime.expired());
    EXPECT_TRUE(ctx, rtp->closeCalls > 0);
    EXPECT_TRUE(ctx, rtcp->closeCalls > 0);
}

} // namespace

void runMediaRtpUdpSenderCompositionTests(TestContext& ctx)
{
    testRtcpNotAcceptedPreservesTransaction(ctx);
    testAmbiguousRtcpPoisonsWithoutSecondSyscall(ctx);
    testFuAMidFailureCountsOnlyAcceptedDatagrams(ctx);
    testSinkLifetimeAndSenderGuardReentry(ctx);
}
