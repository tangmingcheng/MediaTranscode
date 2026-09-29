#pragma once

#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

namespace media::ffmpeg::graph {

// Owned and replaced on a single worker. The generation authority controls
// activation/publication; the owner-thread purge mailbox controls retirement.
template <typename Data>
class MediaProtocolOutputGenerationData final : public MediaProtocolOutputGenerationSessionState {
public:
    std::unique_ptr<Data> data = std::make_unique<Data>();

private:
    ::media::Status prepareForGenerationPurge() override
    {
        try { m_prepared = std::make_unique<Data>(); }
        catch (const std::bad_alloc&) {
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
                "Protocol output generation data"));
        }
        return ::media::Status::success();
    }
    void resetForGenerationPurge() noexcept override { data = std::move(m_prepared); }
    std::unique_ptr<Data> m_prepared;
};

} // namespace media::ffmpeg::graph
