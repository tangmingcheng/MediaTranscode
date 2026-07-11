#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/hwcontext.h>
}
#include <sstream>
namespace media::ffmpeg::graph {
namespace {
bool rkmppRuntimeAvailable()
{
    return MediaHardwareCapabilityProbe::decoderExists("h264_rkmpp") ||
           MediaHardwareCapabilityProbe::decoderExists("hevc_rkmpp") ||
           MediaHardwareCapabilityProbe::encoderExists("h264_rkmpp") ||
           MediaHardwareCapabilityProbe::encoderExists("hevc_rkmpp");
}

MediaHardwareCapability probe(MediaHardwareDeviceKind kind, const std::string& hwaccelName)
{
    if (kind == MediaHardwareDeviceKind::None) return {true, "software path"};
    if (kind == MediaHardwareDeviceKind::RKMPP) {
        const bool available = rkmppRuntimeAvailable();
        return {available, available ? "hardware backend found" : "hardware backend not found"};
    }
    if (hwaccelName.empty()) return {false, "hardware backend not found"};
    const AVHWDeviceType type = av_hwdevice_find_type_by_name(hwaccelName.c_str());
    if (type == AV_HWDEVICE_TYPE_NONE) return {false, "hardware backend not found"};
    AVBufferRef* device = nullptr;
    const int result = av_hwdevice_ctx_create(&device, type, nullptr, nullptr, 0);
    if (device) av_buffer_unref(&device);
    return {result >= 0, result >= 0 ? "hardware device created" : "hardware device creation failed"};
}

std::string key(MediaHardwareDeviceKind kind, const std::string& hwaccelName)
{
    return std::string(mediaHardwareDeviceKindName(kind)) + ":" + hwaccelName;
}
}

bool MediaHardwareCapabilityProbe::decoderExists(const std::string& name) noexcept { return !name.empty() && avcodec_find_decoder_by_name(name.c_str()) != nullptr; }
bool MediaHardwareCapabilityProbe::encoderExists(const std::string& name) noexcept { return !name.empty() && avcodec_find_encoder_by_name(name.c_str()) != nullptr; }
bool MediaHardwareCapabilityProbe::filterExists(const std::string& name) noexcept { return !name.empty() && avfilter_get_by_name(name.c_str()) != nullptr; }
void MediaHardwareCapabilityProbe::apply(MediaPipelineStagePlan& stage,
                                         const MediaPipelinePlannerOptions& options)
{
    if (!stage.available) return;
    if (!stage.hardware) {
        stage.availabilityReason = "available";
        return;
    }
    std::lock_guard lock(m_mutex);
    const std::string cacheKey = key(stage.deviceKind, stage.hwaccelName);
    auto [iter, inserted] = m_cache.try_emplace(cacheKey, probe(stage.deviceKind, stage.hwaccelName));
    if (inserted && stage.deviceKind != MediaHardwareDeviceKind::None) {
        std::ostringstream out;
        out << "backend=" << mediaHardwareDeviceKindName(stage.deviceKind)
            << " status=" << (iter->second.available ? "found" : "not_found");
        if (iter->second.available) {
            out << " probe=device_create";
            if (!stage.hwaccelName.empty()) out << " hwaccel=" << stage.hwaccelName;
            out << " note=device_creation_verified";
        }
        mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                                MediaGraphDiagnosticPhase::PlannerCapability,
                                out.str());
    }
    stage.available = iter->second.available;
    stage.availabilityReason = iter->second.reason;
}
}
