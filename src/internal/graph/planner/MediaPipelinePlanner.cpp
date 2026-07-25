#include "internal/graph/planner/MediaPipelinePlanner.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/MediaPipelineScorer.h"
#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
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

bool isSoftwareChain(const MediaPipelineChainPlan& chain,
                     const MediaPipelinePlannerOptions& options) noexcept
{
    return !chain.decoder.hardware && !chain.encoder.hardware &&
           (!options.filterRequired || !chain.filter.hardware);
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

::media::Status validateCommonPlannerOptions(const MediaPipelinePlannerOptions& options,
                                             const std::string& context)
{
    if (options.targetWidth < 0 || options.targetHeight < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(context + " requires non-negative target dimensions"));
    }
    if ((options.targetWidth > 0) != (options.targetHeight > 0)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(context + " requires target width and height together"));
    }
    if (options.preferredHardware.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(context + " requires explicit hardware preference"));
    }
    if (options.disableHardware &&
        options.preferredHardware != "software") {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(context + " requires software hardware preference when GPU is disabled"));
    }
    return ::media::Status::success();
}

::media::Status validateRealtimePlannerOptions(const MediaPipelinePlannerOptions& options,
                                               const std::string& context)
{
    auto common = validateCommonPlannerOptions(options, context);
    if (!common) {
        return common;
    }
    if (options.openTimeoutMs <= 0 ||
        options.readTimeoutMs <= 0 ||
        options.analyzeDurationUs <= 0 ||
        options.probeSizeBytes <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(context + " requires explicit positive realtime input options"));
    }
    return ::media::Status::success();
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
    plan.filterRequired = options.filterRequired;

    plan.enabled = true;
    plan.sourceStreamIndex = inputInfo.streamIndex;
    plan.inputCodecName = canonicalCodecName(inputInfo.codecName);
    plan.outputCodecName = canonicalCodecName(options.outputCodecName.empty() ? plan.inputCodecName : options.outputCodecName);
    if (inputInfo.width > 0 && inputInfo.height > 0) {
        options.probeWidth = inputInfo.width;
        options.probeHeight = inputInfo.height;
    }
    if (inputInfo.frameRate.isKnown()) {
        options.probeFrameRate = inputInfo.frameRate;
    }

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

    MediaHardwareCapabilityProbe hardwareProbe;
    auto selected = MediaPipelinePlanner::selectHighestRankedCandidate(
        plan.candidates, options);
    if (!selected) {
        return ::media::Result<MediaPipelinePlan>::failure(selected.error());
    }

    plan.selected = plan.candidates.at(selected.value());
    auto preflight = MediaPipelinePlanner::preflightSelectedCandidate(
        plan.selected, options, hardwareProbe);
    if (!preflight) {
        return ::media::Result<MediaPipelinePlan>::failure(preflight.error());
    }
    logSelectedPlan(options, plan);
    return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
}

} // namespace

::media::Result<std::size_t> MediaPipelinePlanner::selectHighestRankedCandidate(
    const std::vector<MediaPipelineChainPlan>& candidates,
    const MediaPipelinePlannerOptions& options)
{
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const MediaPipelineChainPlan& candidate = candidates[index];
        if (!candidate.available) {
            continue;
        }

        if (options.disableHardware) {
            if (isSoftwareChain(candidate, options)) {
                return ::media::Result<std::size_t>::success(index);
            }
            continue;
        }

        if (!candidate.allHardware) {
            continue;
        }

        return ::media::Result<std::size_t>::success(index);
    }

    return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::hardwareUnavailable(
            options.disableHardware
                ? "no available explicit software decoder/filter/encoder chain found"
                : "no structurally available hardware decoder/filter/encoder chain found"));
}

::media::Status MediaPipelinePlanner::preflightSelectedCandidate(
    MediaPipelineChainPlan& selected,
    const MediaPipelinePlannerOptions& options,
    MediaHardwareCapabilityProbe& hardwareProbe)
{
    if (options.disableHardware) {
        return ::media::Status::success();
    }
    return hardwareProbe.validate(selected, options);
}

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
    auto optionsStatus = validateCommonPlannerOptions(options, "planVideoTranscodeFile");
    if (!optionsStatus) {
        return ::media::Result<MediaPipelinePlan>::failure(optionsStatus.error());
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
    auto optionsStatus = validateRealtimePlannerOptions(options, "planVideoTranscodeRealtimeUrl");
    if (!optionsStatus) {
        return ::media::Result<MediaPipelinePlan>::failure(optionsStatus.error());
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
    auto optionsStatus = validateCommonPlannerOptions(options, "planVideoTranscodeKnownInput");
    if (!optionsStatus) {
        return ::media::Result<MediaPipelinePlan>::failure(optionsStatus.error());
    }
    return buildVideoTranscodePlan(std::move(inputInfo), inputUrl, std::move(options));
}

} // namespace media::ffmpeg::graph
