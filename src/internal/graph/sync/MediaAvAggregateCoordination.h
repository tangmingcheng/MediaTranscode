#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"

#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

class MediaNodeWakeup;

struct MediaAvAggregateAudioReady final {
    MediaAudioPlaybackOrigin origin;
    MediaCanonicalAudioSampleInterval interval;
    friend bool operator==(const MediaAvAggregateAudioReady& a,
                           const MediaAvAggregateAudioReady& b) noexcept
    {
        return a.origin == b.origin && a.interval.begin == b.interval.begin &&
            a.interval.end == b.interval.end && a.interval.sampleRate == b.interval.sampleRate;
    }
};

struct MediaAvAggregateInputCompletion final {
    std::uint64_t minimumGeneration;
    std::optional<MediaRunningTime> end;
    bool sealed;
};

struct MediaAvAggregateDisplayCommit final {
    std::uint64_t decodedSequence;
    MediaRunningTime outputEnd;
};

struct MediaAvAggregateSourceCompletion final {
    MediaAvAggregateInputCompletion video;
    MediaAvAggregateInputCompletion audio;
    MediaVideoFrameLineageProduct videoProduct;
    std::optional<MediaAvAggregateDisplayCommit> finalDisplayCommit;
};

struct MediaAvAggregateDrainSnapshot final {
    bool requested = false;
    bool videoClosed = false;
    bool audioClosed = false;
};

struct MediaAvAggregateCoordinationSnapshot final {
    std::optional<MediaAvAggregateAudioReady> audioReady;
    bool allInputsSealed;
    std::optional<MediaRunningTime> finalOutputEnd;
};

// Only preparation and trusted-end facts cross owner threads. Media buffers,
// publication permits, and the output activation remain with their DAG owners.
class MediaAvAggregateCoordination final {
public:
    explicit MediaAvAggregateCoordination(const MediaAvContinuousAggregateTopology& plan);
    ::media::Status bindWakeup(MediaStreamKind owner, std::shared_ptr<MediaNodeWakeup> wakeup);
    ::media::Status publishAudioReady(MediaAvAggregateAudioReady ready);
    ::media::Status observeInputEnd(std::size_t source, MediaStreamKind stream,
                                   std::uint64_t generation, MediaRunningTime end);
    ::media::Status sealInput(std::size_t source, MediaStreamKind stream);
    // Only the video owner calls this after EOF and after all pending display
    // contributions committed. The output interval is not an input duration.
    ::media::Status sealDisplayInput(std::size_t source, std::uint64_t generation,
        std::optional<MediaAvAggregateDisplayCommit> lastCommit);
    ::media::Status purgeInput(std::size_t source, MediaStreamKind stream,
                              std::uint64_t oldGeneration, std::uint64_t nextGeneration);
    MediaAvAggregateCoordinationSnapshot snapshot() const;
    ::media::Status requestDrain();
    ::media::Status publishOutputClosed(MediaStreamKind stream);
    MediaAvAggregateDrainSnapshot drainSnapshot() const;
    void stop() noexcept;

private:
    MediaAvAggregateInputCompletion* input(std::size_t source, MediaStreamKind stream) noexcept;
    mutable std::mutex m_mutex;
    MediaFixedCapacityStorage<MediaAvAggregateSourceCompletion> m_sources;
    const std::size_t m_audioSource;
    const int m_sampleRate;
    std::optional<MediaAvAggregateAudioReady> m_audioReady;
    std::weak_ptr<MediaNodeWakeup> m_videoWakeup;
    std::weak_ptr<MediaNodeWakeup> m_audioWakeup;
    bool m_stopped = false;
    MediaAvAggregateDrainSnapshot m_drain;
};

} // namespace media::ffmpeg::graph
