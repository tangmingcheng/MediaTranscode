#include "internal/graph/planner/MediaPipelinePlanner.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/MediaPipelineHardwareBackendConstraint.h"
#include "internal/graph/planner/MediaEncoderRateControlPlanner.h"
#include "internal/graph/planner/MediaPipelineScorer.h"
#include "internal/graph/planner/capability/MediaHardwareCapabilityProbe.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <limits>
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
    return !chain.decoder.hardware() && !chain.encoder.hardware() &&
           (!chain.filterActive || !chain.filter.hardware());
}

bool isRkmppChain(const MediaPipelineChainPlan& chain) noexcept
{
    return chain.decoder.deviceKind() == MediaHardwareDeviceKind::RKMPP ||
           chain.encoder.deviceKind() == MediaHardwareDeviceKind::RKMPP;
}

bool sameFrameDomain(const MediaHardwareDescriptor& left,
                     const MediaHardwareDescriptor& right) noexcept
{
    return left.deviceKind == right.deviceKind &&
           left.frameKind == right.frameKind &&
           left.deviceName == right.deviceName &&
           left.pixelFormat == right.pixelFormat &&
           left.surfacePixelFormat == right.surfacePixelFormat &&
           left.zeroCopyPreferred == right.zeroCopyPreferred &&
           left.requiresHardwareDeviceContext == right.requiresHardwareDeviceContext &&
           left.requiresHardwareFramesContext == right.requiresHardwareFramesContext;
}

bool completeRkmppFrameContract(const MediaHardwareDescriptor& contract) noexcept
{
    return contract.deviceKind == MediaHardwareDeviceKind::RKMPP &&
           contract.frameKind == MediaHardwareFrameKind::Hardware &&
           contract.deviceName == "rkmpp" &&
           contract.pixelFormat == "drm_prime" &&
           contract.surfacePixelFormat == "nv12" &&
           contract.zeroCopyPreferred &&
           !contract.requiresHardwareDeviceContext &&
           !contract.requiresHardwareFramesContext;
}

::media::Status validateRkmppFrameContracts(
    const MediaPipelineChainPlan& chain,
    const MediaPipelinePlannerOptions& options)
{
    if (!chain.decoder.outputFrame || !chain.encoder.inputFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::hardwareUnavailable(
                "RKMPP frame contract requires decoder output and encoder input"));
    }
    if (chain.transferDirection != MediaHardwareTransferDirection::None) {
        return ::media::Status::failure(
            ::media::ErrorInfo::hardwareUnavailable(
                "RKMPP frame contract requires no boundary transfer"));
    }

    const MediaHardwareDescriptor& decoderOutput = *chain.decoder.outputFrame;
    const MediaHardwareDescriptor& encoderInput = *chain.encoder.inputFrame;
    if (!completeRkmppFrameContract(decoderOutput) ||
        !completeRkmppFrameContract(encoderInput)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::hardwareUnavailable(
                "RKMPP frame contract is incomplete or not DRM PRIME/NV12 zero-copy"));
    }

    if (!chain.allHardware || !chain.sameHardwareDevice || !chain.zeroCopy ||
        !chain.decoder.hardware() || !chain.decoder.zeroCopy() ||
        !chain.encoder.hardware() || !chain.encoder.zeroCopy() ||
        !sameFrameDomain(decoderOutput, encoderInput)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::hardwareUnavailable(
                "RKMPP frame contract is inconsistent or crosses frame domains"));
    }

    if (!chain.filterActive) {
        if (!chain.filter.filterName.empty()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::hardwareUnavailable(
                    "RKMPP frame contract forbids a source-size filter"));
        }
        return ::media::Status::success();
    }

    const std::string expectedFilter =
        "scale_rkrga=w=" + std::to_string(options.targetWidth) +
        ":h=" + std::to_string(options.targetHeight) + ":format=nv12";
    if (chain.filter.filterName != expectedFilter ||
        !chain.filter.inputFrame || !chain.filter.outputFrame ||
        !completeRkmppFrameContract(*chain.filter.inputFrame) ||
        !completeRkmppFrameContract(*chain.filter.outputFrame) ||
        !sameFrameDomain(decoderOutput, *chain.filter.inputFrame) ||
        !sameFrameDomain(*chain.filter.inputFrame, *chain.filter.outputFrame) ||
        !sameFrameDomain(*chain.filter.outputFrame, encoderInput) ||
        !chain.filter.hardware() || !chain.filter.zeroCopy()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::hardwareUnavailable(
                "RKMPP frame contract requires one complete zero-copy filter domain"));
    }

    return ::media::Status::success();
}

void logSelectedPlan(const MediaPipelinePlannerOptions& options,
                     const MediaPipelinePlan& plan)
{
    const MediaPipelineChainPlan& selected = plan.selected;
    const MediaHardwareDescriptor* encoderInput =
        selected.encoder.inputFrame ? &*selected.encoder.inputFrame : nullptr;

    std::ostringstream out;
    out << "branch_mode=" << mediaBranchModeName(plan.branchMode)
        << " selected_chain=" << selected.label
        << " score=" << selected.score
        << " source_stream=" << plan.sourceStreamIndex
        << " decoder=" << stageDisplayName(selected.decoder)
        << " filter=" << stageDisplayName(selected.filter)
        << " encoder=" << stageDisplayName(selected.encoder)
        << " encoder_pix_fmt=" << emptyAsNone(encoderInput ? encoderInput->pixelFormat : std::string())
        << " encoder_surface_fmt=" << emptyAsNone(encoderInput ? encoderInput->surfacePixelFormat : std::string())
        << " backend=" << mediaHardwareDeviceKindName(selected.decoder.deviceKind())
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
    auto backendValidation = MediaPipelineHardwareBackendConstraint::validate(
        options.hardwareBackend, options.disableHardware, context);
    if (!backendValidation) {
        return backendValidation;
    }
    if (options.targetWidth < 0 || options.targetHeight < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(context + " requires non-negative target dimensions"));
    }
    if ((options.targetWidth > 0) != (options.targetHeight > 0)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(context + " requires target width and height together"));
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

void materializeVideoExecutionContract(MediaPipelineChainPlan& chain)
{
    chain.decoderLineagePropagation =
        chain.decoder.deviceKind() == MediaHardwareDeviceKind::RKMPP
        ? MediaVideoLineagePropagation::SubmissionOrder
        : MediaVideoLineagePropagation::CodecCopyOpaque;
    chain.encoderLineagePropagation =
        chain.encoder.deviceKind() == MediaHardwareDeviceKind::RKMPP
        ? MediaVideoLineagePropagation::SubmissionOrder
        : MediaVideoLineagePropagation::CodecCopyOpaque;
    chain.filterImplementation = !chain.filterActive
        ? MediaVideoFilterImplementation::None
        : chain.filter.deviceKind() == MediaHardwareDeviceKind::RKMPP
        ? MediaVideoFilterImplementation::Rga
        : MediaVideoFilterImplementation::Generic;
    chain.encoderAbortPolicy =
        chain.encoder.deviceKind() == MediaHardwareDeviceKind::RKMPP
        ? MediaVideoEncoderAbortPolicy::DrainThenAbort
        : MediaVideoEncoderAbortPolicy::Immediate;
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

    plan.enabled = true;
    plan.sourceStreamIndex = inputInfo.streamIndex;
    plan.inputCodecName = canonicalCodecName(inputInfo.codecName);
    plan.outputCodecName = canonicalCodecName(options.outputCodecName.empty() ? plan.inputCodecName : options.outputCodecName);
    if (inputInfo.width > 0 && inputInfo.height > 0) {
        options.probeWidth = inputInfo.width;
        options.probeHeight = inputInfo.height;
    }
    if (inputInfo.frameRate.isKnown()) {
        options.sourceFrameRate = inputInfo.frameRate;
    }

    const bool resizeRequested = options.targetWidth > 0 || options.targetHeight > 0;
    const bool canCopyPackets =
        options.hardwareBackend == MediaHardwareBackendRequest::Auto &&
        options.allowPacketCopy &&
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
    plan.filterActive = plan.selected.filterActive;
    MediaEncoderRateControlRequest rateControlRequest =
        options.encoderRateControl;
    if (!rateControlRequest.targetBitrateKbps &&
        inputInfo.bitrateBitsPerSecond > 0) {
        const std::int64_t kbps =
            (inputInfo.bitrateBitsPerSecond + 999) / 1000;
        if (kbps > std::numeric_limits<int>::max()) {
            return ::media::Result<MediaPipelinePlan>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "input bitrate exceeds planner rate-control range"));
        }
        rateControlRequest.targetBitrateKbps = static_cast<int>(kbps);
    }
    auto rateControl = MediaEncoderRateControlPlanner::plan(
        plan.selected.encoder.ffmpegName,
        plan.selected.encoder.deviceKind(),
        rateControlRequest);
    if (!rateControl) {
        return ::media::Result<MediaPipelinePlan>::failure(rateControl.error());
    }
    plan.selected.encoder.encoderRateControl =
        std::move(rateControl).value();
    const MediaRational encoderFrameRate = options.targetFrameRate.isKnown()
        ? options.targetFrameRate : options.sourceFrameRate;
    const int encoderWidth = options.targetWidth > 0
        ? options.targetWidth : options.probeWidth;
    const int encoderHeight = options.targetHeight > 0
        ? options.targetHeight : options.probeHeight;
    const auto& requestedOpen = options.encoderOpenRequest;
    plan.selected.encoder.encoderOpenContract = MediaEncoderOpenContract{
        plan.selected.encoder.ffmpegName,
        encoderWidth,
        encoderHeight,
        encoderFrameRate,
        *plan.selected.encoder.encoderRateControl,
        requestedOpen.quality,
        requestedOpen.preset,
        requestedOpen.tune,
        requestedOpen.profile,
        requestedOpen.level,
        requestedOpen.gop,
        requestedOpen.bFrames,
        requestedOpen.globalHeader,
        options.lowLatency};
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

        if (!MediaPipelineHardwareBackendConstraint::accepts(
                candidate, candidate.filterActive, options.hardwareBackend)) {
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
            options.hardwareBackend == MediaHardwareBackendRequest::RKMPP
                ? "no complete RKMPP decoder/filter/encoder chain found"
                : options.disableHardware
                ? "no available explicit software decoder/filter/encoder chain found"
                : "no structurally available hardware decoder/filter/encoder chain found"));
}

::media::Status MediaPipelinePlanner::preflightSelectedCandidate(
    MediaPipelineChainPlan& selected,
    const MediaPipelinePlannerOptions& options,
    MediaHardwareCapabilityProbe& hardwareProbe)
{
    materializeVideoExecutionContract(selected);
    if (isRkmppChain(selected)) {
        auto contractStatus = validateRkmppFrameContracts(selected, options);
        if (!contractStatus) {
            return contractStatus;
        }
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
