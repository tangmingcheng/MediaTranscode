#include "internal/FFmpegUtils.h"

#include <string>

namespace media::ffmpeg {

    bool isHardwareEncoderName(const char* name)
    {
        if (!name) {
            return false;
        }

        const std::string encoderName(name);
        return encoderName.find("_rkmpp") != std::string::npos ||
            encoderName.find("_mf") != std::string::npos ||
            encoderName.find("_qsv") != std::string::npos ||
            encoderName.find("_nvenc") != std::string::npos ||
            encoderName.find("_vaapi") != std::string::npos ||
            encoderName.find("_amf") != std::string::npos;
    }

} // namespace media::ffmpeg
