#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

struct MediaRawRtpPreparedByteBudgetSnapshot final {
    std::size_t capacity;
    std::size_t observedBytes;
    std::size_t retainedBytes;
    bool runtimeActive;
};

class MediaRawRtpPreparedByteBudget final {
public:
    static ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>
    create(std::size_t capacity);

    ::media::Status observe(std::size_t bytes);
    ::media::Status retain(std::size_t bytes, std::string_view stream);
    ::media::Status observeAndRetain(
        std::size_t bytes, std::string_view stream);
    ::media::Status release(std::size_t bytes);
    ::media::Status fail(::media::ErrorInfo error);
    ::media::Status sealPreflight();
    ::media::Status requireSealed() const;
    ::media::Status validate() const;
    MediaRawRtpPreparedByteBudgetSnapshot snapshot() const noexcept;

private:
    explicit MediaRawRtpPreparedByteBudget(std::size_t capacity) noexcept;
    ::media::Status observeLocked(std::size_t bytes);
    ::media::Status retainLocked(
        std::size_t bytes, std::string_view stream);
    ::media::Status failLocked(::media::ErrorInfo error);

    const std::size_t m_capacity;
    mutable std::mutex m_mutex;
    std::size_t m_observedBytes = 0;
    std::size_t m_retainedBytes = 0;
    bool m_runtimeActive = false;
    std::optional<::media::ErrorInfo> m_error;
};

} // namespace media::ffmpeg::graph
