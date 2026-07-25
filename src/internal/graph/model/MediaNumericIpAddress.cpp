#include "internal/graph/model/MediaNumericIpAddress.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <utility>

namespace media::ffmpeg::graph {
namespace {

int nativeAddressFamily(MediaIpAddressFamily family) noexcept
{
    switch (family) {
    case MediaIpAddressFamily::Ipv4:
        return AF_INET;
    case MediaIpAddressFamily::Ipv6:
        return AF_INET6;
    }
    return AF_UNSPEC;
}

} // namespace

MediaNumericIpAddress::MediaNumericIpAddress(
    MediaIpAddressFamily addressFamily,
    std::string presentation,
    std::array<std::uint8_t, 16> canonicalBytes) noexcept
    : m_addressFamily(addressFamily),
      m_presentation(std::move(presentation)),
      m_canonicalBytes(canonicalBytes)
{
}

::media::Result<MediaNumericIpAddress> MediaNumericIpAddress::create(
    MediaIpAddressFamily addressFamily, std::string presentation)
{
    const int nativeFamily = nativeAddressFamily(addressFamily);
    std::array<std::uint8_t, 16> canonicalBytes{};
    if (nativeFamily == AF_UNSPEC ||
        presentation.find('\0') != std::string::npos) {
        return ::media::Result<MediaNumericIpAddress>::failure(
            ::media::ErrorInfo::invalidArgument(
                "numeric IP address has an unsupported family or embedded NUL"));
    }
#ifdef _WIN32
    const bool parsed = InetPtonA(
        nativeFamily, presentation.c_str(), canonicalBytes.data()) == 1;
#else
    const bool parsed = inet_pton(
        nativeFamily, presentation.c_str(), canonicalBytes.data()) == 1;
#endif
    if (!parsed) {
        return ::media::Result<MediaNumericIpAddress>::failure(
            ::media::ErrorInfo::invalidArgument(
                "numeric IP address does not match the selected family"));
    }
    std::array<char, INET6_ADDRSTRLEN> canonicalPresentation{};
#ifdef _WIN32
    const char* formatted = InetNtopA(
        nativeFamily, canonicalBytes.data(), canonicalPresentation.data(),
        static_cast<DWORD>(canonicalPresentation.size()));
#else
    const char* formatted = inet_ntop(
        nativeFamily, canonicalBytes.data(), canonicalPresentation.data(),
        canonicalPresentation.size());
#endif
    if (!formatted) {
        return ::media::Result<MediaNumericIpAddress>::failure(
            ::media::ErrorInfo::internalError(
                "failed to format a parsed numeric IP address"));
    }
    return ::media::Result<MediaNumericIpAddress>::success(
        MediaNumericIpAddress(
            addressFamily, std::string(formatted), canonicalBytes));
}

bool MediaNumericIpAddress::isMulticast() const noexcept
{
    switch (m_addressFamily) {
    case MediaIpAddressFamily::Ipv4:
        return m_canonicalBytes[0] >= 224 && m_canonicalBytes[0] <= 239;
    case MediaIpAddressFamily::Ipv6:
        return m_canonicalBytes[0] == 0xff;
    }
    return false;
}

bool operator==(
    const MediaNumericIpAddress& left,
    const MediaNumericIpAddress& right) noexcept
{
    return left.m_addressFamily == right.m_addressFamily &&
           left.m_canonicalBytes == right.m_canonicalBytes;
}

} // namespace media::ffmpeg::graph
