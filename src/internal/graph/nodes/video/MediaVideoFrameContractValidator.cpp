#include "internal/graph/nodes/video/MediaVideoFrameContractValidator.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
#include <libavutil/pixdesc.h>
}

#include <sstream>
#include <optional>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaHardwareDeviceKind> deviceKind(const std::string& value,
                                                    const char* stage)
{
    if (value == "software") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::None);
    if (value == "rkmpp") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::RKMPP);
    if (value == "drm_prime") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::DRMPrime);
    if (value == "cuda") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::CUDA);
    if (value == "qsv") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::QSV);
    if (value == "vaapi") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::VAAPI);
    if (value == "d3d11va") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::D3D11VA);
    if (value == "videotoolbox") return ::media::Result<MediaHardwareDeviceKind>::success(MediaHardwareDeviceKind::VideoToolbox);
    return ::media::Result<MediaHardwareDeviceKind>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(stage) + " has unsupported frame-contract device: " + value));
}

::media::Result<MediaHardwareFrameKind> frameKind(const std::string& value,
                                                  const char* stage)
{
    if (value == "software") return ::media::Result<MediaHardwareFrameKind>::success(MediaHardwareFrameKind::Software);
    if (value == "hardware") return ::media::Result<MediaHardwareFrameKind>::success(MediaHardwareFrameKind::Hardware);
    if (value == "hardware_mapped") return ::media::Result<MediaHardwareFrameKind>::success(MediaHardwareFrameKind::HardwareMapped);
    return ::media::Result<MediaHardwareFrameKind>::failure(
        ::media::ErrorInfo::invalidArgument(std::string(stage) + " has unsupported frame-contract kind: " + value));
}

::media::Result<int> requiredDimension(const MediaNodeOptions* options,
                                       const std::string& key,
                                       const char* stage)
{
    return requiredPositiveIntNodeOption(options, stage, key.c_str());
}

::media::ErrorInfo contractFailure(const char* stage, const std::string& detail)
{
    return ::media::ErrorInfo::hardwareUnavailable(
        std::string(stage) + " frame violates planner contract: " + detail);
}

constexpr std::uint32_t drmFourcc(char first, char second, char third, char fourth) noexcept
{
    return static_cast<std::uint32_t>(first) |
           (static_cast<std::uint32_t>(second) << 8U) |
           (static_cast<std::uint32_t>(third) << 16U) |
           (static_cast<std::uint32_t>(fourth) << 24U);
}

struct DrmSurfaceContract {
    std::uint32_t format = 0;
    int planeCount = 0;
};

std::optional<DrmSurfaceContract> drmSurfaceContract(
    const std::string& surfacePixelFormat) noexcept
{
    if (surfacePixelFormat == "nv12") {
        return DrmSurfaceContract{drmFourcc('N', 'V', '1', '2'), 2};
    }
    if (surfacePixelFormat == "nv16") {
        return DrmSurfaceContract{drmFourcc('N', 'V', '1', '6'), 2};
    }
    if (surfacePixelFormat == "p010le") {
        return DrmSurfaceContract{drmFourcc('P', '0', '1', '0'), 2};
    }
    return std::nullopt;
}

bool bufferOwnsDescriptor(const AVBufferRef* buffer,
                          std::uintptr_t descriptorAddress) noexcept
{
    if (!buffer || !buffer->data || buffer->size < sizeof(AVDRMFrameDescriptor)) {
        return false;
    }
    const auto backingAddress = reinterpret_cast<std::uintptr_t>(buffer->data);
    const auto backingSize = static_cast<std::uintptr_t>(buffer->size);
    return descriptorAddress >= backingAddress &&
           descriptorAddress - backingAddress <=
               backingSize - sizeof(AVDRMFrameDescriptor);
}

bool descriptorHasBufferOwner(const AVFrame& frame,
                              std::uintptr_t descriptorAddress) noexcept
{
    for (const AVBufferRef* buffer : frame.buf) {
        if (bufferOwnsDescriptor(buffer, descriptorAddress)) return true;
    }
    if (frame.nb_extended_buf > 0 && !frame.extended_buf) return false;
    for (int index = 0; index < frame.nb_extended_buf; ++index) {
        if (bufferOwnsDescriptor(frame.extended_buf[index], descriptorAddress)) {
            return true;
        }
    }
    return false;
}

::media::Status validateDrmDescriptor(const AVFrame& frame,
                                      const std::string& surfacePixelFormat,
                                      MediaVideoFrameRuntimeFacts& facts,
                                      const char* stage)
{
    if (frame.format != AV_PIX_FMT_DRM_PRIME) {
        return ::media::Status::failure(contractFailure(stage, "expected AV_PIX_FMT_DRM_PRIME"));
    }
    if (!frame.data[0]) {
        return ::media::Status::failure(contractFailure(stage, "missing DRM descriptor or backing AVBufferRef"));
    }
    const auto descriptorAddress = reinterpret_cast<std::uintptr_t>(frame.data[0]);
    if (!descriptorHasBufferOwner(frame, descriptorAddress)) {
        return ::media::Status::failure(
            contractFailure(stage, "DRM descriptor has no backing AVBufferRef owner"));
    }
    const auto* descriptor = reinterpret_cast<const AVDRMFrameDescriptor*>(frame.data[0]);
    const auto expectedSurface = drmSurfaceContract(surfacePixelFormat);
    if (!expectedSurface) {
        return ::media::Status::failure(
            contractFailure(stage, "unsupported DRM surface contract: " + surfacePixelFormat));
    }
    if (descriptor->nb_objects <= 0 || descriptor->nb_objects > AV_DRM_MAX_PLANES ||
        descriptor->nb_layers <= 0 || descriptor->nb_layers > AV_DRM_MAX_PLANES) {
        return ::media::Status::failure(contractFailure(stage, "invalid DRM object/layer count"));
    }
    for (int object = 0; object < descriptor->nb_objects; ++object) {
        if (descriptor->objects[object].fd < 0 || descriptor->objects[object].size == 0) {
            return ::media::Status::failure(contractFailure(stage, "invalid DRM object backing"));
        }
    }
    int planeCount = 0;
    for (int layer = 0; layer < descriptor->nb_layers; ++layer) {
        const AVDRMLayerDescriptor& layerDescriptor = descriptor->layers[layer];
        if (layerDescriptor.format == 0 || layerDescriptor.nb_planes <= 0 ||
            layerDescriptor.nb_planes > AV_DRM_MAX_PLANES) {
            return ::media::Status::failure(contractFailure(stage, "invalid DRM layer"));
        }
        if (layerDescriptor.format != expectedSurface->format) {
            std::ostringstream detail;
            detail << "expected DRM surface " << surfacePixelFormat
                   << " fourcc=0x" << std::hex << expectedSurface->format
                   << " but received fourcc=0x" << layerDescriptor.format;
            return ::media::Status::failure(contractFailure(stage, detail.str()));
        }
        if (layerDescriptor.nb_planes != expectedSurface->planeCount) {
            std::ostringstream detail;
            detail << "expected " << expectedSurface->planeCount
                   << " DRM planes for " << surfacePixelFormat
                   << " but received " << layerDescriptor.nb_planes;
            return ::media::Status::failure(contractFailure(stage, detail.str()));
        }
        planeCount += layerDescriptor.nb_planes;
        if (planeCount > AV_DRM_MAX_PLANES) {
            return ::media::Status::failure(contractFailure(stage, "too many DRM planes"));
        }
        for (int plane = 0; plane < layerDescriptor.nb_planes; ++plane) {
            const AVDRMPlaneDescriptor& planeDescriptor = layerDescriptor.planes[plane];
            if (planeDescriptor.object_index < 0 ||
                planeDescriptor.object_index >= descriptor->nb_objects) {
                return ::media::Status::failure(contractFailure(stage, "invalid DRM plane"));
            }
            const auto& objectDescriptor = descriptor->objects[planeDescriptor.object_index];
            if (planeDescriptor.pitch <= 0 || planeDescriptor.offset < 0 ||
                static_cast<std::size_t>(planeDescriptor.offset) >= objectDescriptor.size) {
                return ::media::Status::failure(contractFailure(stage, "invalid DRM plane"));
            }
            const auto remaining = objectDescriptor.size -
                                   static_cast<std::size_t>(planeDescriptor.offset);
            if (static_cast<std::size_t>(planeDescriptor.pitch) > remaining) {
                return ::media::Status::failure(
                    contractFailure(stage, "DRM plane pitch exceeds its object backing"));
            }
        }
    }
    facts.drmPrime = true;
    facts.software = false;
    facts.bufferIdentity = reinterpret_cast<std::uintptr_t>(frame.data[0]);
    facts.drmObjectFd = descriptor->objects[0].fd;
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaHardwareDescriptor>
MediaVideoFrameContractValidator::contractFromOptions(
    const MediaNodeOptions* options,
    const std::string& prefix,
    const char* stage)
{
    auto present = requiredBoolNodeOption(options, stage, (prefix + ".present").c_str());
    if (!present) return ::media::Result<MediaHardwareDescriptor>::failure(present.error());
    if (!present.value()) {
        return ::media::Result<MediaHardwareDescriptor>::failure(
            ::media::ErrorInfo::invalidArgument(std::string(stage) + " requires a planner frame contract"));
    }
    auto deviceText = requiredNodeOption(options, stage, (prefix + ".device").c_str());
    auto kindText = requiredNodeOption(options, stage, (prefix + ".frame_kind").c_str());
    auto pixelFormat = requiredNodeOption(options, stage, (prefix + ".pixel_format").c_str());
    const std::string surfacePixelFormatKey = prefix + ".surface_pixel_format";
    auto width = requiredDimension(options, prefix + ".width", stage);
    auto height = requiredDimension(options, prefix + ".height", stage);
    auto requiresDevice = requiredBoolNodeOption(
        options, stage, (prefix + ".requires_hw_device_ctx").c_str());
    auto requiresFrames = requiredBoolNodeOption(
        options, stage, (prefix + ".requires_hw_frames_ctx").c_str());
    if (!options || !options->has(surfacePixelFormatKey)) {
        return ::media::Result<MediaHardwareDescriptor>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(stage) + " requires planner option " + surfacePixelFormatKey));
    }
    if (!deviceText || !kindText || !pixelFormat ||
        !width || !height || !requiresDevice || !requiresFrames) {
        if (!deviceText) return ::media::Result<MediaHardwareDescriptor>::failure(deviceText.error());
        if (!kindText) return ::media::Result<MediaHardwareDescriptor>::failure(kindText.error());
        if (!pixelFormat) return ::media::Result<MediaHardwareDescriptor>::failure(pixelFormat.error());
        if (!width) return ::media::Result<MediaHardwareDescriptor>::failure(width.error());
        if (!height) return ::media::Result<MediaHardwareDescriptor>::failure(height.error());
        if (!requiresDevice) return ::media::Result<MediaHardwareDescriptor>::failure(requiresDevice.error());
        return ::media::Result<MediaHardwareDescriptor>::failure(requiresFrames.error());
    }
    auto parsedDevice = deviceKind(deviceText.value(), stage);
    auto parsedKind = frameKind(kindText.value(), stage);
    if (!parsedDevice) return ::media::Result<MediaHardwareDescriptor>::failure(parsedDevice.error());
    if (!parsedKind) return ::media::Result<MediaHardwareDescriptor>::failure(parsedKind.error());

    MediaHardwareDescriptor contract;
    contract.deviceKind = parsedDevice.value();
    contract.frameKind = parsedKind.value();
    contract.pixelFormat = pixelFormat.value();
    contract.surfacePixelFormat = options->value(surfacePixelFormatKey);
    contract.size = MediaSize{width.value(), height.value()};
    contract.requiresHardwareDeviceContext = requiresDevice.value();
    contract.requiresHardwareFramesContext = requiresFrames.value();
    return ::media::Result<MediaHardwareDescriptor>::success(std::move(contract));
}

::media::Result<MediaVideoFrameRuntimeFacts>
MediaVideoFrameContractValidator::validate(
    const AVFrame& frame,
    const MediaHardwareDescriptor& contract,
    const char* stage)
{
    if (!contract.size.isValid()) {
        return ::media::Result<MediaVideoFrameRuntimeFacts>::failure(
            ::media::ErrorInfo::invalidArgument(std::string(stage) + " planner frame contract has invalid dimensions"));
    }
    if (frame.width != contract.size.width || frame.height != contract.size.height) {
        std::ostringstream detail;
        detail << "expected " << contract.size.width << 'x' << contract.size.height
               << " but received " << frame.width << 'x' << frame.height;
        return ::media::Result<MediaVideoFrameRuntimeFacts>::failure(contractFailure(stage, detail.str()));
    }

    MediaVideoFrameRuntimeFacts facts;
    if (contract.deviceKind == MediaHardwareDeviceKind::RKMPP ||
        contract.deviceKind == MediaHardwareDeviceKind::DRMPrime ||
        contract.pixelFormat == "drm_prime") {
        auto drm = validateDrmDescriptor(frame, contract.surfacePixelFormat, facts, stage);
        if (!drm) return ::media::Result<MediaVideoFrameRuntimeFacts>::failure(drm.error());
        return ::media::Result<MediaVideoFrameRuntimeFacts>::success(facts);
    }

    const char* actualName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame.format));
    const std::string actual = actualName ? actualName : "unknown";
    if (actual != contract.pixelFormat) {
        return ::media::Result<MediaVideoFrameRuntimeFacts>::failure(
            contractFailure(stage, "expected format " + contract.pixelFormat + " but received " + actual));
    }
    const AVPixFmtDescriptor* pixel = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame.format));
    const bool hardware = frame.hw_frames_ctx || (pixel && (pixel->flags & AV_PIX_FMT_FLAG_HWACCEL));
    if (contract.isHardwareBacked() != hardware) {
        return ::media::Result<MediaVideoFrameRuntimeFacts>::failure(
            contractFailure(stage, contract.isHardwareBacked() ? "expected hardware frame" : "expected software frame"));
    }
    facts.software = !hardware;
    facts.bufferIdentity = reinterpret_cast<std::uintptr_t>(frame.buf[0] ? frame.buf[0]->data : frame.data[0]);
    return ::media::Result<MediaVideoFrameRuntimeFacts>::success(facts);
}

std::string MediaVideoFrameContractValidator::describe(
    const AVFrame& frame,
    const MediaVideoFrameRuntimeFacts& facts)
{
    const char* name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame.format));
    std::ostringstream out;
    out << "format=" << (name ? name : "unknown")
        << " size=" << frame.width << 'x' << frame.height
        << " buffer_identity=0x" << std::hex << facts.bufferIdentity << std::dec
        << " drm_prime=" << (facts.drmPrime ? 1 : 0)
        << " software_frame=" << (facts.software ? 1 : 0);
    if (facts.drmPrime) out << " drm_fd=" << facts.drmObjectFd;
    return out.str();
}

} // namespace media::ffmpeg::graph
