#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

struct AVFrame;

namespace media::ffmpeg::graph {

struct MediaVideoFrameRuntimeFacts {
    bool drmPrime = false;
    bool software = false;
    std::uintptr_t bufferIdentity = 0;
    int drmObjectFd = -1;
};

class MediaVideoFrameContractValidator final {
public:
    static ::media::Result<MediaHardwareDescriptor> contractFromOptions(
        const MediaNodeOptions* options,
        const std::string& prefix,
        const char* stage);

    static ::media::Result<MediaVideoFrameRuntimeFacts> validate(
        const AVFrame& frame,
        const MediaHardwareDescriptor& contract,
        const char* stage);

    static std::string describe(const AVFrame& frame,
                                const MediaVideoFrameRuntimeFacts& facts);

private:
    MediaVideoFrameContractValidator() = default;
};

} // namespace media::ffmpeg::graph
