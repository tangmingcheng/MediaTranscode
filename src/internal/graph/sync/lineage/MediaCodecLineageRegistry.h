#pragma once

#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"
#include "internal/graph/sync/lineage/MediaCodecLineage.h"
#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

#include "internal/graph/diagnostics/MediaCodecTokenObservation.h"

#include <cstddef>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

// Runtime allocation requests, excluding referenced lineage and FFmpeg-owned headers.
struct MediaCodecLineageRuntimeStorage final {
    std::size_t capacity;
    std::uint64_t registryBytes;
    std::uint64_t stateBytes;
    std::uint64_t treeBytes;
    std::uint64_t opaqueBytes;
    std::uint64_t totalBytes;
};

// Embedded registry: preparation block costs, without an outer registry owner.
struct MediaCodecLineagePreparedStorage final {
    std::size_t capacity;
    std::uint64_t stateBytes;
    std::uint64_t treeBytes;
    std::uint64_t opaqueBytes;
    std::uint64_t totalBytes;
};

class MediaCodecLineageRegistry final : public MediaAvGenerationPurgeTarget {
public:
    static ::media::Result<MediaCodecLineageRuntimeStorage> runtimeStorage(std::size_t capacity);
    static ::media::Result<MediaCodecLineagePreparedStorage> preparedStorage(std::size_t capacity);
    static ::media::Result<MediaCodecLineageRegistry> create(
        std::size_t capacity, MediaLineageAllocationScope allocation);

    MediaCodecLineageRegistry(MediaCodecLineageRegistry&&) noexcept;
    MediaCodecLineageRegistry& operator=(MediaCodecLineageRegistry&&) noexcept;
    ~MediaCodecLineageRegistry() override;
    MediaCodecLineageRegistry(const MediaCodecLineageRegistry&) = delete;
    MediaCodecLineageRegistry& operator=(const MediaCodecLineageRegistry&) = delete;

    ::media::Result<MediaFfmpegLineageToken> submit(MediaCodecLineage lineage);

    void observeAcceptedSend(const AVBufferRef* opaque, std::int64_t pts,
        int timeBaseNumerator, int timeBaseDenominator) noexcept;

    template<MediaCodecLineageProduct T>
    ::media::Result<MediaCodecLineageValue<T>> resolve(
        const MediaFfmpegLineageToken& token, std::uint64_t generation)
    {
        auto entry = resolveEntry(token, generation);
        return entry ? selectMediaCodecLineage<T>(entry.value())
            : ::media::Result<MediaCodecLineageValue<T>>::failure(entry.error());
    }

    template<MediaCodecLineageProduct T>
    ::media::Result<MediaCodecLineageValue<T>> resolve(
        const AVBufferRef* opaque, std::uint64_t generation)
    {
        auto token = mediaFfmpegLineageToken(opaque);
        return token ? resolve<T>(token.value(), generation)
            : ::media::Result<MediaCodecLineageValue<T>>::failure(token.error());
    }

    template<MediaCodecLineageProduct T>
    ::media::Result<std::optional<MediaCodecLineageValue<T>>>
    resolveOutput(const AVBufferRef* opaque, MediaCodecTokenObservation* observation)
    {
        using Result = ::media::Result<std::optional<MediaCodecLineageValue<T>>>;
        auto entry = resolveOutputEntry(opaque, observation);
        if (!entry) return Result::failure(entry.error());
        if (!entry.value()) return Result::success(std::nullopt);
        auto selected = selectMediaCodecLineage<T>(*entry.value());
        return selected ? Result::success(std::move(selected).value())
            : Result::failure(selected.error());
    }
    [[nodiscard]] bool retainedOutputIsCurrent(
        const MediaCanonicalLineage& lineage) const noexcept;
    ::media::Status finishGeneration(std::uint64_t generation);
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;

private:
    using State = MediaCodecLineageRegistryState;
    ::media::Result<MediaCodecLineage> resolveEntry(
        const MediaFfmpegLineageToken& token, std::uint64_t generation);
    ::media::Result<std::optional<MediaCodecLineage>> resolveOutputEntry(
        const AVBufferRef* opaque, MediaCodecTokenObservation* observation);
    explicit MediaCodecLineageRegistry(std::shared_ptr<State> state);
    std::shared_ptr<State> m_state;

};

} // namespace media::ffmpeg::graph
