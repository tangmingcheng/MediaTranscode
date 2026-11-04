#pragma once

#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"

namespace media::ffmpeg::graph {

// Runtime dependency shared by source-frame processors. The source registrar
// binds it before execution; display identity remains independent of storage.
class MediaSourceReferenceAllocation {
public:
    ::media::Status bindSourceReferenceResource(
        std::shared_ptr<MediaPreparationMemoryResource> allocation);
protected:
    ~MediaSourceReferenceAllocation() = default;
    void freezeSourceReferenceResource() noexcept { m_frozen = true; }
    const std::shared_ptr<MediaPreparationMemoryResource>& sourceReferenceResource() const noexcept
    { return m_resource; }
private:
    std::shared_ptr<MediaPreparationMemoryResource> m_resource;
    bool m_frozen = false;
};

} // namespace media::ffmpeg::graph
