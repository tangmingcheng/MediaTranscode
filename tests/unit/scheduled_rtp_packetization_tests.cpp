#include "common/TestAssert.h"

#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.h"
#include "internal/graph/nodes/output/MediaScheduledRtpCodecParametersMaterializer.h"
#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/protocol/sdp/MediaAacLatmSdpCodecDescriptionFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h"
#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDatagramWriteAvio.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avio.h>
#include <libavutil/mem.h>
}

#include <array>
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

::media::ffmpeg::CodecContextPtr materializerContext(
    MediaStreamKind stream,
    AVRational timeBase,
    std::span<const std::uint8_t> extradata)
{
    auto context = ::media::ffmpeg::makeCodecContext(nullptr);
    if (!context) return {};
    const bool video = stream == MediaStreamKind::Video;
    context->codec_type = video ? AVMEDIA_TYPE_VIDEO : AVMEDIA_TYPE_AUDIO;
    context->codec_id = video ? AV_CODEC_ID_H264 : AV_CODEC_ID_AAC;
    context->time_base = timeBase;
    context->extradata = static_cast<std::uint8_t*>(
        av_mallocz(extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!context->extradata) return {};
    std::copy(extradata.begin(), extradata.end(), context->extradata);
    context->extradata_size = static_cast<int>(extradata.size());
    if (video) {
        context->width = 1920;
        context->height = 1080;
    } else {
        context->sample_rate = 48'000;
        context->frame_size = 1024;
        av_channel_layout_default(&context->ch_layout, 2);
    }
    return context;
}

void testScheduledRtpCodecParametersSeparateEncoderAndRtpTimeBases(
    TestContext& ctx)
{
    const std::array<std::uint8_t, 8> annexB{
        0, 0, 0, 1, 0x67, 0x64, 0, 0x1f};
    auto context = materializerContext(
        MediaStreamKind::Video, AVRational{1, 30}, annexB);
    auto plan = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Video, "h264", 1, 90'000, 96, 1200);
    EXPECT_TRUE(ctx, context != nullptr && plan);
    if (!context || !plan) return;
    auto parameters = MediaScheduledRtpCodecParametersMaterializer::materialize(
        *context, plan.value());
    EXPECT_TRUE(ctx, parameters);
    if (parameters) {
        EXPECT_EQ(ctx, parameters.value()->codec_id, AV_CODEC_ID_H264);
    }

    context->codec_id = AV_CODEC_ID_HEVC;
    EXPECT_FALSE(ctx,
                 MediaScheduledRtpCodecParametersMaterializer::materialize(
                     *context, plan.value()));
    context->codec_id = AV_CODEC_ID_H264;
    context->time_base = AVRational{0, 1};
    EXPECT_FALSE(ctx,
                 MediaScheduledRtpCodecParametersMaterializer::materialize(
                     *context, plan.value()));
}

void testScheduledRtpCodecParametersCanonicalizeNativeAacAsc(
    TestContext& ctx)
{
    const std::array<std::uint8_t, 5> nativeAsc{
        0x11, 0x90, 0x56, 0xE5, 0x00};
    auto context = materializerContext(
        MediaStreamKind::Audio, AVRational{1, 48'000}, nativeAsc);
    auto plan = MediaScheduledRtpPacketizationPlan::create(
        MediaStreamKind::Audio, "aac", 1, 48'000, 97, 1200, 1024);
    EXPECT_TRUE(ctx, context != nullptr && plan);
    if (!context || !plan) return;
    auto parameters = MediaScheduledRtpCodecParametersMaterializer::materialize(
        *context, plan.value());
    EXPECT_TRUE(ctx, parameters);
    if (parameters) {
        EXPECT_EQ(ctx, parameters.value()->extradata_size, 2);
        EXPECT_EQ(ctx, parameters.value()->extradata[0],
                  static_cast<std::uint8_t>(0x11));
        EXPECT_EQ(ctx, parameters.value()->extradata[1],
                  static_cast<std::uint8_t>(0x90));
        EXPECT_TRUE(ctx,
                    MediaAacLatmSdpCodecDescriptionFactory::create(
                        *parameters.value()));
        EXPECT_TRUE(ctx,
                    ScheduledRtpMuxStreamConfig::create(
                        MediaStreamKind::Audio, *parameters.value(),
                        AVRational{1, 48'000},
                        MediaScheduledRtpPacketizationMode::AacLatm,
                        97, 0x10203040u, 1200));
    }

    context->extradata[4] = 0x80;
    EXPECT_FALSE(ctx,
                 MediaScheduledRtpCodecParametersMaterializer::materialize(
                     *context, plan.value()));
    context->extradata[4] = 0x00;
    context->sample_rate = 44'100;
    EXPECT_FALSE(ctx,
                 MediaScheduledRtpCodecParametersMaterializer::materialize(
                     *context, plan.value()));
    context->sample_rate = 48'000;
    context->frame_size = 960;
    EXPECT_FALSE(ctx,
                 MediaScheduledRtpCodecParametersMaterializer::materialize(
                     *context, plan.value()));
}

static_assert(!std::is_default_constructible_v<FFmpegDatagramWriteAvioConfig>);
static_assert(!std::is_default_constructible_v<MediaRtpDatagramRewriteParameters>);
static_assert(!std::is_default_constructible_v<MediaRtpDatagramRewriteIdentity>);
static_assert(!std::is_default_constructible_v<MediaRtpDatagramRewriteResult>);
static_assert(!std::is_default_constructible_v<ScheduledRtpMuxStreamConfig>);
static_assert(!std::is_constructible_v<MediaRtpTimestamp, std::uint64_t, std::uint32_t>);

std::uint32_t readU32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

MediaRtpTimestamp mappedTimestamp(TestContext& ctx, std::uint32_t base, std::int64_t nanoseconds)
{
    auto mapper = MediaRtpOutputClockMapper::create(
        90'000, base, MediaRunningTime::fromNanoseconds(0));
    EXPECT_TRUE(ctx, mapper);
    auto mapped = mapper.value().map(MediaRunningTime::fromNanoseconds(nanoseconds));
    EXPECT_TRUE(ctx, mapped);
    return mapped.value();
}

void testDatagramWriteAvioLifecycleAndFailure(TestContext& ctx)
{
    EXPECT_FALSE(ctx, FFmpegDatagramWriteAvioConfig::create(0));
    EXPECT_FALSE(ctx, FFmpegDatagramWriteAvioConfig::create(12));
    auto config = FFmpegDatagramWriteAvioConfig::create(1200);
    EXPECT_TRUE(ctx, config);
    if (!config) return;

    std::vector<std::vector<std::uint8_t>> delivered;
    auto avio = FFmpegDatagramWriteAvio::create(
        config.value(),
        [&delivered](std::span<const std::uint8_t> bytes) {
            delivered.emplace_back(bytes.begin(), bytes.end());
            return ::media::Status::success();
        });
    EXPECT_TRUE(ctx, avio);
    if (!avio) return;
    EXPECT_FALSE(ctx, avio.value()->context() != nullptr);
    EXPECT_TRUE(ctx, avio.value()->open());
    EXPECT_FALSE(ctx, avio.value()->open());
    EXPECT_EQ(ctx, avio.value()->context()->buffer_size, 1200);
    EXPECT_EQ(ctx, avio.value()->context()->max_packet_size, 1200);

    const std::array<std::uint8_t, 5> datagram{1, 2, 3, 4, 5};
    avio_write(avio.value()->context(), datagram.data(), static_cast<int>(datagram.size()));
    avio_flush(avio.value()->context());
    EXPECT_EQ(ctx, delivered.size(), static_cast<std::size_t>(1));
    if (!delivered.empty()) {
        EXPECT_EQ(ctx, delivered.front(), std::vector<std::uint8_t>(datagram.begin(), datagram.end()));
    }
    EXPECT_TRUE(ctx, avio.value()->close());
    EXPECT_TRUE(ctx, avio.value()->context() == nullptr);
    EXPECT_EQ(ctx, delivered.size(), static_cast<std::size_t>(1));
    EXPECT_FALSE(ctx, avio.value()->close());
    EXPECT_TRUE(ctx, avio.value()->reset());
    for (int cycle = 0; cycle < 64; ++cycle) {
        EXPECT_TRUE(ctx, avio.value()->open());
        EXPECT_TRUE(ctx, avio.value()->context() != nullptr);
        EXPECT_TRUE(ctx, avio.value()->context()->buffer != nullptr);
        EXPECT_TRUE(ctx, avio.value()->close());
        EXPECT_TRUE(ctx, avio.value()->reset());
    }

    const auto sinkError = ::media::ErrorInfo::ioFailure("structured sink failure", -77);
    auto failing = FFmpegDatagramWriteAvio::create(
        config.value(),
        [sinkError](std::span<const std::uint8_t>) {
            return ::media::Status::failure(sinkError);
        });
    EXPECT_TRUE(ctx, failing);
    if (failing) {
        const auto failingOpened = failing.value()->open();
        EXPECT_TRUE(ctx, failingOpened);
        if (!failingOpened) return;
        avio_write(failing.value()->context(), datagram.data(), static_cast<int>(datagram.size()));
        avio_flush(failing.value()->context());
        EXPECT_TRUE(ctx, failing.value()->sinkFailure().has_value());
        if (failing.value()->sinkFailure()) {
            EXPECT_EQ(ctx, failing.value()->sinkFailure()->code, ::media::ErrorCode::IoFailure);
            EXPECT_EQ(ctx, failing.value()->sinkFailure()->nativeCode, -77);
            EXPECT_EQ(ctx, failing.value()->sinkFailure()->message, std::string("structured sink failure"));
        }
        const auto closed = failing.value()->close();
        EXPECT_FALSE(ctx, closed);
        if (!closed) {
            EXPECT_EQ(ctx, closed.error().code, ::media::ErrorCode::IoFailure);
            EXPECT_EQ(ctx, closed.error().nativeCode, -77);
        }
    }

    EXPECT_FALSE(ctx, FFmpegDatagramWriteAvio::create(config.value(), {}));
}

void testRtpDatagramRewritePreservesStructure(TestContext& ctx)
{
    const auto timestamp = mappedTimestamp(ctx, 0xFFFFFFF0u, 1'000'000'000);
    auto identity = MediaRtpDatagramRewriteIdentity::create(96, 0x11223344u);
    EXPECT_TRUE(ctx, identity);
    if (!identity) return;
    const MediaRtpDatagramRewriteParameters parameters(
        identity.value(), timestamp);
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriteIdentity::create(128, 1u));
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriteIdentity::create(95, 1u));
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriteIdentity::create(96, 0u));
    std::vector<std::uint8_t> minimal{
        0x80, 0xE0, 0x12, 0x34,
        0, 0, 0, 1,
        0xAA, 0xBB, 0xCC, 0xDD,
        9, 8, 7};
    std::vector<std::uint8_t> rewritten;
    rewritten.reserve(1200);
    const auto* const scratchStorage = rewritten.data();
    const auto scratchCapacity = rewritten.capacity();
    auto rewriteStatus = MediaRtpDatagramRewriter::rewrite(
        minimal, parameters, rewritten);
    EXPECT_TRUE(ctx, rewriteStatus);
    EXPECT_TRUE(ctx, rewritten.data() == scratchStorage);
    EXPECT_EQ(ctx, rewritten.capacity(), scratchCapacity);
    if (rewriteStatus) {
        EXPECT_EQ(ctx, rewriteStatus.value().payloadOctets(), static_cast<std::size_t>(3));
        EXPECT_EQ(ctx, rewritten[0], 0x80u);
        EXPECT_EQ(ctx, rewritten[1], 0xE0u);
        EXPECT_EQ(ctx, rewritten[2], 0x12u);
        EXPECT_EQ(ctx, rewritten[3], 0x34u);
        EXPECT_EQ(ctx, readU32(rewritten, 4), timestamp.wire());
        EXPECT_EQ(ctx, readU32(rewritten, 8), 0x11223344u);
        EXPECT_EQ(ctx, std::vector<std::uint8_t>(rewritten.begin() + 12, rewritten.end()),
                  std::vector<std::uint8_t>({9, 8, 7}));
    }

    std::vector<std::uint8_t> complex{
        0xB2, 0x60, 0x00, 0x02,
        0, 0, 0, 2,
        0, 0, 0, 3,
        1, 2, 3, 4,
        5, 6, 7, 8,
        0xBE, 0xDE, 0, 1,
        10, 11, 12, 13,
        21, 22, 0, 3};
    const auto originalComplex = complex;
    auto complexRewriteStatus = MediaRtpDatagramRewriter::rewrite(
        complex, parameters, rewritten);
    EXPECT_TRUE(ctx, complexRewriteStatus);
    EXPECT_TRUE(ctx, rewritten.data() == scratchStorage);
    EXPECT_EQ(ctx, rewritten.capacity(), scratchCapacity);
    if (complexRewriteStatus) {
        EXPECT_EQ(ctx, complexRewriteStatus.value().payloadOctets(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, rewritten[0], originalComplex[0]);
        EXPECT_EQ(ctx, rewritten[1], originalComplex[1]);
        EXPECT_EQ(ctx, rewritten[2], originalComplex[2]);
        EXPECT_EQ(ctx, rewritten[3], originalComplex[3]);
        EXPECT_EQ(ctx, std::vector<std::uint8_t>(rewritten.begin() + 12,
                                                 rewritten.end()),
                  std::vector<std::uint8_t>(originalComplex.begin() + 12, originalComplex.end()));
    }

    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(
        std::span<const std::uint8_t>{}, parameters, rewritten));
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(
        std::vector<std::uint8_t>(11, 0), parameters, rewritten));
    auto wrongVersion = minimal;
    wrongVersion[0] = 0x40;
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(wrongVersion, parameters, rewritten));
    auto wrongPayloadType = minimal;
    wrongPayloadType[1] = 97;
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(wrongPayloadType, parameters, rewritten));
    auto rtcp = minimal;
    rtcp[1] = 200;
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(rtcp, parameters, rewritten));
    auto truncatedCsrc = minimal;
    truncatedCsrc[0] = 0x82;
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(truncatedCsrc, parameters, rewritten));
    auto truncatedExtension = minimal;
    truncatedExtension[0] = 0x90;
    truncatedExtension.resize(15);
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(truncatedExtension, parameters, rewritten));
    auto badPadding = minimal;
    badPadding[0] = 0xA0;
    badPadding.back() = 0;
    EXPECT_FALSE(ctx, MediaRtpDatagramRewriter::rewrite(badPadding, parameters, rewritten));
}

std::uint16_t readU16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[offset]) << 8) |
        static_cast<std::uint16_t>(bytes[offset + 1]));
}

std::vector<std::vector<std::uint8_t>> reconstructH264Nals(
    TestContext& ctx,
    const std::vector<std::vector<std::uint8_t>>& datagrams)
{
    std::vector<std::vector<std::uint8_t>> nals;
    EXPECT_TRUE(ctx, !datagrams.empty());
    if (datagrams.empty()) return nals;
    const std::uint32_t timestamp = readU32(datagrams.front(), 4);
    std::uint16_t sequence = readU16(datagrams.front(), 2);
    bool fuActive = false;
    for (const auto& datagram : datagrams) {
        EXPECT_TRUE(ctx, datagram.size() > static_cast<std::size_t>(12));
        if (datagram.size() <= 12) return {};
        EXPECT_EQ(ctx, readU32(datagram, 4), timestamp);
        EXPECT_EQ(ctx, readU16(datagram, 2), sequence++);
        const std::span<const std::uint8_t> payload(
            datagram.data() + 12, datagram.size() - 12);
        const std::uint8_t type = payload[0] & 0x1Fu;
        if (type >= 1 && type <= 23) {
            EXPECT_FALSE(ctx, fuActive);
            nals.emplace_back(payload.begin(), payload.end());
        } else if (type == 24) {
            EXPECT_FALSE(ctx, fuActive);
            std::size_t offset = 1;
            while (offset < payload.size()) {
                EXPECT_TRUE(ctx, offset + 2 <= payload.size());
                if (offset + 2 > payload.size()) return {};
                const std::size_t size = readU16(payload, offset);
                offset += 2;
                EXPECT_TRUE(ctx, size > 0 && size <= payload.size() - offset);
                if (size == 0 || size > payload.size() - offset) return {};
                nals.emplace_back(
                    payload.begin() + static_cast<std::ptrdiff_t>(offset),
                    payload.begin() + static_cast<std::ptrdiff_t>(offset + size));
                offset += size;
            }
        } else if (type == 28) {
            EXPECT_TRUE(ctx, payload.size() >= static_cast<std::size_t>(3));
            if (payload.size() < 3) return {};
            const bool start = (payload[1] & 0x80u) != 0;
            const bool end = (payload[1] & 0x40u) != 0;
            if (start) {
                EXPECT_FALSE(ctx, fuActive);
                nals.emplace_back();
                nals.back().push_back(static_cast<std::uint8_t>(
                    (payload[0] & 0xE0u) | (payload[1] & 0x1Fu)));
                fuActive = true;
            } else {
                EXPECT_TRUE(ctx, fuActive);
                if (!fuActive) return {};
            }
            nals.back().insert(
                nals.back().end(), payload.begin() + 2, payload.end());
            if (end) fuActive = false;
        } else {
            EXPECT_TRUE(ctx, false);
            return {};
        }
    }
    EXPECT_FALSE(ctx, fuActive);
    EXPECT_TRUE(ctx, (datagrams.back()[1] & 0x80u) != 0);
    return nals;
}

void expectH264FuA(TestContext& ctx,
                   const std::vector<std::vector<std::uint8_t>>& datagrams,
                   std::span<const std::uint8_t> expectedNal)
{
    EXPECT_TRUE(ctx, datagrams.size() > static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, !expectedNal.empty());
    if (datagrams.size() <= 1 || expectedNal.empty()) return;

    const std::uint16_t firstSequence = readU16(datagrams.front(), 2);
    for (std::size_t index = 0; index < datagrams.size(); ++index) {
        const auto& datagram = datagrams[index];
        EXPECT_TRUE(ctx, datagram.size() > static_cast<std::size_t>(14));
        if (datagram.size() <= 14) return;
        const std::uint8_t fuIndicator = datagram[12];
        const std::uint8_t fuHeader = datagram[13];
        EXPECT_EQ(ctx, fuIndicator & 0x1Fu, 28u);
        EXPECT_EQ(ctx, (fuHeader & 0x80u) != 0, index == 0);
        EXPECT_EQ(ctx, (fuHeader & 0x40u) != 0, index + 1 == datagrams.size());
        EXPECT_EQ(ctx, (datagram[1] & 0x80u) != 0,
                  index + 1 == datagrams.size());
        EXPECT_EQ(ctx, readU16(datagram, 2),
                  static_cast<std::uint16_t>(firstSequence + index));
    }
    const auto reconstructed = reconstructH264Nals(ctx, datagrams);
    EXPECT_EQ(ctx, reconstructed.size(), static_cast<std::size_t>(1));
    if (!reconstructed.empty()) {
        EXPECT_EQ(ctx, reconstructed.front(),
                  std::vector<std::uint8_t>(
                      expectedNal.begin(), expectedNal.end()));
    }
}

::media::ffmpeg::CodecParametersPtr makeH264Parameters()
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) return {};
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->codec_id = AV_CODEC_ID_H264;
    parameters->width = 1920;
    parameters->height = 1080;
    return parameters;
}

::media::ffmpeg::CodecParametersPtr makeH264AvccParameters()
{
    auto parameters = makeH264Parameters();
    if (!parameters) return {};
    const std::array<std::uint8_t, 36> avcc{
        1, 0x4D, 0x40, 0x32, 0xFF, 0xE1, 0, 21,
        0x67, 0x4D, 0x40, 0x32, 0x95, 0x90, 0x02, 0x80,
        0x0B, 0x5B, 0x01, 0x10, 0, 0, 0x3E, 0x80,
        0, 0x0E, 0xA6, 0x08, 0x40,
        1, 0, 4, 0x68, 0xEB, 0x8F, 0x20};
    parameters->extradata = static_cast<std::uint8_t*>(av_mallocz(
        avcc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!parameters->extradata) return {};
    std::copy(avcc.begin(), avcc.end(), parameters->extradata);
    parameters->extradata_size = static_cast<int>(avcc.size());
    return parameters;
}

::media::ffmpeg::CodecParametersPtr makeAacParameters()
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) return {};
    parameters->codec_type = AVMEDIA_TYPE_AUDIO;
    parameters->codec_id = AV_CODEC_ID_AAC;
    parameters->sample_rate = 48'000;
    av_channel_layout_default(&parameters->ch_layout, 2);
    parameters->extradata = static_cast<std::uint8_t*>(av_mallocz(AV_INPUT_BUFFER_PADDING_SIZE + 2));
    if (!parameters->extradata) return {};
    parameters->extradata[0] = 0x11;
    parameters->extradata[1] = 0x90;
    parameters->extradata_size = 2;
    return parameters;
}

void testScheduledMuxRealPacketization(TestContext& ctx)
{
    auto h264 = makeH264Parameters();
    EXPECT_TRUE(ctx, h264 != nullptr);
    if (!h264) return;
    auto config = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *h264, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x01020304u, 256);
    EXPECT_TRUE(ctx, config);
    EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *h264, AVRational{0, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x01020304u, 256));
    EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *h264, AVRational{1, -90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x01020304u, 256));
    if (!config) return;

    std::vector<std::vector<std::uint8_t>> datagrams;
    ScheduledRtpMuxFfmpegSession session(
        [&datagrams](std::span<const std::uint8_t> bytes, std::size_t) {
            datagrams.emplace_back(bytes.begin(), bytes.end());
            return ::media::Status::success();
        });
    EXPECT_FALSE(ctx, session.open());
    EXPECT_TRUE(ctx, session.configure(std::move(config.value())));
    auto duplicateConfig = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *h264, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x01020304u, 256);
    EXPECT_TRUE(ctx, duplicateConfig);
    if (duplicateConfig) EXPECT_FALSE(ctx, session.configure(std::move(duplicateConfig.value())));
    const auto opened = session.open();
    EXPECT_TRUE(ctx, opened);
    EXPECT_FALSE(ctx, session.open());

    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    const int packetAllocation = av_new_packet(packet.get(), 1000);
    EXPECT_TRUE(ctx, packetAllocation >= 0);
    if (packetAllocation < 0) return;
    packet->data[0] = 0;
    packet->data[1] = 0;
    packet->data[2] = 0;
    packet->data[3] = 1;
    packet->data[4] = 0x65;
    for (int index = 5; index < packet->size; ++index) packet->data[index] = static_cast<std::uint8_t>(index);
    packet->pts = 0;
    packet->dts = 0;
    packet->duration = 3000;
    packet->flags = AV_PKT_FLAG_KEY;

    const auto firstTimestamp = mappedTimestamp(ctx, 0xFFFFFFF0u, 1'000'000'000);
    const auto firstWrite = session.writeAccessUnit(*packet, firstTimestamp);
    EXPECT_TRUE(ctx, firstWrite);
    expectH264FuA(
        ctx,
        datagrams,
        std::span<const std::uint8_t>(
            packet->data + 4, static_cast<std::size_t>(packet->size - 4)));
    for (const auto& datagram : datagrams) {
        EXPECT_TRUE(ctx, datagram.size() >= static_cast<std::size_t>(12));
        if (datagram.size() >= 12) {
            EXPECT_EQ(ctx, datagram[1] & 0x7Fu, 96u);
            EXPECT_EQ(ctx, readU32(datagram, 4), firstTimestamp.wire());
            EXPECT_EQ(ctx, readU32(datagram, 8), 0x01020304u);
            EXPECT_FALSE(ctx, datagram[1] >= 192 && datagram[1] <= 223);
        }
    }

    const auto beforeSecond = datagrams.size();
    packet->pts = 3000;
    packet->dts = 3000;
    const auto secondTimestamp = mappedTimestamp(ctx, 0xFFFFFFF0u, 2'000'000'000);
    EXPECT_TRUE(ctx, session.writeAccessUnit(*packet, secondTimestamp));
    EXPECT_TRUE(ctx, datagrams.size() > beforeSecond);
    for (std::size_t index = beforeSecond; index < datagrams.size(); ++index) {
        EXPECT_EQ(ctx, readU32(datagrams[index], 4), secondTimestamp.wire());
    }
    EXPECT_TRUE(ctx, session.writeTrailer());
    for (const auto& datagram : datagrams) {
        EXPECT_FALSE(ctx, datagram.size() > 1 &&
                          datagram[1] >= 192 && datagram[1] <= 223);
    }
    EXPECT_FALSE(ctx, session.writeAccessUnit(*packet, secondTimestamp));
    EXPECT_TRUE(ctx, session.reset());
    EXPECT_FALSE(ctx, session.writeTrailer());

    auto aac = makeAacParameters();
    EXPECT_TRUE(ctx, aac != nullptr);
    if (aac) {
        auto missingSampleRate = makeAacParameters();
        EXPECT_TRUE(ctx, missingSampleRate != nullptr);
        if (missingSampleRate) {
            missingSampleRate->sample_rate = 0;
            EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
                MediaStreamKind::Audio, *missingSampleRate, AVRational{1, 48'000},
                MediaScheduledRtpPacketizationMode::AacLatm,
                97, 0x05060708u, 1200));
        }
        auto missingChannelLayout = makeAacParameters();
        EXPECT_TRUE(ctx, missingChannelLayout != nullptr);
        if (missingChannelLayout) {
            av_channel_layout_uninit(&missingChannelLayout->ch_layout);
            EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
                MediaStreamKind::Audio, *missingChannelLayout, AVRational{1, 48'000},
                MediaScheduledRtpPacketizationMode::AacLatm,
                97, 0x05060708u, 1200));
        }
        auto missingAsc = makeAacParameters();
        EXPECT_TRUE(ctx, missingAsc != nullptr);
        if (missingAsc) {
            av_freep(&missingAsc->extradata);
            missingAsc->extradata_size = 0;
            EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
                MediaStreamKind::Audio, *missingAsc, AVRational{1, 48'000},
                MediaScheduledRtpPacketizationMode::AacLatm,
                97, 0x05060708u, 1200));
        }
        EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
            MediaStreamKind::Audio, *aac, AVRational{1, 44'100},
            MediaScheduledRtpPacketizationMode::AacLatm,
            97, 0x05060708u, 1200));
        EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
            MediaStreamKind::Video, *aac, AVRational{1, 48'000},
            MediaScheduledRtpPacketizationMode::AacLatm,
            97, 0x05060708u, 1200));
        EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
            MediaStreamKind::Audio, *aac, AVRational{1, 48'000},
            MediaScheduledRtpPacketizationMode::H264AnnexB,
            97, 0x05060708u, 1200));
        EXPECT_FALSE(ctx, ScheduledRtpMuxStreamConfig::create(
            MediaStreamKind::Video, *h264, AVRational{1, 90'000},
            MediaScheduledRtpPacketizationMode::AacLatm,
            96, 0x01020304u, 1200));
        auto audioConfig = ScheduledRtpMuxStreamConfig::create(
            MediaStreamKind::Audio, *aac, AVRational{1, 48'000},
            MediaScheduledRtpPacketizationMode::AacLatm,
            97, 0x05060708u, 1200);
        EXPECT_TRUE(ctx, audioConfig);
        if (audioConfig) {
            std::vector<std::vector<std::uint8_t>> audioDatagrams;
            ScheduledRtpMuxFfmpegSession audioSession(
                [&audioDatagrams](std::span<const std::uint8_t> bytes, std::size_t) {
                    audioDatagrams.emplace_back(bytes.begin(), bytes.end());
                    return ::media::Status::success();
                });
            EXPECT_TRUE(ctx, audioSession.configure(std::move(audioConfig.value())));
            const auto audioOpened = audioSession.open();
            EXPECT_TRUE(ctx, audioOpened);
            auto audioPacket = ::media::ffmpeg::makePacket();
            EXPECT_TRUE(ctx, audioPacket != nullptr);
            if (audioPacket) {
                const int audioPacketAllocation = av_new_packet(audioPacket.get(), 100);
                EXPECT_TRUE(ctx, audioPacketAllocation >= 0);
                if (audioPacketAllocation < 0) return;
                audioPacket->pts = 0;
                audioPacket->dts = 0;
                audioPacket->duration = 1024;
                const auto audioTime = mappedTimestamp(ctx, 100u, 1'000'000'000);
                EXPECT_TRUE(ctx, audioSession.writeAccessUnit(*audioPacket, audioTime));
                EXPECT_TRUE(ctx, !audioDatagrams.empty());
                if (!audioDatagrams.empty()) {
                    EXPECT_EQ(ctx, readU32(audioDatagrams.front(), 4), audioTime.wire());
                    EXPECT_EQ(ctx, readU32(audioDatagrams.front(), 8), 0x05060708u);
                }
                const auto beforeSecondAudio = audioDatagrams.size();
                audioPacket->pts = 1024;
                audioPacket->dts = 1024;
                const auto secondAudioTime = mappedTimestamp(ctx, 100u, 2'000'000'000);
                EXPECT_TRUE(ctx, audioSession.writeAccessUnit(*audioPacket, secondAudioTime));
                EXPECT_TRUE(ctx, audioDatagrams.size() > beforeSecondAudio);
                for (std::size_t index = beforeSecondAudio;
                     index < audioDatagrams.size(); ++index) {
                    EXPECT_EQ(ctx, readU32(audioDatagrams[index], 4), secondAudioTime.wire());
                }
                EXPECT_TRUE(ctx, audioSession.writeTrailer());
                for (const auto& datagram : audioDatagrams) {
                    EXPECT_FALSE(ctx, datagram.size() > 1 &&
                                      datagram[1] >= 192 && datagram[1] <= 223);
                }
            }
        }
    }
}

void testScheduledMuxNormalizesAvccForExplicitAnnexB(TestContext& ctx)
{
    auto h264 = makeH264AvccParameters();
    EXPECT_TRUE(ctx, h264 != nullptr);
    if (!h264) return;
    auto config = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *h264, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x0A0B0C0Du, 256);
    EXPECT_TRUE(ctx, config);
    if (!config) return;

    std::vector<std::vector<std::uint8_t>> datagrams;
    ScheduledRtpMuxFfmpegSession session(
        [&datagrams](std::span<const std::uint8_t> bytes, std::size_t) {
            datagrams.emplace_back(bytes.begin(), bytes.end());
            return ::media::Status::success();
        });
    EXPECT_TRUE(ctx, session.configure(std::move(config.value())));
    EXPECT_TRUE(ctx, session.open());

    auto lengthPrefixed = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, lengthPrefixed != nullptr);
    if (!lengthPrefixed) return;
    EXPECT_TRUE(ctx, av_new_packet(lengthPrefixed.get(), 6) >= 0);
    if (!lengthPrefixed->data) return;
    const std::array<std::uint8_t, 6> avccAccessUnit{
        0, 0, 0, 2, 0x65, 0x55};
    std::copy(
        avccAccessUnit.begin(), avccAccessUnit.end(), lengthPrefixed->data);
    const auto timestamp = mappedTimestamp(ctx, 100u, 1'000'000'000);
    const auto rejected = session.writeAccessUnit(*lengthPrefixed, timestamp);
    EXPECT_FALSE(ctx, rejected);
    if (!rejected) {
        EXPECT_EQ(ctx, rejected.error().code, ::media::ErrorCode::InvalidArgument);
    }
    EXPECT_TRUE(ctx, datagrams.empty());

    std::vector<std::vector<std::uint8_t>> expected{
        {0x09, 0xF0},
        {0x67, 0x4D, 0x40, 0x32, 0x95, 0x90, 0x02, 0x80,
         0x0B, 0x5B, 0x01, 0x10, 0, 0, 0x3E, 0x80,
         0, 0x0E, 0xA6, 0x08, 0x40},
        {0x68, 0xEB, 0x8F, 0x20},
        std::vector<std::uint8_t>(1'000, 0x55)};
    expected.back()[0] = 0x65;
    std::size_t packetBytes = 0;
    for (const auto& nal : expected) packetBytes += 4 + nal.size();
    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    EXPECT_TRUE(ctx, av_new_packet(packet.get(), static_cast<int>(packetBytes)) >= 0);
    if (!packet->data) return;
    std::size_t offset = 0;
    for (const auto& nal : expected) {
        packet->data[offset++] = 0;
        packet->data[offset++] = 0;
        packet->data[offset++] = 0;
        packet->data[offset++] = 1;
        std::copy(nal.begin(), nal.end(), packet->data + offset);
        offset += nal.size();
    }
    packet->pts = packet->dts = 0;
    packet->flags = AV_PKT_FLAG_KEY;
    EXPECT_TRUE(ctx, session.writeAccessUnit(*packet, timestamp));
    EXPECT_EQ(ctx, reconstructH264Nals(ctx, datagrams), expected);
    EXPECT_TRUE(ctx, session.writeTrailer());
}

void testScheduledMuxPropagatesSinkFailure(TestContext& ctx)
{
    auto h264 = makeH264Parameters();
    EXPECT_TRUE(ctx, h264 != nullptr);
    if (!h264) return;
    auto config = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *h264, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x01020304u, 256);
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    bool injectPartialFailure = true;
    std::size_t callbackCount = 0;
    std::size_t successfulDatagrams = 0;
    ScheduledRtpMuxFfmpegSession session(
        [&injectPartialFailure, &callbackCount, &successfulDatagrams](
            std::span<const std::uint8_t>, std::size_t) {
            ++callbackCount;
            if (injectPartialFailure && callbackCount == 2) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::ioFailure(
                        "fragment sink failure", -99));
            }
            ++successfulDatagrams;
            return ::media::Status::success();
        });
    EXPECT_TRUE(ctx, session.configure(std::move(config.value())));
    EXPECT_TRUE(ctx, session.open());
    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    const int allocated = av_new_packet(packet.get(), 1000);
    EXPECT_TRUE(ctx, allocated >= 0);
    if (allocated < 0) return;
    packet->data[0] = 0;
    packet->data[1] = 0;
    packet->data[2] = 0;
    packet->data[3] = 1;
    packet->data[4] = 0x65;
    for (int index = 5; index < packet->size; ++index) {
        packet->data[index] = static_cast<std::uint8_t>(index);
    }
    packet->pts = packet->dts = 0;
    packet->flags = AV_PKT_FLAG_KEY;
    const auto timestamp = mappedTimestamp(ctx, 0u, 0);
    const auto status = session.writeAccessUnit(*packet, timestamp);
    EXPECT_FALSE(ctx, status);
    EXPECT_EQ(ctx, callbackCount, static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, successfulDatagrams, static_cast<std::size_t>(1));
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, ::media::ErrorCode::IoFailure);
        EXPECT_EQ(ctx, status.error().nativeCode, -99);
        EXPECT_EQ(ctx, status.error().message, std::string("fragment sink failure"));
    }
    const auto callbacksAtFailure = callbackCount;
    const auto poisoned = session.writeAccessUnit(*packet, timestamp);
    EXPECT_FALSE(ctx, poisoned);
    if (!poisoned) {
        EXPECT_EQ(ctx, poisoned.error().code, ::media::ErrorCode::IoFailure);
        EXPECT_EQ(ctx, poisoned.error().nativeCode, -99);
        EXPECT_EQ(ctx, poisoned.error().message, std::string("fragment sink failure"));
    }
    const auto poisonedTrailer = session.writeTrailer();
    EXPECT_FALSE(ctx, poisonedTrailer);
    if (!poisonedTrailer) {
        EXPECT_EQ(ctx, poisonedTrailer.error().code, ::media::ErrorCode::IoFailure);
        EXPECT_EQ(ctx, poisonedTrailer.error().nativeCode, -99);
        EXPECT_EQ(ctx, poisonedTrailer.error().message, std::string("fragment sink failure"));
    }
    EXPECT_EQ(ctx, callbackCount, callbacksAtFailure);
    EXPECT_TRUE(ctx, session.reset());

    injectPartialFailure = false;
    auto resetConfig = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *h264, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x01020304u, 256);
    EXPECT_TRUE(ctx, resetConfig);
    if (!resetConfig) return;
    EXPECT_TRUE(ctx, session.configure(std::move(resetConfig.value())));
    EXPECT_TRUE(ctx, session.open());
    EXPECT_TRUE(ctx, session.writeAccessUnit(*packet, timestamp));
    EXPECT_TRUE(ctx, callbackCount > callbacksAtFailure);
    EXPECT_TRUE(ctx, session.writeTrailer());
}

} // namespace

void runScheduledRtpPacketizationTests(TestContext& ctx)
{
    testScheduledRtpCodecParametersSeparateEncoderAndRtpTimeBases(ctx);
    testScheduledRtpCodecParametersCanonicalizeNativeAacAsc(ctx);
    testDatagramWriteAvioLifecycleAndFailure(ctx);
    testRtpDatagramRewritePreservesStructure(ctx);
    testScheduledMuxRealPacketization(ctx);
    testScheduledMuxNormalizesAvccForExplicitAnnexB(ctx);
    testScheduledMuxPropagatesSinkFailure(ctx);
}
