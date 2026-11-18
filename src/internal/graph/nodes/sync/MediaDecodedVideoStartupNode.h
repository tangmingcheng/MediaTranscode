#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/sync/lineage/MediaVideoReferenceLineageState.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/model/MediaStartupSharedObjectPlan.h"
#include <string>

namespace media::ffmpeg::graph {

// Packages real decoder output for the existing startup coordinator.
// It neither decodes again nor turns a display point into a duration interval.
class MediaDecodedVideoStartupNode final : public FFmpegNodeRuntime {
public:
    MediaDecodedVideoStartupNode(MediaNodeId id,
        std::shared_ptr<MediaAvSyncGroupRuntime> group,
        std::shared_ptr<MediaPreparationMemoryResource> allocation,
        MediaStartupSharedObjectPlan envelopeStorage,
        std::string sourceIdentity, std::uint64_t initialGeneration);
    static MediaNodeKind staticKind() noexcept { return MediaNodeKind::DecodedVideoStartup; }
    static std::string_view generationPurgeIdentity() noexcept { return "decoded_video_startup"; }
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept { return m_purge; }
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;
protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;
private:
    void clear() noexcept;
    std::shared_ptr<MediaPreparationMemoryResource> m_allocation;
    MediaStartupSharedObjectPlan m_envelopeStorage;
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    std::string m_sourceIdentity;
    std::uint64_t m_initialGeneration;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_purge;
    MediaVideoReferenceLineageState m_lineage;
};

} // namespace media::ffmpeg::graph
