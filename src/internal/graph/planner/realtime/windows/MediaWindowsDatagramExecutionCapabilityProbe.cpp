#include "internal/graph/planner/realtime/MediaDatagramExecutionCapabilityProbe.h"

#ifdef _WIN32

namespace media::ffmpeg::graph {

::media::Result<MediaDatagramExecutionCapability>
MediaDatagramExecutionCapabilityProbe::scan(
    std::string_view serviceScopeId,
    std::uint64_t maximumWireBytesPerSecond,
    std::uint64_t maximumWireDatagramBytes) noexcept
{
    using Result = ::media::Result<MediaDatagramExecutionCapability>;
    if (serviceScopeId.empty() || maximumWireBytesPerSecond == 0 ||
        maximumWireDatagramBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Windows Datagram execution probe requires a service scope and positive managed capacity"));
    }
    return Result::success(MediaDatagramExecutionCapability{
        MediaDatagramTransportExecutionKind::UserspaceNonblocking,
        "Windows userspace nonblocking Datagram pacing adapter",
        std::nullopt});
}

} // namespace media::ffmpeg::graph

#endif
