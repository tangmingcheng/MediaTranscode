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
    ::media::Status permitActivatedGeneration(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    ::media::Status validateCommitGeneration(
        std::uint64_t generation) const;
    std::optional<std::uint64_t> activationTransitionSequence(
        std::uint64_t generation) const noexcept;
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;
    void resetLifecycle() noexcept;

private:
    mutable std::mutex m_mutex;
    std::string m_plannedIdentity;
    std::optional<std::uint64_t> m_permittedGeneration;
    std::optional<std::uint64_t> m_pendingGeneration;
    std::optional<std::uint64_t> m_pendingTransitionSequence;
    std::optional<std::uint64_t> m_lastTransitionSequence;
};

} // namespace media::ffmpeg::graph
