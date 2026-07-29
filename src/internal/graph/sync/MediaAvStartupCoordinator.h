#pragma once

#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/sync/MediaAvSyncStateMachine.h"
#include "internal/graph/sync/MediaAvSyncError.h"
#include "internal/graph/model/MediaAvSyncSourceClockMode.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/sync/startup/MediaAvStartupSelectionWork.h"

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

struct MediaAvStartupAccessUnit final {
    MediaAvStartupStream stream;
    std::string identity;
    std::uint64_t sequence;
    std::uint64_t payloadBytes;
    std::optional<MediaRunningTime> presentationTime;
    MediaRunningTime duration;
    MediaSourceClockReadiness readiness;
    std::uint64_t generation;
    bool keyFrame;
    std::optional<MediaAvAudioSampleSpan> audio;
};

struct MediaAvStartupConfig final {
    bool requireVideoKeyFrame;
    bool trimAudioToCommonStart;
    bool allowDegradedClock;
    MediaAvSyncSourceClockMode sourceClockMode;
    MediaRunningTime maximumWait;
    MediaRunningTime preroll;
    MediaRunningTime keyFrameWait;
    MediaRunningTime maximumAudioTrim;
    MediaRunningTime maximumInitialSkew;
    MediaRunningTime maximumGap;
    MediaRunningTime outputLead;
    std::size_t videoCapacity;
    std::size_t audioCapacity;
    std::uint64_t videoByteCapacity;
    std::uint64_t audioByteCapacity;
    std::uint64_t maximumVideoUnitBytes;
    std::uint64_t maximumAudioUnitBytes;
    std::string videoIdentity;
    std::string audioIdentity;
};

struct MediaAvStartupSelection final {
    MediaAvStartupUnitId id;
    std::uint32_t trimLeadingSamples;
};

struct MediaAvStartupRelease final {
    MediaPlaybackEpoch epoch;
    std::vector<MediaAvStartupSelection> video;
    std::vector<MediaAvStartupSelection> audio;
};

enum class MediaAvStartupDisposition {
    Buffered,
    DroppedNotReady,
    DroppedOldGeneration,
    PassThrough,
    DroppedDuplicateOrRegressed
};

struct MediaAvStartupDecision final {
    MediaAvStartupDisposition disposition;
    std::optional<MediaAvStartupRelease> release;
    std::vector<MediaAvStartupUnitId> purged;
};

class MediaAvStartupCoordinator final {
public:
    static MediaAvSyncResult<MediaAvStartupCoordinator> create(MediaAvStartupConfig config);

    ~MediaAvStartupCoordinator();
    MediaAvStartupCoordinator(MediaAvStartupCoordinator&&) noexcept;
    MediaAvStartupCoordinator& operator=(MediaAvStartupCoordinator&&) noexcept;
    MediaAvStartupCoordinator(const MediaAvStartupCoordinator&) = delete;
    MediaAvStartupCoordinator& operator=(const MediaAvStartupCoordinator&) = delete;

    MediaAvSyncResult<MediaAvStartupDecision> submit(MediaAvStartupAccessUnit unit,
                                                      MediaRunningTime observedAt);
    MediaAvSyncStatus poll(MediaRunningTime observedAt);
    MediaAvSyncStatus endOfStream(MediaAvStartupStream stream);
    MediaAvSyncStatus fail(std::string reason);
    void stop() noexcept;
    void abort() noexcept;
    MediaAvSyncStatus reset() noexcept;

    MediaAvSyncState state() const noexcept;
    const std::optional<std::uint64_t>& generation() const noexcept;
    const std::optional<MediaPlaybackEpoch>& playbackEpoch() const noexcept;
    bool terminalEofReached() const noexcept;

private:
    friend struct MediaAvStartupCoordinatorTestAccess;
    explicit MediaAvStartupCoordinator(MediaAvStartupConfig config);
    MediaAvSyncStatus validateUnit(const MediaAvStartupAccessUnit& unit) const;
    MediaAvSyncResult<std::vector<MediaAvStartupUnitId>> advanceGeneration(
        std::uint64_t generation,
        MediaRunningTime observedAt);
    MediaAvSyncResult<MediaAvStartupDecision> tryRelease(MediaRunningTime observedAt);
    std::vector<MediaAvStartupUnitId> purge() noexcept;
    MediaRunningTime advanceWatermark(MediaRunningTime observedAt) noexcept;
    MediaAvSyncError startupError(
        MediaAvSyncErrorCode code,
        std::string operation,
        const MediaAvStartupAccessUnit* unit,
        std::string detail) const;
    MediaAvSyncStatus markFailed(MediaAvSyncErrorCode code, std::string reason);

    MediaAvStartupConfig m_config;
    MediaAvSyncStateMachine m_state;
    std::optional<MediaRunningTime> m_acquisitionStartedAt;
    std::optional<MediaRunningTime> m_keyFrameWaitStartedAt;
    std::optional<MediaRunningTime> m_processedWatermark;
    std::unique_ptr<MediaAvStartupStreamStore> m_video;
    std::unique_ptr<MediaAvStartupStreamStore> m_audio;
    MediaAvStartupSelectionWork m_lastAttemptSelectionWork;
    MediaAvStartupSelectionWork m_cumulativeSelectionWork;
    bool m_videoLocked = false;
    bool m_audioLocked = false;
    std::optional<std::uint64_t> m_lastVideoSequence;
    std::optional<std::uint64_t> m_lastAudioSequence;
    std::uint64_t m_videoBytes = 0;
    std::uint64_t m_audioBytes = 0;
    std::optional<MediaPlaybackEpoch> m_epoch;
    bool m_videoEof = false;
    bool m_audioEof = false;
};

} // namespace media::ffmpeg::graph
