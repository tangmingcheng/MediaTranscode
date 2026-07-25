#include "common/TestAssert.h"

#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
}

#include <cstdint>
#include <initializer_list>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaTsMuxPlan plan(MediaTsH264InputLayout layout =
                        MediaTsH264InputLayout::LengthPrefixed,
                    std::uint8_t nalLengthBytes = 4,
                    MediaTsAacAdtsPlan aac = {0, 2, 3, 2})
{
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x100, 0x101, 0x102, 0x101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        layout, nalLengthBytes, MediaTsParameterSetPolicy::BeforeRandomAccess,
        aac,
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024}).value();
}

void setExtradata(AVCodecParameters& parameters,
                  std::initializer_list<std::uint8_t> bytes)
{
    parameters.extradata = static_cast<std::uint8_t*>(
        av_mallocz(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    parameters.extradata_size = static_cast<int>(bytes.size());
    std::copy(bytes.begin(), bytes.end(), parameters.extradata);
}

::media::ffmpeg::CodecParametersPtr videoParameters(
    std::initializer_list<std::uint8_t> extradata)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->codec_id = AV_CODEC_ID_H264;
    setExtradata(*parameters, extradata);
    return parameters;
}

::media::ffmpeg::CodecParametersPtr audioParameters(
    std::initializer_list<std::uint8_t> extradata)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    parameters->codec_type = AVMEDIA_TYPE_AUDIO;
    parameters->codec_id = AV_CODEC_ID_AAC;
    parameters->sample_rate = 48'000;
    parameters->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    setExtradata(*parameters, extradata);
    return parameters;
}

void testValidAvccIsCopied(TestContext& ctx)
{
    auto parameters = videoParameters({
        1, 0x64, 0, 0x1F, 0xFF, 0xE1,
        0, 4, 0x67, 0x64, 0, 0x1F,
        1, 0, 2, 0x68, 0xEE});
    auto materialized = MediaTsFfmpegStreamConfigMaterializer::video(
        plan(), *parameters);
    EXPECT_TRUE(ctx, materialized);
    if (!materialized) return;
    EXPECT_EQ(ctx, materialized.value().layout(),
              MediaTsH264InputLayout::LengthPrefixed);
    EXPECT_EQ(ctx, materialized.value().nalLengthBytes(),
              static_cast<std::uint8_t>(4));
    EXPECT_EQ(ctx, materialized.value().spsAnnexB(),
              std::vector<std::uint8_t>({0, 0, 0, 1, 0x67, 0x64, 0, 0x1F}));
    EXPECT_EQ(ctx, materialized.value().ppsAnnexB(),
              std::vector<std::uint8_t>({0, 0, 0, 1, 0x68, 0xEE}));
}

void testValidAnnexBIsCopied(TestContext& ctx)
{
    auto parameters = videoParameters({
        0, 0, 0, 1, 0x67, 0x64, 0, 0x1F,
        0, 0, 0, 1, 0x68, 0xEE});
    auto materialized = MediaTsFfmpegStreamConfigMaterializer::video(
        plan(MediaTsH264InputLayout::AnnexB), *parameters);
    EXPECT_TRUE(ctx, materialized);
    if (!materialized) return;
    EXPECT_EQ(ctx, materialized.value().layout(), MediaTsH264InputLayout::AnnexB);
    EXPECT_EQ(ctx, materialized.value().spsAnnexB(),
              std::vector<std::uint8_t>({0, 0, 0, 1, 0x67, 0x64, 0, 0x1F}));
    EXPECT_EQ(ctx, materialized.value().ppsAnnexB(),
              std::vector<std::uint8_t>({0, 0, 0, 1, 0x68, 0xEE}));
}

void testValidAacLcMatchesPlanAndParameters(TestContext& ctx)
{
    auto parameters = audioParameters({0x11, 0x90});
    auto materialized = MediaTsFfmpegStreamConfigMaterializer::audio(
        plan(), *parameters);
    EXPECT_TRUE(ctx, materialized);
    if (!materialized) return;
    EXPECT_EQ(ctx, materialized.value().audioObjectType(),
              static_cast<std::uint8_t>(2));
    EXPECT_EQ(ctx, materialized.value().samplingFrequencyIndex(),
              static_cast<std::uint8_t>(3));
    EXPECT_EQ(ctx, materialized.value().channelConfiguration(),
              static_cast<std::uint8_t>(2));
}

void testNativeFfmpegNoSbrAscDialectIsAccepted(TestContext& ctx)
{
    auto parameters = audioParameters({0x11, 0x90, 0x56, 0xE5, 0x00});
    auto materialized = MediaTsFfmpegStreamConfigMaterializer::audio(
        plan(), *parameters);
    EXPECT_TRUE(ctx, materialized);
    if (!materialized) return;
    EXPECT_EQ(ctx, materialized.value().audioObjectType(),
              static_cast<std::uint8_t>(2));
    EXPECT_EQ(ctx, materialized.value().samplingFrequencyIndex(),
              static_cast<std::uint8_t>(3));
    EXPECT_EQ(ctx, materialized.value().channelConfiguration(),
              static_cast<std::uint8_t>(2));
}

void testInvalidFfmpegAscDialectsAreRejected(TestContext& ctx)
{
    for (const auto& bytes : {
             std::vector<std::uint8_t>{0x11, 0x90, 0x56},
             std::vector<std::uint8_t>{0x11, 0x90, 0x56, 0xE5},
             std::vector<std::uint8_t>{0x11, 0x90, 0x57, 0xE5, 0x00},
             std::vector<std::uint8_t>{0x11, 0x90, 0x56, 0xE5, 0x80},
             std::vector<std::uint8_t>{0x11, 0x90, 0x56, 0xE5, 0x01},
             std::vector<std::uint8_t>{0x11, 0x90, 0x56, 0xE5, 0x00, 0x00}}) {
        auto parameters = ::media::ffmpeg::makeCodecParameters();
        parameters->codec_type = AVMEDIA_TYPE_AUDIO;
        parameters->codec_id = AV_CODEC_ID_AAC;
        parameters->sample_rate = 48'000;
        parameters->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        parameters->extradata = static_cast<std::uint8_t*>(
            av_mallocz(bytes.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        parameters->extradata_size = static_cast<int>(bytes.size());
        std::copy(bytes.begin(), bytes.end(), parameters->extradata);
        EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::audio(
            plan(), *parameters));
    }
}

void testMalformedAndIncompleteVideoConfigIsRejected(TestContext& ctx)
{
    auto truncated = videoParameters({1, 0x64, 0, 0x1F, 0xFF, 0xE1, 0, 4, 0x67});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::video(
        plan(), *truncated));

    auto missingPps = videoParameters({
        1, 0x64, 0, 0x1F, 0xFF, 0xE1, 0, 4, 0x67, 0x64, 0, 0x1F, 0});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::video(
        plan(), *missingPps));

    auto duplicateSps = videoParameters({
        1, 0x64, 0, 0x1F, 0xFF, 0xE2,
        0, 2, 0x67, 0x64, 0, 2, 0x67, 0x42,
        1, 0, 2, 0x68, 0xEE});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::video(
        plan(), *duplicateSps));
}

void testVideoLayoutAndNalWidthAreNeverInferred(TestContext& ctx)
{
    auto avcc = videoParameters({
        1, 0x64, 0, 0x1F, 0xFF, 0xE1,
        0, 4, 0x67, 0x64, 0, 0x1F,
        1, 0, 2, 0x68, 0xEE});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::video(
        plan(MediaTsH264InputLayout::AnnexB), *avcc));
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::video(
        plan(MediaTsH264InputLayout::LengthPrefixed, 2), *avcc));

    auto annexB = videoParameters({
        0, 0, 0, 1, 0x67, 0x64,
        0, 0, 0, 1, 0x68, 0xEE});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::video(
        plan(MediaTsH264InputLayout::LengthPrefixed), *annexB));
}

void testWrongCodecsAreRejected(TestContext& ctx)
{
    auto video = videoParameters({
        1, 0x64, 0, 0x1F, 0xFF, 0xE1,
        0, 2, 0x67, 0x64, 1, 0, 2, 0x68, 0xEE});
    video->codec_id = AV_CODEC_ID_HEVC;
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::video(plan(), *video));

    auto audio = audioParameters({0x11, 0x90});
    audio->codec_id = AV_CODEC_ID_MP3;
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::audio(plan(), *audio));
}

void testAacFactsMustAgreeExactly(TestContext& ctx)
{
    auto wrongObjectType = audioParameters({0x09, 0x90});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::audio(
        plan(), *wrongObjectType));

    auto wrongRate = audioParameters({0x11, 0x90});
    wrongRate->sample_rate = 44'100;
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::audio(
        plan(), *wrongRate));

    auto wrongChannels = audioParameters({0x11, 0x90});
    wrongChannels->ch_layout = AV_CHANNEL_LAYOUT_MONO;
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::audio(
        plan(), *wrongChannels));

    auto wrongPlan = plan(MediaTsH264InputLayout::LengthPrefixed, 4,
                          MediaTsAacAdtsPlan{0, 2, 4, 2});
    auto parameters = audioParameters({0x11, 0x90});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::audio(
        wrongPlan, *parameters));

    auto truncated = audioParameters({0x11});
    EXPECT_FALSE(ctx, MediaTsFfmpegStreamConfigMaterializer::audio(
        plan(), *truncated));
}

void testMaterializedConfigOutlivesFfmpegParameters(TestContext& ctx)
{
    auto parameters = videoParameters({
        1, 0x64, 0, 0x1F, 0xFF, 0xE1,
        0, 4, 0x67, 0x64, 0, 0x1F,
        1, 0, 2, 0x68, 0xEE});
    auto materialized = MediaTsFfmpegStreamConfigMaterializer::video(
        plan(), *parameters);
    EXPECT_TRUE(ctx, materialized);
    parameters.reset();
    if (materialized) {
        EXPECT_EQ(ctx, materialized.value().spsAnnexB(),
                  std::vector<std::uint8_t>({0, 0, 0, 1, 0x67, 0x64, 0, 0x1F}));
    }
}

} // namespace

void runMpegTsFfmpegConfigMaterializerTests(TestContext& ctx)
{
    testValidAvccIsCopied(ctx);
    testValidAnnexBIsCopied(ctx);
    testValidAacLcMatchesPlanAndParameters(ctx);
    testNativeFfmpegNoSbrAscDialectIsAccepted(ctx);
    testInvalidFfmpegAscDialectsAreRejected(ctx);
    testMalformedAndIncompleteVideoConfigIsRejected(ctx);
    testVideoLayoutAndNalWidthAreNeverInferred(ctx);
    testWrongCodecsAreRejected(ctx);
    testAacFactsMustAgreeExactly(ctx);
    testMaterializedConfigOutlivesFfmpegParameters(ctx);
}
