#include "internal/graph/runtime/network/MediaUdpDatagramSenderPort.h"
#include <utility>

namespace media::ffmpeg::graph {

MediaUdpDatagramEndpoint::MediaUdpDatagramEndpoint(
    MediaNumericIpAddress address,
    std::uint16_t port) noexcept
    : m_address(std::move(address)),
      m_port(port)
{
}

::media::Result<MediaUdpDatagramEndpoint> MediaUdpDatagramEndpoint::create(
    MediaIpAddressFamily addressFamily,
    std::string numericAddress,
    std::uint16_t port)
{
    auto address = MediaNumericIpAddress::create(
        addressFamily, std::move(numericAddress));
    if (!address) {
        return ::media::Result<MediaUdpDatagramEndpoint>::failure(
            address.error());
    }
    return ::media::Result<MediaUdpDatagramEndpoint>::success(
        MediaUdpDatagramEndpoint(std::move(address.value()), port));
}

MediaUdpDatagramSenderPortOpenRequest::MediaUdpDatagramSenderPortOpenRequest(
    MediaUdpDatagramEndpoint localEndpoint,
    MediaUdpDatagramEndpoint remoteEndpoint,
    int sendBufferBytes,
    std::size_t maximumDatagramBytes,
    MediaUdpSenderIoBehavior ioBehavior) noexcept
    : m_localEndpoint(std::move(localEndpoint)),
      m_remoteEndpoint(std::move(remoteEndpoint)),
      m_sendBufferBytes(sendBufferBytes),
      m_maximumDatagramBytes(maximumDatagramBytes),
      m_ioBehavior(ioBehavior)
{
}

::media::Result<MediaUdpDatagramSenderPortOpenRequest>
MediaUdpDatagramSenderPortOpenRequest::create(
    MediaUdpDatagramEndpoint localEndpoint,
    MediaUdpDatagramEndpoint remoteEndpoint,
    int sendBufferBytes,
    std::size_t maximumDatagramBytes,
    MediaUdpSenderIoBehavior ioBehavior)
{
    if (localEndpoint.addressFamily() != remoteEndpoint.addressFamily() ||
        remoteEndpoint.port() == 0 || sendBufferBytes <= 0 ||
        maximumDatagramBytes == 0 ||
        maximumDatagramBytes > kMediaUdpMaximumPayloadBytes ||
        ioBehavior != MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure) {
        return ::media::Result<MediaUdpDatagramSenderPortOpenRequest>::failure(
            ::media::ErrorInfo::invalidArgument(
                "invalid explicit UDP sender port open request"));
    }
    return ::media::Result<MediaUdpDatagramSenderPortOpenRequest>::success(
        MediaUdpDatagramSenderPortOpenRequest(
            std::move(localEndpoint), std::move(remoteEndpoint), sendBufferBytes,
            maximumDatagramBytes, ioBehavior));
}

MediaUdpDatagramSendOutcome::MediaUdpDatagramSendOutcome(
    MediaUdpDatagramSendKind kind,
    std::size_t acceptedBytes,
    std::optional<::media::ErrorInfo> error)
    : m_kind(kind), m_acceptedBytes(acceptedBytes), m_error(std::move(error))
{
}

MediaUdpDatagramSendOutcome MediaUdpDatagramSendOutcome::accepted(
    std::size_t acceptedBytes)
{
    return MediaUdpDatagramSendOutcome(
        MediaUdpDatagramSendKind::Accepted, acceptedBytes, std::nullopt);
}

MediaUdpDatagramSendOutcome MediaUdpDatagramSendOutcome::notAccepted(
    ::media::ErrorInfo error)
{
    return MediaUdpDatagramSendOutcome(
        MediaUdpDatagramSendKind::NotAccepted, 0, std::move(error));
}

MediaUdpDatagramSendOutcome MediaUdpDatagramSendOutcome::ambiguousPartial(
    ::media::ErrorInfo error, std::size_t acceptedBytes)
{
    return MediaUdpDatagramSendOutcome(
        MediaUdpDatagramSendKind::AmbiguousPartial, acceptedBytes,
        std::move(error));
}

MediaUdpAmbiguousDeliveryError::MediaUdpAmbiguousDeliveryError(
    ::media::ErrorInfo cause,
    std::size_t acceptedBytes)
    : std::runtime_error(cause.describe()),
      m_cause(std::move(cause)),
      m_acceptedBytes(acceptedBytes)
{
}

} // namespace media::ffmpeg::graph
