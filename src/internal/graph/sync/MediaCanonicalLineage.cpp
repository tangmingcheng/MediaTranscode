#include "internal/graph/sync/MediaCanonicalLineage.h"

namespace media::ffmpeg::graph {

::media::Status validateMediaCanonicalLineage(
    const MediaCanonicalLineage& lineage) noexcept
{
    if (lineage.sourceIdentity.empty() || lineage.sourceSequence.value() == 0 ||
        lineage.generation == 0 || lineage.duration.nanoseconds() < 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Canonical lineage contract is incomplete"));
    }
    if (lineage.decodeOrder == MediaDecodeOrderMode::ReorderedRequiresDecodeTime &&
        !lineage.decode) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Reordered canonical lineage requires decode time"));
    }
    return ::media::Status::success();
}

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
createMediaCanonicalLineage(const MediaMappedTimestamp& mapped,
                            MediaDecodeOrderMode decodeOrder,
                            MediaSourceAccessUnitSequence sourceSequence)
{
    if (!mapped.duration()) {
        return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical lineage requires mapped protocol duration"));
    }
    return createMediaCanonicalLineage(
        mapped.presentationTime(), mapped.decodeTime(), *mapped.duration(),
        decodeOrder, mapped.sourceIdentity(), sourceSequence,
        mapped.confidence(), mapped.generation());
}

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
createMediaCanonicalLineage(
    MediaRunningTime presentation,
    std::optional<MediaRunningTime> decode,
    MediaRunningTime duration,
    MediaDecodeOrderMode decodeOrder,
    std::string sourceIdentity,
    MediaSourceAccessUnitSequence sourceSequence,
    MediaTimeMappingConfidence mappingConfidence,
    std::uint64_t generation)
{
    auto lineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{presentation, decode, duration, decodeOrder,
                              std::move(sourceIdentity), sourceSequence,
                              mappingConfidence, generation});
    if (auto valid = validateMediaCanonicalLineage(*lineage); !valid) {
        return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
            valid.error());
    }
    return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::success(
        std::move(lineage));
}

} // namespace media::ffmpeg::graph
