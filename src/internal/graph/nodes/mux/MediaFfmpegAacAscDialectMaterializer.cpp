#include "internal/graph/nodes/mux/MediaFfmpegAacAscDialectMaterializer.h"

#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint32_t SyncExtensionType = 0x2B7;
constexpr std::uint32_t SbrAudioObjectType = 5;

::media::Result<std::uint32_t> readBits(
    std::span<const std::uint8_t> bytes,
    std::size_t& bitOffset,
    std::size_t bitCount)
{
    if (bitCount == 0 || bitCount > 32 ||
        bitOffset > bytes.size() * 8 ||
        bitCount > bytes.size() * 8 - bitOffset) {
        return ::media::Result<std::uint32_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg AAC ASC sync extension is truncated"));
    }
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < bitCount; ++index) {
        const std::size_t absolute = bitOffset + index;
        value = (value << 1) |
            ((bytes[absolute / 8] >> (7 - absolute % 8)) & 1u);
    }
    bitOffset += bitCount;
    return ::media::Result<std::uint32_t>::success(value);
}

::media::ErrorInfo invalidDialect(const char* reason)
{
    return ::media::ErrorInfo::invalidArgument(
        std::string("FFmpeg AAC ASC dialect ") + reason);
}

} // namespace

::media::Result<std::array<std::uint8_t, 2>>
MediaFfmpegAacAscDialectMaterializer::canonicalize(
    std::span<const std::uint8_t> bytes)
{
    using Result = ::media::Result<std::array<std::uint8_t, 2>>;
    if (bytes.size() == 2) {
        return Result::success(
            std::array<std::uint8_t, 2>{bytes[0], bytes[1]});
    }
    if (bytes.size() != 5) {
        return Result::failure(invalidDialect(
            "must be strict two-byte ASC or exact five-byte no-SBR form"));
    }
    std::size_t bitOffset = 16;
    auto syncExtension = readBits(bytes, bitOffset, 11);
    auto extensionObjectType = readBits(bytes, bitOffset, 5);
    auto sbrPresent = readBits(bytes, bitOffset, 1);
    auto trailingPadding = readBits(bytes, bitOffset, 7);
    if (!syncExtension || !extensionObjectType || !sbrPresent ||
        !trailingPadding || bitOffset != bytes.size() * 8 ||
        syncExtension.value() != SyncExtensionType ||
        extensionObjectType.value() != SbrAudioObjectType ||
        sbrPresent.value() != 0 || trailingPadding.value() != 0) {
        return Result::failure(invalidDialect(
            "has invalid sync extension, SBR signaling, or trailing bits"));
    }
    return Result::success(
        std::array<std::uint8_t, 2>{bytes[0], bytes[1]});
}

} // namespace media::ffmpeg::graph
