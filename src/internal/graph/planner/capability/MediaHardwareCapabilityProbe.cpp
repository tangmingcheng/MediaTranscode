#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}
namespace media::ffmpeg::graph {
bool MediaHardwareCapabilityProbe::decoderExists(const std::string& name) noexcept { return !name.empty() && avcodec_find_decoder_by_name(name.c_str()) != nullptr; }
bool MediaHardwareCapabilityProbe::encoderExists(const std::string& name) noexcept { return !name.empty() && avcodec_find_encoder_by_name(name.c_str()) != nullptr; }
bool MediaHardwareCapabilityProbe::filterExists(const std::string& name) noexcept { return !name.empty() && avfilter_get_by_name(name.c_str()) != nullptr; }
}
