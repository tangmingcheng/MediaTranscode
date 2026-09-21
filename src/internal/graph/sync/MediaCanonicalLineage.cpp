#include "internal/graph/sync/MediaCanonicalLineage.h"

#include <type_traits>

namespace media::ffmpeg::graph {

MediaCanonicalAccessUnitSequence MediaCanonicalLineage::canonicalSequence() const noexcept
{
    return std::visit([](const auto& value) {
        using Identity = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Identity, MediaSourceAccessUnitIdentity>) {
            return MediaCanonicalAccessUnitSequence(value.sourceSequence.value());
        } else {
            return MediaCanonicalAccessUnitSequence(value.outputSequence.value());
        }
    }, identity);
}

bool sameMediaCanonicalTimeline(const MediaCanonicalLineage& left,
                                const MediaCanonicalLineage& right) noexcept
{
    if (left.generation != right.generation || left.identity.index() != right.identity.index()) {
        return false;
    }
    if (const auto* source = std::get_if<MediaSourceAccessUnitIdentity>(&left.identity)) {
        return source->sourceIdentity ==
            std::get<MediaSourceAccessUnitIdentity>(right.identity).sourceIdentity;
    }
    const auto& output = std::get<MediaOutputAccessUnitIdentity>(left.identity);
    const auto& other = std::get<MediaOutputAccessUnitIdentity>(right.identity);
    return output.outputIdentity == other.outputIdentity && output.stream == other.stream;
}

::media::Status validateMediaCanonicalLineage(
    const MediaCanonicalLineage& lineage) noexcept
{
    const bool validIdentity = std::visit([](const auto& identity) {
        using Identity = std::decay_t<decltype(identity)>;
        if constexpr (std::is_same_v<Identity, MediaSourceAccessUnitIdentity>) {
            return !identity.sourceIdentity.empty() && identity.sourceSequence.value() != 0;
        } else {
            return !identity.outputIdentity.empty() && identity.outputSequence.value() != 0 &&
                (identity.stream == MediaScheduledStream::Video ||
                 identity.stream == MediaScheduledStream::Audio);
        }
    }, lineage.identity);
    if (!validIdentity || lineage.generation == 0 ||
        lineage.duration.nanoseconds() < 0) {
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
                              MediaSourceAccessUnitIdentity{std::move(sourceIdentity), sourceSequence},
                              mappingConfidence, generation});
    if (auto valid = validateMediaCanonicalLineage(*lineage); !valid) {
        return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
            valid.error());
    }
    return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::success(
        std::move(lineage));
}

::media::Result<std::shared_ptr<const MediaCanonicalLineage>>
createMediaCanonicalOutputLineage(
    MediaRunningTime presentation,
    std::optional<MediaRunningTime> decode,
    MediaRunningTime duration,
    MediaDecodeOrderMode decodeOrder,
    MediaOutputAccessUnitIdentity identity,
    MediaTimeMappingConfidence mappingConfidence,
    std::uint64_t generation)
{
    auto lineage = std::make_shared<const MediaCanonicalLineage>(
        MediaCanonicalLineage{presentation, decode, duration, decodeOrder,
                              std::move(identity), mappingConfidence, generation});
    if (auto valid = validateMediaCanonicalLineage(*lineage); !valid) {
        return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::failure(
            valid.error());
    }
    return ::media::Result<std::shared_ptr<const MediaCanonicalLineage>>::success(
        std::move(lineage));
}

} // namespace media::ffmpeg::graph
