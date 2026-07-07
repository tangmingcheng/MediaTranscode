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

bool sameHardwareDevice(const MediaPipelineChainPlan& chain, const MediaPipelinePlannerOptions& options) noexcept
{
    if (options.filterRequired) {
        return chain.decoder.deviceKind == chain.filter.deviceKind &&
               chain.filter.deviceKind == chain.encoder.deviceKind &&
               chain.decoder.deviceKind != MediaHardwareDeviceKind::None &&
               chain.decoder.deviceKind != MediaHardwareDeviceKind::Unknown;
    }

    return chain.decoder.deviceKind == chain.encoder.deviceKind &&
           chain.decoder.deviceKind != MediaHardwareDeviceKind::None &&
           chain.decoder.deviceKind != MediaHardwareDeviceKind::Unknown;
}

bool containsHardwareStage(const MediaPipelineChainPlan& chain, const MediaPipelinePlannerOptions& options) noexcept
{
    return chain.decoder.hardware || chain.encoder.hardware ||
           (options.filterRequired && chain.filter.hardware);
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
            out << "; hardware=" << mediaHardwareDeviceKindName(stage.deviceKind) << " unavailable";
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
    const char* scoreText = options.filterRequired ? "1000+1000+1000" : "1000+1000";
    const char* sameDeviceScoreText = options.filterRequired ? "900+900+900" : "900+900";
    const char* transferScoreText = options.filterRequired ? "800+800+800" : "800+800";
    const char* softwareScoreText = options.filterRequired ? "300+300+300" : "300+300";

    if (chain.allHardware && chain.sameHardwareDevice && chain.zeroCopy) {
        return std::string("full hardware zero-copy chain; score=") + scoreText;
    }
    if (chain.allHardware && chain.sameHardwareDevice) {
        return std::string("full hardware same-device chain; score=") + sameDeviceScoreText;
    }
    if (chain.allHardware) {
        return std::string("full hardware chain with transfer risk; score=") + transferScoreText;
    }
    if (chain.decoder.hardware || chain.encoder.hardware || (options.filterRequired && chain.filter.hardware)) {
        return options.filterRequired ? "mixed hardware/software chain" : "mixed hardware/software chain; filter stage not required";
    }
    return std::string("explicit software chain; score=") + softwareScoreText;
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
    if (!options.preferGpu && containsHardwareStage(chain, options)) {
        return unavailableChain(std::move(chain), "hardware disabled by request");
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

    chain.allHardware = chain.decoder.hardware && chain.encoder.hardware &&
                        (!options.filterRequired || chain.filter.hardware);
    chain.sameHardwareDevice = chain.allHardware && sameHardwareDevice(chain, options);
    chain.zeroCopy = chain.sameHardwareDevice &&
                     chain.decoder.zeroCopy && chain.encoder.zeroCopy &&
                     (!options.filterRequired || chain.filter.zeroCopy);

    chain.score = availableStageSemanticScore(chain.decoder, chain) +
                  availableStageSemanticScore(chain.encoder, chain);
    if (options.filterRequired) {
        chain.score += availableStageSemanticScore(chain.filter, chain);
    }

    if (preferredHardwareMatch(chain, options)) {
        chain.score += 100;
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

    std::sort(chains.begin(), chains.end(), [](const MediaPipelineChainPlan& a, const MediaPipelineChainPlan& b) {
        return a.score > b.score;
    });

    for (const MediaPipelineChainPlan& chain : chains) {
        logCandidate(options, chain);
    }

    return chains;
}

} // namespace media::ffmpeg::graph
