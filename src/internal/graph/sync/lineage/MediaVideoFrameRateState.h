#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvGenerationPurgeTarget.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

class MediaVideoFrameRateState final : public MediaAvGenerationPurgeTarget {
public:
    struct TaggedFrame final {
        MediaBufferRef buffer;
        std::uint64_t generation = 0;
    };

    struct Data final {
        bool initialized = false;
        bool started = false;
        bool flushed = false;
        AVRational inputTimeBase {0, 1};
        AVRational targetFramePeriod {0, 1};
        std::int64_t startPts = 0;
        std::int64_t nextOutputIndex = 0;
        std::int64_t lastInputPts = AV_NOPTS_VALUE;
        std::int64_t lastOutputPts = AV_NOPTS_VALUE;
        TaggedFrame lastInputFrame;
        std::deque<TaggedFrame> pendingFrames;
        std::uint64_t activeGeneration = 0;
        std::uint64_t expectedGeneration = 0;
        std::uint64_t lastTransitionSequence = 0;
    };

    explicit MediaVideoFrameRateState(bool requireCanonicalLineage) noexcept;

    [[nodiscard]] std::unique_lock<std::recursive_mutex> lock() const;
    [[nodiscard]] Data& data() noexcept;
    [[nodiscard]] const Data& data() const noexcept;
    [[nodiscard]] bool requiresCanonicalLineage() const noexcept;

    void resetLifecycle() noexcept;
    ::media::Status activateGeneration(std::uint64_t generation);
    ::media::Status purge(const MediaAvGenerationPurge& purge) override;

private:
    void resetTimelineLocked() noexcept;

    bool m_requireCanonicalLineage;
    mutable std::recursive_mutex m_mutex;
    Data m_data;
};

} // namespace media::ffmpeg::graph
