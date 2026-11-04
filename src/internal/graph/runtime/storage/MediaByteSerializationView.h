#pragma once
#include "media_transcode/Result.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <span>

namespace media::ffmpeg::graph {
// Synchronous borrow only. Neither this view nor its source may escape preparation.
class MediaByteSerializationView final {
public:
    static MediaByteSerializationView fromBytes(std::span<const std::uint8_t> bytes) noexcept
    {
        return {bytes.data(), bytes.size(), [](const void* source, std::span<std::uint8_t> output) {
            if (!output.empty()) std::memmove(output.data(), source, output.size());
            return ::media::Status::success();
        }};
    }
    template<auto Serialize, class Plan>
    static MediaByteSerializationView fromPlan(std::reference_wrapper<const Plan> borrowed) noexcept
    {
        const auto& plan = borrowed.get();
        static_assert(noexcept(plan.bytes()));
        return {std::addressof(plan), plan.bytes(), [](const void* source, std::span<std::uint8_t> output) {
            return Serialize(*static_cast<const Plan*>(source), output);
        }};
    }
    std::size_t size() const noexcept { return m_bytes; }
    ::media::Status writeInto(std::span<std::uint8_t> output) const
    {
        if (output.size() != m_bytes || (m_bytes && !m_source))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Serialized byte output differs from its borrowed source"));
        return m_write(m_source, output);
    }
private:
    using Write = ::media::Status (*)(const void*, std::span<std::uint8_t>);
    MediaByteSerializationView(const void* source, std::size_t bytes, Write write) noexcept
        : m_source(source), m_bytes(bytes), m_write(write) {}
    const void* m_source;
    std::size_t m_bytes;
    Write m_write;
};
} // namespace media::ffmpeg::graph
