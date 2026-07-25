#include "common/TestAssert.h"

#include "internal/graph/nodes/mux/ScheduledRtpSenderConfig.h"
#include "internal/graph/nodes/mux/ScheduledRtpSenderSession.h"
#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSessionFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
}

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

static_assert(!std::is_default_constructible_v<ScheduledRtpSenderConfig>);
static_assert(!std::is_default_constructible_v<ScheduledRtpSenderCounters>);
static_assert(!std::is_default_constructible_v<ScheduledRtcpDispatchDiagnostics>);
static_assert(!std::is_default_constructible_v<ScheduledRtcpDispatchResult>);
static_assert(!std::is_default_constructible_v<MediaRtcpSenderReportCommitToken>);

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

::media::ffmpeg::CodecParametersPtr makeAacParameters()
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (!parameters) return {};
    parameters->codec_type = AVMEDIA_TYPE_AUDIO;
    parameters->codec_id = AV_CODEC_ID_AAC;
    parameters->sample_rate = 48'000;
    av_channel_layout_default(&parameters->ch_layout, 2);
    parameters->extradata = static_cast<std::uint8_t*>(
        av_mallocz(AV_INPUT_BUFFER_PADDING_SIZE + 2));
    if (!parameters->extradata) return {};
    parameters->extradata[0] = 0x11;
    parameters->extradata[1] = 0x90;
    parameters->extradata_size = 2;
    return parameters;
}

std::uint32_t readU32(std::span<const std::uint8_t> bytes,
                      std::size_t offset)
{
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

ScheduledRtpMuxFfmpegSessionFactory& packetizerFactory()
{
    static ScheduledRtpMuxFfmpegSessionFactory factory;
    return factory;
}

::media::Result<std::vector<std::uint8_t>> parseRtpPayloadIndependently(
    std::span<const std::uint8_t> datagram)
{
    using PayloadResult = ::media::Result<std::vector<std::uint8_t>>;
    if (datagram.size() < 12 || (datagram[0] >> 6) != 2) {
        return PayloadResult::failure(
            ::media::ErrorInfo::invalidArgument("invalid test RTP packet"));
    }
    std::size_t offset = 12 + 4 * static_cast<std::size_t>(datagram[0] & 0x0F);
    if (offset > datagram.size()) {
        return PayloadResult::failure(
            ::media::ErrorInfo::invalidArgument("truncated test RTP CSRC list"));
    }
    if ((datagram[0] & 0x10) != 0) {
        if (datagram.size() - offset < 4) {
            return PayloadResult::failure(
                ::media::ErrorInfo::invalidArgument("truncated test RTP extension"));
        }
        const auto words = (static_cast<std::size_t>(datagram[offset + 2]) << 8) |
                           static_cast<std::size_t>(datagram[offset + 3]);
        offset += 4 + 4 * words;
        if (offset > datagram.size()) {
            return PayloadResult::failure(
                ::media::ErrorInfo::invalidArgument("invalid test RTP extension"));
        }
    }
    auto end = datagram.size();
    if ((datagram[0] & 0x20) != 0) {
        const auto padding = static_cast<std::size_t>(datagram.back());
        if (padding == 0 || padding > end - offset) {
            return PayloadResult::failure(
                ::media::ErrorInfo::invalidArgument("invalid test RTP padding"));
        }
        end -= padding;
    }
    return PayloadResult::success(
        std::vector<std::uint8_t>(datagram.begin() + offset,
                                  datagram.begin() + end));
}

class OpenFailingPacketizerSession final : public ScheduledRtpPacketizerSession {
public:
    OpenFailingPacketizerSession(
        ScheduledRtpMuxStreamConfig config,
        ScheduledRtpRewrittenDatagramSink sink)
        : m_config(std::move(config)), m_sink(std::move(sink))
    {
    }

    ::media::Status open() override
    {
        return ::media::Status::failure(
            ::media::ErrorInfo::ioFailure("scripted packetizer open failure", -71));
    }

    ::media::Status writeAccessUnit(
        const AVPacket&, MediaRtpTimestamp) override
    {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scripted packetizer must not receive an access unit"));
    }

private:
    ScheduledRtpMuxStreamConfig m_config;
    ScheduledRtpRewrittenDatagramSink m_sink;
};

class OpenFailingPacketizerFactory final : public ScheduledRtpPacketizerFactory {
public:
    ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>> create(
        ScheduledRtpMuxStreamConfig config,
        ScheduledRtpRewrittenDatagramSink sink) override
    {
        std::unique_ptr<ScheduledRtpPacketizerSession> session =
            std::make_unique<OpenFailingPacketizerSession>(
                std::move(config), std::move(sink));
        return ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>::success(
            std::move(session));
    }
};

class EmptySuccessPacketizerFactory final : public ScheduledRtpPacketizerFactory {
public:
    ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>> create(
        ScheduledRtpMuxStreamConfig,
        ScheduledRtpRewrittenDatagramSink) override
    {
        return ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>>::success(
            nullptr);
    }
};

::media::Result<ScheduledRtpSenderConfig> makeVideoConfig(
    std::uint64_t generation,
    ScheduledRtpSenderCounters counters,
    std::string cname = "sync@test",
    int maximumDatagramBytes = 256)
{
    auto parameters = makeH264Parameters();
    if (!parameters) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            ::media::ErrorInfo::allocationFailed("test H264 parameters"));
    }
    auto stream = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video,
        *parameters,
        AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96,
        0x11223344u,
        maximumDatagramBytes);
    if (!stream) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(stream.error());
    }
    auto epoch = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0),
        std::chrono::seconds(1'700'000'000));
    if (!epoch) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(epoch.error());
    }
    auto mapper = MediaRtpOutputClockMapper::create(
        90'000,
        0xFFFFFFF0u,
        MediaRunningTime::fromNanoseconds(0));
    if (!mapper) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(mapper.error());
    }
    auto schedule = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(5'000'000'000),
        generation);
    if (!schedule) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(schedule.error());
    }
    return ScheduledRtpSenderConfig::create(
        std::move(stream.value()),
        epoch.value(),
        mapper.value(),
        schedule.value(),
        std::move(cname),
        generation,
        counters);
}

::media::Result<ScheduledRtpSenderConfig> makeAudioConfig(
    std::uint64_t generation,
    ScheduledRtpSenderCounters counters)
{
    auto parameters = makeAacParameters();
    if (!parameters) {
        return ::media::Result<ScheduledRtpSenderConfig>::failure(
            ::media::ErrorInfo::allocationFailed("test AAC parameters"));
    }
    auto stream = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Audio,
        *parameters,
        AVRational{1, 48'000},
        MediaScheduledRtpPacketizationMode::AacLatm,
        97,
        0x55667788u,
        1200);
    auto epoch = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::seconds(100));
    auto mapper = MediaRtpOutputClockMapper::create(
        48'000, 200u, MediaRunningTime::fromNanoseconds(0));
    auto schedule = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(1'000'000'000),
        MediaRunningTime::fromNanoseconds(5'000'000'000), generation);
    if (!stream) return ::media::Result<ScheduledRtpSenderConfig>::failure(stream.error());
    if (!epoch) return ::media::Result<ScheduledRtpSenderConfig>::failure(epoch.error());
    if (!mapper) return ::media::Result<ScheduledRtpSenderConfig>::failure(mapper.error());
    if (!schedule) return ::media::Result<ScheduledRtpSenderConfig>::failure(schedule.error());
    return ScheduledRtpSenderConfig::create(
        std::move(stream.value()), epoch.value(), mapper.value(), schedule.value(),
        "sync@test", generation, counters);
}

ScheduledRtpSenderCounters zeroCounters(TestContext& ctx)
{
    auto counters = ScheduledRtpSenderCounters::create(0, 0);
    EXPECT_TRUE(ctx, counters);
    return counters.value();
}

::media::ffmpeg::PacketPtr makeLargeH264Packet()
{
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet || av_new_packet(packet.get(), 1000) < 0) return {};
    packet->data[0] = 0;
    packet->data[1] = 0;
    packet->data[2] = 0;
    packet->data[3] = 1;
    packet->data[4] = 0x65;
    for (int index = 5; index < packet->size; ++index) {
        packet->data[index] = static_cast<std::uint8_t>(index);
    }
    packet->pts = packet->dts = 12345;
    packet->flags = AV_PKT_FLAG_KEY;
    return packet;
}

void testCompleteConfigurationAndClockValidation(TestContext& ctx)
{
    auto counters = zeroCounters(ctx);
    EXPECT_FALSE(ctx, makeVideoConfig(0, counters));
    EXPECT_FALSE(ctx, makeVideoConfig(1, counters, ""));
    EXPECT_FALSE(ctx, makeVideoConfig(1, counters, std::string("bad\xC0\xAF", 5)));

    auto parameters = makeH264Parameters();
    EXPECT_TRUE(ctx, parameters != nullptr);
    if (!parameters) return;
    auto stream = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *parameters, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x11223344u, 256);
    auto epoch = MediaSharedNtpEpoch::create(
        MediaRunningTime::fromNanoseconds(0), std::chrono::seconds(1));
    auto wrongMapper = MediaRtpOutputClockMapper::create(
        48'000, 123u, MediaRunningTime::fromNanoseconds(0));
    auto schedule = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(1), 2);
    EXPECT_TRUE(ctx, stream);
    EXPECT_TRUE(ctx, epoch);
    EXPECT_TRUE(ctx, wrongMapper);
    EXPECT_TRUE(ctx, schedule);
    if (stream && epoch && wrongMapper && schedule) {
        EXPECT_FALSE(ctx, ScheduledRtpSenderConfig::create(
            std::move(stream.value()), epoch.value(), wrongMapper.value(),
            schedule.value(), "sync@test", 1, counters));
    }

    auto clockStream = ScheduledRtpMuxStreamConfig::create(
        MediaStreamKind::Video, *parameters, AVRational{1, 90'000},
        MediaScheduledRtpPacketizationMode::H264AnnexB,
        96, 0x11223344u, 256);
    auto generationOneSchedule = MediaRtcpSenderReportSchedule::create(
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(1),
        MediaRunningTime::fromNanoseconds(1), 1);
    EXPECT_TRUE(ctx, clockStream);
    EXPECT_TRUE(ctx, generationOneSchedule);
    if (clockStream && epoch && wrongMapper && generationOneSchedule) {
        EXPECT_FALSE(ctx, ScheduledRtpSenderConfig::create(
            std::move(clockStream.value()), epoch.value(), wrongMapper.value(),
            generationOneSchedule.value(), "sync@test", 1, counters));
    }

    auto valid = makeVideoConfig(1, counters);
    EXPECT_TRUE(ctx, valid);
    if (!valid) return;
    EXPECT_FALSE(ctx, ScheduledRtpSenderSession::create(
        std::move(valid.value()), {},
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        packetizerFactory()));

    auto emptyFactoryConfig = makeVideoConfig(1, counters);
    EXPECT_TRUE(ctx, emptyFactoryConfig);
    if (!emptyFactoryConfig) return;
    EmptySuccessPacketizerFactory emptyFactory;
    auto emptyPacketizer = ScheduledRtpSenderSession::create(
        std::move(emptyFactoryConfig.value()),
        [](std::span<const std::uint8_t>, std::size_t) {
            return ::media::Status::success();
        },
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        emptyFactory);
    EXPECT_FALSE(ctx, emptyPacketizer);
    if (!emptyPacketizer) {
        EXPECT_EQ(ctx, emptyPacketizer.error().code,
                  ::media::ErrorCode::InternalError);
        EXPECT_EQ(ctx, emptyPacketizer.error().message,
                  std::string("scheduled RTP packetizer factory returned no session"));
    }
}

void testSenderLifecycleAndOpenFailurePoison(TestContext& ctx)
{
    auto createdConfig = makeVideoConfig(1, zeroCounters(ctx));
    EXPECT_TRUE(ctx, createdConfig);
    if (!createdConfig) return;
    auto created = ScheduledRtpSenderSession::create(
        std::move(createdConfig.value()),
        [](std::span<const std::uint8_t>, std::size_t) {
            return ::media::Status::success();
        },
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        packetizerFactory());
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto packet = makeLargeH264Packet();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    EXPECT_FALSE(ctx, created.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000)));
    EXPECT_FALSE(ctx, created.value()->sendAccessUnit(
        *packet, MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, created.value()->open());
    const auto duplicateOpen = created.value()->open();
    EXPECT_FALSE(ctx, duplicateOpen);
    EXPECT_TRUE(ctx, created.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000)));

    auto failingConfig = makeAudioConfig(2, zeroCounters(ctx));
    EXPECT_TRUE(ctx, failingConfig);
    if (!failingConfig) return;
    OpenFailingPacketizerFactory openFailingFactory;
    auto failing = ScheduledRtpSenderSession::create(
        std::move(failingConfig.value()),
        [](std::span<const std::uint8_t>, std::size_t) {
            return ::media::Status::success();
        },
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        openFailingFactory);
    EXPECT_TRUE(ctx, failing);
    if (!failing) return;
    const auto openFailure = failing.value()->open();
    EXPECT_FALSE(ctx, openFailure);
    if (openFailure) return;
    const auto poisonedAu = failing.value()->sendAccessUnit(
        *packet, MediaRunningTime::fromNanoseconds(0));
    const auto poisonedSr = failing.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_FALSE(ctx, poisonedAu);
    EXPECT_FALSE(ctx, poisonedSr);
    if (!poisonedAu && !poisonedSr) {
        EXPECT_EQ(ctx, poisonedAu.error().message, openFailure.error().message);
        EXPECT_EQ(ctx, poisonedSr.error().message, openFailure.error().message);
    }
}

void testFragmentCountersAndPartialFailurePoison(TestContext& ctx)
{
    auto config = makeVideoConfig(1, zeroCounters(ctx));
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    std::size_t acceptedPackets = 0;
    std::uint64_t acceptedOctets = 0;
    auto session = ScheduledRtpSenderSession::create(
        std::move(config.value()),
        [&acceptedPackets, &acceptedOctets](std::span<const std::uint8_t>,
                                             std::size_t payloadOctets) {
            if (acceptedPackets == 1) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::ioFailure("RTP fragment failed", -41));
            }
            ++acceptedPackets;
            acceptedOctets += payloadOctets;
            return ::media::Status::success();
        },
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        packetizerFactory());
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    EXPECT_TRUE(ctx, session.value()->open());
    auto packet = makeLargeH264Packet();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    const auto sent = session.value()->sendAccessUnit(
        *packet, MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_FALSE(ctx, sent);
    EXPECT_EQ(ctx, acceptedPackets, static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, session.value()->counters().packetCount(), std::uint64_t{1});
    EXPECT_EQ(ctx, session.value()->counters().octetCount(), acceptedOctets);
    const auto poisoned = session.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_FALSE(ctx, poisoned);
    if (!sent && !poisoned) {
        EXPECT_EQ(ctx, sent.error().message, std::string("RTP fragment failed"));
        EXPECT_EQ(ctx, poisoned.error().message, sent.error().message);
    }
}

void testSuccessfulH264AndAacPayloadCounters(TestContext& ctx)
{
    auto videoConfig = makeVideoConfig(1, zeroCounters(ctx));
    EXPECT_TRUE(ctx, videoConfig);
    if (!videoConfig) return;
    std::vector<std::size_t> videoPayloads;
    std::vector<std::uint32_t> videoTimestamps;
    std::vector<std::uint8_t> videoReport;
    auto video = ScheduledRtpSenderSession::create(
        std::move(videoConfig.value()),
        [&videoPayloads, &videoTimestamps](
            std::span<const std::uint8_t> datagram,
            std::size_t payloadOctets) {
            if (datagram.size() != payloadOctets + 12) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::internalError("unexpected test RTP header"));
            }
            videoPayloads.push_back(payloadOctets);
            videoTimestamps.push_back(readU32(datagram, 4));
            return ::media::Status::success();
        },
        [&videoReport](std::span<const std::uint8_t> datagram) {
            videoReport.assign(datagram.begin(), datagram.end());
            return ::media::Status::success();
        },
        packetizerFactory());
    EXPECT_TRUE(ctx, video);
    if (!video) return;
    EXPECT_TRUE(ctx, video.value()->open());
    auto videoPacket = makeLargeH264Packet();
    EXPECT_TRUE(ctx, videoPacket != nullptr);
    if (!videoPacket) return;
    EXPECT_TRUE(ctx, video.value()->sendAccessUnit(
        *videoPacket, MediaRunningTime::fromNanoseconds(1'000'000'000)));
    EXPECT_TRUE(ctx, videoPayloads.size() > static_cast<std::size_t>(1));
    for (const auto timestamp : videoTimestamps) {
        EXPECT_EQ(ctx, timestamp, 0x00015F80u);
    }
    const auto videoOctets = std::accumulate(
        videoPayloads.begin(), videoPayloads.end(), std::uint64_t{0});
    EXPECT_EQ(ctx, video.value()->counters().packetCount(),
              static_cast<std::uint64_t>(videoPayloads.size()));
    EXPECT_EQ(ctx, video.value()->counters().octetCount(), videoOctets);
    EXPECT_EQ(ctx, videoOctets,
              static_cast<std::uint64_t>(videoPacket->size - 5) +
                  2 * static_cast<std::uint64_t>(videoPayloads.size()));
    auto videoSr = video.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_TRUE(ctx, videoSr);
    EXPECT_TRUE(ctx, videoReport.size() >= static_cast<std::size_t>(28));
    if (videoReport.size() >= 28) {
        EXPECT_EQ(ctx, readU32(videoReport, 20),
                  static_cast<std::uint32_t>(videoPayloads.size()));
        EXPECT_EQ(ctx, readU32(videoReport, 24),
                  static_cast<std::uint32_t>(videoOctets));
    }

    auto audioConfig = makeAudioConfig(1, zeroCounters(ctx));
    EXPECT_TRUE(ctx, audioConfig);
    if (!audioConfig) return;
    std::vector<std::size_t> audioPayloadOctets;
    std::vector<std::vector<std::uint8_t>> audioDatagrams;
    auto audio = ScheduledRtpSenderSession::create(
        std::move(audioConfig.value()),
        [&audioPayloadOctets, &audioDatagrams](
            std::span<const std::uint8_t> datagram,
            std::size_t payloadOctets) {
            audioPayloadOctets.push_back(payloadOctets);
            audioDatagrams.emplace_back(datagram.begin(), datagram.end());
            return ::media::Status::success();
        },
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        packetizerFactory());
    EXPECT_TRUE(ctx, audio);
    if (!audio) return;
    EXPECT_TRUE(ctx, audio.value()->open());
    auto audioPacket = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, audioPacket != nullptr);
    if (!audioPacket || av_new_packet(audioPacket.get(), 100) < 0) return;
    for (int index = 0; index < audioPacket->size; ++index) {
        audioPacket->data[index] = static_cast<std::uint8_t>(index);
    }
    audioPacket->pts = audioPacket->dts = 999;
    EXPECT_TRUE(ctx, audio.value()->sendAccessUnit(
        *audioPacket, MediaRunningTime::fromNanoseconds(0)));
    EXPECT_EQ(ctx, audioDatagrams.size(), static_cast<std::size_t>(1));
    const auto firstAudioOctets = audio.value()->counters().octetCount();
    audioPacket->pts = audioPacket->dts = 2023;
    EXPECT_TRUE(ctx, audio.value()->sendAccessUnit(
        *audioPacket, MediaRunningTime::fromNanoseconds(1'000'000'000)));
    EXPECT_EQ(ctx, audioDatagrams.size(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, audio.value()->counters().packetCount(), std::uint64_t{2});
    std::uint64_t independentlyParsedOctets = 0;
    for (std::size_t index = 0; index < audioDatagrams.size(); ++index) {
        auto payload = parseRtpPayloadIndependently(audioDatagrams[index]);
        EXPECT_TRUE(ctx, payload);
        if (!payload) continue;
        EXPECT_EQ(ctx, audioPayloadOctets[index], payload.value().size());
        independentlyParsedOctets += payload.value().size();
        EXPECT_EQ(ctx, payload.value().size(), static_cast<std::size_t>(101));
        if (payload.value().size() == 101) {
            EXPECT_EQ(ctx, payload.value()[0], 100u);
            EXPECT_TRUE(ctx, std::equal(
                audioPacket->data, audioPacket->data + audioPacket->size,
                payload.value().begin() + 1));
        }
    }
    EXPECT_EQ(ctx, audio.value()->counters().octetCount(),
              independentlyParsedOctets);
    if (audioPayloadOctets.size() == 2) {
        EXPECT_EQ(ctx, audio.value()->counters().octetCount() - firstAudioOctets,
                  static_cast<std::uint64_t>(audioPayloadOctets[1]));
    }
}

void testCounterBoundaryFailsBeforeDelivery(TestContext& ctx)
{
    auto seeded = ScheduledRtpSenderCounters::create(
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max());
    EXPECT_TRUE(ctx, seeded);
    if (!seeded) return;
    auto config = makeVideoConfig(1, seeded.value());
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    std::size_t rtpCalls = 0;
    std::vector<std::uint8_t> senderReport;
    auto session = ScheduledRtpSenderSession::create(
        std::move(config.value()),
        [&rtpCalls](std::span<const std::uint8_t>, std::size_t) {
            ++rtpCalls;
            return ::media::Status::success();
        },
        [&senderReport](std::span<const std::uint8_t> datagram) {
            senderReport.assign(datagram.begin(), datagram.end());
            return ::media::Status::success();
        },
        packetizerFactory());
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    EXPECT_TRUE(ctx, session.value()->open());
    auto report = session.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_TRUE(ctx, report);
    EXPECT_TRUE(ctx, senderReport.size() >= static_cast<std::size_t>(28));
    if (senderReport.size() >= 28) {
        EXPECT_EQ(ctx, readU32(senderReport, 20),
                  std::numeric_limits<std::uint32_t>::max());
        EXPECT_EQ(ctx, readU32(senderReport, 24),
                  std::numeric_limits<std::uint32_t>::max());
    }
    auto packet = makeLargeH264Packet();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    EXPECT_FALSE(ctx, session.value()->sendAccessUnit(
        *packet, MediaRunningTime::fromNanoseconds(0)));
    EXPECT_EQ(ctx, rtpCalls, static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, session.value()->counters().packetCount(),
              static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()));
}

void testTransactionalSenderReportRetry(TestContext& ctx)
{
    auto config = makeVideoConfig(7, zeroCounters(ctx));
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    std::vector<std::vector<std::uint8_t>> reports;
    bool failRtcp = true;
    auto session = ScheduledRtpSenderSession::create(
        std::move(config.value()),
        [](std::span<const std::uint8_t>, std::size_t) {
            return ::media::Status::success();
        },
        [&reports, &failRtcp](std::span<const std::uint8_t> bytes) {
            reports.emplace_back(bytes.begin(), bytes.end());
            if (failRtcp) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::ioFailure("RTCP send failed", -55));
            }
            return ::media::Status::success();
        },
        packetizerFactory());
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    EXPECT_TRUE(ctx, session.value()->open());

    auto early = session.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(999'999'999));
    EXPECT_TRUE(ctx, early);
    if (early) EXPECT_EQ(ctx, early.value().kind(), ScheduledRtcpDispatchKind::NotDue);
    if (early) {
        EXPECT_EQ(ctx, early.value().nextDeadline(),
                  MediaRunningTime::fromNanoseconds(1'000'000'000));
        EXPECT_TRUE(ctx, early.value().sentDiagnostics() == nullptr);
    }
    EXPECT_TRUE(ctx, reports.empty());

    auto failed = session.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_FALSE(ctx, failed);
    EXPECT_EQ(ctx, reports.size(), static_cast<std::size_t>(1));
    failRtcp = false;
    auto retried = session.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'100'000'000));
    EXPECT_TRUE(ctx, retried);
    EXPECT_EQ(ctx, reports.size(), static_cast<std::size_t>(2));
    if (retried) {
        EXPECT_EQ(ctx, retried.value().kind(), ScheduledRtcpDispatchKind::Sent);
        const auto* diagnosticsPointer = retried.value().sentDiagnostics();
        EXPECT_TRUE(ctx, diagnosticsPointer != nullptr);
        if (!diagnosticsPointer) return;
        const auto& diagnostics = *diagnosticsPointer;
        EXPECT_EQ(ctx, diagnostics.scheduledDeadline(),
                  MediaRunningTime::fromNanoseconds(1'000'000'000));
        EXPECT_EQ(ctx, diagnostics.reportInstant(),
                  MediaRunningTime::fromNanoseconds(1'100'000'000));
        EXPECT_EQ(ctx, diagnostics.nextDeadline(),
                  MediaRunningTime::fromNanoseconds(2'000'000'000));
        EXPECT_EQ(ctx, diagnostics.lateness(),
                  MediaRunningTime::fromNanoseconds(100'000'000));
        EXPECT_EQ(ctx, diagnostics.skippedIntervals(), std::uint64_t{0});
        EXPECT_EQ(ctx, diagnostics.ntpTimestamp().seconds, 3'908'988'801u);
        EXPECT_EQ(ctx, diagnostics.ntpTimestamp().fraction, 429'496'729u);
        EXPECT_EQ(ctx, diagnostics.rtpTimestamp().wire(), 0x000182A8u);
        EXPECT_EQ(ctx, diagnostics.senderCounters().packetCount(), std::uint64_t{0});
        EXPECT_TRUE(ctx, reports.back().size() >= static_cast<std::size_t>(38 + 9));
        if (reports.back().size() >= 47) {
            EXPECT_EQ(ctx, readU32(reports.back(), 4), 0x11223344u);
            EXPECT_EQ(ctx, readU32(reports.back(), 8), 3'908'988'801u);
            EXPECT_EQ(ctx, readU32(reports.back(), 12), 429'496'729u);
            EXPECT_EQ(ctx, readU32(reports.back(), 16), 0x000182A8u);
            EXPECT_EQ(ctx, reports.back()[36], 1u);
            EXPECT_EQ(ctx, reports.back()[37], 9u);
            EXPECT_EQ(ctx, std::string(reports.back().begin() + 38,
                                       reports.back().begin() + 47),
                      std::string("sync@test"));
        }
    }
    auto nextEarly = session.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'500'000'000));
    EXPECT_TRUE(ctx, nextEarly);
    if (nextEarly) {
        EXPECT_EQ(ctx, nextEarly.value().kind(), ScheduledRtcpDispatchKind::NotDue);
    }
}

void testOperationReentryAndAmbiguousRtcpFailure(TestContext& ctx)
{
    auto config = makeVideoConfig(1, zeroCounters(ctx));
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    ScheduledRtpSenderSession* raw = nullptr;
    bool reentryRejected = false;
    auto session = ScheduledRtpSenderSession::create(
        std::move(config.value()),
        [&raw, &reentryRejected](std::span<const std::uint8_t>, std::size_t) {
            auto nested = raw->dispatchSenderReport(
                MediaRunningTime::fromNanoseconds(1'000'000'000));
            reentryRejected = !nested &&
                nested.error().code == ::media::ErrorCode::InvalidArgument;
            return ::media::Status::success();
        },
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        packetizerFactory());
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    raw = session.value().get();
    EXPECT_TRUE(ctx, raw->open());
    auto packet = makeLargeH264Packet();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;
    EXPECT_TRUE(ctx, raw->sendAccessUnit(
        *packet, MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, reentryRejected);

    auto reportReentryConfig = makeVideoConfig(3, zeroCounters(ctx));
    EXPECT_TRUE(ctx, reportReentryConfig);
    if (!reportReentryConfig) return;
    ScheduledRtpSenderSession* reportRaw = nullptr;
    bool reportReentryRejected = false;
    auto reportReentry = ScheduledRtpSenderSession::create(
        std::move(reportReentryConfig.value()),
        [](std::span<const std::uint8_t>, std::size_t) {
            return ::media::Status::success();
        },
        [&reportRaw, &reportReentryRejected, &packet](
            std::span<const std::uint8_t>) {
            auto nested = reportRaw->sendAccessUnit(
                *packet, MediaRunningTime::fromNanoseconds(0));
            reportReentryRejected = !nested &&
                nested.error().code == ::media::ErrorCode::InvalidArgument;
            return ::media::Status::success();
        },
        packetizerFactory());
    EXPECT_TRUE(ctx, reportReentry);
    if (!reportReentry) return;
    reportRaw = reportReentry.value().get();
    EXPECT_TRUE(ctx, reportRaw->open());
    EXPECT_TRUE(ctx, reportRaw->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000)));
    EXPECT_TRUE(ctx, reportReentryRejected);
    EXPECT_EQ(ctx, reportRaw->counters().packetCount(), std::uint64_t{0});

    auto throwingConfig = makeVideoConfig(2, zeroCounters(ctx));
    EXPECT_TRUE(ctx, throwingConfig);
    if (!throwingConfig) return;
    auto throwing = ScheduledRtpSenderSession::create(
        std::move(throwingConfig.value()),
        [](std::span<const std::uint8_t>, std::size_t) {
            return ::media::Status::success();
        },
        [](std::span<const std::uint8_t>) -> ::media::Status {
            throw std::runtime_error("ambiguous transport failure");
        },
        packetizerFactory());
    EXPECT_TRUE(ctx, throwing);
    if (!throwing) return;
    EXPECT_TRUE(ctx, throwing.value()->open());
    auto first = throwing.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_FALSE(ctx, first);
    auto poisoned = throwing.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(2'000'000'000));
    EXPECT_FALSE(ctx, poisoned);
    if (!first && !poisoned) {
        EXPECT_EQ(ctx, first.error().message, poisoned.error().message);
        EXPECT_EQ(ctx, first.error().code, ::media::ErrorCode::InternalError);
    }
}

void testAmbiguousRtpFailurePoisonsSender(TestContext& ctx)
{
    auto config = makeVideoConfig(1, zeroCounters(ctx));
    EXPECT_TRUE(ctx, config);
    if (!config) return;
    auto session = ScheduledRtpSenderSession::create(
        std::move(config.value()),
        [](std::span<const std::uint8_t>, std::size_t) -> ::media::Status {
            throw std::runtime_error("ambiguous RTP transport failure");
        },
        [](std::span<const std::uint8_t>) { return ::media::Status::success(); },
        packetizerFactory());
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    EXPECT_TRUE(ctx, session.value()->open());
    auto packet = makeLargeH264Packet();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!packet) return;

    const auto first = session.value()->sendAccessUnit(
        *packet, MediaRunningTime::fromNanoseconds(0));
    const auto poisonedAu = session.value()->sendAccessUnit(
        *packet, MediaRunningTime::fromNanoseconds(1'000'000'000));
    const auto poisonedSr = session.value()->dispatchSenderReport(
        MediaRunningTime::fromNanoseconds(1'000'000'000));
    EXPECT_FALSE(ctx, first);
    EXPECT_FALSE(ctx, poisonedAu);
    EXPECT_FALSE(ctx, poisonedSr);
    EXPECT_EQ(ctx, session.value()->counters().packetCount(), std::uint64_t{0});
    EXPECT_EQ(ctx, session.value()->counters().octetCount(), std::uint64_t{0});
    if (!first && !poisonedAu && !poisonedSr) {
        EXPECT_EQ(ctx, first.error().code, ::media::ErrorCode::InternalError);
        EXPECT_EQ(ctx, first.error().message,
                  std::string("datagram sink threw across the FFmpeg callback boundary"));
        EXPECT_EQ(ctx, poisonedAu.error().message, first.error().message);
        EXPECT_EQ(ctx, poisonedSr.error().message, first.error().message);
    }
}

} // namespace

void runScheduledRtpSenderTests(TestContext& ctx)
{
    testCompleteConfigurationAndClockValidation(ctx);
    testSenderLifecycleAndOpenFailurePoison(ctx);
    testFragmentCountersAndPartialFailurePoison(ctx);
    testSuccessfulH264AndAacPayloadCounters(ctx);
    testCounterBoundaryFailsBeforeDelivery(ctx);
    testTransactionalSenderReportRetry(ctx);
    testOperationReentryAndAmbiguousRtcpFailure(ctx);
    testAmbiguousRtpFailurePoisonsSender(ctx);
}
