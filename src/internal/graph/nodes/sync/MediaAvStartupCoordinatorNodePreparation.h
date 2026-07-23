#pragma once

#include "internal/graph/core/MediaNode.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaAvStartupCoordinatorNode;
class MediaAvStartupGenerationState;

class MediaAvStartupCoordinatorNodePreparation final {
public:
    MediaAvStartupCoordinatorNodePreparation(
        MediaAvStartupCoordinatorNodePreparation&&) noexcept = default;
    MediaAvStartupCoordinatorNodePreparation& operator=(
        MediaAvStartupCoordinatorNodePreparation&&) noexcept = default;

private:
    MediaAvStartupCoordinatorNodePreparation(
        std::unique_ptr<MediaAvStartupCoordinator> coordinator,
        std::shared_ptr<MediaAvStartupGenerationState> generationState,
        int outputAudioSampleRate);

    std::unique_ptr<MediaAvStartupCoordinator> m_coordinator;
    std::shared_ptr<MediaAvStartupGenerationState> m_generationState;
    int m_outputAudioSampleRate = 0;

    friend class MediaAvStartupCoordinatorNode;
    friend ::media::Result<MediaAvStartupCoordinatorNodePreparation>
    prepareMediaAvStartupCoordinatorNode(const MediaNode& node);
};

::media::Result<MediaAvStartupCoordinatorNodePreparation>
prepareMediaAvStartupCoordinatorNode(const MediaNode& node);

} // namespace media::ffmpeg::graph
