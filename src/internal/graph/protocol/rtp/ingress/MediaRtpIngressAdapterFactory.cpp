#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapterFactory.h"

#include "internal/graph/protocol/rtp/ingress/linux/MediaLinuxRtpIngressAdapter.h"
#include "internal/graph/protocol/rtp/ingress/windows/MediaWindowsRtpIngressAdapter.h"

namespace media::ffmpeg::graph {

::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>
MediaRtpIngressAdapterFactory::create(
    MediaRtpUdpTransport transport,
    const MediaRtpIngressPlan& plan)
{
    if (auto status = plan.validateProduct(); !status) {
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            status.error());
    }
    switch (plan.adapterKind()) {
    case MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages:
        return MediaLinuxRtpIngressAdapter::create(
            std::move(transport), plan);
    case MediaRtpIngressAdapterKind::WindowsRegisteredIo:
    case MediaRtpIngressAdapterKind::LinuxIoUringZeroCopy:
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::unsupported(
                "planner-selected RTP ingress adapter is not implemented"));
    case MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue:
        return MediaWindowsRtpIngressAdapter::create(
            std::move(transport), plan);
    default:
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP ingress plan contains an unknown adapter kind"));
    }
}

} // namespace media::ffmpeg::graph
