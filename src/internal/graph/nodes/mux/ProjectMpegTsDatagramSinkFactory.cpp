#include "internal/graph/nodes/mux/ProjectMpegTsDatagramSinkFactory.h"

#include "internal/graph/protocol/mpegts/MediaTsUdpDatagramSink.h"
#include "internal/graph/protocol/mpegts/MediaTsScheduledDatagramSink.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"
#include "internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuilder.h"

#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

class GenerationBorrowedByteSink final : public MediaOutputByteSink {
public:
    explicit GenerationBorrowedByteSink(MediaOutputByteSink& sink) noexcept
        : m_sink(sink)
    {
    }

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override
    {
        return m_sink.write(bytes);
    }

    ::media::Status flush() override { return m_sink.flush(); }
    ::media::Status close() override { return ::media::Status::success(); }

private:
    MediaOutputByteSink& m_sink;
};

} // namespace

::media::Result<bool>
ProjectMpegTsDatagramSinkFactory::bindingsReady(
    const MediaTsMuxPlan& muxPlan,
    const MediaOutputByteSink* udpByteSink)
{
    switch (muxPlan.parameters().transportKind) {
    case MediaOutputTransportKind::UdpDatagrams:
        return ::media::Result<bool>::success(udpByteSink != nullptr);
    case MediaOutputTransportKind::RtpAvp:
        if (udpByteSink) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS RTP binding rejects a UDP byte sink"));
        }
        return ::media::Result<bool>::success(true);
    }
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS binding contains an unsupported transport"));
}

::media::Result<ProjectMpegTsDatagramBinding>
ProjectMpegTsDatagramSinkFactory::create(
    const MediaProjectMpegTsRuntimeOutputPlan& outputPlan,
    const MediaTsMuxPlan& muxPlan,
    const MediaProtocolOutputActivation& activation,
    MediaOutputByteSink* udpByteSink)
{
    if (const auto* udp = std::get_if<MediaMpegTsUdpOutputPlan>(
            &outputPlan.transport)) {
        if (muxPlan.parameters().transportKind !=
                MediaOutputTransportKind::UdpDatagrams ||
            !udpByteSink) {
            return ::media::Result<ProjectMpegTsDatagramBinding>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS UDP sink requires only its exact byte-sink binding"));
        }
        (void)udp;
        auto created = MediaTsUdpDatagramSink::create(
            std::make_unique<GenerationBorrowedByteSink>(*udpByteSink));
        if (!created) {
            return ::media::Result<ProjectMpegTsDatagramBinding>::failure(
                created.error());
        }
        return ::media::Result<ProjectMpegTsDatagramBinding>::success(
            ProjectMpegTsDatagramBinding{
                std::move(created).value(), {}});
    }

    const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
        &outputPlan.transport);
    if (!rtp ||
        muxPlan.parameters().transportKind !=
            MediaOutputTransportKind::RtpAvp ||
        udpByteSink) {
        return ::media::Result<ProjectMpegTsDatagramBinding>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS RTP mux requires its exact scheduled batch binding"));
    }
    (void)rtp;
    auto builder = MediaScheduledDatagramBatchBuilder::create(
        activation.generation,
        outputPlan.scheduledBatchMaximumBytes);
    if (!builder) {
        return ::media::Result<ProjectMpegTsDatagramBinding>::failure(
            builder.error());
    }
    auto created = MediaTsScheduledDatagramSink::create(
        builder.value(), muxPlan.parameters().packetSize);
    if (!created) {
        return ::media::Result<ProjectMpegTsDatagramBinding>::failure(
            created.error());
    }
    return ::media::Result<ProjectMpegTsDatagramBinding>::success(
        ProjectMpegTsDatagramBinding{
            std::move(created).value(), std::move(builder).value()});
}

} // namespace media::ffmpeg::graph
