#pragma once

#include "media_transcode/Result.h"
#include <array>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

enum class MediaDatagramProtocolEndpointRole : std::uint8_t {
    Unknown = 0, VideoRtp = 1, VideoRtcp = 2, AudioRtp = 3,
    AudioRtcp = 4, MpegTsUdp = 5, MpegTsRtp = 6, MpegTsRtcp = 7
};

enum class MediaDatagramProtocolLayout : std::uint8_t {
    Unknown = 0, VideoRtp = 1, SeparateAvRtp = 2,
    MpegTsRtp = 3, MpegTsUdp = 4
};

struct MediaDatagramRtpProtocolRoles final {
    MediaDatagramProtocolEndpointRole rtp;
    MediaDatagramProtocolEndpointRole rtcp;
};

struct MediaDatagramProtocolTopology final {
    std::span<const MediaDatagramProtocolEndpointRole> endpointRoles;
    std::span<const MediaDatagramRtpProtocolRoles> rtpInstances;
};

inline ::media::Result<MediaDatagramProtocolTopology> mediaDatagramProtocolTopology(
    MediaDatagramProtocolLayout layout)
{
    using Role = MediaDatagramProtocolEndpointRole;
    using Result = ::media::Result<MediaDatagramProtocolTopology>;
    static constexpr std::array video{Role::VideoRtp, Role::VideoRtcp};
    static constexpr std::array av{Role::VideoRtp, Role::VideoRtcp,
                                    Role::AudioRtp, Role::AudioRtcp};
    static constexpr std::array tsRtp{Role::MpegTsRtp, Role::MpegTsRtcp};
    static constexpr std::array tsUdp{Role::MpegTsUdp};
    static constexpr std::array videoInstances{
        MediaDatagramRtpProtocolRoles{Role::VideoRtp, Role::VideoRtcp}};
    static constexpr std::array avInstances{
        MediaDatagramRtpProtocolRoles{Role::VideoRtp, Role::VideoRtcp},
        MediaDatagramRtpProtocolRoles{Role::AudioRtp, Role::AudioRtcp}};
    static constexpr std::array tsInstances{
        MediaDatagramRtpProtocolRoles{Role::MpegTsRtp, Role::MpegTsRtcp}};
    static constexpr std::array<MediaDatagramRtpProtocolRoles, 0> noRtpInstances{};
    switch (layout) {
    case MediaDatagramProtocolLayout::VideoRtp: return Result::success({video, videoInstances});
    case MediaDatagramProtocolLayout::SeparateAvRtp: return Result::success({av, avInstances});
    case MediaDatagramProtocolLayout::MpegTsRtp: return Result::success({tsRtp, tsInstances});
    case MediaDatagramProtocolLayout::MpegTsUdp: return Result::success({tsUdp, noRtpInstances});
    default: return Result::failure(::media::ErrorInfo::invalidArgument(
        "Datagram protocol layout is unknown or unsupported"));
    }
}

} // namespace media::ffmpeg::graph
