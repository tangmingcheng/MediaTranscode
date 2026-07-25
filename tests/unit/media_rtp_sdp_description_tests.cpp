#include "common/TestAssert.h"

#include "internal/graph/protocol/sdp/MediaAacLatmSdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaH264SdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpDescription.h"
#include "internal/graph/protocol/sdp/MediaRtpSdpSerializer.h"
#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
}

#include <cstdint>
#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

static_assert(!std::is_default_constructible_v<MediaSdpSessionIdentity>);
static_assert(!std::is_default_constructible_v<MediaRtpSdpMediaIdentity>);
static_assert(!std::is_default_constructible_v<MediaRtpSdpDescription>);
static_assert(!std::is_default_constructible_v<MediaH264SdpCodecDescription>);
static_assert(!std::is_default_constructible_v<MediaAacLatmSdpCodecDescription>);

template <typename T>
concept ExposesH264CodecFactConstruction = requires {
    T::create(std::string{}, std::string{}, 1);
};

template <typename T>
concept ExposesAacCodecFactConstruction = requires {
    T::create(48'000, 2, 41, std::string{}, false);
};

static_assert(!ExposesH264CodecFactConstruction<MediaH264SdpCodecDescription>);
static_assert(!ExposesAacCodecFactConstruction<MediaAacLatmSdpCodecDescription>);

::media::ffmpeg::CodecParametersPtr makeParameters(
    AVMediaType mediaType,
    AVCodecID codecId,
    const std::vector<std::uint8_t>& extradata)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) return {};
    parameters->codec_type = mediaType;
    parameters->codec_id = codecId;
    if (!extradata.empty()) {
        parameters->extradata = static_cast<std::uint8_t*>(
            av_mallocz(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!parameters->extradata) return {};
        std::copy(extradata.begin(), extradata.end(), parameters->extradata);
        parameters->extradata_size = static_cast<int>(extradata.size());
    }
    return parameters;
}

std::vector<std::uint8_t> avccExtradata()
{
    return {
        1, 0x64, 0x00, 0x1f, 0xff, 0xe1,
        0x00, 0x08, 0x67, 0x64, 0x00, 0x1f, 0xac, 0xd9, 0x40, 0x50,
        0x01, 0x00, 0x04, 0x68, 0xee, 0x3c, 0x80,
        0xfd, 0xf8, 0xf8, 0x00
    };
}

void testH264AvccAndAnnexBProduceAllParameterSets(TestContext& ctx)
{
    auto avcc = makeParameters(AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, avccExtradata());
    EXPECT_TRUE(ctx, avcc != nullptr);
    if (!avcc) return;
    auto fromAvcc = MediaH264SdpCodecDescriptionFactory::create(*avcc);
    EXPECT_TRUE(ctx, fromAvcc);
    if (fromAvcc) {
        EXPECT_EQ(ctx, fromAvcc.value().profileLevelId(), std::string("64001F"));
        EXPECT_EQ(ctx, fromAvcc.value().spropParameterSets(),
                  std::string("Z2QAH6zZQFA=,aO48gA=="));
        EXPECT_EQ(ctx, fromAvcc.value().packetizationMode(), 1);
    }

    const std::vector<std::uint8_t> annexB{
        0, 0, 0, 0, 1, 0x67, 0x64, 0x00, 0x1f, 0xac, 0xd9, 0x40, 0x50,
        0, 0, 1, 0x68, 0xee, 0x3c, 0x80, 0, 0};
    auto annexParameters = makeParameters(AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, annexB);
    EXPECT_TRUE(ctx, annexParameters != nullptr);
    if (!annexParameters) return;
    auto fromAnnexB = MediaH264SdpCodecDescriptionFactory::create(*annexParameters);
    EXPECT_TRUE(ctx, fromAnnexB);
    if (fromAnnexB) {
        EXPECT_EQ(ctx, fromAnnexB.value().profileLevelId(), std::string("64001F"));
        EXPECT_EQ(ctx, fromAnnexB.value().spropParameterSets(),
                  std::string("Z2QAH6zZQFA=,aO48gA=="));
    }

    const std::vector<std::uint8_t> multiple{
        1, 0x64, 0, 0x1f, 0xff, 0xe2,
        0, 8, 0x67, 0x64, 0, 0x1f, 0xac, 0xd9, 0x40, 0x50,
        0, 5, 0x67, 0x64, 0, 0x1f, 0x01,
        2,
        0, 4, 0x68, 0xee, 0x3c, 0x80,
        0, 2, 0x68, 0x01};
    auto multipleParameters = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, multiple);
    auto multipleDescription =
        MediaH264SdpCodecDescriptionFactory::create(*multipleParameters);
    EXPECT_TRUE(ctx, multipleDescription);
    if (multipleDescription) {
        EXPECT_EQ(ctx, multipleDescription.value().spropParameterSets(),
                  std::string("Z2QAH6zZQFA=,Z2QAHwE=,aO48gA==,aAE="));
    }
}

void testH264RejectsMalformedOrInconsistentExtradata(TestContext& ctx)
{
    auto malformedReserved = avccExtradata();
    malformedReserved[4] = 0x03;
    auto malformed = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, malformedReserved);
    EXPECT_FALSE(ctx, MediaH264SdpCodecDescriptionFactory::create(*malformed));

    auto mismatchedHeader = avccExtradata();
    mismatchedHeader[1] = 0x42;
    auto mismatched = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, mismatchedHeader);
    EXPECT_FALSE(ctx, MediaH264SdpCodecDescriptionFactory::create(*mismatched));

    auto emptyNal = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264,
        {0, 0, 1, 0, 0, 1, 0x67, 0x64, 0, 0x1f, 0, 0, 1, 0x68, 1});
    EXPECT_FALSE(ctx, MediaH264SdpCodecDescriptionFactory::create(*emptyNal));

    auto missingPps = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264,
        {0, 0, 1, 0x67, 0x64, 0, 0x1f});
    EXPECT_FALSE(ctx, MediaH264SdpCodecDescriptionFactory::create(*missingPps));

    auto wrongCodec = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_HEVC, avccExtradata());
    EXPECT_FALSE(ctx, MediaH264SdpCodecDescriptionFactory::create(*wrongCodec));

    auto forbiddenBit = avccExtradata();
    forbiddenBit[8] |= 0x80;
    auto forbidden = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, forbiddenBit);
    EXPECT_FALSE(ctx, MediaH264SdpCodecDescriptionFactory::create(*forbidden));

}

void testAacLatmUsesStreamMuxConfigGoldenVectors(TestContext& ctx)
{
    auto makeAac = [](int sampleRate, int channels, std::vector<std::uint8_t> asc) {
        auto parameters = makeParameters(AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC, asc);
        if (parameters) {
            parameters->sample_rate = sampleRate;
            av_channel_layout_default(&parameters->ch_layout, channels);
        }
        return parameters;
    };
    auto aac48 = makeAac(48'000, 2, {0x11, 0x90});
    auto aac44 = makeAac(44'100, 2, {0x12, 0x10});
    EXPECT_TRUE(ctx, aac48 != nullptr && aac44 != nullptr);
    if (!aac48 || !aac44) return;
    auto latm48 = MediaAacLatmSdpCodecDescriptionFactory::create(*aac48);
    auto latm44 = MediaAacLatmSdpCodecDescriptionFactory::create(*aac44);
    EXPECT_TRUE(ctx, latm48 && latm44);
    if (latm48) {
        EXPECT_EQ(ctx, latm48.value().sampleRate(), 48'000);
        EXPECT_EQ(ctx, latm48.value().channels(), 2);
        EXPECT_EQ(ctx, latm48.value().profileLevelId(), 41);
        EXPECT_EQ(ctx, latm48.value().streamMuxConfigHex(), std::string("400023203FC0"));
        EXPECT_FALSE(ctx, latm48.value().configurationPresent());
    }
    if (latm44) {
        EXPECT_EQ(ctx, latm44.value().profileLevelId(), 41);
        EXPECT_EQ(ctx, latm44.value().streamMuxConfigHex(), std::string("400024203FC0"));
    }

    auto aac24 = makeAac(24'000, 1, {0x13, 0x08});
    auto aac32 = makeAac(32'000, 2, {0x12, 0x90});
    auto latm24 = MediaAacLatmSdpCodecDescriptionFactory::create(*aac24);
    auto latm32 = MediaAacLatmSdpCodecDescriptionFactory::create(*aac32);
    EXPECT_TRUE(ctx, latm24 && latm32);
    if (latm24) EXPECT_EQ(ctx, latm24.value().profileLevelId(), 40);
    if (latm32) EXPECT_EQ(ctx, latm32.value().profileLevelId(), 41);

    auto surround = makeAac(48'000, 6, {0x11, 0xb0});
    auto tooFast = makeAac(96'000, 2, {0x10, 0x10});
    auto mismatch = makeAac(44'100, 2, {0x11, 0x90});
    auto generic = makeParameters(AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC, {0x11, 0x90});
    generic->sample_rate = 48'000;
    auto shortFrames = makeAac(48'000, 2, {0x11, 0x94});
    EXPECT_FALSE(ctx, MediaAacLatmSdpCodecDescriptionFactory::create(*surround));
    EXPECT_FALSE(ctx, MediaAacLatmSdpCodecDescriptionFactory::create(*tooFast));
    EXPECT_FALSE(ctx, MediaAacLatmSdpCodecDescriptionFactory::create(*mismatch));
    EXPECT_FALSE(ctx, MediaAacLatmSdpCodecDescriptionFactory::create(*generic));
    EXPECT_FALSE(ctx, MediaAacLatmSdpCodecDescriptionFactory::create(*shortFrames));
}

::media::Result<MediaSdpSessionIdentity> makeSession(
    MediaIpAddressFamily family,
    std::string address)
{
    return MediaSdpSessionIdentity::create(
        "encoder", 1'700'000'000ULL, 7, "MediaTranscode RTP",
        family, std::move(address), "sync-group@example");
}

::media::Result<MediaRtpSdpMediaIdentity> makeMedia(
    MediaSdpMediaKind kind,
    MediaIpAddressFamily family,
    std::string address,
    std::string rtcpAddress,
    std::uint16_t rtpPort,
    std::uint16_t rtcpPort,
    std::uint8_t payloadType,
    std::uint32_t ssrc,
    int clockRate,
    int channels)
{
    return MediaRtpSdpMediaIdentity::create(
        kind, family, std::move(address), std::move(rtcpAddress), rtpPort, rtcpPort,
        payloadType, ssrc, clockRate, channels);
}

void testTypedDescriptionAndExactSerialization(TestContext& ctx)
{
    auto h264Parameters = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, avccExtradata());
    auto aacParameters = makeParameters(
        AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC, {0x11, 0x90});
    aacParameters->sample_rate = 48'000;
    av_channel_layout_default(&aacParameters->ch_layout, 2);
    auto h264 = MediaH264SdpCodecDescriptionFactory::create(*h264Parameters);
    auto aac = MediaAacLatmSdpCodecDescriptionFactory::create(*aacParameters);
    auto session = makeSession(MediaIpAddressFamily::Ipv4, "192.0.2.1");
    auto video = makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                           "192.0.2.2", "192.0.2.12", 5004, 7001, 96, 0x11223344u, 90'000, 0);
    auto audio = makeMedia(MediaSdpMediaKind::Audio, MediaIpAddressFamily::Ipv4,
                           "192.0.2.3", "192.0.2.13", 6004, 8001, 97, 0x55667788u, 48'000, 2);
    EXPECT_TRUE(ctx, h264 && aac && session && video && audio);
    if (!h264 || !aac || !session || !video || !audio) return;
    std::vector<MediaRtpSdpMediaDescription> media;
    auto videoDescription = MediaRtpSdpMediaDescription::create(
        std::move(video.value()), std::move(h264.value()));
    auto audioDescription = MediaRtpSdpMediaDescription::create(
        std::move(audio.value()), std::move(aac.value()));
    EXPECT_TRUE(ctx, videoDescription && audioDescription);
    if (!videoDescription || !audioDescription) return;
    media.emplace_back(std::move(videoDescription.value()));
    media.emplace_back(std::move(audioDescription.value()));
    auto description = MediaRtpSdpDescription::create(
        std::move(session.value()), std::move(media));
    EXPECT_TRUE(ctx, description);
    if (!description) return;
    auto serialized = MediaRtpSdpSerializer::serialize(description.value());
    EXPECT_TRUE(ctx, serialized);
    if (!serialized) return;
    const std::string expected =
        "v=0\r\n"
        "o=encoder 1700000000 7 IN IP4 192.0.2.1\r\n"
        "s=MediaTranscode RTP\r\n"
        "t=0 0\r\n"
        "m=video 5004 RTP/AVP 96\r\n"
        "c=IN IP4 192.0.2.2\r\n"
        "a=rtcp:7001 IN IP4 192.0.2.12\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=64001F;sprop-parameter-sets=Z2QAH6zZQFA=,aO48gA==\r\n"
        "a=ssrc:287454020 cname:sync-group@example\r\n"
        "m=audio 6004 RTP/AVP 97\r\n"
        "c=IN IP4 192.0.2.3\r\n"
        "a=rtcp:8001 IN IP4 192.0.2.13\r\n"
        "a=rtpmap:97 MP4A-LATM/48000/2\r\n"
        "a=fmtp:97 profile-level-id=41;cpresent=0;config=400023203FC0\r\n"
        "a=ssrc:1432778632 cname:sync-group@example\r\n";
    EXPECT_EQ(ctx, serialized.value(), expected);
    EXPECT_TRUE(ctx, serialized.value().find('\0') == std::string::npos);
    EXPECT_TRUE(ctx, serialized.value().size() >= 2 &&
                     serialized.value().substr(serialized.value().size() - 2) == "\r\n");
}

void testIpv6AndContractRejections(TestContext& ctx)
{
    const std::string embeddedNulIpv4("127.0.0.1\0INJECT", 16);
    EXPECT_FALSE(ctx, MediaNumericIpAddress::create(
        MediaIpAddressFamily::Ipv4, embeddedNulIpv4));
    auto session = makeSession(MediaIpAddressFamily::Ipv6, "2001:db8::1");
    EXPECT_TRUE(ctx, session);
    EXPECT_FALSE(ctx, makeSession(MediaIpAddressFamily::Ipv4, "ff15::1"));
    EXPECT_FALSE(ctx, makeSession(MediaIpAddressFamily::Ipv4, "239.20.0.1"));
    EXPECT_FALSE(ctx, makeSession(MediaIpAddressFamily::Ipv6, "ff15::1"));
    EXPECT_FALSE(ctx, makeSession(
        static_cast<MediaIpAddressFamily>(2), "::1"));
    EXPECT_FALSE(ctx, makeSession(
        MediaIpAddressFamily::Ipv4, embeddedNulIpv4));
    EXPECT_FALSE(ctx, MediaSdpSessionIdentity::create(
        "bad user", 1, 1, "name", MediaIpAddressFamily::Ipv4,
        "127.0.0.1", "cname"));
    EXPECT_FALSE(ctx, MediaSdpSessionIdentity::create(
        "user", 1, 1, "bad\r\nname", MediaIpAddressFamily::Ipv4,
        "127.0.0.1", "cname"));
    EXPECT_FALSE(ctx, MediaSdpSessionIdentity::create(
        "user", 1, 1, "name", MediaIpAddressFamily::Ipv4,
        "127.0.0.1", std::string("bad\0cname", 9)));
    EXPECT_FALSE(ctx, MediaSdpSessionIdentity::create(
        "user", 1, 1, std::string("bad\xC3\x28", 5),
        MediaIpAddressFamily::Ipv4, "127.0.0.1", "cname"));

    EXPECT_FALSE(ctx, makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                                "127.0.0.1", "127.0.0.1", 5004, 5004, 96, 1, 90'000, 0));
    EXPECT_FALSE(ctx, makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                                "127.0.0.1", "127.0.0.1", 0, 5005, 96, 1, 90'000, 0));
    EXPECT_FALSE(ctx, makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                                "127.0.0.1", "127.0.0.1", 5004, 5005, 95, 1, 90'000, 0));
    EXPECT_FALSE(ctx, makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                                "127.0.0.1", "127.0.0.1", 5004, 5005, 96, 0, 90'000, 0));
    EXPECT_FALSE(ctx, makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                                "127.0.0.1", "127.0.0.1", 5004, 5005, 96, 1, 48'000, 0));
    EXPECT_FALSE(ctx, makeMedia(MediaSdpMediaKind::Audio, MediaIpAddressFamily::Ipv4,
                                "127.0.0.1", "127.0.0.1", 6004, 6005, 97, 2, 48'000, 0));
    EXPECT_FALSE(ctx, makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                                "239.20.0.2", "239.20.0.12", 5004, 5005,
                                96, 1, 90'000, 0));
    EXPECT_TRUE(ctx, makeMedia(MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
                               "192.0.2.2", "192.0.2.12", 5004, 5004,
                               96, 1, 90'000, 0));
    EXPECT_FALSE(ctx, makeMedia(
        MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv6,
        "::1", "0:0:0:0:0:0:0:1", 5004, 5004,
        96, 1, 90'000, 0));
    EXPECT_FALSE(ctx, makeMedia(
        MediaSdpMediaKind::Video, static_cast<MediaIpAddressFamily>(2),
        "::1", "::2", 5004, 5005, 96, 1, 90'000, 0));
    EXPECT_FALSE(ctx, makeMedia(
        MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv4,
        embeddedNulIpv4, "127.0.0.2", 5004, 5005,
        96, 1, 90'000, 0));
}

void testIpv6VideoOnlyExactSerialization(TestContext& ctx)
{
    auto parameters = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, avccExtradata());
    auto codec = MediaH264SdpCodecDescriptionFactory::create(*parameters);
    auto identity = makeMedia(
        MediaSdpMediaKind::Video, MediaIpAddressFamily::Ipv6,
        "ff15::2", "ff15::12", 5004, 9001, 96, 1234, 90'000, 0);
    auto session = makeSession(MediaIpAddressFamily::Ipv6, "2001:db8::1");
    EXPECT_TRUE(ctx, codec && identity && session);
    if (!codec || !identity || !session) return;
    auto media = MediaRtpSdpMediaDescription::create(
        std::move(identity.value()), std::move(codec.value()));
    EXPECT_TRUE(ctx, media);
    if (!media) return;
    std::vector<MediaRtpSdpMediaDescription> items;
    items.emplace_back(std::move(media.value()));
    auto description = MediaRtpSdpDescription::create(
        std::move(session.value()), std::move(items));
    EXPECT_TRUE(ctx, description);
    if (!description) return;
    auto serialized = MediaRtpSdpSerializer::serialize(description.value());
    EXPECT_TRUE(ctx, serialized);
    if (!serialized) return;
    EXPECT_EQ(ctx, serialized.value(),
        std::string(
            "v=0\r\n"
            "o=encoder 1700000000 7 IN IP6 2001:db8::1\r\n"
            "s=MediaTranscode RTP\r\n"
            "t=0 0\r\n"
            "m=video 5004 RTP/AVP 96\r\n"
            "c=IN IP6 ff15::2\r\n"
            "a=rtcp:9001 IN IP6 ff15::12\r\n"
            "a=rtpmap:96 H264/90000\r\n"
            "a=fmtp:96 packetization-mode=1;profile-level-id=64001F;sprop-parameter-sets=Z2QAH6zZQFA=,aO48gA==\r\n"
            "a=ssrc:1234 cname:sync-group@example\r\n"));
}

void testDescriptionTopologyIsCompleteAndCollisionFree(TestContext& ctx)
{
    auto h264Parameters = makeParameters(
        AVMEDIA_TYPE_VIDEO, AV_CODEC_ID_H264, avccExtradata());
    auto aacParameters = makeParameters(
        AVMEDIA_TYPE_AUDIO, AV_CODEC_ID_AAC, {0x11, 0x90});
    aacParameters->sample_rate = 48'000;
    av_channel_layout_default(&aacParameters->ch_layout, 2);

    auto makeVideoDescription = [&](std::string address, std::string rtcpAddress,
                                    std::uint16_t rtp, std::uint16_t rtcp,
                                    std::uint8_t payloadType, std::uint32_t ssrc,
                                    MediaIpAddressFamily family) {
        auto codec = MediaH264SdpCodecDescriptionFactory::create(*h264Parameters);
        auto identity = makeMedia(
            MediaSdpMediaKind::Video, family, std::move(address),
            std::move(rtcpAddress), rtp, rtcp, payloadType, ssrc, 90'000, 0);
        if (!codec) {
            return ::media::Result<MediaRtpSdpMediaDescription>::failure(codec.error());
        }
        if (!identity) {
            return ::media::Result<MediaRtpSdpMediaDescription>::failure(identity.error());
        }
        return MediaRtpSdpMediaDescription::create(
            std::move(identity.value()), std::move(codec.value()));
    };
    auto makeAudioDescription = [&](std::string address, std::string rtcpAddress,
                                    std::uint16_t rtp, std::uint16_t rtcp,
                                    std::uint8_t payloadType, std::uint32_t ssrc) {
        auto codec = MediaAacLatmSdpCodecDescriptionFactory::create(*aacParameters);
        auto identity = makeMedia(
            MediaSdpMediaKind::Audio, MediaIpAddressFamily::Ipv4,
            std::move(address), std::move(rtcpAddress), rtp, rtcp,
            payloadType, ssrc, 48'000, 2);
        if (!codec) {
            return ::media::Result<MediaRtpSdpMediaDescription>::failure(codec.error());
        }
        if (!identity) {
            return ::media::Result<MediaRtpSdpMediaDescription>::failure(identity.error());
        }
        return MediaRtpSdpMediaDescription::create(
            std::move(identity.value()), std::move(codec.value()));
    };

    auto session = makeSession(MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto video = makeVideoDescription(
        "127.0.0.1", "127.0.0.1", 5004, 7001, 96, 10,
        MediaIpAddressFamily::Ipv4);
    EXPECT_TRUE(ctx, session && video);
    if (!session || !video) return;
    std::vector<MediaRtpSdpMediaDescription> videoOnly;
    videoOnly.emplace_back(std::move(video.value()));
    EXPECT_TRUE(ctx, MediaRtpSdpDescription::create(
        std::move(session.value()), std::move(videoOnly)));

    auto wrongFamilySession = makeSession(MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto ipv6Video = makeVideoDescription(
        "::1", "::1", 5004, 7001, 96, 10, MediaIpAddressFamily::Ipv6);
    EXPECT_TRUE(ctx, wrongFamilySession && ipv6Video);
    if (!wrongFamilySession || !ipv6Video) return;
    std::vector<MediaRtpSdpMediaDescription> wrongFamily;
    wrongFamily.emplace_back(std::move(ipv6Video.value()));
    EXPECT_FALSE(ctx, MediaRtpSdpDescription::create(
        std::move(wrongFamilySession.value()), std::move(wrongFamily)));

    auto audio = makeAudioDescription(
        "127.0.0.1", "127.0.0.1", 7001, 8001, 97, 11);
    auto collisionSession = makeSession(MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto collisionVideo = makeVideoDescription(
        "127.0.0.1", "127.0.0.1", 5004, 7001, 96, 10,
        MediaIpAddressFamily::Ipv4);
    EXPECT_TRUE(ctx, audio && collisionSession && collisionVideo);
    if (!audio || !collisionSession || !collisionVideo) return;
    std::vector<MediaRtpSdpMediaDescription> collision;
    collision.emplace_back(std::move(collisionVideo.value()));
    collision.emplace_back(std::move(audio.value()));
    EXPECT_FALSE(ctx, MediaRtpSdpDescription::create(
        std::move(collisionSession.value()), std::move(collision)));

    auto distinctAddressSession = makeSession(
        MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto distinctAddressVideo = makeVideoDescription(
        "127.0.0.1", "127.0.0.1", 5004, 7001, 96, 10,
        MediaIpAddressFamily::Ipv4);
    auto distinctAddressAudio = makeAudioDescription(
        "127.0.0.2", "127.0.0.2", 7001, 8001, 97, 11);
    EXPECT_TRUE(ctx, distinctAddressSession && distinctAddressVideo &&
                     distinctAddressAudio);
    if (!distinctAddressSession || !distinctAddressVideo ||
        !distinctAddressAudio) return;
    std::vector<MediaRtpSdpMediaDescription> distinctEndpoints;
    distinctEndpoints.emplace_back(std::move(distinctAddressVideo.value()));
    distinctEndpoints.emplace_back(std::move(distinctAddressAudio.value()));
    EXPECT_TRUE(ctx, MediaRtpSdpDescription::create(
        std::move(distinctAddressSession.value()), std::move(distinctEndpoints)));

    auto reverseSession = makeSession(MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto reverseVideo = makeVideoDescription(
        "127.0.0.1", "127.0.0.1", 5004, 5005, 96, 10,
        MediaIpAddressFamily::Ipv4);
    auto reverseAudio = makeAudioDescription(
        "127.0.0.1", "127.0.0.1", 6004, 6005, 97, 11);
    EXPECT_TRUE(ctx, reverseSession && reverseVideo && reverseAudio);
    if (!reverseSession || !reverseVideo || !reverseAudio) return;
    std::vector<MediaRtpSdpMediaDescription> reverse;
    reverse.emplace_back(std::move(reverseAudio.value()));
    reverse.emplace_back(std::move(reverseVideo.value()));
    EXPECT_FALSE(ctx, MediaRtpSdpDescription::create(
        std::move(reverseSession.value()), std::move(reverse)));

    auto audioOnlySession = makeSession(MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto audioOnlyDescription = makeAudioDescription(
        "127.0.0.1", "127.0.0.1", 6004, 6005, 97, 11);
    EXPECT_TRUE(ctx, audioOnlySession && audioOnlyDescription);
    if (!audioOnlySession || !audioOnlyDescription) return;
    std::vector<MediaRtpSdpMediaDescription> audioOnly;
    audioOnly.emplace_back(std::move(audioOnlyDescription.value()));
    EXPECT_FALSE(ctx, MediaRtpSdpDescription::create(
        std::move(audioOnlySession.value()), std::move(audioOnly)));

    auto duplicatePayloadSession = makeSession(
        MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto duplicatePayloadVideo = makeVideoDescription(
        "127.0.0.1", "127.0.0.1", 5004, 5005, 96, 10,
        MediaIpAddressFamily::Ipv4);
    auto duplicatePayloadAudio = makeAudioDescription(
        "127.0.0.1", "127.0.0.1", 6004, 6005, 96, 11);
    EXPECT_TRUE(ctx, duplicatePayloadSession && duplicatePayloadVideo &&
                     duplicatePayloadAudio);
    if (!duplicatePayloadSession || !duplicatePayloadVideo ||
        !duplicatePayloadAudio) return;
    std::vector<MediaRtpSdpMediaDescription> duplicatePayload;
    duplicatePayload.emplace_back(std::move(duplicatePayloadVideo.value()));
    duplicatePayload.emplace_back(std::move(duplicatePayloadAudio.value()));
    EXPECT_FALSE(ctx, MediaRtpSdpDescription::create(
        std::move(duplicatePayloadSession.value()), std::move(duplicatePayload)));

    auto duplicateSsrcSession = makeSession(
        MediaIpAddressFamily::Ipv4, "127.0.0.1");
    auto duplicateSsrcVideo = makeVideoDescription(
        "127.0.0.1", "127.0.0.1", 5004, 5005, 96, 10,
        MediaIpAddressFamily::Ipv4);
    auto duplicateSsrcAudio = makeAudioDescription(
        "127.0.0.1", "127.0.0.1", 6004, 6005, 97, 10);
    EXPECT_TRUE(ctx, duplicateSsrcSession && duplicateSsrcVideo &&
                     duplicateSsrcAudio);
    if (!duplicateSsrcSession || !duplicateSsrcVideo ||
        !duplicateSsrcAudio) return;
    std::vector<MediaRtpSdpMediaDescription> duplicateSsrc;
    duplicateSsrc.emplace_back(std::move(duplicateSsrcVideo.value()));
    duplicateSsrc.emplace_back(std::move(duplicateSsrcAudio.value()));
    EXPECT_FALSE(ctx, MediaRtpSdpDescription::create(
        std::move(duplicateSsrcSession.value()), std::move(duplicateSsrc)));

    auto h264Codec = MediaH264SdpCodecDescriptionFactory::create(*h264Parameters);
    auto audioIdentity = makeMedia(
        MediaSdpMediaKind::Audio, MediaIpAddressFamily::Ipv4,
        "127.0.0.1", "127.0.0.1", 6004, 6005, 97, 11, 48'000, 2);
    EXPECT_TRUE(ctx, h264Codec && audioIdentity);
    if (!h264Codec || !audioIdentity) return;
    EXPECT_FALSE(ctx, MediaRtpSdpMediaDescription::create(
        std::move(audioIdentity.value()), std::move(h264Codec.value())));
}

} // namespace

int main()
{
    TestContext ctx;
    testH264AvccAndAnnexBProduceAllParameterSets(ctx);
    testH264RejectsMalformedOrInconsistentExtradata(ctx);
    testAacLatmUsesStreamMuxConfigGoldenVectors(ctx);
    testTypedDescriptionAndExactSerialization(ctx);
    testIpv6AndContractRejections(ctx);
    testIpv6VideoOnlyExactSerialization(ctx);
    testDescriptionTopologyIsCompleteAndCollisionFree(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
