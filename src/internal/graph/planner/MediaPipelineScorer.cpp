#include "internal/graph/planner/MediaPipelineScorer.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

namespace {

constexpr int kUnavailableBaseScore = -100000;
constexpr int kUnavailableStagePenalty = 1000;
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

int priorityTotal(const MediaPipelineChainPlan& chain) noexcept
{
    return chain.decoder.score + chain.filter.score + chain.encoder.score;
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

} // namespace

MediaPipelineChainPlan MediaPipelineScorer::scoreChain(MediaPipelineChainPlan chain,
                                                       const MediaPipelinePlannerOptions& /*options*/)
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
        chain.score = kUnavailableBaseScore - missing * kUnavailableStagePenalty;
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

    std::sort(chains.begin(), chains.end(), [&](const MediaPipelineChainPlan& lhs,
                                                const MediaPipelineChainPlan& rhs) {
        if (lhs.available != rhs.available) {
            return lhs.available && !rhs.available;
        }
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }

        const bool lhsPreferred = preferredHardwareMatch(lhs, options);
        const bool rhsPreferred = preferredHardwareMatch(rhs, options);
        if (lhsPreferred != rhsPreferred) {
            return lhsPreferred && !rhsPreferred;
        }

        if (lhs.zeroCopy != rhs.zeroCopy) {
            return lhs.zeroCopy && !rhs.zeroCopy;
        }
        if (lhs.sameHardwareDevice != rhs.sameHardwareDevice) {
            return lhs.sameHardwareDevice && !rhs.sameHardwareDevice;
        }
        if (lhs.allHardware != rhs.allHardware) {
            return lhs.allHardware && !rhs.allHardware;
        }

        const int lhsPriority = priorityTotal(lhs);
        const int rhsPriority = priorityTotal(rhs);
        if (lhsPriority != rhsPriority) {
            return lhsPriority > rhsPriority;
        }

        return lhs.label < rhs.label;
    });

    return chains;
}

} // namespace media::ffmpeg::graph
