#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"

#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsFramedAccessUnit final {
public:
    MediaTsFramedAccessUnit(const MediaTsFramedAccessUnit&) = delete;
    MediaTsFramedAccessUnit& operator=(const MediaTsFramedAccessUnit&) = delete;
    MediaTsFramedAccessUnit(MediaTsFramedAccessUnit&&) noexcept = default;
    MediaTsFramedAccessUnit& operator=(MediaTsFramedAccessUnit&&) noexcept = default;

    // The caller must keep bytes alive until the synchronous consumer returns.
    static MediaTsFramedAccessUnit borrowed(std::span<const std::uint8_t> bytes);
    static MediaTsFramedAccessUnit owned(std::vector<std::uint8_t> bytes);
    std::span<const std::uint8_t> bytes() const noexcept;

private:
    explicit MediaTsFramedAccessUnit(
        std::variant<std::span<const std::uint8_t>, std::vector<std::uint8_t>> storage);

    std::variant<std::span<const std::uint8_t>, std::vector<std::uint8_t>> m_storage;
};

class MediaTsH264AccessUnitFramer final {
public:
    static ::media::Result<MediaTsFramedAccessUnit> frame(
        const MediaTsMuxPlan& plan,
        const MediaTsMaterializedVideoConfig& config,
        std::span<const std::uint8_t> payload,
        bool randomAccess);
};

} // namespace media::ffmpeg::graph
