#pragma once

#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptor.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

class MediaWireDatagramLayoutValidator final {
public:
    explicit MediaWireDatagramLayoutValidator(
        std::uint64_t payloadBytes) noexcept;

    ::media::Status accept(const MediaWireDatagramLayout& descriptor);
    ::media::Status finish() const;
    std::uint64_t generation() const noexcept;

private:
    std::uint64_t m_payloadBytes;
    std::uint64_t m_expectedOffset = 0;
    std::optional<std::uint64_t> m_generation;
    std::optional<MediaRunningTime> m_previousRelease;
    std::optional<MediaRunningTime> m_previousDeadline;
};

class MediaWireDatagramDescriptorValidator final {
public:
    explicit MediaWireDatagramDescriptorValidator(std::uint64_t payloadBytes) noexcept;
    ::media::Status accept(const MediaWireDatagramDescriptor& descriptor);
    ::media::Status finish() const;
    std::uint64_t generation() const noexcept;
private:
    MediaWireDatagramLayoutValidator m_layout;
    std::optional<std::uint64_t> m_previousSequence;
};

} // namespace media::ffmpeg::graph
