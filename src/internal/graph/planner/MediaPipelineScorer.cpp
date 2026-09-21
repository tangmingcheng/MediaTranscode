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

bool completeSourceFrameContracts(const MediaVideoSourcePlan& source) noexcept
{
    if (!source.decoder.outputFrame || source.transferDirection == MediaHardwareTransferDirection::Unknown)
        return false;
    if (!source.filterActive) return true;
    return source.filter.inputFrame && source.filter.outputFrame &&
        (source.transferDirection != MediaHardwareTransferDirection::None ||
         MediaPipelineScorer::sameFrameDomain(*source.decoder.outputFrame, *source.filter.inputFrame));
}

bool completeFrameContracts(const MediaPipelineChainPlan& chain,
                            const MediaPipelinePlannerOptions&) noexcept
{
    if (!completeSourceFrameContracts(chain) || !chain.encoder.inputFrame) return false;
    const auto& output = chain.filterActive ? chain.filter.outputFrame : chain.decoder.outputFrame;
    return chain.transferDirection != MediaHardwareTransferDirection::None ||
        MediaPipelineScorer::sameFrameDomain(*output, *chain.encoder.inputFrame);
}

bool sameHardwareDevice(const MediaPipelineChainPlan& chain, const MediaPipelinePlannerOptions& options) noexcept
{
    if (chain.filterActive) {
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
                                bool allHardware, bool sameHardwareDevice, bool zeroCopy) noexcept
{
    if (!stage.hardware()) {
        return kSoftwareStageScore;
    }

    if (allHardware && sameHardwareDevice && zeroCopy) {
        return kFullHardwareZeroCopyStageScore;
    }

    if (allHardware && sameHardwareDevice) {
        return kFullHardwareSameDeviceStageScore;
    }

    if (allHardware) {
        return kFullHardwareTransferStageScore;
    }

    return kMixedHardwareStageScore;
}

int declaredStagePriority(const MediaPipelineChainPlan& chain,
                          const MediaPipelinePlannerOptions& options) noexcept
{
    int priority = chain.decoder.priority + chain.encoder.priority;
    if (chain.filterActive) {
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
    if (chain.filterActive) {
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
    if (chain.decoder.hardware() || chain.encoder.hardware() || (chain.filterActive && chain.filter.hardware())) {
        return chain.filterActive ? "mixed hardware/software chain" : "mixed hardware/software chain; filter stage not active";
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
        if (chain.filterActive) {
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

bool MediaPipelineScorer::sameFrameDomain(const MediaHardwareDescriptor& left,
                                           const MediaHardwareDescriptor& right) noexcept
{
    return left.deviceKind == right.deviceKind && left.frameKind == right.frameKind &&
        left.deviceName == right.deviceName && left.pixelFormat == right.pixelFormat &&
        left.surfacePixelFormat == right.surfacePixelFormat &&
        left.zeroCopyPreferred == right.zeroCopyPreferred;
}

MediaVideoSourcePlan MediaPipelineScorer::scoreSource(MediaVideoSourcePlan source,
                                                     const MediaHardwareDescriptor& target)
{
    const auto& output = source.filterActive ? source.filter.outputFrame : source.decoder.outputFrame;
    source.available = completeSourceFrameContracts(source) && output &&
        *output == target && source.decoder.available &&
        (!source.filterActive || source.filter.available);
    source.allHardware = source.available && source.decoder.hardware() &&
        (!source.filterActive || source.filter.hardware());
    source.sameHardwareDevice = source.allHardware &&
        source.decoder.deviceKind() == target.deviceKind &&
        (!source.filterActive || source.filter.deviceKind() == target.deviceKind);
    source.zeroCopy = source.sameHardwareDevice && source.decoder.zeroCopy() &&
        (!source.filterActive || source.filter.zeroCopy()) && target.zeroCopyPreferred;
    source.score = source.available
        ? availableStageSemanticScore(source.decoder, source.allHardware,
            source.sameHardwareDevice, source.zeroCopy) + source.decoder.priority : kUnavailableScore;
    if (source.available && source.filterActive)
        source.score += availableStageSemanticScore(source.filter, source.allHardware,
            source.sameHardwareDevice, source.zeroCopy) + source.filter.priority;
    source.reason = source.available ? "source candidate frame contracts matched; execution unverified"
                                    : "source candidate is unavailable or has inconsistent frame contracts";
    return source;
}

MediaPipelineChainPlan MediaPipelineScorer::scoreChain(MediaPipelineChainPlan chain,
                                                       const MediaPipelinePlannerOptions& options)
{
    if (!completeFrameContracts(chain, options)) {
        return unavailableChain(
            std::move(chain),
            "unavailable chain; incomplete or inconsistent planner frame/transfer contracts");
    }
    chain.available = chain.decoder.available && chain.encoder.available &&
                      (!chain.filterActive || chain.filter.available);

    if (!chain.available) {
        chain.allHardware = false;
        chain.sameHardwareDevice = false;
        chain.zeroCopy = false;
        chain.score = kUnavailableScore;
        chain.reason = unavailableReason(chain, options);
        return chain;
    }

    chain.allHardware = chain.decoder.hardware() && chain.encoder.hardware() &&
                        (!chain.filterActive || chain.filter.hardware());
    chain.sameHardwareDevice = chain.allHardware && sameHardwareDevice(chain, options);
    chain.zeroCopy = chain.sameHardwareDevice &&
                     chain.decoder.zeroCopy() && chain.encoder.zeroCopy() &&
                     (!chain.filterActive || chain.filter.zeroCopy());

    chain.score = availableStageSemanticScore(chain.decoder, chain.allHardware, chain.sameHardwareDevice, chain.zeroCopy) +
                  availableStageSemanticScore(chain.encoder, chain.allHardware, chain.sameHardwareDevice, chain.zeroCopy) +
                  declaredStagePriority(chain, options);
    if (chain.filterActive) {
        chain.score += availableStageSemanticScore(chain.filter, chain.allHardware, chain.sameHardwareDevice, chain.zeroCopy);
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
