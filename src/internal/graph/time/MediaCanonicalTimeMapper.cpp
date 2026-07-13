#include "internal/graph/time/MediaCanonicalTimeMapper.h"

#include <array>
#include <limits>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {

namespace {

struct SignedMagnitude final {
    bool negative;
    std::uint64_t magnitude;
};

struct Magnitude65 final {
    std::uint64_t low;
    unsigned high;
};

SignedMagnitude signedMagnitude(std::int64_t value, bool negate) noexcept
{
    const bool negative = value < 0;
    const auto magnitude = negative
        ? static_cast<std::uint64_t>(-(value + 1)) + 1
        : static_cast<std::uint64_t>(value);
    return SignedMagnitude{
        magnitude == 0 ? false : negative != negate,
        magnitude};
}

std::optional<std::int64_t> checkedAffineNanoseconds(std::int64_t sourceTime,
                                                     std::int64_t sourceEpoch,
                                                     std::int64_t runningTimeEpoch) noexcept
{
    const std::array<SignedMagnitude, 3> terms{
        signedMagnitude(sourceTime, false),
        signedMagnitude(sourceEpoch, true),
        signedMagnitude(runningTimeEpoch, false)};
    Magnitude65 positiveMagnitude{0, 0};
    Magnitude65 negativeMagnitude{0, 0};
    for (const auto term : terms) {
        auto& total = term.negative ? negativeMagnitude : positiveMagnitude;
        if (term.magnitude > std::numeric_limits<std::uint64_t>::max() - total.low) {
            ++total.high;
        }
        total.low += term.magnitude;
    }

    const auto compareMagnitude = [](Magnitude65 lhs, Magnitude65 rhs) noexcept {
        if (lhs.high != rhs.high) return lhs.high < rhs.high ? -1 : 1;
        if (lhs.low == rhs.low) return 0;
        return lhs.low < rhs.low ? -1 : 1;
    };
    const auto subtractMagnitude = [](Magnitude65 larger,
                                      Magnitude65 smaller) noexcept
        -> std::optional<std::uint64_t> {
        if (larger.high == smaller.high) return larger.low - smaller.low;
        if (larger.high == smaller.high + 1 && larger.low < smaller.low) {
            return larger.low - smaller.low;
        }
        return std::nullopt;
    };

    const int comparison = compareMagnitude(positiveMagnitude, negativeMagnitude);
    if (comparison >= 0) {
        const auto magnitude = subtractMagnitude(positiveMagnitude, negativeMagnitude);
        if (!magnitude) return std::nullopt;
        if (*magnitude > static_cast<std::uint64_t>(
                            std::numeric_limits<std::int64_t>::max())) {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(*magnitude);
    }

    const auto magnitude = subtractMagnitude(negativeMagnitude, positiveMagnitude);
    if (!magnitude) return std::nullopt;
    constexpr auto minimumMagnitude = std::uint64_t{1} << 63;
    if (*magnitude > minimumMagnitude) {
        return std::nullopt;
    }
    return *magnitude == minimumMagnitude
        ? std::numeric_limits<std::int64_t>::min()
        : -static_cast<std::int64_t>(*magnitude);
}

MediaAvSyncError syncError(const MediaCanonicalTimeMapperConfig& context,
                           MediaAvSyncErrorCode code,
                           MediaAvSyncErrorState state,
                           std::string operation,
                           std::string observedStreamIdentity,
                           std::optional<std::uint64_t> expectedGeneration,
                           std::optional<std::uint64_t> observedGeneration,
                           std::optional<MediaRunningTime> observedSourceTime,
                           std::string detail)
{
    return MediaAvSyncError(
        code,
        context.topology,
        state,
        std::move(operation),
        context.sourceIdentity,
        std::move(observedStreamIdentity),
        expectedGeneration,
        observedGeneration,
        observedSourceTime,
        context.sourceEpoch,
        context.runningTimeEpoch,
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),
        std::move(detail));
}

} // namespace

::media::Result<MediaCanonicalTimeMapper, MediaAvSyncError>
MediaCanonicalTimeMapper::create(MediaCanonicalTimeMapperConfig config)
{
    if (config.sourceIdentity.empty()) {
        return ::media::Result<MediaCanonicalTimeMapper, MediaAvSyncError>::failure(
            syncError(config,
                      MediaAvSyncErrorCode::EmptySourceIdentity,
                      MediaAvSyncErrorState::Configuring,
                      "canonical_map.create",
                      config.sourceIdentity,
                      config.generation,
                      config.generation,
                      std::nullopt,
                      "planner must bind a non-empty source identity"));
    }
    return ::media::Result<MediaCanonicalTimeMapper, MediaAvSyncError>::success(
        MediaCanonicalTimeMapper(std::move(config)));
}

MediaCanonicalTimeMapper::MediaCanonicalTimeMapper(
    MediaCanonicalTimeMapperConfig config) noexcept
    : m_config(std::move(config))
{
}

::media::Result<MediaMappedTimestamp, MediaAvSyncError>
MediaCanonicalTimeMapper::map(const MediaCanonicalSourceTimestamp& source) const
{
    if (source.sourceIdentity() != m_config.sourceIdentity) {
        return ::media::Result<MediaMappedTimestamp, MediaAvSyncError>::failure(
            syncError(m_config,
                      MediaAvSyncErrorCode::SourceIdentityMismatch,
                      MediaAvSyncErrorState::Mapping,
                      "canonical_map",
                      source.sourceIdentity(),
                      m_config.generation,
                      source.generation(),
                      source.presentationTime(),
                      "source identity does not match mapper binding"));
    }
    if (source.generation() != m_config.generation) {
        return ::media::Result<MediaMappedTimestamp, MediaAvSyncError>::failure(
            syncError(m_config,
                      MediaAvSyncErrorCode::GenerationMismatch,
                      MediaAvSyncErrorState::Mapping,
                      "canonical_map",
                      source.sourceIdentity(),
                      m_config.generation,
                      source.generation(),
                      source.presentationTime(),
                      "source generation does not match mapper generation"));
    }
    if (!source.presentationTime()) {
        return ::media::Result<MediaMappedTimestamp, MediaAvSyncError>::failure(
            syncError(m_config,
                      MediaAvSyncErrorCode::MissingSourceEvidence,
                      MediaAvSyncErrorState::Mapping,
                      "canonical_map.presentation",
                      source.sourceIdentity(),
                      m_config.generation,
                      source.generation(),
                      std::nullopt,
                      "presentation source time is required"));
    }
    if (source.duration() && source.duration()->nanoseconds() < 0) {
        return ::media::Result<MediaMappedTimestamp, MediaAvSyncError>::failure(
            syncError(m_config,
                      MediaAvSyncErrorCode::InvalidDuration,
                      MediaAvSyncErrorState::Mapping,
                      "canonical_map.duration",
                      source.sourceIdentity(),
                      m_config.generation,
                      source.generation(),
                      source.duration(),
                      "duration must not be negative"));
    }

    auto presentation = mapOne(*source.presentationTime(), "presentation");
    if (!presentation) {
        return ::media::Result<MediaMappedTimestamp, MediaAvSyncError>::failure(
            presentation.error());
    }

    std::optional<MediaRunningTime> decode;
    if (source.decodeTime()) {
        auto mappedDecode = mapOne(*source.decodeTime(), "decode");
        if (!mappedDecode) {
            return ::media::Result<MediaMappedTimestamp, MediaAvSyncError>::failure(
                mappedDecode.error());
        }
        decode = mappedDecode.value();
    }

    return ::media::Result<MediaMappedTimestamp, MediaAvSyncError>::success(
        MediaMappedTimestamp(
            presentation.value(),
            decode,
            source.duration(),
            source.generation(),
            source.sourceIdentity(),
            source.confidence()));
}

::media::Result<void, MediaAvSyncError> MediaCanonicalTimeMapper::reset(
    MediaCanonicalTimeMapperConfig config)
{
    if (config.sourceIdentity.empty()) {
        return ::media::Result<void, MediaAvSyncError>::failure(syncError(
            m_config,
            MediaAvSyncErrorCode::EmptySourceIdentity,
            MediaAvSyncErrorState::Resetting,
            "canonical_map.reset",
            config.sourceIdentity,
            m_config.generation,
            config.generation,
            config.sourceEpoch,
            "planner must bind a non-empty source identity"));
    }
    if (config.generation <= m_config.generation) {
        const auto expectedGeneration = m_config.generation ==
                std::numeric_limits<std::uint64_t>::max()
            ? std::optional<std::uint64_t>{}
            : std::optional<std::uint64_t>{m_config.generation + 1};
        return ::media::Result<void, MediaAvSyncError>::failure(syncError(
            m_config,
            MediaAvSyncErrorCode::InvalidGenerationTransition,
            MediaAvSyncErrorState::Resetting,
            "canonical_map.reset",
            config.sourceIdentity,
            expectedGeneration,
            config.generation,
            config.sourceEpoch,
            "new generation must be greater than the active generation"));
    }
    m_config = std::move(config);
    return ::media::Result<void, MediaAvSyncError>::success();
}

::media::Result<MediaRunningTime, MediaAvSyncError>
MediaCanonicalTimeMapper::mapOne(MediaRunningTime sourceTime,
                                 const char* field) const
{
    const auto mapped = checkedAffineNanoseconds(
        sourceTime.nanoseconds(),
        m_config.sourceEpoch.nanoseconds(),
        m_config.runningTimeEpoch.nanoseconds());
    if (!mapped) {
        return ::media::Result<MediaRunningTime, MediaAvSyncError>::failure(
            syncError(m_config,
                      MediaAvSyncErrorCode::TimeOverflow,
                      MediaAvSyncErrorState::Mapping,
                      std::string("canonical_map.") + field,
                      m_config.sourceIdentity,
                      m_config.generation,
                      m_config.generation,
                      sourceTime,
                      "affine source-to-running result is outside signed nanoseconds"));
    }
    return ::media::Result<MediaRunningTime, MediaAvSyncError>::success(
        MediaRunningTime::fromNanoseconds(*mapped));
}

} // namespace media::ffmpeg::graph
