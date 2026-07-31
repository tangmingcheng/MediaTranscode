#include "internal/graph/nodes/mux/ProjectMpegTsDatagramSinkFactory.h"

#include "internal/graph/nodes/output/MediaMpegTsRtpDatagramSink.h"
#include "internal/graph/protocol/mpegts/MediaTsUdpDatagramSink.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderSocket.h"

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
    const std::shared_ptr<const MediaSharedNtpEpoch>& sharedNtpEpoch,
    const MediaOutputByteSink* udpByteSink)
{
    switch (muxPlan.parameters().transportKind) {
    case MediaOutputTransportKind::UdpDatagrams:
        if (sharedNtpEpoch) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS UDP binding rejects an RTP NTP epoch"));
        }
        return ::media::Result<bool>::success(udpByteSink != nullptr);
    case MediaOutputTransportKind::RtpAvp:
        if (udpByteSink) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS RTP binding rejects a UDP byte sink"));
        }
        return ::media::Result<bool>::success(
            static_cast<bool>(sharedNtpEpoch));
    }
    return ::media::Result<bool>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS binding contains an unsupported transport"));
}

::media::Result<std::unique_ptr<MediaTsDatagramSink>>
ProjectMpegTsDatagramSinkFactory::create(
    const MediaProjectMpegTsRuntimeOutputPlan& outputPlan,
    const MediaTsMuxPlan& muxPlan,
    const MediaPlaybackEpoch& epoch,
    const std::shared_ptr<const MediaSharedNtpEpoch>& sharedNtpEpoch,
    const std::shared_ptr<MediaMpegTsRtpContinuityState>& rtpContinuity,
    MediaOutputByteSink* udpByteSink)
{
    if (const auto* udp = std::get_if<MediaMpegTsUdpOutputPlan>(
            &outputPlan.transport)) {
        if (muxPlan.parameters().transportKind !=
                MediaOutputTransportKind::UdpDatagrams ||
            sharedNtpEpoch || rtpContinuity || !udpByteSink) {
            return ::media::Result<
                std::unique_ptr<MediaTsDatagramSink>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS UDP sink requires only its exact byte-sink binding"));
        }
        (void)udp;
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

    const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
        &outputPlan.transport);
    if (!rtp ||
        muxPlan.parameters().transportKind !=
            MediaOutputTransportKind::RtpAvp ||
        !sharedNtpEpoch || !rtpContinuity || udpByteSink) {
        return ::media::Result<
            std::unique_ptr<MediaTsDatagramSink>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS RTP sink requires only shared NTP and its exact transport binding"));
    }
    auto socketRuntime = MediaSocketRuntime::create();
    if (!socketRuntime) {
        return ::media::Result<
            std::unique_ptr<MediaTsDatagramSink>>::failure(
            socketRuntime.error());
    }
    MediaUdpDatagramSenderSocketFactory datagramPortFactory(
        std::move(socketRuntime).value());
    auto created = MediaMpegTsRtpDatagramSink::create(
        *rtp, epoch, *sharedNtpEpoch, rtpContinuity,
        datagramPortFactory);
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
