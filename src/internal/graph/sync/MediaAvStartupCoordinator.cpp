#include "internal/graph/sync/MediaAvStartupCoordinator.h"
#include "internal/graph/sync/startup/MediaAvStartupCoverageIndex.h"
#include "internal/graph/sync/startup/MediaAvStartupLimits.h"
#include "internal/graph/sync/startup/MediaAvStartupStreamStore.h"
#include "internal/graph/sync/startup/MediaAvStartupWindowSelector.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr MediaRunningTime Zero = MediaRunningTime::fromNanoseconds(0);

::media::Result<std::uint32_t> trimSamples(MediaRunningTime trimTime,
                                           const MediaAvAudioSampleSpan& span)
{
    constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000ULL;
    const auto nanoseconds = static_cast<std::uint64_t>(trimTime.nanoseconds());
    const auto seconds = nanoseconds / NanosecondsPerSecond;
    const auto remainder = nanoseconds % NanosecondsPerSecond;
    if (seconds > std::numeric_limits<std::uint64_t>::max() / span.sampleRate) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument("Audio trim sample count overflow"));
    }
    const auto wholeSamples = seconds * span.sampleRate;
    const auto fractionalProduct = remainder * span.sampleRate;
    const auto fractionalSamples = fractionalProduct / NanosecondsPerSecond +
        (fractionalProduct % NanosecondsPerSecond == 0 ? 0 : 1);
    if (wholeSamples > std::numeric_limits<std::uint64_t>::max() - fractionalSamples) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument("Audio trim sample count overflow"));
    }
    const auto samples = wholeSamples + fractionalSamples;
    if (samples > span.sampleCount ||
        samples > std::numeric_limits<std::uint32_t>::max()) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument("Audio trim is not representable"));
    }
    return ::media::Result<std::uint32_t>::success(
        static_cast<std::uint32_t>(samples));
}

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
    MediaAvStartupConfig config)
{
    const bool videoCapacityOverflow = config.maximumVideoUnitBytes != 0 &&
        config.videoCapacity > std::numeric_limits<std::uint64_t>::max() /
                                   config.maximumVideoUnitBytes;
    const bool audioCapacityOverflow = config.maximumAudioUnitBytes != 0 &&
        config.audioCapacity > std::numeric_limits<std::uint64_t>::max() /
                                   config.maximumAudioUnitBytes;
    if (!config.requireVideoKeyFrame || !config.trimAudioToCommonStart ||
        config.allowDegradedClock ||
        config.maximumWait <= Zero || config.preroll <= Zero ||
        config.keyFrameWait <= Zero || config.maximumAudioTrim <= Zero ||
        config.maximumInitialSkew <= Zero || config.maximumGap <= Zero ||
        config.outputLead <= Zero ||
        config.maximumAudioTrim > config.preroll ||
        config.maximumGap >= config.preroll ||
        config.maximumInitialSkew >= config.outputLead ||
        config.preroll >= config.keyFrameWait ||
        config.keyFrameWait > config.maximumWait ||
        config.videoCapacity == 0 || config.audioCapacity == 0 ||
        config.videoCapacity > MediaAvStartupMaximumUnitCapacity ||
        config.audioCapacity > MediaAvStartupMaximumUnitCapacity ||
        config.videoByteCapacity == 0 || config.audioByteCapacity == 0 ||
        config.maximumVideoUnitBytes == 0 || config.maximumAudioUnitBytes == 0 ||
        videoCapacityOverflow || audioCapacityOverflow ||
        (!videoCapacityOverflow &&
         config.videoByteCapacity != static_cast<std::uint64_t>(config.videoCapacity) *
                                         config.maximumVideoUnitBytes) ||
        (!audioCapacityOverflow &&
         config.audioByteCapacity != static_cast<std::uint64_t>(config.audioCapacity) *
                                         config.maximumAudioUnitBytes) ||
        config.maximumVideoUnitBytes >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        config.maximumAudioUnitBytes >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        config.videoByteCapacity >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        config.audioByteCapacity >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        config.videoIdentity.empty() || config.audioIdentity.empty() ||
        config.videoIdentity == config.audioIdentity) {
        const auto zero = MediaRunningTime::fromNanoseconds(0);
        return MediaAvSyncResult<MediaAvStartupCoordinator>::failure(MediaAvSyncError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            config.topology,
            MediaAvSyncErrorState::Startup,
            "create_startup_coordinator",
            config.videoIdentity,
            config.audioIdentity,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            zero,
            zero,
            0,
            config.maximumWait.nanoseconds(),
            "MediaAvStartupCoordinator requires a complete ordered startup policy"));
    }
    return MediaAvSyncResult<MediaAvStartupCoordinator>::success(
        MediaAvStartupCoordinator(std::move(config)));
}

MediaAvStartupCoordinator::MediaAvStartupCoordinator(MediaAvStartupConfig config)
    : m_config(std::move(config))
    , m_state(m_config.topology)
    , m_video(std::make_unique<MediaAvStartupStreamStore>(m_config.maximumGap))
    , m_audio(std::make_unique<MediaAvStartupStreamStore>(m_config.maximumGap))
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
    const auto maximumUnitBytes = unit.stream == MediaAvStartupStream::Video
        ? m_config.maximumVideoUnitBytes
        : m_config.maximumAudioUnitBytes;
    if (unit.sequence == 0 || unit.payloadBytes == 0 ||
        unit.payloadBytes > maximumUnitBytes || unit.duration <= Zero) {
        return MediaAvSyncStatus::failure(startupError(
            unit.payloadBytes > maximumUnitBytes
                ? MediaAvSyncErrorCode::StartupCapacityExceeded
                : MediaAvSyncErrorCode::InvalidDuration,
            "validate_startup_unit", &unit,
            "startup unit requires bounded payload, positive sequence and duration"));
    }
    const auto& expectedIdentity = unit.stream == MediaAvStartupStream::Video
        ? m_config.videoIdentity
        : m_config.audioIdentity;
    if (unit.identity != expectedIdentity) {
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
    } else if (auto status = validateMediaAvAudioSampleSpanDuration(
                   *unit.audio, unit.duration); !status) {
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

MediaAvSyncResult<std::vector<MediaAvStartupUnitId>>
MediaAvStartupCoordinator::advanceGeneration(
    std::uint64_t generation,
    MediaRunningTime observedAt)
{
    MediaAvSyncStatus status = m_state.generation()
        ? m_state.transition(MediaAvSyncEvent::RequireReacquisition, generation)
        : m_state.transition(MediaAvSyncEvent::BeginAcquisition, generation);
    if (!status) {
        return MediaAvSyncResult<std::vector<MediaAvStartupUnitId>>::failure(status.error());
    }
    auto purged = purge();
    m_acquisitionStartedAt = observedAt;
    return MediaAvSyncResult<std::vector<MediaAvStartupUnitId>>::success(std::move(purged));
}

MediaAvSyncResult<MediaAvStartupDecision> MediaAvStartupCoordinator::submit(
    MediaAvStartupAccessUnit unit,
    MediaRunningTime observedAt)
{
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
    const MediaRunningTime effectiveNow = advanceWatermark(observedAt);
    std::vector<MediaAvStartupUnitId> purged;
    if (!m_state.generation()) {
        auto advanced = advanceGeneration(unit.generation, effectiveNow);
        if (!advanced) return MediaAvSyncResult<MediaAvStartupDecision>::failure(advanced.error());
        purged = std::move(advanced).value();
    } else if (unit.generation < *m_state.generation()) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {MediaAvStartupDisposition::DroppedOldGeneration,
             std::nullopt, {unitId(unit)}});
    } else if (unit.generation > *m_state.generation()) {
        auto advanced = advanceGeneration(unit.generation, effectiveNow);
        if (!advanced) return MediaAvSyncResult<MediaAvStartupDecision>::failure(advanced.error());
        purged = std::move(advanced).value();
    }

    const bool usableClock = unit.readiness == MediaSourceClockReadiness::Locked;
    if (!usableClock) {
        const bool acquisitionAlreadyActive =
            m_state.state() == MediaAvSyncState::AcquiringClock;
        if (auto status = m_state.transition(
                MediaAvSyncEvent::RequireReacquisition,
                unit.generation); !status) {
            return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
        }
        auto reacquired = purge();
        purged.insert(purged.end(), reacquired.begin(), reacquired.end());
        if (!acquisitionAlreadyActive) m_acquisitionStartedAt = effectiveNow;
        purged.push_back(unitId(unit));
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {MediaAvStartupDisposition::DroppedNotReady, std::nullopt, std::move(purged)});
    }

    const bool streamEnded = unit.stream == MediaAvStartupStream::Video
        ? m_videoEof
        : m_audioEof;
    if (streamEnded) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            "submit", &unit, "media arrived after end of stream"));
    }
    if (m_state.state() == MediaAvSyncState::Running) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {MediaAvStartupDisposition::PassThrough, std::nullopt, std::move(purged)});
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
        : m_config.audioCapacity;
    const std::uint64_t byteCapacity = unit.stream == MediaAvStartupStream::Video
        ? m_config.videoByteCapacity
        : m_config.audioByteCapacity;
    if (store.size() >= capacity || unit.payloadBytes > byteCapacity - bufferedBytes) {
        auto failed = markFailed(MediaAvSyncErrorCode::StartupCapacityExceeded,
                                 "startup buffer capacity exceeded");
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(failed.error());
    }
    if (!store.empty() && unit.sequence <= store.backSequence()) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            "submit", &unit, "per-stream sequence regressed or repeated"));
    }
    const auto payloadBytes = unit.payloadBytes;
    if (unit.stream == MediaAvStartupStream::Video) m_videoLocked = true;
    else m_audioLocked = true;
    auto appended = store.append(std::move(unit));
    if (!appended) {
        auto failed = markFailed(MediaAvSyncErrorCode::TimeOverflow,
                                 appended.error().message);
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(failed.error());
    }
    if (!appended.value()) {
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(startupError(
            MediaAvSyncErrorCode::StartupInvalidTransition,
            "submit", nullptr, "duplicate presentation identity"));
    }
    bufferedBytes += payloadBytes;
    m_cumulativeSelectionWork.coverageOperations =
        m_video->cumulativeCoverageWork().coverageOperations +
        m_audio->cumulativeCoverageWork().coverageOperations;
    m_cumulativeSelectionWork.orderedIndexMutations =
        m_video->cumulativeCoverageWork().orderedIndexMutations +
        m_audio->cumulativeCoverageWork().orderedIndexMutations;

    if (m_videoLocked && m_audioLocked &&
        m_state.state() == MediaAvSyncState::AcquiringClock) {
        if (auto status = m_state.transition(MediaAvSyncEvent::ClocksLocked,
                                             *m_state.generation()); !status) {
            return MediaAvSyncResult<MediaAvStartupDecision>::failure(status.error());
        }
    }
    auto decision = tryRelease(effectiveNow);
    if (!decision) return MediaAvSyncResult<MediaAvStartupDecision>::failure(decision.error());
    auto& releasePurged = decision.value().purged;
    releasePurged.insert(releasePurged.begin(), purged.begin(), purged.end());
    return decision;
}

MediaAvSyncResult<MediaAvStartupDecision>
MediaAvStartupCoordinator::tryRelease(MediaRunningTime observedAt)
{
    if (m_state.state() != MediaAvSyncState::PrimingStreams) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {MediaAvStartupDisposition::Buffered, std::nullopt, {}});
    }
    if (m_audio->empty()) {
        return MediaAvSyncResult<MediaAvStartupDecision>::success(
            {MediaAvStartupDisposition::Buffered, std::nullopt, {}});
    }
    m_lastAttemptSelectionWork = {};
    auto videoCoverage = MediaAvStartupCoverageIndex::build(
        m_video->presentationSnapshot(), m_lastAttemptSelectionWork);
    auto audioCoverage = MediaAvStartupCoverageIndex::build(
        m_audio->presentationSnapshot(), m_lastAttemptSelectionWork);
    auto selected = MediaAvStartupWindowSelector::select(
        videoCoverage, audioCoverage, m_config,
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
            {MediaAvStartupDisposition::Buffered, std::nullopt, {}});
    }
    const auto& window = *selected.value();
    auto audioTrimTime = window.sourceStart.checkedSubtract(
        *window.audio->presentationTime);
    if (!audioTrimTime || audioTrimTime.value() < Zero ||
        audioTrimTime.value() > m_config.maximumAudioTrim) {
        auto failed = markFailed(MediaAvSyncErrorCode::AudioTrimLimitExceeded,
                                 "audio trim exceeds planned bound");
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(
            failed.error());
    }
    auto trim = trimSamples(audioTrimTime.value(), *window.audio->audio);
    if (!trim) {
        auto failed = markFailed(MediaAvSyncErrorCode::AudioTrimLimitExceeded,
                                 trim.error().message);
        return MediaAvSyncResult<MediaAvStartupDecision>::failure(
            failed.error());
    }

    MediaAvStartupRelease batch{
        MediaPlaybackEpoch{window.sourceStart, Zero, *m_state.generation()}, {}, {}};
    auto purged = m_video->prefixBefore(window.video, m_lastAttemptSelectionWork);
    auto audioPurged = m_audio->prefixBefore(window.audio, m_lastAttemptSelectionWork);
    purged.insert(purged.end(), audioPurged.begin(), audioPurged.end());
    m_video->appendSuffixSelections(window.video, 0, batch.video,
                                    m_lastAttemptSelectionWork);
    m_audio->appendSuffixSelections(window.audio, trim.value(), batch.audio,
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
    m_audio->clear();
    m_videoBytes = 0;
    m_audioBytes = 0;
    m_epoch = batch.epoch;
    return MediaAvSyncResult<MediaAvStartupDecision>::success(
        {MediaAvStartupDisposition::Buffered,
         std::optional<MediaAvStartupRelease>(std::move(batch)),
         std::move(purged)});
}

MediaAvSyncStatus MediaAvStartupCoordinator::poll(MediaRunningTime observedAt)
{
    if (!m_acquisitionStartedAt || m_state.state() == MediaAvSyncState::Running ||
        m_state.state() == MediaAvSyncState::Idle) return MediaAvSyncStatus::success();
    const MediaRunningTime effectiveNow = advanceWatermark(observedAt);
    auto elapsed = effectiveNow.checkedSubtract(*m_acquisitionStartedAt);
    if (!elapsed) return MediaAvSyncStatus::failure(startupError(
        MediaAvSyncErrorCode::TimeOverflow, "poll", nullptr, elapsed.error().message));
    if (elapsed.value() >= m_config.maximumWait) {
        return markFailed(MediaAvSyncErrorCode::StartupTimeout, "startup timeout");
    }
    const auto videoSnapshot = m_keyFrameWaitStartedAt
        ? m_video->presentationSnapshot()
        : std::vector<MediaAvStartupIndexedUnit>{};
    if (m_keyFrameWaitStartedAt &&
        std::none_of(videoSnapshot.begin(), videoSnapshot.end(), [&](const auto& item) {
            return !m_config.requireVideoKeyFrame || item.unit->keyFrame;
        })) {
        auto keyFrameElapsed = effectiveNow.checkedSubtract(*m_keyFrameWaitStartedAt);
        if (!keyFrameElapsed) return MediaAvSyncStatus::failure(startupError(
            MediaAvSyncErrorCode::TimeOverflow, "poll_key_frame", nullptr,
            keyFrameElapsed.error().message));
        if (keyFrameElapsed.value() >= m_config.keyFrameWait) {
            return markFailed(MediaAvSyncErrorCode::KeyFrameTimeout,
                              "video key frame wait timeout");
        }
    }
    return MediaAvSyncStatus::success();
}

MediaAvSyncStatus MediaAvStartupCoordinator::endOfStream(MediaAvStartupStream stream)
{
    if (m_state.state() == MediaAvSyncState::Running) {
        bool& ended = stream == MediaAvStartupStream::Video ? m_videoEof : m_audioEof;
        if (ended) {
            return MediaAvSyncStatus::failure(startupError(
                MediaAvSyncErrorCode::StartupInvalidTransition,
                "end_of_stream", nullptr, "duplicate stream EOF"));
        }
        ended = true;
        return MediaAvSyncStatus::success();
    }
    return markFailed(MediaAvSyncErrorCode::EofBeforeRelease,
                      "end of stream before atomic startup release");
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
    purge();
}

void MediaAvStartupCoordinator::abort() noexcept
{
    if (m_state.state() == MediaAvSyncState::Idle) {
        (void)m_state.transition(MediaAvSyncEvent::BeginAcquisition, 0);
    }
    const auto generation = m_state.generation().value_or(0);
    (void)m_state.transition(MediaAvSyncEvent::Abort, generation);
    purge();
}

MediaAvSyncStatus MediaAvStartupCoordinator::reset() noexcept
{
    (void)purge();
    m_state.reset();
    m_acquisitionStartedAt.reset();
    m_keyFrameWaitStartedAt.reset();
    m_processedWatermark.reset();
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

bool MediaAvStartupCoordinator::terminalEofReached() const noexcept
{
    return m_videoEof && m_audioEof;
}

std::vector<MediaAvStartupUnitId> MediaAvStartupCoordinator::purge() noexcept
{
    std::vector<MediaAvStartupUnitId> purged;
    auto videoIds = m_video->ids();
    auto audioIds = m_audio->ids();
    purged.reserve(videoIds.size() + audioIds.size());
    purged.insert(purged.end(), videoIds.begin(), videoIds.end());
    purged.insert(purged.end(), audioIds.begin(), audioIds.end());
    m_video->clear();
    m_audio->clear();
    m_videoLocked = false;
    m_audioLocked = false;
    m_keyFrameWaitStartedAt.reset();
    m_videoBytes = 0;
    m_audioBytes = 0;
    m_epoch.reset();
    m_videoEof = false;
    m_audioEof = false;
    m_lastAttemptSelectionWork = {};
    m_cumulativeSelectionWork = {};
    return purged;
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
               : m_config.audioIdentity)
        : std::string{};
    return MediaAvSyncError(
        code,
        m_config.topology,
        MediaAvSyncErrorState::Startup,
        std::move(operation),
        expectedIdentity,
        unit ? unit->identity : std::string{},
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
    (void)purge();
    return failed
        ? MediaAvSyncStatus::failure(
              startupError(code, "atomic_startup", nullptr, std::move(reason)))
        : MediaAvSyncStatus::failure(startupError(
              MediaAvSyncErrorCode::StartupInvalidTransition,
              "atomic_startup", nullptr, failed.error().detail()));
}

} // namespace media::ffmpeg::graph
