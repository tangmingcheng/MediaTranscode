#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsElementaryStreamInfo final {
    uint16_t pid = 0;
    uint8_t streamType = 0;

    bool operator==(const MediaTsElementaryStreamInfo&) const = default;
};

struct MediaTsProgramInfo final {
    uint16_t programNumber = 0;
    uint16_t pmtPid = 0;
    uint8_t pmtVersion = 0;
    uint16_t pcrPid = 0;
    std::vector<MediaTsElementaryStreamInfo> elementaryStreams;

    bool operator==(const MediaTsProgramInfo&) const = default;
};

struct MediaTsProgramInventorySnapshot final {
    uint8_t patVersion = 0;
    std::vector<MediaTsProgramInfo> programs;

    bool operator==(const MediaTsProgramInventorySnapshot&) const = default;
};

class MediaTsProgramInventorySink {
public:
    virtual ~MediaTsProgramInventorySink() = default;
    virtual ::media::Status onProgramInventory(MediaTsProgramInventorySnapshot snapshot) = 0;
};

} // namespace media::ffmpeg::graph
