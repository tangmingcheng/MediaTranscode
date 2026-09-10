#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaGraphWorkerFailurePhase { Preparation, Publication, Runtime, Drain, Release };

struct MediaGraphWorkerFailure final {
    MediaNodeId nodeId;
    MediaNodeKind nodeKind = MediaNodeKind::Unknown;
    std::string nodeName;
    ::media::ErrorInfo error;
    MediaGraphWorkerFailurePhase phase = MediaGraphWorkerFailurePhase::Runtime;
};

class MediaGraphWorkerFailureRecorder final {
public:
    bool recordFirst(MediaGraphWorkerFailure failure);
    bool hasFailure() const noexcept;
    std::optional<MediaGraphWorkerFailure> primaryFailure() const;
    void clear();
    void setPhase(MediaGraphWorkerFailurePhase phase) noexcept { m_phase.store(phase, std::memory_order_release); }
    MediaGraphWorkerFailurePhase phase() const noexcept { return m_phase.load(std::memory_order_acquire); }

private:
    mutable std::mutex m_mutex;
    std::optional<MediaGraphWorkerFailure> m_primaryFailure;
    std::atomic_bool m_hasFailure{ false };
    std::atomic<MediaGraphWorkerFailurePhase> m_phase{MediaGraphWorkerFailurePhase::Runtime};
};

} // namespace media::ffmpeg::graph
