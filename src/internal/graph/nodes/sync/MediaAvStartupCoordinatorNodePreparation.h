#pragma once

#include "internal/graph/core/MediaNode.h"
#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "media_transcode/Result.h"

#include "internal/graph/model/MediaStartupSharedObjectPlan.h"
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
        MediaLineageAllocationScope allocation,
        MediaStartupSharedObjectPlan releaseStorage,
        MediaLineageUniquePtr<MediaAvStartupCoordinator> coordinator,
        std::shared_ptr<MediaAvStartupGenerationState> generationState);

    MediaLineageAllocationScope m_allocation;
    MediaStartupSharedObjectPlan m_releaseStorage;
    MediaLineageUniquePtr<MediaAvStartupCoordinator> m_coordinator;
    std::shared_ptr<MediaAvStartupGenerationState> m_generationState;

    friend class MediaAvStartupCoordinatorNode;
    friend ::media::Result<MediaAvStartupCoordinatorNodePreparation>
    prepareMediaAvStartupCoordinatorNode(const MediaNode& node, MediaLineageAllocationScope allocation);
};

::media::Result<MediaAvStartupCoordinatorNodePreparation>
prepareMediaAvStartupCoordinatorNode(const MediaNode& node, MediaLineageAllocationScope allocation);

} // namespace media::ffmpeg::graph
