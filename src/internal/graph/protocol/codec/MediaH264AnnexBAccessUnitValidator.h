#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace media::ffmpeg::graph {

class MediaH264AnnexBAccessUnitValidator final {
public:
    static ::media::Status validate(
        std::span<const std::uint8_t> bytes)
    {
        if (bytes.empty()) return invalid("is empty");
        std::size_t cursor = 0;
        for (;;) {
            const std::size_t prefix = startCodeSize(bytes, cursor);
            if (prefix == 0) return invalid("does not start each NAL with a start code");
            const std::size_t nalBegin = cursor + prefix;
            if (nalBegin >= bytes.size()) return invalid("contains an empty NAL");
            std::size_t next = nalBegin + 1;
            while (next < bytes.size() && startCodeSize(bytes, next) == 0) {
                ++next;
            }
            const std::size_t nalEnd = next < bytes.size() ? next : bytes.size();
            const std::uint8_t header = bytes[nalBegin];
            const std::uint8_t type = header & 0x1Fu;
            if ((header & 0x80u) != 0 || type == 0 || type > 23) {
                return invalid("contains an invalid NAL header");
            }
            if (nalEnd == bytes.size()) return ::media::Status::success();
            cursor = nalEnd;
        }
    }

private:
    static std::size_t startCodeSize(
        std::span<const std::uint8_t> bytes,
        std::size_t offset) noexcept
    {
        if (offset + 4 <= bytes.size() && bytes[offset] == 0 &&
            bytes[offset + 1] == 0 && bytes[offset + 2] == 0 &&
            bytes[offset + 3] == 1) {
            return 4;
        }
        if (offset + 3 <= bytes.size() && bytes[offset] == 0 &&
            bytes[offset + 1] == 0 && bytes[offset + 2] == 1) {
            return 3;
        }
        return 0;
    }

    static ::media::Status invalid(const char* reason)
    {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("H264 Annex-B access unit ") + reason));
    }
};

} // namespace media::ffmpeg::graph
