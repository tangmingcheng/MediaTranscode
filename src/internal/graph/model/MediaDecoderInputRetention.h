#pragma once

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {
struct MediaDecoderInputRetention final {
    int threadCount;
    int threadType;
    std::uint64_t mainHandoffPackets;
    std::uint64_t frameWorkerPackets;
    std::uint64_t serialPrivatePackets;
    std::string authority;

    std::uint64_t maximumInternalPackets() const noexcept
    {
        return mainHandoffPackets + frameWorkerPackets + serialPrivatePackets;
    }
};
} // namespace media::ffmpeg::graph
