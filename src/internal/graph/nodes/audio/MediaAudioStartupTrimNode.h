#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/lineage/MediaAudioLineageState.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAudioStartupTrimLineageState final : public MediaAudioLineageState {
public:
    MediaAudioStartupTrimLineageState(MediaAudioLineageExecutionMode mode,
                                      std::size_t capacity) noexcept;
    void resetForLifecycle() noexcept;

    std::optional<MediaAudioPlaybackOrigin> origin;
    bool releaseTrimConsumed = false;
    std::uint32_t remainingTrimSamples = 0;
    bool waitingForFirstPostTrimSample = false;
    std::optional<std::int64_t> expectedNextSample;

protected:
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

private:
    void clearOwnedState() noexcept;
};

class MediaAudioStartupTrimNode final : public FFmpegNodeRuntime {
public:
    using LineageState = MediaAudioStartupTrimLineageState;

    MediaAudioStartupTrimNode(MediaNodeId nodeId,
                              std::shared_ptr<MediaAudioStartupTrimLineageState> lineageState);
    MediaAudioStartupTrimNode(MediaNodeId nodeId, MediaAudioPlaybackOrigin origin,
                              std::shared_ptr<MediaAudioStartupTrimLineageState> lineageState);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

    ::media::Result<MediaBufferRef> apply(
        const MediaBufferRef& frame,
        std::uint32_t trimLeadingSamples);
    ::media::Result<MediaBufferRef> applyDecoded(
        const MediaBufferRef& decodedTrimInput);

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept override;

private:
    ::media::Result<MediaBufferRef> validateAndPass(
        const MediaBufferRef& frame,
        bool verifySourceStart);

    std::shared_ptr<MediaAudioStartupTrimLineageState> m_lineageState;
};

} // namespace media::ffmpeg::graph
