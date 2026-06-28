#include "internal/graph/planner/MediaPipelineScorer.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

namespace {

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

int availableStageScore(const MediaPipelineStagePlan& stage, const MediaPipelinePlannerOptions& options)
{
    int score = stage.score;
    score += stage.hardware ? 220 : 40;
    score += stage.zeroCopy ? 60 : 0;

    if (!options.preferGpu && !stage.hardware) {
        score += 80;
    }

    return score;
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

std::string unavailableReason(const MediaPipelineChainPlan& chain)
{
    std::ostringstream out;
    out << "unavailable chain";

    auto appendStage = [&](const MediaPipelineStagePlan& stage) {
        if (stage.available) {
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
        return "full hardware zero-copy chain";
    }
    if (chain.allHardware) {
        return "full hardware chain with transfer risk";
    }
    if (chain.decoder.hardware || chain.filter.hardware || chain.encoder.hardware) {
        return "mixed hardware/software chain";
    }
    return "software fallback chain";
}

} // namespace

MediaPipelineChainPlan MediaPipelineScorer::scoreChain(MediaPipelineChainPlan chain,
                                                       const MediaPipelinePlannerOptions& options)
{
    chain.available = chain.decoder.available && chain.filter.available && chain.encoder.available;

    if (!chain.available) {
        int missing = 0;
        missing += chain.decoder.available ? 0 : 1;
        missing += chain.filter.available ? 0 : 1;
        missing += chain.encoder.available ? 0 : 1;
        chain.allHardware = false;
        chain.sameHardwareDevice = false;
        chain.zeroCopy = false;
        chain.score = -100000 - missing * 1000;
        chain.reason = unavailableReason(chain);
        return chain;
    }

    chain.allHardware = chain.decoder.hardware && chain.filter.hardware && chain.encoder.hardware;
    chain.sameHardwareDevice = chain.allHardware && sameHardwareDevice(chain);
    chain.zeroCopy = chain.sameHardwareDevice &&
                     chain.decoder.zeroCopy && chain.filter.zeroCopy && chain.encoder.zeroCopy;

    chain.score = availableStageScore(chain.decoder, options) +
                  availableStageScore(chain.filter, options) +
                  availableStageScore(chain.encoder, options);

    if (chain.allHardware) {
        chain.score += options.preferGpu ? 900 : 250;
    }
    if (chain.sameHardwareDevice) {
        chain.score += 450;
    }
    if (chain.zeroCopy) {
        chain.score += 350;
    }

    const std::string preferred = lowerCopy(options.preferredHardware);
    if (!preferred.empty() && preferred != "auto") {
        const std::string selectedDevice = lowerCopy(mediaHardwareDeviceKindName(chain.decoder.deviceKind));
        if (selectedDevice == preferred && chain.sameHardwareDevice) {
            chain.score += 500;
        } else if (chain.allHardware) {
            chain.score -= 100;
        }
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

    std::sort(chains.begin(), chains.end(), [](const MediaPipelineChainPlan& lhs,
                                               const MediaPipelineChainPlan& rhs) {
        if (lhs.available != rhs.available) {
            return lhs.available && !rhs.available;
        }
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.label < rhs.label;
    });

    return chains;
}

} // namespace media::ffmpeg::graph
