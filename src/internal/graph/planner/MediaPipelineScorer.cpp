#include "internal/graph/planner/MediaPipelineScorer.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <algorithm>
#include <cctype>
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

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool sameHardwareDevice(const MediaPipelineChainPlan& chain) noexcept
{
    return chain.decoder.deviceKind == chain.filter.deviceKind &&
           chain.filter.deviceKind == chain.encoder.deviceKind &&
           chain.decoder.deviceKind != MediaHardwareDeviceKind::None &&
           chain.decoder.deviceKind != MediaHardwareDeviceKind::Unknown;
}

bool containsHardwareStage(const MediaPipelineChainPlan& chain) noexcept
{
    return chain.decoder.hardware || chain.filter.hardware || chain.encoder.hardware;
}

bool hardwareUnavailable(const MediaPipelineStagePlan& stage)
{
    return stage.hardware && !stage.available &&
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

bool preferredHardwareMatch(const MediaPipelineChainPlan& chain,
                            const MediaPipelinePlannerOptions& options)
{
    const std::string preferred = lowerCopy(options.preferredHardware);
    if (preferred.empty() || preferred == "auto" || !chain.sameHardwareDevice) {
        return false;
    }

    return lowerCopy(mediaHardwareDeviceKindName(chain.decoder.deviceKind)) == preferred;
}

int availableStageSemanticScore(const MediaPipelineStagePlan& stage,
                                const MediaPipelineChainPlan& chain) noexcept
{
    if (!stage.hardware) {
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

std::string unavailableReason(const MediaPipelineChainPlan& chain)
{
    std::ostringstream out;
    out << "unavailable chain; score=0";

    auto appendStage = [&](const MediaPipelineStagePlan& stage) {
        if (stage.available) {
            return;
        }

        if (hardwareUnavailable(stage)) {
            out << "; hardware=" << mediaHardwareDeviceKindName(stage.deviceKind) << " unavailable";
            return;
        }

        out << "; " << mediaPipelineStageRoleName(stage.role)
            << "=" << stageDisplayName(stage)
            << " unavailable: " << stage.availabilityReason;
    };

    appendStage(chain.decoder);
    appendStage(chain.filter);
    appendStage(chain.encoder);
    return out.str();
}

std::string availableReason(const MediaPipelineChainPlan& chain)
{
    if (chain.allHardware && chain.sameHardwareDevice && chain.zeroCopy) {
        return "full hardware zero-copy chain; score=1000+1000+1000";
    }
    if (chain.allHardware && chain.sameHardwareDevice) {
        return "full hardware same-device chain; score=900+900+900";
    }
    if (chain.allHardware) {
        return "full hardware chain with transfer risk; score=800+800+800";
    }
    if (chain.decoder.hardware || chain.filter.hardware || chain.encoder.hardware) {
        return "mixed hardware/software chain";
    }
    return "software fallback chain; score=300+300+300";
}

void logCandidate(const MediaPipelinePlannerOptions& options,
                  const MediaPipelineChainPlan& chain)
{
    std::ostringstream out;
    out << "candidate=" << chain.label
        << " score=" << chain.score
        << " status=" << (chain.available ? "available" : "unavailable");

    if (chain.available) {
        out << " decoder=" << stageDisplayName(chain.decoder)
            << " filter=" << stageDisplayName(chain.filter)
            << " encoder=" << stageDisplayName(chain.encoder)
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
    if (!options.preferGpu && containsHardwareStage(chain)) {
        return unavailableChain(std::move(chain), "hardware disabled by request");
    }

    chain.available = chain.decoder.available && chain.filter.available && chain.encoder.available;

    if (!chain.available) {
        chain.allHardware = false;
        chain.sameHardwareDevice = false;
        chain.zeroCopy = false;
        chain.score = kUnavailableScore;
        chain.reason = unavailableReason(chain);
        return chain;
    }

    chain.allHardware = chain.decoder.hardware && chain.filter.hardware && chain.encoder.hardware;
    chain.sameHardwareDevice = chain.allHardware && sameHardwareDevice(chain);
    chain.zeroCopy = chain.sameHardwareDevice &&
                     chain.decoder.zeroCopy && chain.filter.zeroCopy && chain.encoder.zeroCopy;

    chain.score = availableStageSemanticScore(chain.decoder, chain) +
                  availableStageSemanticScore(chain.filter, chain) +
                  availableStageSemanticScore(chain.encoder, chain);

    if (preferredHardwareMatch(chain, options)) {
        chain.score += 100;
    }

    chain.reason = availableReason(chain);
    return chain;
}

std::vector<MediaPipelineChainPlan> MediaPipelineScorer::scoreAndSortChains(
    std::vector<MediaPipelineChainPlan> chains,
    const MediaPipelinePlannerOptions& options)
{
    for (MediaPipelineChainPlan& chain : chains) {
        chain = scoreChain(std::move(chain), options);
    }

    std::sort(chains.begin(), chains.end(), [](const MediaPipelineChainPlan& a, const MediaPipelineChainPlan& b) {
        return a.score > b.score;
    });

    for (const MediaPipelineChainPlan& chain : chains) {
        logCandidate(options, chain);
    }

    return chains;
}

} // namespace media::ffmpeg::graph
