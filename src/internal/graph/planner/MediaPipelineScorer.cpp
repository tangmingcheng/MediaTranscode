#include "internal/graph/planner/MediaPipelineScorer.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

namespace {

constexpr int kUnavailableScore = 0;
constexpr int kFullHardwareZeroCopyStageScore = 1000;
constexpr int kFullHardwareSameDeviceStageScore = 900;
constexpr int kFullHardwareTransferStageScore = 800;
constexpr int kMixedHardwareStageScore = 650;
constexpr int kSoftwareStageScore = 300;

bool sameFrameDomain(const MediaHardwareDescriptor& left,
                     const MediaHardwareDescriptor& right) noexcept
{
    return left.deviceKind == right.deviceKind &&
           left.frameKind == right.frameKind &&
           left.deviceName == right.deviceName &&
           left.pixelFormat == right.pixelFormat &&
           left.surfacePixelFormat == right.surfacePixelFormat &&
           left.zeroCopyPreferred == right.zeroCopyPreferred;
}

bool completeFrameContracts(const MediaPipelineChainPlan& chain,
                            const MediaPipelinePlannerOptions& options) noexcept
{
    if (!chain.decoder.outputFrame || !chain.encoder.inputFrame ||
        chain.transferDirection == MediaHardwareTransferDirection::Unknown) {
        return false;
    }
    if (options.filterRequired) {
        if (!chain.filter.inputFrame || !chain.filter.outputFrame) {
            return false;
        }
        return chain.transferDirection == MediaHardwareTransferDirection::None
                   ? sameFrameDomain(*chain.decoder.outputFrame, *chain.filter.inputFrame) &&
                         sameFrameDomain(*chain.filter.outputFrame, *chain.encoder.inputFrame)
                   : true;
    }
    return chain.transferDirection == MediaHardwareTransferDirection::None
               ? sameFrameDomain(*chain.decoder.outputFrame, *chain.encoder.inputFrame)
               : true;
}

bool sameHardwareDevice(const MediaPipelineChainPlan& chain, const MediaPipelinePlannerOptions& options) noexcept
{
    if (options.filterRequired) {
        return chain.decoder.deviceKind() == chain.filter.deviceKind() &&
               chain.filter.deviceKind() == chain.encoder.deviceKind() &&
               chain.decoder.deviceKind() != MediaHardwareDeviceKind::None &&
               chain.decoder.deviceKind() != MediaHardwareDeviceKind::Unknown;
    }

    return chain.decoder.deviceKind() == chain.encoder.deviceKind() &&
           chain.decoder.deviceKind() != MediaHardwareDeviceKind::None &&
           chain.decoder.deviceKind() != MediaHardwareDeviceKind::Unknown;
}

bool hardwareUnavailable(const MediaPipelineStagePlan& stage)
{
    return stage.hardware() && !stage.available &&
           stage.availabilityReason.find("hardware backend not found") != std::string::npos;
}

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

int availableStageSemanticScore(const MediaPipelineStagePlan& stage,
                                const MediaPipelineChainPlan& chain) noexcept
{
    if (!stage.hardware()) {
        return kSoftwareStageScore;
    }

    if (chain.allHardware && chain.sameHardwareDevice && chain.zeroCopy) {
        return kFullHardwareZeroCopyStageScore;
    }

    if (chain.allHardware && chain.sameHardwareDevice) {
        return kFullHardwareSameDeviceStageScore;
    }

    if (chain.allHardware) {
        return kFullHardwareTransferStageScore;
    }

    return kMixedHardwareStageScore;
}

int declaredStagePriority(const MediaPipelineChainPlan& chain,
                          const MediaPipelinePlannerOptions& options) noexcept
{
    int priority = chain.decoder.priority + chain.encoder.priority;
    if (options.filterRequired) {
        priority += chain.filter.priority;
    }
    return priority;
}

std::string unavailableReason(const MediaPipelineChainPlan& chain,
                              const MediaPipelinePlannerOptions& options)
{
    std::ostringstream out;
    out << "unavailable chain; score=0";

    auto appendStage = [&](const MediaPipelineStagePlan& stage) {
        if (stage.available) {
            return;
        }

        if (hardwareUnavailable(stage)) {
            out << "; hardware=" << mediaHardwareDeviceKindName(stage.deviceKind()) << " unavailable";
            return;
        }

        out << "; " << mediaPipelineStageRoleName(stage.role)
            << "=" << stageDisplayName(stage)
            << " unavailable: " << stage.availabilityReason;
    };

    appendStage(chain.decoder);
    if (options.filterRequired) {
        appendStage(chain.filter);
    }
    appendStage(chain.encoder);
    return out.str();
}

std::string availableReason(const MediaPipelineChainPlan& chain,
                            const MediaPipelinePlannerOptions& options)
{
    const std::string scoreText = std::to_string(chain.score);

    if (chain.allHardware && chain.sameHardwareDevice && chain.zeroCopy) {
        return "full hardware zero-copy chain; score=" + scoreText;
    }
    if (chain.allHardware && chain.sameHardwareDevice) {
        return "full hardware same-device chain; score=" + scoreText;
    }
    if (chain.allHardware) {
        return "full hardware chain with transfer risk; score=" + scoreText;
    }
    if (chain.decoder.hardware() || chain.encoder.hardware() || (options.filterRequired && chain.filter.hardware())) {
        return options.filterRequired ? "mixed hardware/software chain" : "mixed hardware/software chain; filter stage not required";
    }
    return "explicit software chain; score=" + scoreText;
}

void logCandidate(const MediaPipelinePlannerOptions& options,
                  const MediaPipelineChainPlan& chain)
{
    std::ostringstream out;
    out << "candidate=" << chain.label
        << " score=" << chain.score
        << " status=" << (chain.available ? "available" : "unavailable");

    if (chain.available) {
        out << " decoder=" << stageDisplayName(chain.decoder);
        if (options.filterRequired) {
            out << " filter=" << stageDisplayName(chain.filter);
        } else {
            out << " filter=not_required";
        }
        out << " encoder=" << stageDisplayName(chain.encoder)
            << " zero_copy=" << (chain.zeroCopy ? "true" : "false");
    } else {
        out << " reason=\"" << chain.reason << "\"";
    }

    mediaGraphDiagnosticLog(options.diagnosticLogEnabled,
                            MediaGraphDiagnosticPhase::PlannerScore,
                            out.str());
}

MediaPipelineChainPlan unavailableChain(MediaPipelineChainPlan chain, std::string reason)
{
    chain.available = false;
    chain.allHardware = false;
    chain.sameHardwareDevice = false;
    chain.zeroCopy = false;
    chain.score = kUnavailableScore;
    chain.reason = std::move(reason);
    return chain;
}

} // namespace

MediaPipelineChainPlan MediaPipelineScorer::scoreChain(MediaPipelineChainPlan chain,
                                                       const MediaPipelinePlannerOptions& options)
{
    if (!completeFrameContracts(chain, options)) {
        return unavailableChain(
            std::move(chain),
            "unavailable chain; incomplete or inconsistent planner frame/transfer contracts");
    }
    chain.available = chain.decoder.available && chain.encoder.available &&
                      (!options.filterRequired || chain.filter.available);

    if (!chain.available) {
        chain.allHardware = false;
        chain.sameHardwareDevice = false;
        chain.zeroCopy = false;
        chain.score = kUnavailableScore;
        chain.reason = unavailableReason(chain, options);
        return chain;
    }

    chain.allHardware = chain.decoder.hardware() && chain.encoder.hardware() &&
                        (!options.filterRequired || chain.filter.hardware());
    chain.sameHardwareDevice = chain.allHardware && sameHardwareDevice(chain, options);
    chain.zeroCopy = chain.sameHardwareDevice &&
                     chain.decoder.zeroCopy() && chain.encoder.zeroCopy() &&
                     (!options.filterRequired || chain.filter.zeroCopy());

    chain.score = availableStageSemanticScore(chain.decoder, chain) +
                  availableStageSemanticScore(chain.encoder, chain) +
                  declaredStagePriority(chain, options);
    if (options.filterRequired) {
        chain.score += availableStageSemanticScore(chain.filter, chain);
    }

    chain.reason = availableReason(chain, options);
    return chain;
}

std::vector<MediaPipelineChainPlan> MediaPipelineScorer::scoreAndSortChains(
    std::vector<MediaPipelineChainPlan> chains,
    const MediaPipelinePlannerOptions& options)
{
    for (MediaPipelineChainPlan& chain : chains) {
        chain = scoreChain(std::move(chain), options);
    }

    std::sort(chains.begin(), chains.end(),
              [](const MediaPipelineChainPlan& a, const MediaPipelineChainPlan& b) {
                  if (a.score != b.score) {
                      return a.score > b.score;
                  }
                  return a.label < b.label;
              });

    for (const MediaPipelineChainPlan& chain : chains) {
        logCandidate(options, chain);
    }

    return chains;
}

} // namespace media::ffmpeg::graph
