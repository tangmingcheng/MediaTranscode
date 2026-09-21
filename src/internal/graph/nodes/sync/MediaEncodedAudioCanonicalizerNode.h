#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaEncodedAudioCanonicalizerState;
class MediaAvGenerationPurgeTarget;

class MediaEncodedAudioCanonicalizerNode final : public FFmpegNodeRuntime {
public:
    MediaEncodedAudioCanonicalizerNode(MediaNodeId nodeId, MediaAvSyncGroupKey outputGroup);
    static MediaNodeKind staticKind() noexcept;
    static std::string_view generationPurgeIdentity() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept;
    static ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>
    canonicalize(const MediaBufferRef& encoded,
                 MediaOutputAccessUnitSequence sequence,
                 const MediaAvSyncGroupKey& outputGroup);

    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    bool pendingOutputIsCurrent(
        const MediaBufferRef& buffer) const noexcept override;
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    void resetState() noexcept;
    std::shared_ptr<MediaEncodedAudioCanonicalizerState> m_state;
    MediaAvSyncGroupKey m_outputGroup;
};

} // namespace media::ffmpeg::graph
