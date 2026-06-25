#pragma once

#include <cstdint>
#include <string>

namespace media_transcode::test {

struct MediaProbeInfo {
    bool hasVideo = false;
    bool hasAudio = false;
    int videoStreamCount = 0;
    int audioStreamCount = 0;
    int videoWidth = 0;
    int videoHeight = 0;
    std::string videoCodecName;
    std::string audioCodecName;
    double durationSeconds = 0.0;
    double videoAverageFps = 0.0;
    std::int64_t videoFrameCount = 0;
};

bool probeMediaFile(const std::string& path,
                    MediaProbeInfo& info,
                    std::string& errorMessage);

} // namespace media_transcode::test
