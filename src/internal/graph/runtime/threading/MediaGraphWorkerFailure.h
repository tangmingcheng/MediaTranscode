#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaGraphWorkerFailure final {
    MediaNodeId nodeId;
    MediaNodeKind nodeKind = MediaNodeKind::Unknown;
    std::string nodeName;
    ::media::ErrorInfo error;
};

class MediaGraphWorkerFailureRecorder final {
public:
    bool recordFirst(MediaGraphWorkerFailure failure);
    bool hasFailure() const noexcept;
    std::optional<MediaGraphWorkerFailure> primaryFailure() const;
    void clear();

private:
    mutable std::mutex m_mutex;
    std::optional<MediaGraphWorkerFailure> m_primaryFailure;
    std::atomic_bool m_hasFailure{ false };
};

} // namespace media::ffmpeg::graph
