#include "internal/graph/protocol/MediaUtf8TextValidator.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {
namespace {

bool continuation(std::uint8_t byte) noexcept
{
    return byte >= 0x80 && byte <= 0xBF;
}

bool validUtf8(
    std::string_view text,
    MediaUtf8ControlPolicy controlPolicy) noexcept
{
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto first = static_cast<std::uint8_t>(text[offset]);
        if (first <= 0x7F) {
            if (first < 0x20 || first == 0x7F) {
                const bool allowedCrLf =
                    controlPolicy == MediaUtf8ControlPolicy::AllowCrLf &&
                    (first == '\r' || first == '\n');
                if (!allowedCrLf) return false;
            }
            ++offset;
            continue;
        }
        if (first >= 0xC2 && first <= 0xDF) {
            if (offset + 1 >= text.size()) return false;
            const auto second = static_cast<std::uint8_t>(text[offset + 1]);
            if (!continuation(second) || (first == 0xC2 && second < 0xA0)) {
                return false;
            }
            offset += 2;
            continue;
        }
        if (first >= 0xE0 && first <= 0xEF) {
            if (offset + 2 >= text.size()) return false;
            const auto second = static_cast<std::uint8_t>(text[offset + 1]);
            const auto third = static_cast<std::uint8_t>(text[offset + 2]);
            const bool validSecond = first == 0xE0
                ? second >= 0xA0 && second <= 0xBF
                : first == 0xED
                    ? second >= 0x80 && second <= 0x9F
                    : continuation(second);
            if (!validSecond || !continuation(third)) return false;
            offset += 3;
            continue;
        }
        if (first >= 0xF0 && first <= 0xF4) {
            if (offset + 3 >= text.size()) return false;
            const auto second = static_cast<std::uint8_t>(text[offset + 1]);
            const auto third = static_cast<std::uint8_t>(text[offset + 2]);
            const auto fourth = static_cast<std::uint8_t>(text[offset + 3]);
            const bool validSecond = first == 0xF0
                ? second >= 0x90 && second <= 0xBF
                : first == 0xF4
                    ? second >= 0x80 && second <= 0x8F
                    : continuation(second);
            if (!validSecond || !continuation(third) || !continuation(fourth)) {
                return false;
            }
            offset += 4;
            continue;
        }
        return false;
    }
    return true;
}

} // namespace

::media::Status MediaUtf8TextValidator::validateWellFormed(
    std::string_view text,
    MediaUtf8ControlPolicy controlPolicy)
{
    if (!validUtf8(text, controlPolicy)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "text must contain well-formed UTF-8 with only permitted controls"));
    }
    return ::media::Status::success();
}

::media::Status MediaUtf8TextValidator::validateNonControlText(
    std::string_view text,
    std::size_t maximumBytes,
    std::string_view fieldName)
{
    if (text.empty() || maximumBytes == 0 || text.size() > maximumBytes ||
        !validateWellFormed(text, MediaUtf8ControlPolicy::RejectControls)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fieldName) + " must contain 1 to " +
                std::to_string(maximumBytes) +
                " valid UTF-8 octets without control characters"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
