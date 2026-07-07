#include "internal/graph/planner/MediaPipelinePlanner.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/MediaPipelineScorer.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

std::string stageDisplayName(const MediaPipelineStagePlan& stage)
{
    if (!stage.ffmpegName.empty()) {
        return stage.ffmpegName;
    }
    if (!stage.filterName.empty()) {
        return stage.filterName;
    }
    return stage.componentName;
}

std::string emptyAsNone(const std::string& value)
{
    return value.empty() ? std::string("none") : value;
}

void logSelectedPlan(const MediaPipelinePlannerOptions& options,
                     const MediaPipelinePlan& plan)
{
    const MediaPipelineChainPlan& selected = plan.selected;

    std::ostringstream out;
    out << "branch_mode=" << mediaBranchModeName(plan.branchMode)
        << " selected_chain=" << selected.label
        << " score=" << selected.score
        << " source_stream=" << plan.sourceStreamIndex
        << " decoder=" << stageDisplayName(selected.decoder)
        << " filter=" << stageDisplayName(selected.filter)
        << " encoder=" << stageDisplayName(selected.encoder)
        << " encoder_pix_fmt=" << emptyAsNone(selected.encoder.pixelFormat)
        << " encoder_hw_frames_fmt=" << emptyAsNone(selected.encoder.hardwareFramesFormat)
        << " encoder_surface_fmt=" << emptyAsNone(selected.encoder.surfacePixelFormat)
        << " backend=" << mediaHardwareDeviceKindName(selected.decoder.deviceKind)
        << " zero_copy=" << (selected.zeroCopy ? "true" : "false");

    mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                            MediaGraphDiagnosticPhase::PlannerSelect,
                            out.str());
}

void logCopyPlan(const MediaPipelinePlannerOptions& options,
                 const MediaPipelinePlan& plan)
{
    std::ostringstream out;
    out << "branch_mode=" << mediaBranchModeName(plan.branchMode)
        << " source_stream=" << plan.sourceStreamIndex
        << " codec=" << plan.inputCodecName
        << " reason=" << plan.reason;

    mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                            MediaGraphDiagnosticPhase::PlannerSelect,
                            out.str());
}

::media::Result<MediaPipelinePlan> buildVideoTranscodePlan(
    const MediaInputVideoStreamInfo& inputInfo,
    std::string inputPath,
    MediaPipelinePlannerOptions options)
{
    MediaPipelinePlan plan;
    plan.inputPath = std::move(inputPath);
    plan.outputPath = std::move(options.outputPath);
    plan.diagnosticLogEnabled = options.diagnosticLogEnabled;

    if (!options.includeVideo) {
        plan.enabled = false;
        plan.branchMode = MediaBranchMode::Drop;
        plan.reason = "disabled";
        return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
    }

    plan.enabled = true;
    plan.sourceStreamIndex = inputInfo.streamIndex;
    plan.inputCodecName = canonicalCodecName(inputInfo.codecName);
    plan.outputCodecName = canonicalCodecName(options.outputCodecName.empty() ? plan.inputCodecName : options.outputCodecName);

    const bool resizeRequested = options.targetWidth > 0 || options.targetHeight > 0;
    const bool canCopyPackets = options.allowPacketCopy &&
        !resizeRequested &&
        !options.filterRequired &&
        plan.inputCodecName == plan.outputCodecName;
    plan.branchMode = canCopyPackets ? MediaBranchMode::CopyPacket : MediaBranchMode::TranscodeFrame;
    plan.reason = canCopyPackets ? "copy_packet" : "transcode_frame";

    {
        std::ostringstream out;
        out << "input=" << redactUrlUserInfo(plan.inputPath)
            << " branch_mode=" << mediaBranchModeName(plan.branchMode)
            << " source_stream=" << plan.sourceStreamIndex
            << " input_codec=" << plan.inputCodecName
            << " output_codec=" << plan.outputCodecName;
        mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                                MediaGraphDiagnosticPhase::PlannerInput,
                                out.str());
    }

    if (plan.branchMode == MediaBranchMode::CopyPacket) {
        logCopyPlan(options, plan);
        return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
    }

    auto candidates = MediaPipelineCapabilityScanner::enumerateVideoTranscodeCandidates(
        plan.inputCodecName,
        plan.outputCodecName,
        options);
    plan.candidates = MediaPipelineScorer::scoreAndSortChains(std::move(candidates), options);

    if (plan.candidates.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::unsupported("no media pipeline candidates were generated"));
    }

    auto selected = std::find_if(plan.candidates.begin(), plan.candidates.end(), [&](const MediaPipelineChainPlan& chain) {
        return !options.requireRuntimeAvailability || chain.available;
    });

    if (selected == plan.candidates.end()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::hardwareUnavailable("no available decoder/filter/encoder chain found"));
    }

    plan.selected = *selected;
    logSelectedPlan(options, plan);
    return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
}

} // namespace

const char* mediaPipelineStageRoleName(MediaPipelineStageRole role) noexcept
{
    switch (role) {
    case MediaPipelineStageRole::Decoder:
        return "decoder";
    case MediaPipelineStageRole::Filter:
        return "filter";
    case MediaPipelineStageRole::Encoder:
        return "encoder";
    }
    return "unknown";
}

const char* mediaHardwareDeviceKindName(MediaHardwareDeviceKind kind) noexcept
{
    switch (kind) {
    case MediaHardwareDeviceKind::Unknown:
        return "unknown";
    case MediaHardwareDeviceKind::None:
        return "software";
    case MediaHardwareDeviceKind::D3D11VA:
        return "d3d11va";
    case MediaHardwareDeviceKind::QSV:
        return "qsv";
    case MediaHardwareDeviceKind::CUDA:
        return "cuda";
    case MediaHardwareDeviceKind::VAAPI:
        return "vaapi";
    case MediaHardwareDeviceKind::DRMPrime:
        return "drm_prime";
    case MediaHardwareDeviceKind::RKMPP:
        return "rkmpp";
    case MediaHardwareDeviceKind::VideoToolbox:
        return "videotoolbox";
    case MediaHardwareDeviceKind::MediaCodec:
        return "mediacodec";
    }
    return "unknown";
}

const char* mediaHardwareFrameKindName(MediaHardwareFrameKind kind) noexcept
{
    switch (kind) {
    case MediaHardwareFrameKind::Unknown:
        return "unknown";
    case MediaHardwareFrameKind::Software:
        return "software";
    case MediaHardwareFrameKind::Hardware:
        return "hardware";
    case MediaHardwareFrameKind::HardwareMapped:
        return "hardware_mapped";
    }
    return "unknown";
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoTranscodeFile(
    const std::string& inputPath,
    MediaPipelinePlannerOptions options)
{
    if (inputPath.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("planVideoTranscodeFile requires input path"));
    }

    auto inputInfo = MediaPipelineCapabilityScanner::detectInputVideoStreamInfo(inputPath);
    if (!inputInfo) {
        return ::media::Result<MediaPipelinePlan>::failure(inputInfo.error());
    }
    return buildVideoTranscodePlan(inputInfo.value(), inputPath, std::move(options));
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoTranscodeRealtimeUrl(
    const std::string& inputUrl,
    MediaPipelinePlannerOptions options)
{
    if (inputUrl.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("planVideoTranscodeRealtimeUrl requires input URL"));
    }

    auto inputInfo = MediaPipelineCapabilityScanner::detectRealtimeVideoStreamInfo(inputUrl, options);
    if (!inputInfo) {
        return ::media::Result<MediaPipelinePlan>::failure(inputInfo.error());
    }
    return buildVideoTranscodePlan(inputInfo.value(), inputUrl, std::move(options));
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoTranscodeKnownInput(
    MediaInputVideoStreamInfo inputInfo,
    const std::string& inputUrl,
    MediaPipelinePlannerOptions options)
{
    if (inputUrl.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("planVideoTranscodeKnownInput requires input URL"));
    }
    if (inputInfo.streamIndex < 0 || inputInfo.codecName.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("planVideoTranscodeKnownInput requires stream index and codec"));
    }
    return buildVideoTranscodePlan(std::move(inputInfo), inputUrl, std::move(options));
}

} // namespace media::ffmpeg::graph
