#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaChannel;

struct MediaAtomicOutputBatch final {
    MediaChannel* channel = nullptr;
    std::span<const MediaBufferRef> buffers;
};

class MediaAtomicOutputTransaction final {
public:
    using AcquireResult =
        ::media::Result<std::optional<MediaAtomicOutputTransaction>>;

    static AcquireResult acquire(
        const char* owner,
        std::span<const MediaAtomicOutputBatch> batches);

    MediaAtomicOutputTransaction(MediaAtomicOutputTransaction&&) noexcept =
        default;
    MediaAtomicOutputTransaction& operator=(
        MediaAtomicOutputTransaction&&) noexcept = default;
    MediaAtomicOutputTransaction(const MediaAtomicOutputTransaction&) = delete;
    MediaAtomicOutputTransaction& operator=(
        const MediaAtomicOutputTransaction&) = delete;

    ::media::Status commit();

private:
    struct OwnedBatch final {
        MediaChannel* channel = nullptr;
        std::vector<MediaBufferRef> buffers;
    };

    MediaAtomicOutputTransaction(
        std::string owner,
        std::vector<OwnedBatch> batches,
        std::vector<std::unique_lock<std::mutex>> locks);

    std::string m_owner;
    std::vector<OwnedBatch> m_batches;
    std::vector<std::unique_lock<std::mutex>> m_locks;
    bool m_committed = false;
};

} // namespace media::ffmpeg::graph
