#include "common/TestAssert.h"
#include "unit/fixtures/MediaRtpUdpSenderFakePort.h"

#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace media::ffmpeg::graph;
using namespace media_transcode::test::rtp_udp;
using media_transcode::test::TestContext;

namespace {

static_assert(!std::is_default_constructible_v<MediaRtpUdpSenderConfig>);
static_assert(!std::is_default_constructible_v<MediaRtpUdpLocalPortPolicy>);
static_assert(!std::is_default_constructible_v<MediaUdpDatagramSendOutcome>);

void testConfigIsCompleteAndStrict(TestContext& ctx)
{
    auto fixed = MediaRtpUdpLocalPortPolicy::fixedAdjacent(6000, 6001);
    EXPECT_TRUE(ctx, fixed);
    if (!fixed) return;
    auto config = makeConfig(std::move(fixed.value()), 1400);
    EXPECT_TRUE(ctx, config);
    if (config) {
        EXPECT_EQ(ctx, config.value().remoteRtpEndpoint().port(), std::uint16_t{5004});
        EXPECT_EQ(ctx, config.value().remoteRtcpEndpoint().port(), std::uint16_t{5005});
        EXPECT_EQ(ctx, config.value().maximumDatagramBytes(), static_cast<std::size_t>(1400));
    }

    EXPECT_FALSE(ctx, MediaRtpUdpLocalPortPolicy::fixedAdjacent(6001, 6002));
    EXPECT_FALSE(ctx, MediaRtpUdpLocalPortPolicy::fixedAdjacent(6000, 6002));
    EXPECT_FALSE(ctx, MediaRtpUdpSenderConfig::create(
        MediaIpAddressFamily::Ipv4, "localhost", "127.0.0.1", 5004, 5005,
        osAssignedPolicy(), 1024, 1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure));
    EXPECT_FALSE(ctx, MediaRtpUdpSenderConfig::create(
        MediaIpAddressFamily::Ipv4, "127.0.0.1", "127.0.0.1", 5005, 5006,
        osAssignedPolicy(), 1024, 1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure));
    EXPECT_FALSE(ctx, MediaRtpUdpSenderConfig::create(
        MediaIpAddressFamily::Ipv4, "127.0.0.1", "127.0.0.1", 5004, 5006,
        osAssignedPolicy(), 1024, 1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure));
    EXPECT_FALSE(ctx, MediaRtpUdpSenderConfig::create(
        MediaIpAddressFamily::Ipv4, "127.0.0.1", "127.0.0.1", 5004, 5005,
        osAssignedPolicy(), 0, 1200,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure));
    EXPECT_TRUE(ctx, makeConfig(osAssignedPolicy(), kMediaUdpMaximumPayloadBytes));
    EXPECT_FALSE(ctx, makeConfig(
        osAssignedPolicy(), kMediaUdpMaximumPayloadBytes + 1));
}

void testOpenPublishesAndRoutesAtomically(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    releasePort(rtp);
    releasePort(rtcp);
    FakeSenderPortFactory factory(rtp, rtcp);
    auto config = makeConfig();
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    EXPECT_TRUE(ctx, transport);
    EXPECT_EQ(ctx, factory.createCalls, 2);
    if (!transport) return;
    EXPECT_FALSE(ctx, transport.value()->boundLocalEndpoints().has_value());
    EXPECT_TRUE(ctx, transport.value()->open());
    EXPECT_EQ(ctx, rtp->openCalls, 1);
    EXPECT_EQ(ctx, rtcp->openCalls, 1);
    auto bound = transport.value()->boundLocalEndpoints();
    EXPECT_TRUE(ctx, bound.has_value());
    if (bound) {
        EXPECT_EQ(ctx, bound->rtp().port(), std::uint16_t{41000});
        EXPECT_EQ(ctx, bound->rtcp().port(), std::uint16_t{41002});
    }
    const std::vector<std::uint8_t> datagram{1, 2, 3};
    EXPECT_TRUE(ctx, transport.value()->sendRtp(datagram));
    EXPECT_TRUE(ctx, transport.value()->sendRtcp(datagram));
    EXPECT_EQ(ctx, rtp->sendCalls, 1);
    EXPECT_EQ(ctx, rtcp->sendCalls, 1);
    EXPECT_FALSE(ctx, transport.value()->sendRtp({}));
    const std::vector<std::uint8_t> oversized(1201, 7);
    EXPECT_FALSE(ctx, transport.value()->sendRtcp(oversized));
    EXPECT_EQ(ctx, rtp->sendCalls, 1);
    EXPECT_EQ(ctx, rtcp->sendCalls, 1);
    EXPECT_TRUE(ctx, transport.value()->close());
    EXPECT_TRUE(ctx, transport.value()->close());
    EXPECT_EQ(ctx, transport.value()->state(), MediaRtpUdpSenderTransportState::Closed);
}

void testOpenFailureRollsBackAndPoisons(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    rtcp->openStatus = ::media::Status::failure(
        ::media::ErrorInfo::ioFailure("RTCP bind failed", 10048));
    FakeSenderPortFactory factory(rtp, rtcp);
    auto config = makeConfig();
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    EXPECT_TRUE(ctx, transport);
    if (!transport) return;
    const auto opened = transport.value()->open();
    EXPECT_FALSE(ctx, opened);
    EXPECT_EQ(ctx, rtp->closeCalls, 1);
    EXPECT_EQ(ctx, rtcp->closeCalls, 1);
    EXPECT_EQ(ctx, transport.value()->state(), MediaRtpUdpSenderTransportState::Poisoned);
    EXPECT_FALSE(ctx, transport.value()->boundLocalEndpoints().has_value());
    EXPECT_FALSE(ctx, transport.value()->open());
    EXPECT_EQ(ctx, rtp->openCalls, 1);
    EXPECT_EQ(ctx, rtcp->openCalls, 1);
    EXPECT_TRUE(ctx, transport.value()->close());
    EXPECT_EQ(ctx, transport.value()->state(), MediaRtpUdpSenderTransportState::Poisoned);
}

void testFirstPortOpenFailuresNeverReachRtcp(TestContext& ctx)
{
    const auto run = [&ctx](bool throws) {
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        rtp->throwOnOpen = throws;
        if (!throws) {
            rtp->openStatus = ::media::Status::failure(
                ::media::ErrorInfo::ioFailure("RTP bind failed", 10048));
        }
        FakeSenderPortFactory factory(rtp, rtcp);
        auto config = makeConfig();
        EXPECT_TRUE(ctx, config);
        if (!config) return;
        auto transport = MediaRtpUdpSenderTransport::create(
            std::move(config.value()), factory);
        EXPECT_TRUE(ctx, transport);
        if (!transport) return;
        EXPECT_FALSE(ctx, transport.value()->open());
        EXPECT_EQ(ctx, rtp->openCalls, 1);
        EXPECT_EQ(ctx, rtcp->openCalls, 0);
        EXPECT_EQ(ctx, transport.value()->state(),
                  MediaRtpUdpSenderTransportState::Poisoned);
    };
    run(false);
    run(true);
}

void testEndpointGetterThrowRollsBackAndPoisons(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    rtcp->throwOnRemoteEndpoint = true;
    FakeSenderPortFactory factory(rtp, rtcp);
    auto config = makeConfig();
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    EXPECT_TRUE(ctx, transport);
    if (!transport) return;
    bool escaped = false;
    ::media::Status opened = ::media::Status::success();
    try {
        opened = transport.value()->open();
    } catch (...) {
        escaped = true;
    }
    EXPECT_FALSE(ctx, escaped);
    EXPECT_FALSE(ctx, opened);
    EXPECT_EQ(ctx, rtp->closeCalls, 1);
    EXPECT_EQ(ctx, rtcp->closeCalls, 1);
    EXPECT_EQ(ctx, transport.value()->state(),
              MediaRtpUdpSenderTransportState::Poisoned);
    const auto repeated = transport.value()->open();
    EXPECT_FALSE(ctx, repeated);
    if (!opened && !repeated) {
        EXPECT_EQ(ctx, repeated.error().message, opened.error().message);
    }
}

void testFactoryAndBoundEndpointFailuresAreRejected(TestContext& ctx)
{
    {
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        FakeSenderPortFactory factory(rtp, rtcp);
        factory.returnNullAt = 2;
        auto config = makeConfig();
        EXPECT_TRUE(ctx, config);
        if (config) {
            EXPECT_FALSE(ctx, MediaRtpUdpSenderTransport::create(
                std::move(config.value()), factory));
        }
        EXPECT_EQ(ctx, factory.createCalls, 2);
    }
    {
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        FakeSenderPortFactory factory(rtp, rtcp);
        factory.throwAt = 2;
        auto config = makeConfig();
        EXPECT_TRUE(ctx, config);
        if (config) {
            bool escaped = false;
            ::media::Result<std::unique_ptr<MediaRtpUdpSenderTransport>> transport =
                ::media::Result<std::unique_ptr<MediaRtpUdpSenderTransport>>::failure(
                    ::media::ErrorInfo::internalError("unset"));
            try {
                transport = MediaRtpUdpSenderTransport::create(
                    std::move(config.value()), factory);
            } catch (...) {
                escaped = true;
            }
            EXPECT_FALSE(ctx, escaped);
            EXPECT_FALSE(ctx, transport);
        }
    }
    {
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        FakeSenderPortFactory factory(rtp, rtcp);
        factory.throwAt = 1;
        auto config = makeConfig();
        EXPECT_TRUE(ctx, config);
        if (config) {
            bool escaped = false;
            try {
                EXPECT_FALSE(ctx, MediaRtpUdpSenderTransport::create(
                    std::move(config.value()), factory));
            } catch (...) {
                escaped = true;
            }
            EXPECT_FALSE(ctx, escaped);
            EXPECT_EQ(ctx, factory.createCalls, 1);
        }
    }
    {
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        rtcp->throwOnOpen = true;
        FakeSenderPortFactory factory(rtp, rtcp);
        auto config = makeConfig();
        EXPECT_TRUE(ctx, config);
        if (!config) return;
        auto transport = MediaRtpUdpSenderTransport::create(
            std::move(config.value()), factory);
        EXPECT_TRUE(ctx, transport);
        if (!transport) return;
        EXPECT_FALSE(ctx, transport.value()->open());
        EXPECT_EQ(ctx, transport.value()->state(),
                  MediaRtpUdpSenderTransportState::Poisoned);
        EXPECT_EQ(ctx, rtp->closeCalls, 1);
        EXPECT_EQ(ctx, rtcp->closeCalls, 1);
    }
    {
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        auto duplicate = MediaUdpDatagramEndpoint::create(
            MediaIpAddressFamily::Ipv4, "127.0.0.1", 42'000);
        EXPECT_TRUE(ctx, duplicate);
        if (!duplicate) return;
        rtp->scriptedBoundEndpoint = duplicate.value();
        rtcp->scriptedBoundEndpoint = duplicate.value();
        FakeSenderPortFactory factory(rtp, rtcp);
        auto config = makeConfig();
        EXPECT_TRUE(ctx, config);
        if (!config) return;
        auto transport = MediaRtpUdpSenderTransport::create(
            std::move(config.value()), factory);
        EXPECT_TRUE(ctx, transport);
        if (!transport) return;
        EXPECT_FALSE(ctx, transport.value()->open());
        EXPECT_EQ(ctx, transport.value()->state(),
                  MediaRtpUdpSenderTransportState::Poisoned);
        EXPECT_FALSE(ctx, transport.value()->boundLocalEndpoints().has_value());
    }
    {
        auto fixed = MediaRtpUdpLocalPortPolicy::fixedAdjacent(6'000, 6'001);
        EXPECT_TRUE(ctx, fixed);
        if (!fixed) return;
        auto rtp = std::make_shared<FakePortState>();
        auto rtcp = std::make_shared<FakePortState>();
        FakeSenderPortFactory factory(rtp, rtcp);
        auto config = makeConfig(std::move(fixed.value()));
        EXPECT_TRUE(ctx, config);
        if (!config) return;
        auto transport = MediaRtpUdpSenderTransport::create(
            std::move(config.value()), factory);
        EXPECT_TRUE(ctx, transport);
        if (!transport) return;
        EXPECT_TRUE(ctx, transport.value()->open());
        const auto bound = transport.value()->boundLocalEndpoints();
        EXPECT_TRUE(ctx, bound.has_value());
        if (bound) {
            EXPECT_EQ(ctx, bound->rtp().port(), std::uint16_t{6'000});
            EXPECT_EQ(ctx, bound->rtcp().port(), std::uint16_t{6'001});
        }
    }
}

void testDeliveryCertaintyAndStickyPoison(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    releasePort(rtp);
    releasePort(rtcp);
    rtp->outcomes.push_back(MediaUdpDatagramSendOutcome::notAccepted(
        ::media::ErrorInfo::wouldBlock("send pressure")));
    rtp->outcomes.push_back(MediaUdpDatagramSendOutcome::accepted(3));
    rtcp->outcomes.push_back(MediaUdpDatagramSendOutcome::ambiguousPartial(
        ::media::ErrorInfo::ioFailure("positive short send", 3), 1));
    FakeSenderPortFactory factory(rtp, rtcp);
    auto config = makeConfig();
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    EXPECT_TRUE(ctx, transport);
    if (!transport || !transport.value()->open()) return;
    const std::vector<std::uint8_t> datagram{1, 2, 3};
    const auto safeFailure = transport.value()->sendRtp(datagram);
    EXPECT_FALSE(ctx, safeFailure);
    EXPECT_EQ(ctx, transport.value()->state(), MediaRtpUdpSenderTransportState::Open);
    EXPECT_TRUE(ctx, transport.value()->sendRtp(datagram));
    bool ambiguousThrown = false;
    try {
        (void)transport.value()->sendRtcp(datagram);
    } catch (const MediaUdpAmbiguousDeliveryError& error) {
        ambiguousThrown = true;
        EXPECT_EQ(ctx, error.acceptedBytes(), static_cast<std::size_t>(1));
    }
    EXPECT_TRUE(ctx, ambiguousThrown);
    EXPECT_EQ(ctx, transport.value()->state(), MediaRtpUdpSenderTransportState::Poisoned);
    EXPECT_FALSE(ctx, transport.value()->sendRtcp(datagram));
    EXPECT_EQ(ctx, rtcp->sendCalls, 1);
}

void testReentryAndCrossThreadClose(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    releasePort(rtcp);
    FakeSenderPortFactory factory(rtp, rtcp);
    auto config = makeConfig();
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    EXPECT_TRUE(ctx, transport);
    if (!transport || !transport.value()->open()) return;
    MediaRtpUdpSenderTransport* raw = transport.value().get();
    bool nestedSendRejected = false;
    bool nestedCloseRejected = false;
    rtp->onSend = [&] {
        const std::vector<std::uint8_t> nested{9};
        nestedSendRejected = !raw->sendRtcp(nested);
        nestedCloseRejected = !raw->close();
    };
    const std::vector<std::uint8_t> datagram{1, 2, 3};
    std::atomic<bool> sendFinished = false;
    std::thread sender([&] {
        (void)raw->sendRtp(datagram);
        sendFinished = true;
    });
    {
        std::unique_lock lock(rtp->blockMutex);
        rtp->blockCondition.wait(lock, [&] { return rtp->sendEntered; });
    }
    std::atomic<bool> closeFinished = false;
    std::thread closer([&] {
        (void)raw->close();
        closeFinished = true;
    });
    std::this_thread::yield();
    EXPECT_FALSE(ctx, closeFinished.load());
    const auto concurrentSend = raw->sendRtcp(datagram);
    EXPECT_FALSE(ctx, concurrentSend);
    EXPECT_EQ(ctx, rtcp->sendCalls, 0);
    releasePort(rtp);
    sender.join();
    closer.join();
    EXPECT_TRUE(ctx, sendFinished.load());
    EXPECT_TRUE(ctx, closeFinished.load());
    EXPECT_TRUE(ctx, nestedSendRejected);
    EXPECT_TRUE(ctx, nestedCloseRejected);
    EXPECT_EQ(ctx, rtcp->sendCalls, 0);
    EXPECT_EQ(ctx, raw->state(), MediaRtpUdpSenderTransportState::Closed);
}

void testConcurrentCloseWaitsForPhysicalPortClose(TestContext& ctx)
{
    auto rtp = std::make_shared<FakePortState>();
    auto rtcp = std::make_shared<FakePortState>();
    rtp->blockClose = true;
    FakeSenderPortFactory factory(rtp, rtcp);
    auto config = makeConfig();
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    auto transport = MediaRtpUdpSenderTransport::create(
        std::move(config.value()), factory);
    EXPECT_TRUE(ctx, transport);
    if (!transport || !transport.value()->open()) return;
    MediaRtpUdpSenderTransport* raw = transport.value().get();
    bool reentrantCloseRejected = false;
    bool stateObserved = false;
    bool endpointsObserved = false;
    rtp->onClose = [&] {
        reentrantCloseRejected = !raw->close();
        stateObserved = raw->state() == MediaRtpUdpSenderTransportState::Open;
        endpointsObserved = !raw->boundLocalEndpoints().has_value();
    };
    std::atomic<bool> firstFinished = false;
    std::atomic<bool> secondFinished = false;
    std::mutex secondMutex;
    std::condition_variable secondCondition;
    std::thread first([&] {
        (void)transport.value()->close();
        firstFinished = true;
    });
    {
        std::unique_lock lock(rtp->blockMutex);
        rtp->blockCondition.wait(lock, [&] { return rtp->closeEntered; });
    }
    const std::vector<std::uint8_t> datagram{1, 2, 3};
    EXPECT_FALSE(ctx, raw->sendRtp(datagram));
    EXPECT_FALSE(ctx, raw->open());
    EXPECT_EQ(ctx, rtp->sendCalls, 0);
    EXPECT_EQ(ctx, rtp->openCalls, 1);
    std::thread second([&] {
        (void)transport.value()->close();
        secondFinished = true;
        secondCondition.notify_all();
    });
    bool secondReturnedBeforeRelease = false;
    {
        std::unique_lock lock(secondMutex);
        secondReturnedBeforeRelease = secondCondition.wait_for(
            lock, std::chrono::milliseconds(100),
            [&] { return secondFinished.load(); });
    }
    EXPECT_FALSE(ctx, firstFinished.load());
    EXPECT_FALSE(ctx, secondReturnedBeforeRelease);
    {
        std::lock_guard lock(rtp->blockMutex);
        rtp->releaseClose = true;
        rtp->blockCondition.notify_all();
    }
    first.join();
    second.join();
    EXPECT_TRUE(ctx, firstFinished.load());
    EXPECT_TRUE(ctx, secondFinished.load());
    EXPECT_TRUE(ctx, reentrantCloseRejected);
    EXPECT_TRUE(ctx, stateObserved);
    EXPECT_TRUE(ctx, endpointsObserved);
    EXPECT_EQ(ctx, transport.value()->state(),
              MediaRtpUdpSenderTransportState::Closed);
}

} // namespace

void runMediaRtpUdpSenderTransportTests(TestContext& ctx)
{
    testConfigIsCompleteAndStrict(ctx);
    testOpenPublishesAndRoutesAtomically(ctx);
    testOpenFailureRollsBackAndPoisons(ctx);
    testFirstPortOpenFailuresNeverReachRtcp(ctx);
    testEndpointGetterThrowRollsBackAndPoisons(ctx);
    testFactoryAndBoundEndpointFailuresAreRejected(ctx);
    testDeliveryCertaintyAndStickyPoison(ctx);
    testReentryAndCrossThreadClose(ctx);
    testConcurrentCloseWaitsForPhysicalPortClose(ctx);
}
