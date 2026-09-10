#pragma once

#include "internal/graph/model/MediaEncoderOpenContract.h"

namespace media::ffmpeg::graph {

// Complete pre-probe encoding intent. Readback completion must never mutate
// this witness; transport destinations and session identities are excluded.
struct MediaVideoEncodingRequestContract final {
    std::string sourceCodec;
    std::string outputCodec;
    int sourceWidth;
    int sourceHeight;
    MediaRational sourceFrameRate;
    MediaEncoderOpenContract open;
    bool filterRequired;
    bool allowPacketCopy;
    friend bool operator==(const MediaVideoEncodingRequestContract&,
                           const MediaVideoEncodingRequestContract&) = default;
};

} // namespace media::ffmpeg::graph
