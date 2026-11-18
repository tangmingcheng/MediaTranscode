#pragma once
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"

namespace media::ffmpeg::graph {
// Owns already-admitted publication storage. No allocation or queue growth.
// The node owner thread drains this before preparing another collection.
class MediaPendingWireDatagrams final {
public:
    ::media::Status stage(MediaWireDatagramBatchCollection batches);
    ::media::Status stage(std::shared_ptr<MediaWireDatagramBatchBuffer> terminal);
    bool empty() const noexcept { return m_batches.empty() && !m_terminal; }
    const std::shared_ptr<MediaWireDatagramBatchBuffer>& front() const noexcept
    { return m_batches.empty() ? m_terminal : m_batches.front(); }
    void pop_front() noexcept;
    void clear() noexcept;
private:
    MediaWireDatagramBatchCollection m_batches;
    std::shared_ptr<MediaWireDatagramBatchBuffer> m_terminal;
};
} // namespace media::ffmpeg::graph
