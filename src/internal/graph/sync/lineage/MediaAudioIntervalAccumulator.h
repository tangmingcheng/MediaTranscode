#pragma once

#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "media_transcode/Result.h"

#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAudioLineageCapacity;

struct MediaAudioIntervalFragment final {
    std::shared_ptr<const MediaCanonicalLineage> lineage;
    MediaCanonicalAudioSampleInterval interval;
};

class MediaAudioIntervalAccumulator final {
public:
    ::media::Status push(MediaAudioIntervalFragment fragment);
    ::media::Result<std::vector<MediaAudioIntervalFragment>> take(int samples);
    ::media::Status finish() const;
    ::media::Status settleDroppedSamples(std::int64_t authorizedSamples);
    void reset() noexcept;
    std::int64_t queuedSamples() const noexcept;
    std::size_t fragmentCount() const noexcept;
    ::media::Status observeLineageCapacity(
        MediaAudioLineageCapacity& capacity) const;

private:
    ::media::Status fail(std::string message);
    std::deque<MediaAudioIntervalFragment> m_fragments;
    std::uint64_t m_generation = 0;
    int m_sampleRate = 0;
    std::int64_t m_expectedNextBegin = 0;
    std::int64_t m_queuedSamples = 0;
    bool m_initialized = false;
    bool m_terminalFailure = false;
};

} // namespace media::ffmpeg::graph
