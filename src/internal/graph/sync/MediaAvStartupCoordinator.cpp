#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include "internal/graph/sync/MediaAudioSampleGrid.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/sync/startup/MediaAvStartupReadinessIndex.h"
#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include "internal/graph/sync/startup/MediaAvStartupWindowSelector.h"

#include <algorithm>
#include <limits>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr MediaRunningTime Zero = MediaRunningTime::fromNanoseconds(0);

std::size_t startupResultCapacity(std::initializer_list<std::size_t> counts)
{
    std::size_t total = 0;
    for (const auto count : counts) {
        auto added = MediaCheckedArithmetic::add(total, count, "Startup result capacity");
        if (!added) throw std::length_error(added.error().message);
        if (added.value() > (std::numeric_limits<std::size_t>::max)())
            throw std::length_error("Startup result capacity exceeds addressable size");
        total = static_cast<std::size_t>(added.value());
    }
    return total;
}

std::uint64_t nonNegativeSampleDistance(
    std::int64_t first,
    std::int64_t last) noexcept
{
    if (first >= 0) {
        return static_cast<std::uint64_t>(last) -
               static_cast<std::uint64_t>(first);
    }
    const auto firstMagnitude =
        static_cast<std::uint64_t>(-(first + 1)) + 1;
    if (last < 0) {
        const auto lastMagnitude =
            static_cast<std::uint64_t>(-(last + 1)) + 1;
        return firstMagnitude - lastMagnitude;
    }
    return firstMagnitude + static_cast<std::uint64_t>(last);
}

} // namespace

::media::Result<std::uint32_t> calculateMediaAvAudioTrimSamples(
    MediaRunningTime epochSourceStart,
    const MediaAvAudioSampleSpan& span)
{
    auto grid = MediaAudioSampleGrid::create(
        static_cast<int>(span.sampleRate));
    if (!grid) {
        return ::media::Result<std::uint32_t>::failure(
            grid.error());
    }
    auto epochSample = grid.value().firstSampleAtOrAfter(epochSourceStart);
    if (!epochSample) {
        return ::media::Result<std::uint32_t>::failure(
            epochSample.error());
    }
    if (epochSample.value() < span.firstSample) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup epoch precedes the authoritative audio sample span"));
    }
    const std::uint64_t delta =
        nonNegativeSampleDistance(span.firstSample, epochSample.value());
    if (delta > span.sampleCount) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup epoch exceeds the authoritative audio sample span"));
    }
    return ::media::Result<std::uint32_t>::success(
        static_cast<std::uint32_t>(delta));
}

namespace {

MediaAvStartupUnitId unitId(const MediaAvStartupAccessUnit& unit) noexcept
{
    return {unit.stream, unit.generation, unit.sequence};
}

} // namespace

::media::Status validateMediaAvAudioSampleSpanDuration(
    const MediaAvAudioSampleSpan& span,
    MediaRunningTime duration)
{
    constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000ULL;
    if (span.sampleRate == 0 || span.sampleCount == 0 || duration <= Zero) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("audio sample span must be positive"));
    }
    const std::uint64_t numerator =
        static_cast<std::uint64_t>(span.sampleCount) * NanosecondsPerSecond;
    const std::uint64_t floorNanoseconds = numerator / span.sampleRate;
    const std::uint64_t ceilNanoseconds = floorNanoseconds +
        (numerator % span.sampleRate == 0 ? 0 : 1);
    const auto actual = duration.nanoseconds();
    if (actual < 0 || static_cast<std::uint64_t>(actual) < floorNanoseconds ||
        static_cast<std::uint64_t>(actual) > ceilNanoseconds) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "audio duration does not match its sample span within one nanosecond"));
    }
    return ::media::Status::success();
}

std::size_t MediaAvStartupUnitIdHash::operator()(
    const MediaAvStartupUnitId& id) const noexcept
{
    const auto stream = static_cast<std::size_t>(id.stream);
    const auto generation = std::hash<std::uint64_t>{}(id.generation);
    const auto sequence = std::hash<std::uint64_t>{}(id.sequence);
    return stream ^ (generation + 0x9e3779b9u + (stream << 6) + (stream >> 2)) ^
           (sequence + 0x9e3779b9u + (generation << 6) + (generation >> 2));
}

MediaAvSyncResult<MediaAvStartupCoordinator> MediaAvStartupCoordinator::create(
    MediaAvStartupConfig config, MediaLineageAllocationScope allocation,
    MediaAvStartupIndexStoragePlan videoIndex, std::optional<MediaAvStartupIndexStoragePlan> audioIndex)
{
    const auto* audio = config.audio ? &*config.audio : nullptr;
    const auto* prepared = std::get_if<MediaPreparedLineageAllocation>(&allocation);
    auto indexStatus = MediaAvStartupIndexStoragePlanner::validate(
        videoIndex, config.videoCapacity, config.videoReadiness, allocation);
    if (indexStatus && audio && audioIndex)
        indexStatus = MediaAvStartupIndexStoragePlanner::validate(*audioIndex, audio->capacity,
            MediaVideoStartupReadiness::IntervalCoverage, allocation);
    if (!indexStatus || config.audio.has_value() != audioIndex.has_value() ||
        (config.members != MediaTranscodeStreamSet::VideoOnly &&
         config.members != MediaTranscodeStreamSet::AudioVideo) ||
        (config.members == MediaTranscodeStreamSet::AudioVideo) != config.audio.has_value() ||
        !encodeMediaVideoStartupReadiness(config.videoReadiness) ||
        (config.videoReadiness == MediaVideoStartupReadiness::DecodedPresentationSpan) != (prepared != nullptr) ||
        (prepared && !prepared->resource) ||
        std::optional<bool>{config.requireVideoKeyFrame} !=
            mediaVideoStartupKeyFrameRequirement(config.videoReadiness) || config.allowDegradedClock ||
        config.maximumWait <= Zero || config.preroll <= Zero ||
        config.keyFrameWait <= Zero || config.maximumGap <= Zero ||
        config.outputLead <= Zero || config.maximumGap >= config.preroll ||
        config.preroll >= config.keyFrameWait || config.keyFrameWait > config.maximumWait ||
        config.videoCapacity == 0 ||
        config.videoCapacity > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
        config.videoByteCapacity == 0 || config.maximumVideoUnitBytes == 0 ||
        config.maximumVideoUnitBytes > config.videoByteCapacity ||
        config.videoByteCapacity > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        config.videoIdentity.empty() ||
        (audio && (audio->maximumTrim <= Zero || audio->maximumInitialSkew <= Zero ||
                   audio->maximumTrim > config.preroll ||
                   audio->maximumInitialSkew >= config.outputLead ||
                   audio->capacity == 0 ||
                   audio->capacity > static_cast<std::size_t>((std::numeric_limits<int>::max)()) ||
                   audio->byteCapacity == 0 || audio->maximumUnitBytes == 0 ||
                   audio->maximumUnitBytes > audio->byteCapacity ||
                   audio->byteCapacity > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
                   audio->identity.empty() || audio->identity == config.videoIdentity ||
                   audio->outputSampleRate <= 0))) {
        const auto zero = MediaRunningTime::fromNanoseconds(0);
        return MediaAvSyncResult<MediaAvStartupCoordinator>::failure(MediaAvSyncError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            config.sourceClockMode,
            MediaAvSyncErrorState::Startup,
            "create_startup_coordinator",
            config.videoIdentity,
            audio ? audio->identity : std::string{},
            std::nullopt,
            std::nullopt,
            std::nullopt,
            zero,
            zero,
            0,
            config.maximumWait.nanoseconds(),
            !indexStatus ? indexStatus.error().message :
                "MediaAvStartupCoordinator requires a complete ordered startup policy"));
    }
    // Prepare scratch and index heads before accepting any media. Capture error
    // context before the configuration is moved into the constructed owner.
    auto storageFailure = MediaAvSyncError(MediaAvSyncErrorCode::StartupCapacityExceeded,
        config.sourceClockMode, MediaAvSyncErrorState::Startup,
        "create_startup_coordinator", config.videoIdentity,
        audio ? audio->identity : std::string{}, std::nullopt, std::nullopt, std::nullopt,
        Zero, Zero, 0, config.maximumWait.nanoseconds(),
        "Startup coordinator storage allocation exceeds available storage");
    try {
        return MediaAvSyncResult<MediaAvStartupCoordinator>::success(
            MediaAvStartupCoordinator(std::move(config), std::move(allocation),
                std::move(videoIndex), std::move(audioIndex)));
    } catch (const std::bad_alloc&) {
        return MediaAvSyncResult<MediaAvStartupCoordinator>::failure(std::move(storageFailure));
    } catch (const std::length_error&) {
        return MediaAvSyncResult<MediaAvStartupCoordinator>::failure(std::move(storageFailure));
    }
}

MediaAvStartupCoordinator::MediaAvStartupCoordinator(MediaAvStartupConfig config, MediaLineageAllocationScope allocation,
    MediaAvStartupIndexStoragePlan videoIndex, std::optional<MediaAvStartupIndexStoragePlan> audioIndex)
    : m_allocation(allocation)
    , m_config(std::move(config))
    , m_state(m_config.sourceClockMode)
    , m_video(makeMediaLineageUnique<MediaAvStartupStreamStore>(allocation, m_config.maximumGap, std::move(videoIndex), allocation))
    , m_audio(m_config.audio
          ? makeMediaLineageUnique<MediaAvStartupStreamStore>(allocation, m_config.maximumGap, std::move(*audioIndex), allocation)
          : MediaLineageUniquePtr<MediaAvStartupStreamStore>(nullptr,
                MediaLineageObjectDeleter<MediaAvStartupStreamStore>{allocation}))
{
}

MediaAvStartupCoordinator::~MediaAvStartupCoordinator() = default;
MediaAvStartupCoordinator::MediaAvStartupCoordinator(
    MediaAvStartupCoordinator&&) noexcept = default;
MediaAvStartupCoordinator& MediaAvStartupCoordinator::operator=(
    MediaAvStartupCoordinator&&) noexcept = default;

MediaAvSyncStatus MediaAvStartupCoordinator::validateUnit(
    const MediaAvStartupAccessUnit& unit) const
{
    if ((unit.stream != MediaAvStartupStream::Video &&
         unit.stream != MediaAvStartupStream::Audio) ||
        (unit.stream == MediaAvStartupStream::Audio && !m_config.audio))
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::SourceIdentityMismatch, "validate_startup_unit", &unit,
            "startup unit is not a planned stream member"));
    const auto maximumUnitBytes = unit.stream == MediaAvStartupStream::Video
        ? m_config.maximumVideoUnitBytes
        : m_config.audio->maximumUnitBytes;
    const auto* duration = std::get_if<MediaRunningTime>(&unit.timing);
    const bool point = unit.stream == MediaAvStartupStream::Video &&
        m_config.videoReadiness == MediaVideoStartupReadiness::DecodedPresentationSpan;
    if (unit.sequence == 0 || unit.payloadBytes == 0 ||
        unit.payloadBytes > maximumUnitBytes ||
        (point ? duration != nullptr : (!duration || *duration <= Zero))) {
        return MediaAvSyncStatus::failure(startupError(
            unit.payloadBytes > maximumUnitBytes
                ? MediaAvSyncErrorCode::StartupCapacityExceeded
                : MediaAvSyncErrorCode::InvalidDuration,
            "validate_startup_unit", &unit,
            "startup unit requires bounded payload and its planned interval or display-point timing"));
    }
    const auto& expectedIdentity = unit.stream == MediaAvStartupStream::Video
        ? m_config.videoIdentity
        : m_config.audio->identity;
    if (unit.identity.view() != expectedIdentity) {
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::SourceIdentityMismatch,
            "validate_startup_unit", &unit, "stream identity mismatch"));
    }
    if (unit.stream == MediaAvStartupStream::Video) {
        if (unit.audio) return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::InvalidDuration, "validate_startup_unit", &unit,
            "video startup unit cannot carry audio samples"));
    } else if (!unit.audio || unit.audio->sampleRate == 0 || unit.audio->sampleCount == 0 ||
               unit.keyFrame) {
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::InvalidDuration, "validate_startup_unit", &unit,
            "audio startup unit requires an explicit non-empty sample span"));
    } else if (
        unit.audio->firstSample >
        std::numeric_limits<std::int64_t>::max() -
            static_cast<std::int64_t>(unit.audio->sampleCount)) {
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::InvalidDuration, "validate_startup_unit", &unit,
            "audio startup sample span end is not representable"));
    } else if (auto status = validateMediaAvAudioSampleSpanDuration(
                   *unit.audio, *duration); !status) {
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::InvalidDuration, "validate_startup_unit", &unit,
            status.error().message));
    }
    const bool usableClock = unit.readiness == MediaSourceClockReadiness::Locked;
    if (usableClock && !unit.presentationTime) {
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::MissingCanonicalTime,
            "validate_startup_unit", &unit,
            "usable startup unit requires canonical presentation time"));
    }
    if (!usableClock && unit.presentationTime) {
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::MissingCanonicalTime, "validate_startup_unit", &unit,
            "unlocked startup unit cannot carry canonical presentation time"));
    }
    return MediaAvSyncStatus::success();
}

MediaAvSyncResult<MediaAvStartupIds>
MediaAvStartupCoordinator::advanceGeneration(
    std::uint64_t generation,
    MediaRunningTime observedAt)
{
    MediaAvSyncStatus status = m_state.generation()
        ? m_state.transition(MediaAvSyncEvent::RequireReacquisition, generation)
        : m_state.transition(MediaAvSyncEvent::BeginAcquisition, generation);
    if (!status) {
        return MediaAvSyncResult<MediaAvStartupIds>::failure(status.error());
    }
    auto purged = purge({}, std::nullopt);
    m_lastVideoSequence.reset();
    m_lastVideoPresentation.reset();
    m_lastAudioSequence.reset();
    m_acquisitionStartedAt = observedAt;
    return MediaAvSyncResult<MediaAvStartupIds>::success(std::move(purged));
}

MediaAvSyncResult<MediaAvStartupDecision> MediaAvStartupCoordinator::submit(
    MediaAvStartupAccessUnit unit,
    MediaRunningTime observedAt)
try {
    if (auto status = validateUnit(unit); !status) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
    }
    if (m_state.state() == MediaAvSyncState::Failed ||
        m_state.state() == MediaAvSyncState::Stopped ||
        m_state.state() == MediaAvSyncState::Aborted) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
            m_state.state() == MediaAvSyncState::Aborted
                ? MediaAvSyncErrorCode::StartupAborted
                : MediaAvSyncErrorCode::StartupInvalidTransition,
            "submit", &unit, "startup coordinator is not accepting media"));
    }
    if (m_state.state() == MediaAvSyncState::WaitingForEvidence) {
        MediaAvStartupIds dropped(1, *MediaLineageAllocator<MediaAvStartupUnitId>{m_allocation}.resource());
        dropped.push_back(unitId(unit));
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::DroppedNotReady, std::nullopt, std::move(dropped)});
    }
    const MediaRunningTime effectiveNow = advanceWatermark(observedAt);
    MediaAvStartupIds purged(0, *MediaLineageAllocator<MediaAvStartupUnitId>{m_allocation}.resource());
    if (!m_state.generation()) {
        auto advanced = advanceGeneration(unit.generation, effectiveNow);
        if (!advanced) return MediaAvSyncResult<MediaAvStartupDecision>::failure(advanced.error());
        purged = std::move(advanced).value();
    } else if (unit.generation < *m_state.generation()) {
        MediaAvStartupIds dropped(1, *MediaLineageAllocator<MediaAvStartupUnitId>{m_allocation}.resource());
        dropped.push_back(unitId(unit));
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::DroppedOldGeneration,
             std::nullopt, std::move(dropped)});
    } else if (unit.generation > *m_state.generation()) {
        auto advanced = advanceGeneration(unit.generation, effectiveNow);
        if (!advanced) return MediaAvSyncResult<MediaAvStartupDecision>::failure(advanced.error());
        purged = std::move(advanced).value();
    }

    if (!m_acquisitionStartedAt && m_state.state() == MediaAvSyncState::AcquiringClock)
        m_acquisitionStartedAt = effectiveNow;
    const bool usableClock = unit.readiness == MediaSourceClockReadiness::Locked;
    if (!usableClock) {
        const bool acquisitionAlreadyActive =
            m_state.state() == MediaAvSyncState::AcquiringClock;
        if (auto status = m_state.transition(
                MediaAvSyncEvent::RequireReacquisition,
                unit.generation); !status) {
            return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
        }
        purged = purge(purged.view(), unitId(unit));
        if (!acquisitionAlreadyActive) m_acquisitionStartedAt = effectiveNow;
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::DroppedNotReady, std::nullopt, std::move(purged)});
    }

    const bool streamEnded = (unit.stream == MediaAvStartupStream::Video
        ? m_videoClosure : m_audioClosure) != MediaAvStartupMemberClosure::Open;
    if (streamEnded) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            "submit", &unit, "media arrived after end of stream"));
    }
    auto& lastSequence = unit.stream == MediaAvStartupStream::Video
        ? m_lastVideoSequence
        : m_lastAudioSequence;
    const bool displayPoint = unit.stream == MediaAvStartupStream::Video &&
        m_config.videoReadiness == MediaVideoStartupReadiness::DecodedPresentationSpan;
    if (displayPoint &&
        ((lastSequence && unit.sequence <= *lastSequence) ||
         (m_lastVideoPresentation && *unit.presentationTime < *m_lastVideoPresentation))) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            "submit", &unit, "decoded display order regressed within the current generation"));
    }
    if (lastSequence && unit.sequence <= *lastSequence) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::DroppedDuplicateOrRegressed,
             std::nullopt, std::move(purged)});
    }
    if (m_state.state() == MediaAvSyncState::Running) {
        lastSequence = unit.sequence;
        if (displayPoint) m_lastVideoPresentation = unit.presentationTime;
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::PassThrough, std::nullopt, std::move(purged)});
    }

    if (unit.stream == MediaAvStartupStream::Video &&
        m_config.requireVideoKeyFrame && !unit.keyFrame &&
        !m_keyFrameWaitStartedAt) {
        m_keyFrameWaitStartedAt = effectiveNow;
    }

    auto& store = unit.stream == MediaAvStartupStream::Video ? *m_video : *m_audio;
    auto& bufferedBytes = unit.stream == MediaAvStartupStream::Video
        ? m_videoBytes
        : m_audioBytes;
    const std::size_t capacity = unit.stream == MediaAvStartupStream::Video
        ? m_config.videoCapacity
        : m_config.audio->capacity;
    const std::uint64_t byteCapacity = unit.stream == MediaAvStartupStream::Video
        ? m_config.videoByteCapacity
        : m_config.audio->byteCapacity;
    if (store.size() >= capacity || unit.payloadBytes > byteCapacity - bufferedBytes) {
        auto failed = markFailed(MediaAvSyncErrorCode::StartupCapacityExceeded,
                                 "startup buffer capacity exceeded");
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(failed.error());
    }
    const auto payloadBytes = unit.payloadBytes;
    const auto sequence = unit.sequence;
    const auto presentation = unit.presentationTime;
    if (unit.stream == MediaAvStartupStream::Video) m_videoLocked = true;
    else m_audioLocked = true;
    auto appended = store.append(std::move(unit));
    if (!appended) {
        auto failed = markFailed(appended.error().code == ::media::ErrorCode::AllocationFailed
                                     ? MediaAvSyncErrorCode::StartupCapacityExceeded
                                     : MediaAvSyncErrorCode::TimeOverflow,
                                 appended.error().message);
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(failed.error());
    }
    if (!appended.value()) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            "submit", nullptr, "duplicate presentation identity"));
    }
    lastSequence = sequence;
    if (displayPoint) m_lastVideoPresentation = presentation;
    bufferedBytes += payloadBytes;
    m_cumulativeSelectionWork.coverageOperations =
        m_video->cumulativeCoverageWork().coverageOperations +
        (m_audio ? m_audio->cumulativeCoverageWork().coverageOperations : 0);
    m_cumulativeSelectionWork.orderedIndexMutations =
        m_video->cumulativeCoverageWork().orderedIndexMutations +
        (m_audio ? m_audio->cumulativeCoverageWork().orderedIndexMutations : 0);

    if (m_videoLocked && (!m_audio || m_audioLocked) &&
        m_state.state() == MediaAvSyncState::AcquiringClock) {
        if (auto status = m_state.transition(MediaAvSyncEvent::ClocksLocked,
                                             *m_state.generation()); !status) {
            return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
        }
    }
    return tryRelease(effectiveNow, std::move(purged));
} catch (const std::bad_alloc&) {
    auto failed = markFailed(MediaAvSyncErrorCode::StartupCapacityExceeded, "Startup result allocation failed");
    return MediaAvSyncResult<MediaAvStartupDecision>::failure(failed.error());
} catch (const std::length_error&) {
    auto failed = markFailed(MediaAvSyncErrorCode::StartupCapacityExceeded, "Startup result capacity is not representable");
    return MediaAvSyncResult<MediaAvStartupDecision>::failure(failed.error());
}

MediaAvSyncResult<MediaAvStartupDecision>
MediaAvStartupCoordinator::tryRelease(MediaRunningTime observedAt, MediaAvStartupIds prior)
{
    if (m_state.state() != MediaAvSyncState::PrimingStreams) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::Buffered, std::nullopt, std::move(prior)});
    }
    if (m_audio && m_audio->empty()) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::Buffered, std::nullopt, std::move(prior)});
    }
    m_lastAttemptSelectionWork = {};
    auto videoCoverage = MediaAvStartupReadinessIndex::build(
        m_video->presentationSnapshot(), m_lastAttemptSelectionWork);
    std::optional<MediaAvStartupReadinessIndex> audioCoverage;
    if (m_audio) audioCoverage = MediaAvStartupReadinessIndex::build(
        m_audio->presentationSnapshot(), m_lastAttemptSelectionWork);
    auto selected = MediaAvStartupWindowSelector::select(
        videoCoverage, audioCoverage ? &*audioCoverage : nullptr, m_config,
        m_lastAttemptSelectionWork);
    m_cumulativeSelectionWork.indexedUnits +=
        m_lastAttemptSelectionWork.indexedUnits;
    m_cumulativeSelectionWork.candidateOperations +=
        m_lastAttemptSelectionWork.candidateOperations;
    if (!selected) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(
            startupError(MediaAvSyncErrorCode::TimeOverflow,
                         "select_common_window", nullptr,
                         selected.error().message));
    }
    if (!selected.value()) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {m_allocation, MediaAvStartupDisposition::Buffered, std::nullopt, std::move(prior)});
    }
    const auto& window = *selected.value();
    std::uint32_t trimLeadingSamples = 0;
    if (m_config.audio && m_config.audio->trimToCommonStart) {
        auto audioTrimTime = window.sourceStart.checkedSubtract(
            *window.audio->presentationTime);
        if (!audioTrimTime || audioTrimTime.value() < Zero ||
            audioTrimTime.value() > m_config.audio->maximumTrim) {
            auto failed = markFailed(
                MediaAvSyncErrorCode::AudioTrimLimitExceeded,
                "audio trim exceeds planned bound");
            return MediaAvSyncResult<MediaAvStartupDecision>::failure(
                failed.error());
        }
        auto trim = calculateMediaAvAudioTrimSamples(
            window.sourceStart, *window.audio->audio);
        if (!trim) {
            auto failed = markFailed(
                MediaAvSyncErrorCode::AudioTrimLimitExceeded,
                trim.error().message);
            return MediaAvSyncResult<MediaAvStartupDecision>::failure(
                failed.error());
        }
        trimLeadingSamples = trim.value();
    } else if (window.audio && *window.audio->presentationTime < window.sourceStart) {
        auto failed = markFailed(
            MediaAvSyncErrorCode::AudioTrimLimitExceeded,
            "complete access-unit startup selected audio before the epoch");
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(
            failed.error());
    }

    auto videoPrefix = m_video->prefixCount(window.video, m_lastAttemptSelectionWork);
    if (!videoPrefix) return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
        MediaAvSyncErrorCode::StartupInvalidTransition, "release_partition", nullptr, videoPrefix.error().message));
    auto audioPrefix = m_audio ? m_audio->prefixCount(window.audio, m_lastAttemptSelectionWork)
        : ::media::Result<std::size_t>::success(0);
    if (!audioPrefix) return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
        MediaAvSyncErrorCode::StartupInvalidTransition, "release_partition", nullptr, audioPrefix.error().message));
    auto* resource = MediaLineageAllocator<MediaAvStartupUnitId>{m_allocation}.resource();
    MediaAvStartupRelease batch{
        m_allocation, MediaPlaybackEpoch{window.sourceStart, Zero, *m_state.generation()},
        MediaAvStartupSelections(m_video->size() - videoPrefix.value(), *resource),
        MediaAvStartupSelections(m_audio ? m_audio->size() - audioPrefix.value() : 0, *resource)};
    MediaAvStartupIds purged(startupResultCapacity({prior.size(), videoPrefix.value(), audioPrefix.value()}), *resource);
    for (const auto& id : prior) purged.push_back(id);
    m_video->appendPrefixIds(window.video, purged, m_lastAttemptSelectionWork);
    if (m_audio) m_audio->appendPrefixIds(window.audio, purged, m_lastAttemptSelectionWork);
    m_video->appendSuffixSelections(window.video, 0, batch.video,
                                    m_lastAttemptSelectionWork);
    if (m_audio) m_audio->appendSuffixSelections(window.audio, trimLeadingSamples, batch.audio,
                                    m_lastAttemptSelectionWork);
    auto releaseTime = observedAt.checkedAdd(m_config.outputLead);
    if (!releaseTime) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(
            startupError(MediaAvSyncErrorCode::TimeOverflow,
                         "compute_master_release", nullptr,
                         releaseTime.error().message));
    }
    batch.epoch.masterRelease = releaseTime.value();
    if (auto status = m_state.transition(MediaAvSyncEvent::StreamsPrimed,
                                         *m_state.generation()); !status) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
    }
    if (auto status = m_state.transition(MediaAvSyncEvent::Release,
                                         *m_state.generation()); !status) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
    }
    if (auto status = m_state.transition(MediaAvSyncEvent::Run,
                                         *m_state.generation()); !status) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
    }
    m_video->clear();
    if (m_audio) m_audio->clear();
    m_videoBytes = 0;
    m_audioBytes = 0;
    m_epoch = batch.epoch;
    return MediaAvSyncResult<MediaAvStartupDecision>::success(
        {m_allocation, MediaAvStartupDisposition::Buffered,
         std::optional<MediaAvStartupRelease>(std::move(batch)),
         std::move(purged)});
}

MediaAvSyncResult<std::optional<MediaAvStartupDeadlines>>
MediaAvStartupCoordinator::deadlines() const
{
    using Result = MediaAvSyncResult<std::optional<MediaAvStartupDeadlines>>;
    if (!m_acquisitionStartedAt) return Result::success(std::nullopt);
    switch (m_state.state()) {
    case MediaAvSyncState::AcquiringClock:
    case MediaAvSyncState::PrimingStreams:
    case MediaAvSyncState::Armed:
    case MediaAvSyncState::Released:
        break;
    default:
        return Result::success(std::nullopt);
    }
    auto acquisition = m_acquisitionStartedAt->checkedAdd(m_config.maximumWait);
    if (!acquisition) return Result::failure(startupError(
        MediaAvSyncErrorCode::TimeOverflow, "acquisition_deadline", nullptr,
        acquisition.error().message));
    std::optional<MediaRunningTime> keyFrame;
    if (m_keyFrameWaitStartedAt &&
        (m_config.requireVideoKeyFrame ? !m_video->hasKeyFrame() : m_video->empty())) {
        auto expiry = m_keyFrameWaitStartedAt->checkedAdd(m_config.keyFrameWait);
        if (!expiry) return Result::failure(startupError(
            MediaAvSyncErrorCode::TimeOverflow, "key_frame_deadline", nullptr,
            expiry.error().message));
        keyFrame = expiry.value();
    }
    return Result::success(MediaAvStartupDeadlines{acquisition.value(), keyFrame});
}

MediaAvSyncResult<MediaAvStartupPollOutcome>
MediaAvStartupCoordinator::poll(MediaRunningTime observedAt)
try {
    using Result = MediaAvSyncResult<MediaAvStartupPollOutcome>;
    auto pending = deadlines();
    if (!pending) return Result::failure(pending.error());
    if (!pending.value()) return Result::success(std::nullopt);
    const MediaRunningTime effectiveNow = advanceWatermark(observedAt);
    std::optional<MediaAvSyncErrorCode> expiry;
    // Preserve acquisition timeout priority when both deadlines have elapsed.
    if (effectiveNow >= pending.value()->acquisition)
        expiry = MediaAvSyncErrorCode::StartupTimeout;
    else if (pending.value()->keyFrame && effectiveNow >= *pending.value()->keyFrame)
        expiry = MediaAvSyncErrorCode::KeyFrameTimeout;
    if (!expiry) return Result::success(std::nullopt);
    auto error = startupError(*expiry, "poll", nullptr,
        *expiry == MediaAvSyncErrorCode::StartupTimeout
            ? "startup timeout" : m_config.requireVideoKeyFrame
                ? "video key frame wait timeout" : "decoded video frame wait timeout");
    auto waiting = m_state.transition(MediaAvSyncEvent::AttemptExpired, *m_state.generation());
    if (!waiting) return Result::failure(waiting.error());
    auto purged = purge({}, std::nullopt);
    m_acquisitionStartedAt.reset();
    return Result::success(MediaAvStartupAttemptExpired{
        m_allocation, *m_state.generation(), std::move(error), std::move(purged)});
} catch (const std::bad_alloc&) {
    auto failed = markFailed(MediaAvSyncErrorCode::StartupCapacityExceeded, "Startup purge allocation failed");
    return MediaAvSyncResult<MediaAvStartupPollOutcome>::failure(failed.error());
} catch (const std::length_error&) {
    auto failed = markFailed(MediaAvSyncErrorCode::StartupCapacityExceeded, "Startup purge capacity is not representable");
    return MediaAvSyncResult<MediaAvStartupPollOutcome>::failure(failed.error());
}

MediaAvSyncStatus MediaAvStartupCoordinator::retireGeneration(const MediaAvGenerationPurge& transition)
{
    if (!m_state.generation() || *m_state.generation() > transition.oldGeneration ||
        *m_state.generation() < transition.publishedGeneration)
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition, "retire_generation", nullptr,
            "Startup retirement requires the exact published-to-pending generation interval"));
    auto retired = m_state.transition(MediaAvSyncEvent::RequireReacquisition, transition.nextGeneration);
    if (!retired) return retired;
    clearBufferedState();
    m_acquisitionStartedAt.reset();
    m_lastVideoSequence.reset();
    m_lastVideoPresentation.reset();
    m_lastAudioSequence.reset();
    return MediaAvSyncStatus::success();
}

MediaAvSyncStatus MediaAvStartupCoordinator::resumeAfterEvidence(MediaRunningTime observedAt)
{
    if (m_state.state() != MediaAvSyncState::WaitingForEvidence || !m_state.generation())
        return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition, "resume_evidence", nullptr,
            "A new acquisition attempt requires the waiting state"));
    auto resumed = m_state.transition(MediaAvSyncEvent::RequireReacquisition, *m_state.generation());
    if (resumed) m_acquisitionStartedAt = advanceWatermark(observedAt);
    return resumed;
}

MediaAvSyncResult<bool> MediaAvStartupCoordinator::sealMember(
    MediaAvStartupStream stream, MediaAvStartupMemberClosure closure,
    std::optional<std::uint64_t> generation)
{
    using Result = MediaAvSyncResult<bool>;
    if ((stream != MediaAvStartupStream::Video && stream != MediaAvStartupStream::Audio) ||
        (stream == MediaAvStartupStream::Audio && !m_config.audio))
        return Result::failure(startupError(
            MediaAvSyncErrorCode::SourceIdentityMismatch, "end_of_stream", nullptr,
            "startup boundary is not a planned stream member"));
    if ((closure != MediaAvStartupMemberClosure::Eof &&
         closure != MediaAvStartupMemberClosure::EndGeneration) ||
        (closure == MediaAvStartupMemberClosure::EndGeneration &&
         (!generation || *generation == 0 || generation != m_state.generation())))
        return Result::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition, "seal_member", nullptr,
            "startup generation boundary requires the exact current source generation"));
    if (m_state.state() == MediaAvSyncState::Running) {
        auto& ended = stream == MediaAvStartupStream::Video ? m_videoClosure : m_audioClosure;
        const auto other = stream == MediaAvStartupStream::Video ? m_audioClosure : m_videoClosure;
        if (ended != MediaAvStartupMemberClosure::Open ||
            (other != MediaAvStartupMemberClosure::Open && other != closure)) {
            return Result::failure(startupError(
                MediaAvSyncErrorCode::StartupInvalidTransition,
                "seal_member", nullptr, "duplicate or incompatible startup member boundary"));
        }
        if (!m_video->empty() || (m_audio && !m_audio->empty()))
            return Result::failure(startupError(
                MediaAvSyncErrorCode::StartupInvalidTransition, "seal_member", nullptr,
                "running startup boundary still owns unreleased media"));
        ended = closure;
        return Result::success(m_videoClosure == closure && (!m_audio || m_audioClosure == closure));
    }
    auto failed = markFailed(MediaAvSyncErrorCode::EofBeforeRelease,
                      "end of stream before atomic startup release");
    return Result::failure(failed.error());
}

MediaAvSyncStatus MediaAvStartupCoordinator::fail(std::string reason)
{
    return markFailed(MediaAvSyncErrorCode::StartupUpstreamError,
                      "upstream error before atomic startup release: " + reason);
}

void MediaAvStartupCoordinator::stop() noexcept
{
    if (m_state.state() == MediaAvSyncState::Idle) {
        (void)m_state.transition(MediaAvSyncEvent::BeginAcquisition, 0);
    }
    const auto generation = m_state.generation().value_or(0);
    (void)m_state.transition(MediaAvSyncEvent::Stop, generation);
    clearBufferedState();
}

void MediaAvStartupCoordinator::abort() noexcept
{
    if (m_state.state() == MediaAvSyncState::Idle) {
        (void)m_state.transition(MediaAvSyncEvent::BeginAcquisition, 0);
    }
    const auto generation = m_state.generation().value_or(0);
    (void)m_state.transition(MediaAvSyncEvent::Abort, generation);
    clearBufferedState();
}

MediaAvSyncStatus MediaAvStartupCoordinator::reset() noexcept
{
    clearBufferedState();
    m_state.reset();
    m_acquisitionStartedAt.reset();
    m_keyFrameWaitStartedAt.reset();
    m_processedWatermark.reset();
    m_lastVideoSequence.reset();
    m_lastVideoPresentation.reset();
    m_lastAudioSequence.reset();
    return MediaAvSyncStatus::success();
}

MediaAvSyncState MediaAvStartupCoordinator::state() const noexcept { return m_state.state(); }
const std::optional<std::uint64_t>& MediaAvStartupCoordinator::generation() const noexcept
{
    return m_state.generation();
}

const std::optional<MediaPlaybackEpoch>& MediaAvStartupCoordinator::playbackEpoch() const noexcept
{
    return m_epoch;
}

MediaAvStartupIds MediaAvStartupCoordinator::purge(
    std::span<const MediaAvStartupUnitId> prior, std::optional<MediaAvStartupUnitId> trailing)
{
    const auto count = startupResultCapacity({prior.size(), m_video->size(),
        m_audio ? m_audio->size() : 0, std::size_t(trailing.has_value())});
    MediaAvStartupIds purged(count, *MediaLineageAllocator<MediaAvStartupUnitId>{m_allocation}.resource());
    for (const auto& id : prior) purged.push_back(id);
    m_video->appendIds(purged);
    if (m_audio) m_audio->appendIds(purged);
    if (trailing) purged.push_back(*trailing);
    clearBufferedState();
    return purged;
}

void MediaAvStartupCoordinator::clearBufferedState() noexcept
{
    m_video->clear();
    if (m_audio) m_audio->clear();
    m_videoLocked = false;
    m_audioLocked = false;
    m_keyFrameWaitStartedAt.reset();
    m_videoBytes = 0;
    m_audioBytes = 0;
    m_epoch.reset();
    m_videoClosure = MediaAvStartupMemberClosure::Open;
    m_audioClosure = MediaAvStartupMemberClosure::Open;
    m_lastAttemptSelectionWork = {};
    m_cumulativeSelectionWork = {};
}

MediaRunningTime MediaAvStartupCoordinator::advanceWatermark(
    MediaRunningTime observedAt) noexcept
{
    if (!m_processedWatermark || observedAt > *m_processedWatermark) {
        m_processedWatermark = observedAt;
    }
    return *m_processedWatermark;
}

MediaAvSyncError MediaAvStartupCoordinator::startupError(
    MediaAvSyncErrorCode code,
    std::string operation,
    const MediaAvStartupAccessUnit* unit,
    std::string detail) const
{
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    const std::string expectedIdentity = unit
        ? (unit->stream == MediaAvStartupStream::Video
               ? m_config.videoIdentity
               : unit->stream == MediaAvStartupStream::Audio && m_config.audio
                   ? m_config.audio->identity : std::string{})
        : std::string{};
    return MediaAvSyncError(
        code,
        m_config.sourceClockMode,
        MediaAvSyncErrorState::Startup,
        std::move(operation),
        expectedIdentity,
        unit ? std::string(unit->identity.view()) : std::string{},
        m_state.generation(),
        unit ? std::optional<std::uint64_t>(unit->generation) : std::nullopt,
        unit ? unit->presentationTime : std::nullopt,
        m_epoch ? m_epoch->sourceStart : zero,
        m_epoch ? m_epoch->masterRelease : zero,
        0,
        m_config.maximumWait.nanoseconds(),
        std::move(detail));
}

MediaAvSyncStatus MediaAvStartupCoordinator::markFailed(MediaAvSyncErrorCode code,
                                                         std::string reason)
{
    if (!m_state.generation()) {
        auto begin = m_state.transition(MediaAvSyncEvent::BeginAcquisition, 0);
        if (!begin) return begin;
    }
    auto failed = m_state.transition(MediaAvSyncEvent::Fail, *m_state.generation());
    clearBufferedState();
    return failed
        ? MediaAvSyncStatus::failure(
              startupError(code, "atomic_startup", nullptr, std::move(reason)))
        : MediaAvSyncStatus::failure(startupError(
              MediaAvSyncErrorCode::StartupInvalidTransition,
              "atomic_startup", nullptr, failed.error().detail()));
}

} // namespace media::ffmpeg::graph
