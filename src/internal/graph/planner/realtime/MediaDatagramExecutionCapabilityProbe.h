#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaDatagramTransportExecutionKind : std::uint8_t {
    Unknown = 0,
    UserspaceNonblocking = 1,
    LinuxFqSocketPacing = 2
};

struct MediaDatagramExecutionCapability final {
    MediaDatagramTransportExecutionKind execution;
    std::string authority;
    std::optional<std::uint64_t> kernelSocketPacingRateBytesPerSecond;
};

class MediaDatagramExecutionCapabilityProbe final {
public:
    static ::media::Result<MediaDatagramExecutionCapability> scan(
        std::string_view serviceScopeId,
        std::uint64_t maximumWireBytesPerSecond,
        std::uint64_t maximumWireDatagramBytes) noexcept;

private:
    MediaDatagramExecutionCapabilityProbe() = delete;
};

} // namespace media::ffmpeg::graph
