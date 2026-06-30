#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {

struct LocalFileTranscodeOptions {
    std::string inputUrl;
    std::string outputUrl;
    std::string outputFormat;

    bool includeVideo = true;
    bool includeAudio = true;
    bool audioTranscode = false;
    bool useHardwareTransfer = false;
    bool includeMux = true;
    bool includeLifecycle = false;
    bool diagnosticLogEnabled = true;

    std::string videoCodec;
    std::string videoEncoder;
    std::string rateControlMode = "auto";
    std::string speedPreset;
    std::string profile;
    std::string tune;
    std::string level;

    int width = 0;
    int height = 0;
    int fpsNum = 0;
    int fpsDen = 1;
    int videoBitrateKbps = 0;
    int crf = -1;
    int quality = -1;
    int gop = 0;
    int maxBFrames = 0;
    bool disableHardware = false;

    std::string audioCodec = "auto";
    int audioBitrateKbps = 128;
    int audioSampleRate = 0;
    int audioChannels = 0;

    std::size_t metadataQueueCapacity = 1;
    std::size_t packetQueueCapacity = 256;
    std::size_t frameQueueCapacity = 128;
    std::size_t muxQueueCapacity = 256;
};

class LocalFileTranscodeGraphBuilder final {
public:
    static ::media::Result<MediaGraph> build(const LocalFileTranscodeOptions& options);

private:
    static ::media::Status validate(const LocalFileTranscodeOptions& options);
};

} // namespace media::ffmpeg::graph
