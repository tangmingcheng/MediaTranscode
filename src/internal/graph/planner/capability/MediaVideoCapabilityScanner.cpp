#include "internal/graph/planner/capability/MediaVideoCapabilityScanner.h"
#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/opt.h>
}
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), value.begin());
}

std::string filterRootName(const std::string& name)
{
    const std::size_t comma = name.find(',');
    const std::size_t equals = name.find('=');
    std::size_t end = std::string::npos;
    if (comma != std::string::npos && equals != std::string::npos) {
        end = std::min(comma, equals);
    } else if (comma != std::string::npos) {
        end = comma;
    } else if (equals != std::string::npos) {
        end = equals;
    }
    return end == std::string::npos ? name : name.substr(0, end);
}

std::string codecSpecificName(const std::string& codec, const std::string& suffix)
{
    return codec + suffix;
}

bool decoderExists(const std::string& name)
{
    return MediaHardwareCapabilityProbe::decoderExists(name);
}

bool encoderExists(const std::string& name)
{
    return MediaHardwareCapabilityProbe::encoderExists(name);
}

bool filterExists(const std::string& name)
{
    if (name.empty() || startsWith(name, "passthrough")) {
        return true;
    }

    const std::string root = filterRootName(name);
    return MediaHardwareCapabilityProbe::filterExists(root);
}

bool targetResizeRequested(const MediaVideoSourcePlanningOptions& options) noexcept
{
    return options.targetSize.has_value();
}

std::string targetSizeText(const MediaVideoSourcePlanningOptions& options)
{
    return std::to_string(options.targetSize->width) + ":" + std::to_string(options.targetSize->height);
}

std::string cudaFilterName(const MediaVideoSourcePlanningOptions& options)
{
    return targetResizeRequested(options) ? "scale_cuda=" + targetSizeText(options) : "passthrough_cuda";
}

std::string qsvFilterName(const MediaVideoSourcePlanningOptions& options)
{
    return targetResizeRequested(options)
               ? "scale_qsv=w=" + std::to_string(options.targetSize->width) + ":h=" + std::to_string(options.targetSize->height)
               : "passthrough_qsv";
}

std::string d3d11FilterName(const MediaVideoSourcePlanningOptions& options)
{
    return targetResizeRequested(options) ? "scale_d3d11=" + targetSizeText(options) : "passthrough_d3d11va";
}

std::string vaapiFilterName(const MediaVideoSourcePlanningOptions& options)
{
    return targetResizeRequested(options)
               ? "scale_vaapi=w=" + std::to_string(options.targetSize->width) + ":h=" + std::to_string(options.targetSize->height)
               : "passthrough_vaapi";
}

std::string videotoolboxFilterName(const MediaVideoSourcePlanningOptions& options)
{
    return targetResizeRequested(options) ? "scale_videotoolbox=" + targetSizeText(options) : "passthrough_videotoolbox";
}

std::string hardwareFramePixelFormatName(MediaHardwareDeviceKind deviceKind)
{
    switch (deviceKind) {
    case MediaHardwareDeviceKind::CUDA:
        return "cuda";
    case MediaHardwareDeviceKind::QSV:
        return "qsv";
    case MediaHardwareDeviceKind::VAAPI:
        return "vaapi";
    case MediaHardwareDeviceKind::D3D11VA:
        return "d3d11";
    case MediaHardwareDeviceKind::RKMPP:
    case MediaHardwareDeviceKind::DRMPrime:
        return "drm_prime";
    case MediaHardwareDeviceKind::VideoToolbox:
        return "videotoolbox";
    case MediaHardwareDeviceKind::Unknown:
    case MediaHardwareDeviceKind::None:
    case MediaHardwareDeviceKind::MediaCodec:
        break;
    }
    return {};
}

bool requiresGenericFramesContext(MediaHardwareDeviceKind deviceKind) noexcept
{
    switch (deviceKind) {
    case MediaHardwareDeviceKind::CUDA:
    case MediaHardwareDeviceKind::QSV:
    case MediaHardwareDeviceKind::VAAPI:
    case MediaHardwareDeviceKind::D3D11VA:
        return true;
    case MediaHardwareDeviceKind::Unknown:
    case MediaHardwareDeviceKind::None:
    case MediaHardwareDeviceKind::DRMPrime:
    case MediaHardwareDeviceKind::RKMPP:
    case MediaHardwareDeviceKind::VideoToolbox:
    case MediaHardwareDeviceKind::MediaCodec:
        break;
    }
    return false;
}

MediaHardwareDescriptor makeFrameContract(MediaHardwareDeviceKind deviceKind,
                                          const std::string& deviceName,
                                          bool hardware,
                                          bool zeroCopy)
{
    MediaHardwareDescriptor contract;
    contract.deviceKind = deviceKind;
    contract.frameKind = hardware ? MediaHardwareFrameKind::Hardware
                                  : MediaHardwareFrameKind::Software;
    contract.transferDirection = MediaHardwareTransferDirection::None;
    contract.deviceName = deviceName;
    contract.pixelFormat = hardware ? hardwareFramePixelFormatName(deviceKind) : "yuv420p";
    if (deviceKind == MediaHardwareDeviceKind::RKMPP ||
        requiresGenericFramesContext(deviceKind)) {
        contract.surfacePixelFormat = "nv12";
    }
    contract.zeroCopyPreferred = zeroCopy;
    contract.requiresHardwareDeviceContext =
        hardware && deviceKind != MediaHardwareDeviceKind::RKMPP;
    contract.requiresHardwareFramesContext =
        hardware && requiresGenericFramesContext(deviceKind);
    return contract;
}

MediaPipelineStagePlan makeCodecStage(MediaPipelineStageRole role,
                                       std::string componentName,
                                       std::string codecName,
                                       std::string ffmpegName,
                                       std::string hwaccelName,
                                       MediaHardwareDeviceKind deviceKind,
                                       bool hardware,
                                       bool zeroCopy,
                                       int priority)
{
    MediaPipelineStagePlan stage;
    stage.role = role;
    stage.componentName = std::move(componentName);
    stage.codecName = std::move(codecName);
    stage.ffmpegName = std::move(ffmpegName);
    stage.hwaccelName = std::move(hwaccelName);
    stage.priority = priority;
    const bool codecOk = role == MediaPipelineStageRole::Decoder
                             ? decoderExists(stage.ffmpegName)
                             : encoderExists(stage.ffmpegName);
    stage.available = codecOk;
    stage.availabilityReason = codecOk
                                   ? "codec found"
                                   : std::string(role == MediaPipelineStageRole::Decoder ? "decoder not found: " : "encoder not found: ") + stage.ffmpegName;
    MediaHardwareDescriptor contract = makeFrameContract(
        deviceKind, stage.hwaccelName, hardware, zeroCopy);
    if (role == MediaPipelineStageRole::Decoder) {
        stage.outputFrame = std::move(contract);
    } else {
        contract.requiresHardwareDeviceContext = contract.requiresHardwareFramesContext;
        stage.inputFrame = std::move(contract);
    }
    return stage;
}

MediaPipelineStagePlan makeFilterStage(std::string componentName,
                                        std::string filterName,
                                        std::string hwaccelName,
                                        MediaHardwareDeviceKind deviceKind,
                                        bool hardware,
                                        bool zeroCopy,
                                        int priority)
{
    MediaPipelineStagePlan stage;
    stage.role = MediaPipelineStageRole::Filter;
    stage.componentName = std::move(componentName);
    stage.filterName = std::move(filterName);
    stage.hwaccelName = std::move(hwaccelName);
    stage.priority = priority;

    const bool filterOk = stage.filterName.empty() || filterExists(stage.filterName);
    stage.available = filterOk;
    stage.availabilityReason = filterOk ? "filter found" : "filter not found: " + stage.filterName;
    if (!stage.filterName.empty()) {
        MediaHardwareDescriptor contract = makeFrameContract(
            deviceKind, stage.hwaccelName, hardware, zeroCopy);
        stage.inputFrame = contract;
        stage.outputFrame = std::move(contract);
    }
    return stage;
}

struct BackendProfile {
    const char* label;
    const char* decoderComponent;
    const char* decoderSuffix;
    const char* hwaccel;
    MediaHardwareDeviceKind device;
    int decoderPriority;
    int filterPriority;
    const char* encoderComponent;
    const char* encoderSuffix;
    int encoderPriority;
    std::string (*filterName)(const MediaVideoSourcePlanningOptions&);
};

const BackendProfile profiles[] = {
    {"cuda-nvenc", "cuda decoder", "", "cuda", MediaHardwareDeviceKind::CUDA,
        95, 90, "nvenc encoder", "_nvenc", 95, cudaFilterName},
    {"qsv", "qsv decoder", "_qsv", "qsv", MediaHardwareDeviceKind::QSV,
        90, 88, "qsv encoder", "_qsv", 90, qsvFilterName},
    {"d3d11va-mediafoundation", "d3d11va decoder", "", "d3d11va", MediaHardwareDeviceKind::D3D11VA,
        84, 82, "mediafoundation encoder", "_mf", 84, d3d11FilterName},
    {"rkmpp", "rkmpp decoder", "_rkmpp", "rkmpp", MediaHardwareDeviceKind::RKMPP,
        92, 90, "rkmpp encoder", "_rkmpp", 92, nullptr},
    {"vaapi", "vaapi decoder", "", "vaapi", MediaHardwareDeviceKind::VAAPI,
        82, 82, "vaapi encoder", "_vaapi", 82, vaapiFilterName},
    {"videotoolbox", "videotoolbox decoder", "", "videotoolbox", MediaHardwareDeviceKind::VideoToolbox,
        80, 78, "videotoolbox encoder", "_videotoolbox", 80, videotoolboxFilterName}
};

MediaVideoSourcePlan makeSourceCandidate(
    const BackendProfile& profile, const std::string& inputCodec,
    const MediaVideoSourcePlanningOptions& options)
{
    MediaVideoSourcePlan source;
    source.label = profile.label;
    source.decoder = makeCodecStage(MediaPipelineStageRole::Decoder,
        profile.decoderComponent, inputCodec, codecSpecificName(inputCodec, profile.decoderSuffix),
        profile.hwaccel, profile.device, true, true, profile.decoderPriority);
    auto filterName = profile.filterName
        ? ::media::Result<std::string>::success(profile.filterName(options))
        : MediaVideoCapabilityScanner::planRkmppFilter(options);
    source.filter = makeFilterStage(std::string(profile.device == MediaHardwareDeviceKind::RKMPP
        ? "rga/rkmpp" : profile.hwaccel) +
        (targetResizeRequested(options) ? " scale filter" : " passthrough filter"),
        filterName ? filterName.value() : std::string(), profile.hwaccel,
        profile.device, true, true, profile.filterPriority);
    if (!filterName) {
        source.filter.available = false;
        source.filter.availabilityReason = filterName.error().message;
    }
    source.filterActive = profile.device == MediaHardwareDeviceKind::RKMPP
        ? targetResizeRequested(options) : !source.filter.filterName.empty();
    source.transferDirection = MediaHardwareTransferDirection::None;
    const auto outputSize = options.targetSize.value_or(options.sourceSize);
    source.decoder.outputFrame->size = options.sourceSize;
    if (source.filter.inputFrame) source.filter.inputFrame->size = options.sourceSize;
    if (source.filter.outputFrame) source.filter.outputFrame->size = outputSize;
    return source;
}

} // namespace

::media::Result<std::string> MediaVideoCapabilityScanner::planRkmppFilter(
    const MediaVideoSourcePlanningOptions& options)
{
    using Result = ::media::Result<std::string>;
    if (!targetResizeRequested(options)) return Result::success({});
    std::string description = "scale_rkrga=w=" + std::to_string(options.targetSize->width) +
        ":h=" + std::to_string(options.targetSize->height) + ":format=nv12";
    if (options.lowLatency) {
        const AVFilter* filter = avfilter_get_by_name("scale_rkrga");
        const AVClass* filterClass = filter ? filter->priv_class : nullptr;
        const AVOption* depth = filterClass
            ? av_opt_find(&filterClass, "async_depth", nullptr, 0, AV_OPT_SEARCH_FAKE_OBJ)
            : nullptr;
        // RGA retrieves a completed frame only when its FIFO exceeds this depth.
        // Live input can pause indefinitely, so no frame may wait for later input.
        if (!depth || depth->type != AV_OPT_TYPE_INT || depth->min != 0) {
            return Result::failure(::media::ErrorInfo::hardwareUnavailable(
                "RKMPP low-latency filter requires advertised zero-frame async depth"));
        }
        description += ":async_depth=" + std::to_string(static_cast<int>(depth->min));
    }
    return Result::success(std::move(description));
}

::media::Result<std::vector<MediaVideoSourcePlan>>
MediaVideoCapabilityScanner::enumerateSourceCandidates(
    const std::string& inputCodecName, const MediaVideoSourcePlanningOptions& options,
    const MediaHardwareDescriptor& target)
{
    using Result = ::media::Result<std::vector<MediaVideoSourcePlan>>;
    if (inputCodecName.empty() || options.sourceSize.width <= 0 || options.sourceSize.height <= 0 ||
        !options.targetSize || options.targetSize->width <= 0 || options.targetSize->height <= 0 ||
        target.size.width != options.targetSize->width || target.size.height != options.targetSize->height ||
        target.deviceKind == MediaHardwareDeviceKind::Unknown || target.pixelFormat.empty())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source candidates require explicit input dimensions and a resolved target frame domain"));
    std::vector<MediaVideoSourcePlan> sources;
    for (const auto& profile : profiles) {
        if (profile.device != target.deviceKind) continue;
        auto source = makeSourceCandidate(profile, canonicalCodecName(inputCodecName), options);
        const auto& output = source.filterActive ? source.filter.outputFrame : source.decoder.outputFrame;
        if (!output || *output != target)
            continue;
        sources.push_back(std::move(source));
    }
    if (sources.empty()) return Result::failure(::media::ErrorInfo::unsupported(
        "No source candidate matches the selected target frame domain"));
    return Result::success(std::move(sources));
}

std::vector<MediaPipelineChainPlan> MediaVideoCapabilityScanner::enumerateTranscodeCandidates(
    const std::string& inputCodecName, const std::string& outputCodecName,
    const MediaPipelinePlannerOptions& options)
{
    const MediaVideoSourcePlanningOptions sourceOptions{
        {options.probeWidth, options.probeHeight},
        options.targetWidth > 0 && options.targetHeight > 0
            ? std::optional(MediaSize{options.targetWidth, options.targetHeight}) : std::nullopt,
        options.sourceFrameRate, options.lowLatency};
    std::vector<MediaPipelineChainPlan> chains;
    for (const auto& profile : profiles) {
        MediaPipelineChainPlan chain;
        static_cast<MediaVideoSourcePlan&>(chain) =
            makeSourceCandidate(profile, canonicalCodecName(inputCodecName), sourceOptions);
        const auto outputCodec = canonicalCodecName(outputCodecName);
        chain.encoder = makeCodecStage(MediaPipelineStageRole::Encoder,
            profile.encoderComponent, outputCodec, codecSpecificName(outputCodec, profile.encoderSuffix),
            profile.hwaccel, profile.device, true, true, profile.encoderPriority);
        chain.encoder.inputFrame->size = sourceOptions.targetSize.value_or(sourceOptions.sourceSize);
        chains.push_back(std::move(chain));
    }
    return chains;
}

} // namespace media::ffmpeg::graph
