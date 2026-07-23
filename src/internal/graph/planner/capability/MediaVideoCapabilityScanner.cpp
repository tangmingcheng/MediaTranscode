#include "internal/graph/planner/capability/MediaVideoCapabilityScanner.h"
#include "internal/graph/planner/capability/MediaEncoderPacketLayoutCapabilityProvider.h"
#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
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

std::string softwareEncoderName(const std::string& outputCodec)
{
    if (outputCodec == "h264") {
        return "libx264";
    }
    if (outputCodec == "hevc") {
        return "libx265";
    }
    return outputCodec;
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

bool targetResizeRequested(const MediaPipelinePlannerOptions& options) noexcept
{
    return options.targetWidth > 0 && options.targetHeight > 0;
}

std::string targetSizeText(const MediaPipelinePlannerOptions& options)
{
    return std::to_string(options.targetWidth) + ":" + std::to_string(options.targetHeight);
}

std::string cudaFilterName(const MediaPipelinePlannerOptions& options)
{
    return targetResizeRequested(options) ? "scale_cuda=" + targetSizeText(options) : "passthrough_cuda";
}

std::string qsvFilterName(const MediaPipelinePlannerOptions& options)
{
    return targetResizeRequested(options)
               ? "scale_qsv=w=" + std::to_string(options.targetWidth) + ":h=" + std::to_string(options.targetHeight)
               : "passthrough_qsv";
}

std::string d3d11FilterName(const MediaPipelinePlannerOptions& options)
{
    return targetResizeRequested(options) ? "scale_d3d11=" + targetSizeText(options) : "passthrough_d3d11va";
}

std::string rkmppFilterName(const MediaPipelinePlannerOptions& options)
{
    return targetResizeRequested(options) ? "scale_rkrga=" + targetSizeText(options) : "passthrough_rkmpp";
}

std::string vaapiFilterName(const MediaPipelinePlannerOptions& options)
{
    return targetResizeRequested(options)
               ? "scale_vaapi=w=" + std::to_string(options.targetWidth) + ":h=" + std::to_string(options.targetHeight)
               : "passthrough_vaapi";
}

std::string videotoolboxFilterName(const MediaPipelinePlannerOptions& options)
{
    return targetResizeRequested(options) ? "scale_videotoolbox=" + targetSizeText(options) : "passthrough_videotoolbox";
}

std::string softwareFilterName(const MediaPipelinePlannerOptions& options)
{
    return targetResizeRequested(options)
               ? "scale=" + targetSizeText(options) + ":flags=bicubic,format=pix_fmts=yuv420p"
               : "format=pix_fmts=yuv420p";
}

std::string hardwareEncoderPixelFormatName(MediaHardwareDeviceKind deviceKind)
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

std::string hardwareFramesPixelFormatName(MediaHardwareDeviceKind deviceKind)
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
    case MediaHardwareDeviceKind::Unknown:
    case MediaHardwareDeviceKind::None:
    case MediaHardwareDeviceKind::DRMPrime:
    case MediaHardwareDeviceKind::RKMPP:
    case MediaHardwareDeviceKind::VideoToolbox:
    case MediaHardwareDeviceKind::MediaCodec:
        break;
    }
    return {};
}

std::string hardwareSurfacePixelFormatName(MediaHardwareDeviceKind deviceKind)
{
    return hardwareFramesPixelFormatName(deviceKind).empty() ? std::string() : std::string("nv12");
}

void assignEncoderPixelFormats(MediaPipelineStagePlan& stage)
{
    if (stage.role != MediaPipelineStageRole::Encoder) {
        return;
    }

    if (!stage.hardware) {
        stage.pixelFormat = "yuv420p";
        stage.hardwareFramesFormat.clear();
        stage.surfacePixelFormat.clear();
    } else {
        stage.pixelFormat = hardwareEncoderPixelFormatName(stage.deviceKind);
        stage.hardwareFramesFormat = hardwareFramesPixelFormatName(stage.deviceKind);
        stage.surfacePixelFormat = hardwareSurfacePixelFormatName(stage.deviceKind);
    }

    if (stage.pixelFormat.empty()) {
        stage.available = false;
        stage.availabilityReason = "encoder pixel format is not planned for backend: " +
                                   std::string(mediaHardwareDeviceKindName(stage.deviceKind));
    }
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
    stage.deviceKind = deviceKind;
    stage.frameKind = hardware ? MediaHardwareFrameKind::Hardware : MediaHardwareFrameKind::Software;
    stage.hardware = hardware;
    stage.zeroCopy = zeroCopy;
    stage.score = priority;
    if (stage.role == MediaPipelineStageRole::Encoder) {
        stage.encodedPacketLayout =
            MediaEncoderPacketLayoutCapabilityProvider::find(stage.ffmpegName);
    }

    const bool codecOk = role == MediaPipelineStageRole::Decoder
                             ? decoderExists(stage.ffmpegName)
                             : encoderExists(stage.ffmpegName);
    stage.available = codecOk;
    stage.availabilityReason = codecOk
                                   ? "codec found"
                                   : std::string(role == MediaPipelineStageRole::Decoder ? "decoder not found: " : "encoder not found: ") + stage.ffmpegName;
    assignEncoderPixelFormats(stage);
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
    stage.deviceKind = deviceKind;
    stage.frameKind = hardware ? MediaHardwareFrameKind::Hardware : MediaHardwareFrameKind::Software;
    stage.hardware = hardware;
    stage.zeroCopy = zeroCopy;
    stage.score = priority;

    const bool filterOk = filterExists(stage.filterName);
    stage.available = filterOk;
    stage.availabilityReason = filterOk ? "filter found" : "filter not found: " + stage.filterName;
    return stage;
}

MediaPipelineChainPlan makeRawChain(std::string label,
                                    MediaPipelineStagePlan decoder,
                                    MediaPipelineStagePlan filter,
                                    MediaPipelineStagePlan encoder,
                                    MediaHardwareCapabilityProbe& hardwareProbe,
                                    const MediaPipelinePlannerOptions& options)
{
    MediaPipelineChainPlan chain;
    chain.label = std::move(label);
    chain.decoder = std::move(decoder);
    chain.filter = std::move(filter);
    chain.encoder = std::move(encoder);

    if (!chain.decoder.available || !chain.filter.available || !chain.encoder.available) {
        return chain;
    }

    hardwareProbe.apply(chain.decoder, options);
    hardwareProbe.apply(chain.filter, options);
    hardwareProbe.apply(chain.encoder, options);
    return chain;
}

} // namespace

std::vector<MediaPipelineChainPlan> MediaVideoCapabilityScanner::enumerateTranscodeCandidates(
    const std::string& inputCodecName, const std::string& outputCodecName,
    const MediaPipelinePlannerOptions& options)
{
    const std::string inputCodec = canonicalCodecName(inputCodecName);
    const std::string outputCodec = canonicalCodecName(outputCodecName);
    static MediaHardwareCapabilityProbe hardwareProbe;
    std::vector<MediaPipelineChainPlan> chains;

    auto add = [&](std::string label, MediaPipelineStagePlan decoder, MediaPipelineStagePlan filter,
                   MediaPipelineStagePlan encoder)
    {
        chains.push_back(makeRawChain(std::move(label), std::move(decoder), std::move(filter),
                                      std::move(encoder), hardwareProbe, options));
    };

    if (!options.disableHardware)
    {
        add("cuda-nvenc",
            makeCodecStage(MediaPipelineStageRole::Decoder, "cuda decoder", inputCodec,
                           inputCodec, "cuda",
                           MediaHardwareDeviceKind::CUDA, true, true, 95),
            makeFilterStage(
                targetResizeRequested(options) ? "cuda scale filter" : "cuda passthrough filter",
                cudaFilterName(options), "cuda", MediaHardwareDeviceKind::CUDA, true, true, 90),
            makeCodecStage(MediaPipelineStageRole::Encoder, "nvenc encoder", outputCodec,
                           codecSpecificName(outputCodec, "_nvenc"), "cuda",
                           MediaHardwareDeviceKind::CUDA, true, true, 95));

        add("qsv",
            makeCodecStage(MediaPipelineStageRole::Decoder, "qsv decoder", inputCodec,
                           codecSpecificName(inputCodec, "_qsv"), "qsv",
                           MediaHardwareDeviceKind::QSV, true, true, 90),
            makeFilterStage(
                targetResizeRequested(options) ? "qsv scale filter" : "qsv passthrough filter",
                qsvFilterName(options), "qsv", MediaHardwareDeviceKind::QSV, true, true, 88),
            makeCodecStage(MediaPipelineStageRole::Encoder, "qsv encoder", outputCodec,
                           codecSpecificName(outputCodec, "_qsv"), "qsv",
                           MediaHardwareDeviceKind::QSV, true, true, 90));

        add("d3d11va-mediafoundation",
            makeCodecStage(MediaPipelineStageRole::Decoder, "d3d11va decoder", inputCodec,
                           inputCodec, "d3d11va", MediaHardwareDeviceKind::D3D11VA, true, true, 84),
            makeFilterStage(targetResizeRequested(options) ? "d3d11va scale filter"
                                                           : "d3d11va passthrough filter",
                            d3d11FilterName(options), "d3d11va", MediaHardwareDeviceKind::D3D11VA,
                            true, true, 82),
            makeCodecStage(MediaPipelineStageRole::Encoder, "mediafoundation encoder", outputCodec,
                           codecSpecificName(outputCodec, "_mf"), "d3d11va",
                           MediaHardwareDeviceKind::D3D11VA, true, true, 84));

        add("rkmpp",
            makeCodecStage(MediaPipelineStageRole::Decoder, "rkmpp decoder", inputCodec,
                           codecSpecificName(inputCodec, "_rkmpp"), "rkmpp",
                           MediaHardwareDeviceKind::RKMPP, true, true, 92),
            makeFilterStage(targetResizeRequested(options) ? "rga/rkmpp scale filter"
                                                           : "rga/rkmpp passthrough filter",
                            rkmppFilterName(options), "rkmpp", MediaHardwareDeviceKind::RKMPP, true,
                            true, 90),
            makeCodecStage(MediaPipelineStageRole::Encoder, "rkmpp encoder", outputCodec,
                           codecSpecificName(outputCodec, "_rkmpp"), "rkmpp",
                           MediaHardwareDeviceKind::RKMPP, true, true, 92));

        add("vaapi",
            makeCodecStage(MediaPipelineStageRole::Decoder, "vaapi decoder", inputCodec, inputCodec,
                           "vaapi", MediaHardwareDeviceKind::VAAPI, true, true, 82),
            makeFilterStage(
                targetResizeRequested(options) ? "vaapi scale filter" : "vaapi passthrough filter",
                vaapiFilterName(options), "vaapi", MediaHardwareDeviceKind::VAAPI, true, true, 82),
            makeCodecStage(MediaPipelineStageRole::Encoder, "vaapi encoder", outputCodec,
                           codecSpecificName(outputCodec, "_vaapi"), "vaapi",
                           MediaHardwareDeviceKind::VAAPI, true, true, 82));

        add("videotoolbox",
            makeCodecStage(MediaPipelineStageRole::Decoder, "videotoolbox decoder", inputCodec,
                           inputCodec, "videotoolbox", MediaHardwareDeviceKind::VideoToolbox, true,
                           true, 80),
            makeFilterStage(targetResizeRequested(options) ? "videotoolbox scale filter"
                                                           : "videotoolbox passthrough filter",
                            videotoolboxFilterName(options), "videotoolbox",
                            MediaHardwareDeviceKind::VideoToolbox, true, true, 78),
            makeCodecStage(MediaPipelineStageRole::Encoder, "videotoolbox encoder", outputCodec,
                           codecSpecificName(outputCodec, "_videotoolbox"), "videotoolbox",
                           MediaHardwareDeviceKind::VideoToolbox, true, true, 80));
    }

    {
        add("software",
            makeCodecStage(MediaPipelineStageRole::Decoder, "software decoder", inputCodec,
                           inputCodec, "", MediaHardwareDeviceKind::None, false, false, 30),
            makeFilterStage(
                targetResizeRequested(options) ? "software scale filter" : "software format filter",
                softwareFilterName(options), "", MediaHardwareDeviceKind::None, false, false, 30),
            makeCodecStage(MediaPipelineStageRole::Encoder, "software encoder", outputCodec,
                           softwareEncoderName(outputCodec), "", MediaHardwareDeviceKind::None,
                           false, false, 30));

        add("software-native-codec",
            makeCodecStage(MediaPipelineStageRole::Decoder, "software decoder", inputCodec,
                           inputCodec, "", MediaHardwareDeviceKind::None, false, false, 20),
            makeFilterStage(
                targetResizeRequested(options) ? "software scale filter" : "software format filter",
                softwareFilterName(options), "", MediaHardwareDeviceKind::None, false, false, 20),
            makeCodecStage(MediaPipelineStageRole::Encoder, "native software encoder", outputCodec,
                           outputCodec, "", MediaHardwareDeviceKind::None, false, false, 20));
    }

    return chains;
}

} // namespace media::ffmpeg::graph
