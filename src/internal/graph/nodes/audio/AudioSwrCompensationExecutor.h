#pragma once

#include "internal/graph/sync/MediaAudioCorrection.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <cstddef>
#include <deque>
#include <optional>

struct SwrContext;

namespace media::ffmpeg::graph {

struct AudioSwrCompensationWindow final {
    bool correctionRequired = false;
    int maximumOutputSamples = 0;
};

class AudioSwrCompensationExecutor final {
public:
    static ::media::Result<AudioSwrCompensationExecutor> create(
        MediaAudioCorrectionExecutionMode mode,
        std::uint64_t generation,
        std::size_t lookaheadWindows);

    ::media::Status enqueue(const MediaAudioCompensationCommand& command);
    ::media::Result<AudioSwrCompensationWindow> prepare(
        SwrContext* swr,
        std::int64_t outputSampleIndex);
    ::media::Status advance(int producedSamples);
    ::media::Status reset(std::uint64_t generation);
    bool canAccept() const noexcept;
    bool requiresNextWindow() const noexcept;
    ::media::Status settleTerminal() noexcept;
    MediaAudioCorrectionExecutionMode mode() const noexcept;

private:
    AudioSwrCompensationExecutor(MediaAudioCorrectionExecutionMode mode,
                                 std::uint64_t generation,
                                 std::size_t lookaheadWindows) noexcept;

    MediaAudioCorrectionExecutionMode m_mode;
    std::uint64_t m_generation = 0;
    std::size_t m_lookaheadWindows = 0;
    std::uint64_t m_lastSequence = 0;
    std::int64_t m_lastPlannedEnd = 0;
    std::optional<MediaAudioCompensationCommand> m_active;
    std::deque<MediaAudioCompensationCommand> m_pending;
    int m_activeRemaining = 0;
};

} // namespace media::ffmpeg::graph
