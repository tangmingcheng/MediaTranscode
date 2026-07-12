#include "common/EventRuntimeTestCases.h"

#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpSocket.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <span>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

class BlockingTransportOperationObserver final
    : public media::ffmpeg::graph::MediaRtpUdpTransportOperationObserver {
public:
    explicit BlockingTransportOperationObserver(
        media::ffmpeg::graph::MediaRtpUdpTransportOperation operation) noexcept
        : m_operation(operation)
    {
    }

    void afterLifetimeAcquired(
        media::ffmpeg::graph::MediaRtpUdpTransportOperation operation) noexcept override
    {
        if (operation != m_operation) return;
        std::unique_lock lock(m_mutex);
        m_entered = true;
        m_condition.notify_all();
        m_condition.wait(lock, [this] { return m_released; });
    }

    bool waitUntilEntered()
    {
        std::unique_lock lock(m_mutex);
        return m_condition.wait_for(lock, std::chrono::seconds(1),
                                    [this] { return m_entered; });
    }

    void release()
    {
        std::lock_guard lock(m_mutex);
        m_released = true;
        m_condition.notify_all();
    }

private:
    media::ffmpeg::graph::MediaRtpUdpTransportOperation m_operation;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_entered = false;
    bool m_released = false;
};

namespace {

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

#ifdef _WIN32
uint64_t fileTimeTicks(const FILETIME& value)
{
    return (static_cast<uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}
#endif

void testUdpSocketLifecycleAndBindFailure(TestContext& ctx)
{
#ifndef _WIN32
    const auto unsupportedRuntime = MediaSocketRuntime::create();
    EXPECT_FALSE(ctx, unsupportedRuntime);
    if (!unsupportedRuntime) EXPECT_EQ(ctx, unsupportedRuntime.error().code, media::ErrorCode::Unsupported);
    return;
#endif
    auto runtime = MediaSocketRuntime::create();
    EXPECT_TRUE(ctx, runtime);
    if (!runtime) return;
    const MediaUdpSocketConfig config{MediaIpAddressFamily::Ipv4, "127.0.0.1", 0, 262144};
    auto first = MediaUdpSocket::bind(runtime.value(), config);
    EXPECT_TRUE(ctx, first);
    if (!first) return;
    EXPECT_TRUE(ctx, first.value().isOpen());
    EXPECT_TRUE(ctx, first.value().localPort() != 0);

    auto replacement = MediaUdpSocket::bind(runtime.value(), config);
    EXPECT_TRUE(ctx, replacement);
    if (!replacement) return;
    const uint16_t replacedPort = replacement.value().localPort();
    replacement.value() = std::move(first.value());
    auto rebound = MediaUdpSocket::bind(runtime.value(), MediaUdpSocketConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", replacedPort, 262144});
    EXPECT_TRUE(ctx, rebound);

    const MediaUdpSocketConfig duplicate{MediaIpAddressFamily::Ipv4, "127.0.0.1",
                                         replacement.value().localPort(), 262144};
    auto second = MediaUdpSocket::bind(runtime.value(), duplicate);
    EXPECT_FALSE(ctx, second);
    replacement.value().close();
    EXPECT_FALSE(ctx, replacement.value().isOpen());
}

void testRtpUdpTransportReceivesBothChannelsAndCancels(TestContext& ctx)
{
    auto transport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 0, 0, 262144, 2048, 5'000, nullptr});
    EXPECT_FALSE(ctx, transport);
    EXPECT_FALSE(ctx, MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 40'001, 40'002, 262144, 2048, 5'000, nullptr}));
    EXPECT_FALSE(ctx, MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 40'000, 40'004, 262144, 2048, 5'000, nullptr}));
    for (uint16_t port = 40'000; !transport && port < 41'000; port = static_cast<uint16_t>(port + 2)) {
        transport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
            MediaIpAddressFamily::Ipv4, "127.0.0.1", port, static_cast<uint16_t>(port + 1),
            262144, 2048, 5'000, nullptr});
    }
    EXPECT_TRUE(ctx, transport);
    if (!transport) return;
    EXPECT_TRUE(ctx, transport.value().rtpPort() != 0);
    EXPECT_TRUE(ctx, transport.value().rtcpPort() != 0);
    EXPECT_TRUE(ctx, transport.value().rtpPort() != transport.value().rtcpPort());

    auto runtime = MediaSocketRuntime::create();
    EXPECT_TRUE(ctx, runtime);
    if (!runtime) return;
    auto sender = MediaUdpSocket::bind(runtime.value(),
        MediaUdpSocketConfig{MediaIpAddressFamily::Ipv4, "127.0.0.1", 0, 65536});
    EXPECT_TRUE(ctx, sender);
    if (!sender) return;
    const std::array<uint8_t, 3> rtp{1, 2, 3};
    const std::array<uint8_t, 2> rtcp{4, 5};
    EXPECT_TRUE(ctx, sender.value().sendTo("127.0.0.1", transport.value().rtpPort(), rtp));
    EXPECT_TRUE(ctx, sender.value().sendTo("127.0.0.1", transport.value().rtcpPort(), rtcp));

    auto first = transport.value().receive();
    auto second = transport.value().receive();
    EXPECT_TRUE(ctx, first);
    EXPECT_TRUE(ctx, second);
    if (first && second) {
        EXPECT_TRUE(ctx, first.value().channel != second.value().channel);
        EXPECT_TRUE(ctx, (first.value().channel == MediaRtpUdpChannel::Rtp && first.value().bytes == std::vector<uint8_t>(rtp.begin(), rtp.end())) ||
                         (second.value().channel == MediaRtpUdpChannel::Rtp && second.value().bytes == std::vector<uint8_t>(rtp.begin(), rtp.end())));
    }

    EXPECT_FALSE(ctx, transport.value().reset());
    std::promise<void> entered;
    std::promise<::media::Result<MediaRtpUdpDatagram>> receivePromise;
    auto blockedReceive = receivePromise.get_future();
    std::thread receiveThread([&] {
        entered.set_value();
        receivePromise.set_value(transport.value().receive());
    });
    entered.get_future().wait();
    EXPECT_EQ(ctx, blockedReceive.wait_for(std::chrono::milliseconds(25)), std::future_status::timeout);
#ifdef _WIN32
    FILETIME creation{}, exit{}, kernelBefore{}, userBefore{};
    FILETIME kernelAfter{}, userAfter{};
    EXPECT_TRUE(ctx, GetThreadTimes(receiveThread.native_handle(), &creation, &exit,
                                    &kernelBefore, &userBefore) != 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    EXPECT_TRUE(ctx, GetThreadTimes(receiveThread.native_handle(), &creation, &exit,
                                    &kernelAfter, &userAfter) != 0);
    const uint64_t cpuTicks = fileTimeTicks(kernelAfter) + fileTimeTicks(userAfter) -
                              fileTimeTicks(kernelBefore) - fileTimeTicks(userBefore);
    EXPECT_TRUE(ctx, cpuTicks < 50'000); // less than 5 ms CPU while blocked
#endif
    const auto stopStarted = std::chrono::steady_clock::now();
    EXPECT_TRUE(ctx, transport.value().stop());
    EXPECT_EQ(ctx, blockedReceive.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    const auto cancelled = blockedReceive.get();
    receiveThread.join();
    EXPECT_TRUE(ctx, std::chrono::steady_clock::now() - stopStarted < std::chrono::seconds(1));
    EXPECT_FALSE(ctx, cancelled);
    if (!cancelled) EXPECT_EQ(ctx, cancelled.error().code, media::ErrorCode::Cancelled);
    EXPECT_TRUE(ctx, transport.value().reset());

    EXPECT_TRUE(ctx, sender.value().sendTo("127.0.0.1", transport.value().rtpPort(), rtp));
    EXPECT_TRUE(ctx, transport.value().stop());
    const auto cancelledOverReadyNetwork = transport.value().receive();
    EXPECT_FALSE(ctx, cancelledOverReadyNetwork);
    if (!cancelledOverReadyNetwork) {
        EXPECT_EQ(ctx, cancelledOverReadyNetwork.error().code, media::ErrorCode::Cancelled);
    }
    EXPECT_TRUE(ctx, transport.value().reset());
    EXPECT_TRUE(ctx, transport.value().receive());

    std::promise<void> sparseEntered;
    auto sparseReceive = std::async(std::launch::async, [&] {
        sparseEntered.set_value();
        return transport.value().receive();
    });
    sparseEntered.get_future().wait();
    EXPECT_EQ(ctx, sparseReceive.wait_for(std::chrono::milliseconds(25)), std::future_status::timeout);
    EXPECT_TRUE(ctx, sender.value().sendTo("127.0.0.1", transport.value().rtpPort(), rtp));
    const auto afterReset = sparseReceive.get();
    EXPECT_TRUE(ctx, afterReset);
    if (!afterReset) std::cerr << afterReset.error().describe() << '\n';

    auto replacementTransport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 41'000, 41'001, 262144, 2048, 5'000, nullptr});
    EXPECT_TRUE(ctx, replacementTransport);
    if (replacementTransport) {
        const uint16_t replacedRtpPort = replacementTransport.value().rtpPort();
        replacementTransport.value() = std::move(transport.value());
        auto reboundTransport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
            MediaIpAddressFamily::Ipv4, "127.0.0.1", replacedRtpPort,
            static_cast<uint16_t>(replacedRtpPort + 1), 262144, 2048, 5'000, nullptr});
        EXPECT_TRUE(ctx, reboundTransport);
        transport.value() = std::move(replacementTransport.value());
    }

    std::promise<void> closeEntered;
    auto closeReceive = std::async(std::launch::async, [&] {
        closeEntered.set_value();
        return transport.value().receive();
    });
    closeEntered.get_future().wait();
    EXPECT_EQ(ctx, closeReceive.wait_for(std::chrono::milliseconds(25)), std::future_status::timeout);
    EXPECT_TRUE(ctx, transport.value().stop());
    transport.value().close();
    const auto closedReceive = closeReceive.get();
    EXPECT_FALSE(ctx, closedReceive);
    if (!closedReceive) EXPECT_EQ(ctx, closedReceive.error().code, media::ErrorCode::Cancelled);
    EXPECT_FALSE(ctx, transport.value().isOpen());
}

::media::Result<MediaRtpUdpTransport> openObservedTransport(
    std::shared_ptr<MediaRtpUdpTransportOperationObserver> observer,
    uint16_t firstPort)
{
#ifndef _WIN32
    return ::media::Result<MediaRtpUdpTransport>::failure(
        ::media::ErrorInfo::unsupported("Windows transport test"));
#else
    ::media::Result<MediaRtpUdpTransport> transport =
        ::media::Result<MediaRtpUdpTransport>::failure(
            ::media::ErrorInfo::ioFailure("test transport was not opened"));
    for (uint16_t port = firstPort; !transport && port < firstPort + 500;
         port = static_cast<uint16_t>(port + 2)) {
        transport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
            MediaIpAddressFamily::Ipv4, "127.0.0.1", port,
            static_cast<uint16_t>(port + 1), 262144, 2048, 5'000,
            observer});
    }
    return transport;
#endif
}

void testRtpUdpTransportReceiveEntryRacesCloseDeterministically(TestContext& ctx)
{
#ifndef _WIN32
    return;
#else
    auto observer = std::make_shared<BlockingTransportOperationObserver>(
        MediaRtpUdpTransportOperation::Receive);
    auto transport = openObservedTransport(observer, 42'000);
    EXPECT_TRUE(ctx, transport);
    if (!transport) return;

    auto receive = std::async(std::launch::async, [&] {
        return transport.value().receive();
    });
    EXPECT_TRUE(ctx, observer->waitUntilEntered());
    auto close = std::async(std::launch::async, [&] {
        transport.value().close();
    });
    EXPECT_EQ(ctx, close.wait_for(std::chrono::milliseconds(25)),
              std::future_status::timeout);
    observer->release();
    EXPECT_EQ(ctx, receive.wait_for(std::chrono::seconds(1)),
              std::future_status::ready);
    const auto result = receive.get();
    EXPECT_FALSE(ctx, result);
    if (!result) EXPECT_EQ(ctx, result.error().code, media::ErrorCode::Cancelled);
    EXPECT_EQ(ctx, close.wait_for(std::chrono::seconds(1)),
              std::future_status::ready);
    close.get();
#endif
}

void testRtpUdpTransportStopAndAbortRaceCloseDeterministically(TestContext& ctx)
{
#ifndef _WIN32
    return;
#else
    const auto run = [&](MediaRtpUdpTransportOperation operation,
                         uint16_t firstPort) {
        auto observer = std::make_shared<BlockingTransportOperationObserver>(operation);
        auto transport = openObservedTransport(observer, firstPort);
        EXPECT_TRUE(ctx, transport);
        if (!transport) return;

        auto lifecycle = std::async(std::launch::async, [&] {
            return operation == MediaRtpUdpTransportOperation::Stop
                ? transport.value().stop()
                : transport.value().abort();
        });
        EXPECT_TRUE(ctx, observer->waitUntilEntered());
        transport.value().close();
        observer->release();
        EXPECT_EQ(ctx, lifecycle.wait_for(std::chrono::seconds(1)),
                  std::future_status::ready);
        const auto status = lifecycle.get();
        EXPECT_FALSE(ctx, status);
        if (!status) EXPECT_EQ(ctx, status.error().code, media::ErrorCode::Cancelled);
    };

    run(MediaRtpUdpTransportOperation::Stop, 42'500);
    run(MediaRtpUdpTransportOperation::Abort, 43'000);
#endif
}

} // namespace

int main()
{
    media_transcode::test::TestContext ctx;
    testUdpSocketLifecycleAndBindFailure(ctx);
    testRtpUdpTransportReceivesBothChannelsAndCancels(ctx);
    testRtpUdpTransportReceiveEntryRacesCloseDeterministically(ctx);
    testRtpUdpTransportStopAndAbortRaceCloseDeterministically(ctx);
    runEventRuntimeThreadingQueueTests(ctx);
    runEventRuntimeFfmpegOwnershipTests(ctx);
    runEventRuntimeMultiInputTests(ctx);
    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " event runtime expectation(s) failed\n";
        return 1;
    }
    std::cout << "event runtime tests passed\n";
    return 0;
}
