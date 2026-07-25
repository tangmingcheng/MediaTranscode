#include "internal/graph/sync/lineage/MediaVideoLineageDerivation.h"

extern "C" {
#include <libavutil/mathematics.h>
}

namespace media::ffmpeg::graph {

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
deriveMediaVideoLineage(
    const MediaCanonicalLineage& source,
    std::int64_t sourcePts,
    std::int64_t outputPts,
    std::int64_t outputDuration,
    MediaRational timeBase)
{
    if (auto status = validateMediaCanonicalLineage(source); !status) {
        return ::media::Result<
            std::shared_ptr<const MediaCanonicalLineage>>::failure(
                status.error());
    }
    if (!timeBase.isKnown() || outputDuration <= 0) {
        return ::media::Result<
            std::shared_ptr<const MediaCanonicalLineage>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video lineage derivation requires time base and duration"));
    }
    const AVRational sourceTimeBase{timeBase.num, timeBase.den};
    const std::int64_t offsetNs = av_rescale_q(
        outputPts - sourcePts, sourceTimeBase,
        AVRational{1, 1'000'000'000});
    auto presentation = source.presentation.checkedAdd(
        MediaRunningTime::fromNanoseconds(offsetNs));
    if (!presentation) {
        return ::media::Result<
            std::shared_ptr<const MediaCanonicalLineage>>::failure(
                presentation.error());
    }
    const std::int64_t durationNs = av_rescale_q(
        outputDuration, sourceTimeBase, AVRational{1, 1'000'000'000});
    return ::media::Result<
        std::shared_ptr<const MediaCanonicalLineage>>::success(
            std::make_shared<const MediaCanonicalLineage>(
                MediaCanonicalLineage{
                    presentation.value(), presentation.value(),
                    MediaRunningTime::fromNanoseconds(durationNs),
                    MediaDecodeOrderMode::PresentationOrderNoReorder,
                    source.sourceIdentity, source.sourceSequence,
                    source.mappingConfidence, source.generation}));
}

} // namespace media::ffmpeg::graph
