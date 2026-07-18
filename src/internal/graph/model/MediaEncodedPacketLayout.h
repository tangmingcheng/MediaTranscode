#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaEncodedPacketLayoutKind : std::uint8_t {
    StartCodeDelimited = 0,
    LengthPrefixed = 1
};

class MediaEncodedPacketLayout final {
public:
    static MediaEncodedPacketLayout startCodeDelimited() noexcept
    {
        return MediaEncodedPacketLayout(
            MediaEncodedPacketLayoutKind::StartCodeDelimited, std::nullopt);
    }

    static ::media::Result<MediaEncodedPacketLayout> lengthPrefixed(
        std::uint8_t lengthFieldBytes)
    {
        if (lengthFieldBytes == 0) {
            return ::media::Result<MediaEncodedPacketLayout>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "length-prefixed encoded packet layout requires an explicit positive length-field width"));
        }
        return ::media::Result<MediaEncodedPacketLayout>::success(
            MediaEncodedPacketLayout(
                MediaEncodedPacketLayoutKind::LengthPrefixed,
                lengthFieldBytes));
    }

    MediaEncodedPacketLayoutKind kind() const noexcept { return m_kind; }
    std::optional<std::uint8_t> lengthFieldBytes() const noexcept
    {
        return m_lengthFieldBytes;
    }

    friend bool operator==(const MediaEncodedPacketLayout&,
                           const MediaEncodedPacketLayout&) = default;

private:
    MediaEncodedPacketLayout(
        MediaEncodedPacketLayoutKind kind,
        std::optional<std::uint8_t> lengthFieldBytes) noexcept
        : m_kind(kind), m_lengthFieldBytes(lengthFieldBytes)
    {
    }

    MediaEncodedPacketLayoutKind m_kind;
    std::optional<std::uint8_t> m_lengthFieldBytes;
};

} // namespace media::ffmpeg::graph
