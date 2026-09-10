#include "internal/graph/protocol/codec/MediaVideoNalUnitScanner.h"
#include <limits>
#include <new>

namespace media::ffmpeg::graph {
namespace {
std::size_t headerBytes(MediaAnnexBCodec codec) noexcept
{
    return codec == MediaAnnexBCodec::H264 ? 1U : 2U;
}

bool validNal(std::span<const std::uint8_t> nal,
              MediaAnnexBCodec codec) noexcept
{
    if (nal.size() < headerBytes(codec)) return false;
    if (codec == MediaAnnexBCodec::H264) {
        const auto type = nal[0] & 0x1fU;
        return (nal[0] & 0x80U) == 0 && type >= 1U && type <= 23U;
    }
    const auto type = (nal[0] >> 1U) & 0x3fU;
    return (nal[0] & 0x80U) == 0 && type <= 47U &&
        (nal[1] & 0x07U) != 0;
}

std::size_t startCodeSize(std::span<const std::uint8_t> bytes,
                          std::size_t offset) noexcept
{
    if (offset + 4U <= bytes.size() && bytes[offset] == 0 &&
        bytes[offset + 1U] == 0 && bytes[offset + 2U] == 0 &&
        bytes[offset + 3U] == 1) return 4U;
    if (offset + 3U <= bytes.size() && bytes[offset] == 0 &&
        bytes[offset + 1U] == 0 && bytes[offset + 2U] == 1) return 3U;
    return 0;
}

} // namespace

::media::Status MediaVideoNalUnitScanner::visit(
    std::span<const std::uint8_t> bytes, MediaAnnexBCodec codec,
    const MediaEncodedPacketLayout& layout, void* state, Visitor visitor)
{
    if (!visitor || (codec != MediaAnnexBCodec::H264 && codec != MediaAnnexBCodec::Hevc))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("video NAL scan requires a codec and visitor"));
    if (layout.kind() == MediaEncodedPacketLayoutKind::StartCodeDelimited) {
        auto valid = MediaAnnexBAccessUnitValidator::validate(bytes, codec);
        if (!valid) return valid;
        std::size_t cursor = 0;
        while (cursor < bytes.size()) {
            const auto begin = cursor + startCodeSize(bytes, cursor);
            std::size_t end = begin + headerBytes(codec);
            while (end < bytes.size() && startCodeSize(bytes, end) == 0) ++end;
            visitor(state, bytes.subspan(begin, end - begin));
            cursor = end;
        }
        return ::media::Status::success();
    }
    const auto width = layout.lengthFieldBytes();
    if (bytes.empty() || !width || *width == 0 || *width > sizeof(std::uint64_t))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("video NAL length field has invalid geometry"));
    std::size_t cursor = 0;
    while (cursor < bytes.size()) {
        if (*width > bytes.size() - cursor)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("video NAL length field is truncated"));
        std::uint64_t length = 0;
        for (std::uint8_t index = 0; index < *width; ++index)
            length = (length << 8U) | bytes[cursor + index];
        cursor += *width;
        if (!length || length > bytes.size() - cursor || length > (std::numeric_limits<std::size_t>::max)())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("video NAL length exceeds access unit"));
        const auto nal = bytes.subspan(cursor, static_cast<std::size_t>(length));
        if (!validNal(nal, codec))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("video access unit has an invalid NAL header"));
        visitor(state, nal);
        cursor += static_cast<std::size_t>(length);
    }
    return ::media::Status::success();
}

::media::Result<std::vector<std::span<const std::uint8_t>>> MediaVideoNalUnitScanner::scan(
    std::span<const std::uint8_t> bytes, MediaAnnexBCodec codec,
    const MediaEncodedPacketLayout& layout)
{
    using Units = std::vector<std::span<const std::uint8_t>>;
    using Result = ::media::Result<Units>;
    Units units;
    try {
        auto scanned = visit(bytes, codec, layout, &units,
            [](void* state, std::span<const std::uint8_t> unit) { static_cast<Units*>(state)->push_back(unit); });
        if (!scanned) return Result::failure(scanned.error());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("video NAL unit index"));
    }
    return Result::success(std::move(units));
}

} // namespace media::ffmpeg::graph
