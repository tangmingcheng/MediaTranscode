#pragma once
#include "internal/graph/model/MediaVideoEncoderCompletionContract.h"
#include "internal/graph/sync/lineage/MediaVideoEncodeLineage.h"
#include "internal/graph/sync/MediaVideoDecodeProgress.h"

struct AVCodecContext;
struct AVPacket;

namespace media::ffmpeg::graph {
// Embedded in the encoder's generation-purge owner. No frame/payload owners.
class MediaVideoEncodedProgressState final {
public:
    ::media::Status bind(const MediaVideoEncoderCompletionContract* prepared, const AVCodecContext& context,
        MediaVideoLineagePropagation lineagePropagation);
    void revoke() noexcept;
    void suspendForReset() noexcept;
    ::media::Status resumeAfterReset();
    ::media::Status validateConfiguration(const AVCodecContext& context) const;
    ::media::Result<MediaVideoEncodeLineage> prepare(
        std::shared_ptr<const MediaCanonicalLineage> canonical,
        std::optional<MediaVideoPresentationProgress> progress, std::int64_t encoderInputPts);
    ::media::Result<std::optional<MediaVideoDecodeProgress>> complete(
        const MediaVideoEncodeLineage& lineage, const AVPacket& packet);
private:
    // Borrowed from the bound codec metadata, whose owner outlives this state.
    const MediaVideoEncoderCompletionContract* m_prepared = nullptr;
    const AVCodecContext* m_context = nullptr;
    std::optional<std::uint64_t> m_generation;
    std::optional<MediaCanonicalAccessUnitIdentity> m_timeline;
    std::optional<MediaRunningTime> m_inputLowerBound;
    std::uint64_t m_submitted = 0;
    std::uint64_t m_completed = 0;
    bool m_resetPending = false;
    bool m_firstCompletionObserved = false;
};
}
