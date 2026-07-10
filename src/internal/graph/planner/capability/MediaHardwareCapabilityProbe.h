#pragma once
#include <string>
namespace media::ffmpeg::graph {
class MediaHardwareCapabilityProbe final {
public:
    static bool decoderExists(const std::string& name) noexcept;
    static bool encoderExists(const std::string& name) noexcept;
    static bool filterExists(const std::string& name) noexcept;
private:
    MediaHardwareCapabilityProbe() = delete;
};
}
