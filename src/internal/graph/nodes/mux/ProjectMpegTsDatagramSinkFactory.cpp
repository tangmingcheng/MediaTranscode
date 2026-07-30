#include "internal/graph/nodes/mux/ProjectMpegTsDatagramSinkFactory.h"

#include "internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.h"
#include "internal/graph/protocol/mpegts/MediaTsUdpDatagramSink.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"

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

::media::Result<std::unique_ptr<MediaTsDatagramSink>>
ProjectMpegTsDatagramSinkFactory::create(
    const MediaProjectMpegTsRuntimeOutputPlan& outputPlan,
    const MediaPlaybackEpoch& epoch,
    const MediaSharedNtpEpoch* sharedNtpEpoch,
    MediaUdpDatagramSenderPortFactory& datagramPortFactory,
    MediaOutputByteSink* udpByteSink)
{
    const auto muxTransport =
        outputPlan.protocol.muxPlan().parameters().transportKind;
    if (const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
            &outputPlan.transport)) {
        if (muxTransport != MediaOutputTransportKind::RtpAvp ||
            !sharedNtpEpoch || udpByteSink) {
            return ::media::Result<
                std::unique_ptr<MediaTsDatagramSink>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS RTP sink binding differs from its immutable transport plan"));
        }
        auto created = MediaMpegTsRtpDatagramSink::create(
            *rtp, epoch, *sharedNtpEpoch, datagramPortFactory);
        if (!created) {
            return ::media::Result<
                std::unique_ptr<MediaTsDatagramSink>>::failure(
                created.error());
        }
        return ::media::Result<
            std::unique_ptr<MediaTsDatagramSink>>::success(
            std::move(created).value());
    }
    if (!std::holds_alternative<MediaMpegTsUdpOutputPlan>(
            outputPlan.transport) ||
        muxTransport != MediaOutputTransportKind::UdpDatagrams ||
        !udpByteSink) {
        return ::media::Result<
            std::unique_ptr<MediaTsDatagramSink>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS UDP sink binding differs from its immutable transport plan"));
    }
    auto created = MediaTsUdpDatagramSink::create(
        std::make_unique<GenerationBorrowedByteSink>(*udpByteSink));
    if (!created) {
        return ::media::Result<
            std::unique_ptr<MediaTsDatagramSink>>::failure(
            created.error());
    }
    return ::media::Result<
        std::unique_ptr<MediaTsDatagramSink>>::success(
        std::move(created).value());
}

} // namespace media::ffmpeg::graph
