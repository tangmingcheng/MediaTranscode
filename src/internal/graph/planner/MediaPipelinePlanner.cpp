#include "internal/graph/planner/MediaPipelinePlanner.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/MediaPipelineScorer.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <algorithm>
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
        << " decoder=" << emptyAsNone(stageDisplayName(selected.decoder))
        << " transfer=" << emptyAsNone(stageDisplayName(selected.transfer))
        << " filter=" << emptyAsNone(stageDisplayName(selected.filter))
        << " encoder=" << emptyAsNone(stageDisplayName(selected.encoder))
        << " zero_copy=" << (selected.zeroCopy ? "true" : "false")
        << " all_hardware=" << (selected.allHardware ? "true" : "false");

    mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                            MediaGraphDiagnosticPhase::Planner,
                            out.str());
}

::media::Result<MediaPipelinePlan> buildVideoTranscodePlan(
    const MediaInputVideoStreamInfo& source,
    std::string outputCodec,
    MediaPipelinePlannerOptions options)
{
    if (!source.valid()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPipelinePlanner requires valid input video stream info"));
    }

    outputCodec = normalizeCodecName(outputCodec);
    if (outputCodec.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPipelinePlanner requires output codec"));
    }

    MediaPipelinePlan plan;
    plan.enabled = true;
    plan.branchMode = MediaBranchMode::Transcode;
    plan.inputCodecName = normalizeCodecName(source.codecName);
    plan.outputCodecName = std::move(outputCodec);
    plan.inputPixelFormat = source.pixelFormat;
    plan.inputWidth = source.width;
    plan.inputHeight = source.height;
    plan.inputStreamIndex = source.streamIndex;
    plan.inputFrameRate = source.frameRate;
    plan.inputTimeBase = source.timeBase;
    plan.inputBitrateKbps = source.bitrateKbps;

    if (plan.inputCodecName.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPipelinePlanner requires input codec"));
    }

    MediaPipelineCapabilityScanner scanner;
    auto candidates = scanner.scanVideoTranscodeCandidates(source, plan.outputCodecName, options);
    if (!candidates) {
        return ::media::Result<MediaPipelinePlan>::failure(candidates.error());
    }
    plan.candidates = std::move(candidates).value();

    if (plan.candidates.empty()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::unsupported("MediaPipelinePlanner found no usable video pipeline"));
    }

    for (MediaPipelineChainPlan& candidate : plan.candidates) {
        MediaPipelineScorer::score(candidate, options);
    }

    std::sort(plan.candidates.begin(), plan.candidates.end(), [](const MediaPipelineChainPlan& lhs,
                                                                const MediaPipelineChainPlan& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        if (lhs.allHardware != rhs.allHardware) {
            return lhs.allHardware;
        }
        if (lhs.zeroCopy != rhs.zeroCopy) {
            return lhs.zeroCopy;
        }
        return lhs.label < rhs.label;
    });

    auto selected = std::find_if(plan.candidates.begin(), plan.candidates.end(), [&](const MediaPipelineChainPlan& chain) {
        return !options.requireAllHardware || chain.allHardware;
    });
    if (selected == plan.candidates.end()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::unsupported("MediaPipelinePlanner found no all-hardware pipeline"));
    }

    plan.selected = *selected;
    logSelectedPlan(options, plan);
    return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
}

} // namespace

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoTranscode(
    const MediaInputVideoStreamInfo& source,
    std::string outputCodec,
    MediaPipelinePlannerOptions options)
{
    return buildVideoTranscodePlan(source, std::move(outputCodec), std::move(options));
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoTranscodeFile(
    const std::string& inputUrl,
    MediaPipelinePlannerOptions options)
{
    auto source = probeInputVideoStream(inputUrl, options);
    if (!source) {
        return ::media::Result<MediaPipelinePlan>::failure(source.error());
    }
    return buildVideoTranscodePlan(source.value(), options.outputCodecName, std::move(options));
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoPacketCopy(
    const MediaInputVideoStreamInfo& source,
    MediaPipelinePlannerOptions options)
{
    if (!source.valid()) {
        return ::media::Result<MediaPipelinePlan>::failure(
            ::media::ErrorInfo::invalidArgument("MediaPipelinePlanner requires valid input video stream info"));
    }

    MediaPipelinePlan plan;
    plan.enabled = true;
    plan.branchMode = MediaBranchMode::PacketCopy;
    plan.inputCodecName = normalizeCodecName(source.codecName);
    plan.outputCodecName = plan.inputCodecName;
    plan.inputPixelFormat = source.pixelFormat;
    plan.inputWidth = source.width;
    plan.inputHeight = source.height;
    plan.inputStreamIndex = source.streamIndex;
    plan.inputFrameRate = source.frameRate;
    plan.inputTimeBase = source.timeBase;
    plan.inputBitrateKbps = source.bitrateKbps;

    MediaPipelineChainPlan selected;
    selected.label = "packet-copy";
    selected.score = 1000;
    selected.zeroCopy = true;
    selected.allHardware = false;
    plan.selected = selected;
    plan.candidates.push_back(selected);
    logSelectedPlan(options, plan);
    return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
}

::media::Result<MediaPipelinePlan> MediaPipelinePlanner::planVideoDrop(
    const MediaInputVideoStreamInfo& source,
    MediaPipelinePlannerOptions options)
{
    MediaPipelinePlan plan;
    plan.enabled = false;
    plan.branchMode = MediaBranchMode::Drop;
    plan.inputCodecName = normalizeCodecName(source.codecName);
    plan.inputPixelFormat = source.pixelFormat;
    plan.inputWidth = source.width;
    plan.inputHeight = source.height;
    plan.inputStreamIndex = source.streamIndex;
    plan.inputFrameRate = source.frameRate;
    plan.inputTimeBase = source.timeBase;
    plan.inputBitrateKbps = source.bitrateKbps;

    MediaPipelineChainPlan selected;
    selected.label = "drop";
    selected.score = 0;
    plan.selected = selected;
    plan.candidates.push_back(selected);
    logSelectedPlan(options, plan);
    return ::media::Result<MediaPipelinePlan>::success(std::move(plan));
}

} // namespace media::ffmpeg::graph
