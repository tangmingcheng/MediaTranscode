#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "internal/graph/model/MediaNumericIpAddress.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

class MediaH264SdpCodecDescriptionFactory;
class MediaAacLatmSdpCodecDescriptionFactory;

enum class MediaSdpMediaKind {
    Video,
    Audio
};

class MediaH264SdpCodecDescription final {
public:
    const std::string& profileLevelId() const noexcept { return m_profileLevelId; }
    const std::string& spropParameterSets() const noexcept { return m_spropParameterSets; }
    int packetizationMode() const noexcept { return m_packetizationMode; }

private:
    MediaH264SdpCodecDescription(
        std::string profileLevelId, std::string spropParameterSets,
        int packetizationMode) noexcept;

    friend class MediaH264SdpCodecDescriptionFactory;

    std::string m_profileLevelId;
    std::string m_spropParameterSets;
    int m_packetizationMode;
};

class MediaAacLatmSdpCodecDescription final {
public:
    int sampleRate() const noexcept { return m_sampleRate; }
    int channels() const noexcept { return m_channels; }
    int profileLevelId() const noexcept { return m_profileLevelId; }
    bool configurationPresent() const noexcept { return m_configurationPresent; }
    const std::string& streamMuxConfigHex() const noexcept
    {
        return m_streamMuxConfigHex;
    }

private:
    MediaAacLatmSdpCodecDescription(
        int sampleRate, int channels, int profileLevelId,
        std::string streamMuxConfigHex, bool configurationPresent) noexcept;

    friend class MediaAacLatmSdpCodecDescriptionFactory;

    int m_sampleRate;
    int m_channels;
    int m_profileLevelId;
    std::string m_streamMuxConfigHex;
    bool m_configurationPresent;
};

using MediaSdpCodecDescription = std::variant<
    MediaH264SdpCodecDescription,
    MediaAacLatmSdpCodecDescription>;

class MediaSdpSessionIdentity final {
public:
    static ::media::Result<MediaSdpSessionIdentity> create(
        std::string originUsername,
        std::uint64_t sessionId,
        std::uint64_t sessionVersion,
        std::string sessionName,
        MediaIpAddressFamily addressFamily,
        std::string numericAddress,
        std::string cname);

    const std::string& originUsername() const noexcept { return m_originUsername; }
    std::uint64_t sessionId() const noexcept { return m_sessionId; }
    std::uint64_t sessionVersion() const noexcept { return m_sessionVersion; }
    const std::string& sessionName() const noexcept { return m_sessionName; }
    MediaIpAddressFamily addressFamily() const noexcept
    {
        return m_originAddress.addressFamily();
    }
    const std::string& numericAddress() const noexcept
    {
        return m_originAddress.presentation();
    }
    const std::string& cname() const noexcept { return m_cname; }

private:
    MediaSdpSessionIdentity(
        std::string originUsername,
        std::uint64_t sessionId,
        std::uint64_t sessionVersion,
        std::string sessionName,
        MediaNumericIpAddress originAddress,
        std::string cname) noexcept;

    std::string m_originUsername;
    std::uint64_t m_sessionId;
    std::uint64_t m_sessionVersion;
    std::string m_sessionName;
    MediaNumericIpAddress m_originAddress;
    std::string m_cname;
};

class MediaRtpSdpMediaIdentity final {
public:
    static ::media::Result<MediaRtpSdpMediaIdentity> create(
        MediaSdpMediaKind kind,
        MediaIpAddressFamily addressFamily,
        std::string remoteRtpNumericAddress,
        std::string remoteRtcpNumericAddress,
        std::uint16_t remoteRtpPort,
        std::uint16_t remoteRtcpPort,
        std::uint8_t payloadType,
        std::uint32_t ssrc,
        int clockRate,
        int channels);

    MediaSdpMediaKind kind() const noexcept { return m_kind; }
    MediaIpAddressFamily addressFamily() const noexcept
    {
        return m_remoteRtpAddress.addressFamily();
    }
    const MediaNumericIpAddress& remoteRtpAddress() const noexcept
    {
        return m_remoteRtpAddress;
    }
    const MediaNumericIpAddress& remoteRtcpAddress() const noexcept
    {
        return m_remoteRtcpAddress;
    }
    const std::string& remoteRtpNumericAddress() const noexcept
    {
        return m_remoteRtpAddress.presentation();
    }
    const std::string& remoteRtcpNumericAddress() const noexcept
    {
        return m_remoteRtcpAddress.presentation();
    }
    std::uint16_t remoteRtpPort() const noexcept { return m_remoteRtpPort; }
    std::uint16_t remoteRtcpPort() const noexcept { return m_remoteRtcpPort; }
    std::uint8_t payloadType() const noexcept { return m_payloadType; }
    std::uint32_t ssrc() const noexcept { return m_ssrc; }
    int clockRate() const noexcept { return m_clockRate; }
    int channels() const noexcept { return m_channels; }

private:
    MediaRtpSdpMediaIdentity(
        MediaSdpMediaKind kind,
        MediaNumericIpAddress remoteRtpAddress,
        MediaNumericIpAddress remoteRtcpAddress,
        std::uint16_t remoteRtpPort,
        std::uint16_t remoteRtcpPort,
        std::uint8_t payloadType,
        std::uint32_t ssrc,
        int clockRate,
        int channels) noexcept;

    MediaSdpMediaKind m_kind;
    MediaNumericIpAddress m_remoteRtpAddress;
    MediaNumericIpAddress m_remoteRtcpAddress;
    std::uint16_t m_remoteRtpPort;
    std::uint16_t m_remoteRtcpPort;
    std::uint8_t m_payloadType;
    std::uint32_t m_ssrc;
    int m_clockRate;
    int m_channels;
};

class MediaRtpSdpMediaDescription final {
public:
    static ::media::Result<MediaRtpSdpMediaDescription> create(
        MediaRtpSdpMediaIdentity identity,
        MediaSdpCodecDescription codec);

    const MediaRtpSdpMediaIdentity& identity() const noexcept { return m_identity; }
    const MediaSdpCodecDescription& codec() const noexcept { return m_codec; }

private:
    MediaRtpSdpMediaDescription(
        MediaRtpSdpMediaIdentity identity,
        MediaSdpCodecDescription codec) noexcept;

    MediaRtpSdpMediaIdentity m_identity;
    MediaSdpCodecDescription m_codec;
};

class MediaRtpSdpDescription final {
public:
    static ::media::Result<MediaRtpSdpDescription> create(
        MediaSdpSessionIdentity session,
        std::vector<MediaRtpSdpMediaDescription> media);

    const MediaSdpSessionIdentity& session() const noexcept { return m_session; }
    const std::vector<MediaRtpSdpMediaDescription>& media() const noexcept { return m_media; }

private:
    MediaRtpSdpDescription(
        MediaSdpSessionIdentity session,
        std::vector<MediaRtpSdpMediaDescription> media) noexcept;

    MediaSdpSessionIdentity m_session;
    std::vector<MediaRtpSdpMediaDescription> m_media;
};

} // namespace media::ffmpeg::graph
