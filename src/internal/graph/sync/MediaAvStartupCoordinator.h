#pragma once
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/storage/MediaLineageUniquePtr.h"
#include "internal/graph/model/MediaAvStartupIndexStoragePlan.h"

#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/MediaVideoStartupReadiness.h"
#include <variant>
#include "internal/graph/sync/MediaAvSyncStateMachine.h"
#include "internal/graph/sync/MediaCanonicalIdentity.h"
#include "internal/graph/sync/MediaAvSyncError.h"
#include "internal/graph/model/MediaAvSyncSourceClockMode.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/sync/MediaAvGenerationTransition.h"
#include "internal/graph/sync/startup/MediaAvStartupSelectionWork.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAvStartupStreamStore;
struct MediaAvStartupCoordinatorTestAccess;

enum class MediaAvStartupStream {
    Video,
    Audio
};

struct MediaAvAudioSampleSpan final {
    std::int64_t firstSample;
    std::uint32_t sampleRate;
    std::uint32_t sampleCount;
};

::media::Status validateMediaAvAudioSampleSpanDuration(
    const MediaAvAudioSampleSpan& span,
    MediaRunningTime duration);

::media::Result<std::uint32_t> calculateMediaAvAudioTrimSamples(
    MediaRunningTime epochSourceStart,
    const MediaAvAudioSampleSpan& span);

struct MediaAvStartupUnitId final {
    MediaAvStartupStream stream;
    std::uint64_t generation;
    std::uint64_t sequence;

    bool operator==(const MediaAvStartupUnitId&) const noexcept = default;
};

struct MediaAvStartupUnitIdHash final {
    std::size_t operator()(const MediaAvStartupUnitId& id) const noexcept;
};

struct MediaAvStartupDisplayPoint final {};
// A decoded display point deliberately has no interval duration.
using MediaAvStartupTiming = std::variant<MediaRunningTime, MediaAvStartupDisplayPoint>;

struct MediaAvStartupAccessUnit final {
    MediaAvStartupStream stream;
    MediaCanonicalIdentity identity;
    std::uint64_t sequence;
    std::uint64_t payloadBytes;
    std::optional<MediaRunningTime> presentationTime;
    MediaAvStartupTiming timing;
    MediaSourceClockReadiness readiness;
    std::uint64_t generation;
    bool keyFrame;
    std::optional<MediaAvAudioSampleSpan> audio;
};

struct MediaAvStartupAudioConfig final {
    bool trimToCommonStart;
    MediaRunningTime maximumTrim;
    MediaRunningTime maximumInitialSkew;
    std::size_t capacity;
    std::uint64_t byteCapacity;
    std::uint64_t maximumUnitBytes;
    std::string identity;
    int outputSampleRate;
};

struct MediaAvStartupConfig final {
    MediaTranscodeStreamSet members;
    MediaVideoStartupReadiness videoReadiness;
    bool requireVideoKeyFrame;
    bool allowDegradedClock;
    MediaAvSyncSourceClockMode sourceClockMode;
    MediaRunningTime maximumWait;
    MediaRunningTime preroll;
    MediaRunningTime keyFrameWait;
    MediaRunningTime maximumGap;
    MediaRunningTime outputLead;
    std::size_t videoCapacity;
    std::uint64_t videoByteCapacity;
    std::uint64_t maximumVideoUnitBytes;
    std::string videoIdentity;
    std::optional<MediaAvStartupAudioConfig> audio;
};

struct MediaAvStartupSelection final {
    MediaAvStartupUnitId id;
    std::uint32_t trimLeadingSamples;
};

using MediaAvStartupIds = MediaFixedCapacityStorage<MediaAvStartupUnitId>;
using MediaAvStartupSelections = MediaFixedCapacityStorage<MediaAvStartupSelection>;

// Scope precedes backing for destruction. During move assignment, prepared
// allocation headers also retain the resource; runtime storage uses the static PMR.
struct MediaAvStartupRelease final {
    MediaLineageAllocationScope allocation;
    MediaPlaybackEpoch epoch;
    MediaAvStartupSelections video;
    MediaAvStartupSelections audio;
};

enum class MediaAvStartupDisposition {
    Buffered,
    DroppedNotReady,
    DroppedOldGeneration,
    PassThrough,
    DroppedDuplicateOrRegressed
};

struct MediaAvStartupDecision final {
    MediaLineageAllocationScope allocation;
    MediaAvStartupDisposition disposition;
    std::optional<MediaAvStartupRelease> release;
    MediaAvStartupIds purged;
};

struct MediaAvStartupAttemptExpired final {
    MediaLineageAllocationScope allocation;
    std::uint64_t generation;
    MediaAvSyncError error;
    MediaAvStartupIds purged;
};
using MediaAvStartupPollOutcome = std::optional<MediaAvStartupAttemptExpired>;

struct MediaAvStartupDeadlines final {
    MediaRunningTime acquisition;
    std::optional<MediaRunningTime> keyFrame;

    MediaRunningTime wakeAt() const noexcept
    { return keyFrame && *keyFrame < acquisition ? *keyFrame : acquisition; }
};

class MediaAvStartupCoordinator final {
public:
    static MediaAvSyncResult<MediaAvStartupCoordinator> create(MediaAvStartupConfig config,
        MediaLineageAllocationScope allocation, MediaAvStartupIndexStoragePlan videoIndex,
        std::optional<MediaAvStartupIndexStoragePlan> audioIndex);

    ~MediaAvStartupCoordinator();
    MediaAvStartupCoordinator(MediaAvStartupCoordinator&&) noexcept;
    MediaAvStartupCoordinator& operator=(MediaAvStartupCoordinator&&) noexcept;
    MediaAvStartupCoordinator(const MediaAvStartupCoordinator&) = delete;
    MediaAvStartupCoordinator& operator=(const MediaAvStartupCoordinator&) = delete;

    MediaAvSyncResult<MediaAvStartupDecision> submit(MediaAvStartupAccessUnit unit,
                                                      MediaRunningTime observedAt);
    MediaAvSyncResult<MediaAvStartupPollOutcome> poll(MediaRunningTime observedAt);
    MediaAvSyncResult<std::optional<MediaAvStartupDeadlines>> deadlines() const;
    MediaAvSyncStatus resumeAfterEvidence(MediaRunningTime observedAt);
    MediaAvSyncStatus retireGeneration(const MediaAvGenerationPurge& purge);
    MediaAvSyncStatus endOfStream(MediaAvStartupStream stream);
    MediaAvSyncStatus fail(std::string reason);
    void stop() noexcept;
    void abort() noexcept;
    MediaAvSyncStatus reset() noexcept;

    MediaTranscodeStreamSet members() const noexcept { return m_config.members; }
    const MediaAvStartupAudioConfig* audioConfig() const noexcept
    {
        return m_config.audio ? &*m_config.audio : nullptr;
    }
    MediaAvSyncState state() const noexcept;
    const std::optional<std::uint64_t>& generation() const noexcept;
    const std::optional<MediaPlaybackEpoch>& playbackEpoch() const noexcept;
    bool terminalEofReached() const noexcept;

private:
    friend struct MediaAvStartupCoordinatorTestAccess;
    MediaAvStartupCoordinator(MediaAvStartupConfig config, MediaLineageAllocationScope allocation,
        MediaAvStartupIndexStoragePlan videoIndex, std::optional<MediaAvStartupIndexStoragePlan> audioIndex);
    MediaAvSyncStatus validateUnit(const MediaAvStartupAccessUnit& unit) const;
    MediaAvSyncResult<MediaAvStartupIds> advanceGeneration(
        std::uint64_t generation,
        MediaRunningTime observedAt);
    MediaAvSyncResult<MediaAvStartupDecision> tryRelease(MediaRunningTime observedAt, MediaAvStartupIds prior);
    MediaAvStartupIds purge(std::span<const MediaAvStartupUnitId> prior,
        std::optional<MediaAvStartupUnitId> trailing);
    void clearBufferedState() noexcept;
    MediaRunningTime advanceWatermark(MediaRunningTime observedAt) noexcept;
    MediaAvSyncError startupError(
        MediaAvSyncErrorCode code,
        std::string operation,
        const MediaAvStartupAccessUnit* unit,
        std::string detail) const;
    MediaAvSyncStatus markFailed(MediaAvSyncErrorCode code, std::string reason);

    MediaLineageAllocationScope m_allocation;
    MediaAvStartupConfig m_config;
    MediaAvSyncStateMachine m_state;
    std::optional<MediaRunningTime> m_acquisitionStartedAt;
    std::optional<MediaRunningTime> m_keyFrameWaitStartedAt;
    std::optional<MediaRunningTime> m_processedWatermark;
    MediaLineageUniquePtr<MediaAvStartupStreamStore> m_video;
    MediaLineageUniquePtr<MediaAvStartupStreamStore> m_audio;
    MediaAvStartupSelectionWork m_lastAttemptSelectionWork;
    MediaAvStartupSelectionWork m_cumulativeSelectionWork;
    bool m_videoLocked = false;
    bool m_audioLocked = false;
    std::optional<std::uint64_t> m_lastVideoSequence;
    std::optional<MediaRunningTime> m_lastVideoPresentation;
    std::optional<std::uint64_t> m_lastAudioSequence;
    std::uint64_t m_videoBytes = 0;
    std::uint64_t m_audioBytes = 0;
    std::optional<MediaPlaybackEpoch> m_epoch;
    bool m_videoEof = false;
    bool m_audioEof = false;
};

} // namespace media::ffmpeg::graph
