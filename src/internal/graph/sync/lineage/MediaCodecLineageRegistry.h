#pragma once

#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"

#include <cstddef>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaCodecLineageRegistry final : public MediaAvGenerationPurgeTarget {
public:
    static ::media::Result<MediaCodecLineageRegistry> create(
        std::size_t capacity);

    MediaCodecLineageRegistry(MediaCodecLineageRegistry&&) noexcept;
    MediaCodecLineageRegistry& operator=(MediaCodecLineageRegistry&&) noexcept;
    ~MediaCodecLineageRegistry() override;
    MediaCodecLineageRegistry(const MediaCodecLineageRegistry&) = delete;
    MediaCodecLineageRegistry& operator=(const MediaCodecLineageRegistry&) = delete;

    ::media::Result<MediaFfmpegLineageToken> submit(
        std::shared_ptr<const MediaCanonicalLineage> lineage);
    ::media::Result<std::shared_ptr<const MediaCanonicalLineage>> resolve(
        const MediaFfmpegLineageToken& token, std::uint64_t generation);
    ::media::Result<std::shared_ptr<const MediaCanonicalLineage>> resolve(
        const AVBufferRef* opaque, std::uint64_t generation);
    ::media::Result<std::optional<std::shared_ptr<const MediaCanonicalLineage>>>
    resolveOutput(const AVBufferRef* opaque);
    [[nodiscard]] bool retainedOutputIsCurrent(
        const MediaCanonicalLineage& lineage) const noexcept;
    ::media::Status finishGeneration(std::uint64_t generation);
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;

private:
    struct State;
    explicit MediaCodecLineageRegistry(std::unique_ptr<State> state);
    std::shared_ptr<State> m_state;

    friend class MediaFfmpegLineageLeaseControl;
};

} // namespace media::ffmpeg::graph
