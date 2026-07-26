#pragma once

#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaProtocolOutputGenerationState final
    : public MediaAvGenerationPurgeTarget {
public:
    explicit MediaProtocolOutputGenerationState(std::string plannedIdentity);

    std::string_view plannedIdentity() const noexcept;
    ::media::Status observe(std::uint64_t generation);
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;
    void resetLifecycle() noexcept;

private:
    mutable std::mutex m_mutex;
    std::string m_plannedIdentity;
    std::optional<std::uint64_t> m_generation;
    std::optional<std::uint64_t> m_lastTransitionSequence;
};

} // namespace media::ffmpeg::graph
