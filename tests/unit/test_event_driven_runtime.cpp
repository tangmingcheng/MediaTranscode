#include "common/EventRuntimeTestCases.h"

#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpSocket.h"

#include <array>
#include <future>
#include <iostream>
#include <span>

namespace {

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

void testUdpSocketLifecycleAndBindFailure(TestContext& ctx)
{
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
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 0, 0, 262144, 2048, 5'000});
    EXPECT_FALSE(ctx, transport);
    EXPECT_FALSE(ctx, MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 40'001, 40'002, 262144, 2048, 5'000}));
    EXPECT_FALSE(ctx, MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
        MediaIpAddressFamily::Ipv4, "127.0.0.1", 40'000, 40'004, 262144, 2048, 5'000}));
    for (uint16_t port = 40'000; !transport && port < 41'000; port = static_cast<uint16_t>(port + 2)) {
        transport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
            MediaIpAddressFamily::Ipv4, "127.0.0.1", port, static_cast<uint16_t>(port + 1),
            262144, 2048, 5'000});
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

    auto blockedReceive = std::async(std::launch::async, [&transport] {
        return transport.value().receive();
    });
    transport.value().stop();
    const auto cancelled = blockedReceive.get();
    EXPECT_FALSE(ctx, cancelled);
    if (!cancelled) EXPECT_EQ(ctx, cancelled.error().code, media::ErrorCode::Cancelled);
    EXPECT_TRUE(ctx, transport.value().reset());
    EXPECT_TRUE(ctx, sender.value().sendTo("127.0.0.1", transport.value().rtpPort(), rtp));
    const auto afterReset = transport.value().receive();
    EXPECT_TRUE(ctx, afterReset);

    transport.value().abort();
    const auto aborted = transport.value().receive();
    EXPECT_FALSE(ctx, aborted);
    transport.value().close();
    EXPECT_FALSE(ctx, transport.value().isOpen());
}

} // namespace

int main()
{
    media_transcode::test::TestContext ctx;
    testUdpSocketLifecycleAndBindFailure(ctx);
    testRtpUdpTransportReceivesBothChannelsAndCancels(ctx);
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
