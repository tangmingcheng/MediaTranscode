#include "common/TestAssert.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/input/RawRtpInputNode.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <cstdint>
#include <optional>
#include <string>

using media_transcode::test::TestContext;

namespace media::ffmpeg::graph {
namespace {

constexpr int ReceiveBufferBytes = 256 * 1024;
constexpr std::size_t MaximumDatagramBytes = 65'535;
constexpr int ReadTimeoutMs = 20;

MediaRtpUdpTransportConfig transportConfig(std::uint16_t rtpPort)
{
    return MediaRtpUdpTransportConfig{
        MediaIpAddressFamily::Ipv4,
        "127.0.0.1",
        rtpPort,
        static_cast<std::uint16_t>(rtpPort + 1),
        ReceiveBufferBytes,
        MaximumDatagramBytes,
        ReadTimeoutMs,
        nullptr};
}

std::optional<std::uint16_t> findAvailablePortPair()
{
    for (std::uint32_t candidate = 30'000; candidate <= 60'000; candidate += 2) {
        auto transport = MediaRtpUdpTransport::open(
            transportConfig(static_cast<std::uint16_t>(candidate)));
        if (!transport) continue;
        transport.value().close();
        return static_cast<std::uint16_t>(candidate);
    }
    return std::nullopt;
}

MediaNodeOptions rawAacOptions(std::uint16_t rtpPort)
{
    MediaNodeOptions options;
    options.set("rtp.address_family", "ipv4");
    options.set("rtp.bind_address", "127.0.0.1");
    options.set("rtp.port", std::to_string(rtpPort));
    options.set("rtcp.port", std::to_string(rtpPort + 1));
    options.set("rtp.payload_type", "97");
    options.set("rtp.clock_rate", "44100");
    options.set("rtp.receive_buffer_bytes", std::to_string(ReceiveBufferBytes));
    options.set("rtp.maximum_datagram_bytes", std::to_string(MaximumDatagramBytes));
    options.set("rtp.reorder_window_packets", "64");
    options.set("rtp.maximum_reorder_delay_ms", "50");
    options.set("rtp.cancellable_read_timeout_ms", std::to_string(ReadTimeoutMs));
    options.set("rtcp.require_sender_reports", "1");
    options.set("rtcp.require_cname", "1");
    options.set("rtcp.sender_report_timeout_ms", "5000");
    options.set("rtcp.cname_timeout_ms", "5000");
    options.set("rtcp.composition_mode", "strict_compound_rfc3550");
    options.set("rtp.stream_kind", "audio");
    options.set("rtp.codec", "aac");
    options.set("rtp.fmtp",
                "mode=AAC-hbr;sizelength=13;indexlength=3;"
                "indexdeltalength=3;config=1210");
    options.set("rtp.channels", "2");
    options.set("rtp.access_unit_duration_ticks", "1024");
    return options;
}

void startPublishesReceiverReadinessOnlyAfterBothPortsAreBound(TestContext& ctx)
{
    const auto rtpPort = findAvailablePortPair();
    EXPECT_TRUE(ctx, rtpPort.has_value());
    if (!rtpPort) return;

    MediaGraph graph;
    const MediaNodeId nodeId = graph.addNode(MediaNodeKind::RawRtpInput, "raw-audio");
    EXPECT_TRUE(ctx, graph.setNodeOptions(nodeId, rawAacOptions(*rtpPort)));

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    if (!execution.compiled()) return;

    RawRtpInputNode node(nodeId);
    MediaRuntimeNode& runtime = node;
    const auto started = runtime.start(execution);
    EXPECT_TRUE(ctx, started);
    if (!started) return;

    auto competingReceiver = MediaRtpUdpTransport::open(transportConfig(*rtpPort));
    EXPECT_FALSE(ctx, competingReceiver);
    if (competingReceiver) competingReceiver.value().close();

    EXPECT_TRUE(ctx, runtime.stop(execution));
}

} // namespace
} // namespace media::ffmpeg::graph

void runRawRtpInputLifecycleTests(TestContext& ctx)
{
    media::ffmpeg::graph::startPublishesReceiverReadinessOnlyAfterBothPortsAreBound(ctx);
}
