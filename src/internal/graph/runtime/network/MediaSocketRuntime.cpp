#include "internal/graph/runtime/network/MediaSocketRuntime.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace media::ffmpeg::graph {

MediaSocketRuntime::MediaSocketRuntime(bool initialized) noexcept
    : m_initialized(initialized)
{
}

MediaSocketRuntime::~MediaSocketRuntime()
{
#ifdef _WIN32
    if (m_initialized) WSACleanup();
#endif
}

::media::Result<std::shared_ptr<MediaSocketRuntime>> MediaSocketRuntime::create()
{
#ifdef _WIN32
    WSADATA data{};
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        return ::media::Result<std::shared_ptr<MediaSocketRuntime>>::failure(
            ::media::ErrorInfo::ioFailure("WSAStartup failed", result));
    }
    return ::media::Result<std::shared_ptr<MediaSocketRuntime>>::success(
        std::shared_ptr<MediaSocketRuntime>(new MediaSocketRuntime(true)));
#else
    return ::media::Result<std::shared_ptr<MediaSocketRuntime>>::failure(
        ::media::ErrorInfo::unsupported("Socket runtime is currently implemented for Windows"));
#endif
}

} // namespace media::ffmpeg::graph
