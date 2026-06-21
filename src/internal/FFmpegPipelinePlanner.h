#pragma once

#include "internal/FFmpegHardwareBackend.h"
#include "internal/FFmpegHardwareDecoder.h"
#include "internal/FFmpegHardwareEncoderSelector.h"
#include "media_transcode/MediaTranscodeTypes.h"

#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg {

    struct HardwarePipelinePlanAttempt {
        HardwareDeviceType requestedDeviceType = HardwareDeviceType::None;
        HardwareBackendProfile backend;
        HardwareDecoderSupport::Config decoderConfig;
        HardwareEncoderSelection encoderSelection;
        bool decoderAccepted = false;
        bool encoderAccepted = false;
        int score = 0;
        VideoExecutionMode executionMode = VideoExecutionMode::Cpu;
        std::string reason;
    };

    struct HardwarePipelinePlan {
        bool valid = false;
        bool zeroCopy = false;
        bool allowFallback = true;
        VideoExecutionMode executionMode = VideoExecutionMode::Cpu;
        HardwareBackendProfile backend;
        HardwareDecoderSupport::Config decoderConfig;
        HardwareEncoderSelection encoderSelection;
        std::vector<HardwarePipelinePlanAttempt> attempts;
        std::string diagnostic;
    };

    class FFmpegPipelinePlanner {
    public:
        static HardwarePipelinePlan planHardwarePipeline(const TranscodeConfig& config,
                                                         const AVCodec* decoder);

        static std::vector<HardwareDeviceType> backendPriority(HardwareDeviceType requestedDeviceType);
    };

} // namespace media::ffmpeg
