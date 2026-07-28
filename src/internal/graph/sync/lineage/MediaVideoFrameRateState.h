#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"
#include "internal/graph/sync/lineage/MediaVideoLineageState.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg::graph {

enum class MediaVideoFrameRateGenerationDisposition {
    Activate,
    DropStale
};

class MediaVideoFrameRateState final : public MediaVideoLineageState {
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
        MediaInputTerminalTracker terminals { { "frame" } };
        bool eofEmitted = false;
        MediaBufferRef terminalBuffer;
        bool terminalPending = false;
        bool terminalIsEof = false;
    };

    explicit MediaVideoFrameRateState(bool requireCanonicalLineage) noexcept;

    [[nodiscard]] Data& data() noexcept;
    [[nodiscard]] const Data& data() const noexcept;
    [[nodiscard]] bool requiresCanonicalLineage() const noexcept;

    void resetLifecycle() noexcept;
    ::media::Result<MediaVideoFrameRateGenerationDisposition>
    activateGeneration(std::uint64_t generation);

private:
    void resetTimelineLocked() noexcept;
    void clearLineageStorage() noexcept;
    void clearOwnedLineage(const MediaAvGenerationPurge& purge) noexcept override;

    bool m_requireCanonicalLineage;
    Data m_data;
};

} // namespace media::ffmpeg::graph
