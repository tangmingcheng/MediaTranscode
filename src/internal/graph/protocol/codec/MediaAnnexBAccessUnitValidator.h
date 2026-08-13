#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaAnnexBCodec : std::uint8_t {
    H264,
    Hevc
};

class MediaAnnexBAccessUnitValidator final {
public:
    static ::media::Status validate(
        std::span<const std::uint8_t> bytes,
        MediaAnnexBCodec codec)
    {
        if (bytes.empty()) return invalid(codec, "is empty");
        std::size_t cursor = 0;
        for (;;) {
            const std::size_t prefix = startCodeSize(bytes, cursor);
            if (prefix == 0) {
                return invalid(codec, "does not start each NAL with a start code");
            }
            const std::size_t nalBegin = cursor + prefix;
            if (nalBegin >= bytes.size()) {
                return invalid(codec, "contains an empty NAL");
            }
            std::size_t next = nalBegin + headerBytes(codec);
            if (next > bytes.size()) {
                return invalid(codec, "contains a truncated NAL header");
            }
            while (next < bytes.size() && startCodeSize(bytes, next) == 0) {
                ++next;
            }
            const std::size_t nalEnd = next < bytes.size() ? next : bytes.size();
            if (!validHeader(bytes.subspan(nalBegin, nalEnd - nalBegin), codec)) {
                return invalid(codec, "contains an invalid NAL header");
            }
            if (nalEnd == bytes.size()) return ::media::Status::success();
            cursor = nalEnd;
        }
    }

private:
    static std::size_t headerBytes(MediaAnnexBCodec codec) noexcept
    {
        return codec == MediaAnnexBCodec::H264 ? 1u : 2u;
    }

    static bool validHeader(
        std::span<const std::uint8_t> nal,
        MediaAnnexBCodec codec) noexcept
    {
        if (codec == MediaAnnexBCodec::H264) {
            const std::uint8_t type = nal[0] & 0x1Fu;
            return (nal[0] & 0x80u) == 0 && type != 0 && type <= 23;
        }
        const std::uint8_t type = (nal[0] >> 1) & 0x3Fu;
        const std::uint8_t temporalIdPlusOne = nal[1] & 0x07u;
        return (nal[0] & 0x80u) == 0 && type <= 47 &&
            temporalIdPlusOne != 0;
    }

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

    static ::media::Status invalid(
        MediaAnnexBCodec codec,
        const char* reason)
    {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(codec == MediaAnnexBCodec::H264 ? "H264" : "HEVC") +
                " Annex-B access unit " + reason));
    }
};

} // namespace media::ffmpeg::graph
