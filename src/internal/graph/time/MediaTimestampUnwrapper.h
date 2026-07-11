#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaTimestampUnwrapStatus {
    Value,
    Discontinuity
};

enum class MediaTimestampDiscontinuityReason {
    None,
    RawValueOutOfRange,
    BackwardMovement,
    AmbiguousMovement,
    UnwrappedValueOverflow
};

struct MediaTimestampUnwrapResult {
    MediaTimestampUnwrapStatus status = MediaTimestampUnwrapStatus::Discontinuity;
    MediaTimestampDiscontinuityReason reason =
        MediaTimestampDiscontinuityReason::None;
    std::int64_t timestamp = 0;
    std::uint64_t generation = 0;
};

class MediaTimestampUnwrapper final {
public:
    static ::media::Result<MediaTimestampUnwrapper> create(
        std::uint8_t bitWidth,
        std::uint64_t generation);
    static ::media::Result<MediaTimestampUnwrapper> createPcr(
        std::uint64_t generation);

    MediaTimestampUnwrapResult unwrap(std::uint64_t rawTimestamp) noexcept;
    void reset(std::uint64_t generation) noexcept;

    std::uint8_t bitWidth() const noexcept;
    std::uint64_t generation() const noexcept;

private:
    MediaTimestampUnwrapper(std::uint8_t bitWidth,
                            std::uint64_t modulus,
                            std::uint64_t generation) noexcept;

    MediaTimestampUnwrapResult discontinuity(
        MediaTimestampDiscontinuityReason reason) const noexcept;

    std::uint8_t m_bitWidth = 0;
    std::uint64_t m_modulus = 0;
    std::uint64_t m_generation = 0;
    std::uint64_t m_lastRawTimestamp = 0;
    std::int64_t m_lastUnwrappedTimestamp = 0;
    bool m_initialized = false;
};

} // namespace media::ffmpeg::graph
