#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaTsMuxPlan muxPlan()
{
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x100, 0x101, 0x102, 0x101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, 4,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp}).value();
}

MediaBufferRef packetBuffer(MediaStreamKind stream, bool keyFrame, int size = 4)
{
    auto packet = ::media::ffmpeg::makePacket();
    if (packet && size > 0 && av_new_packet(packet.get(), size) == 0) {
        for (int index = 0; index < size; ++index) {
            packet->data[index] = static_cast<std::uint8_t>(index + 1);
        }
    }
    if (packet && keyFrame) packet->flags |= AV_PKT_FLAG_KEY;
    auto buffer = std::make_shared<FFmpegPacketBuffer>(std::move(packet), std::nullopt);
    buffer->setStreamKind(stream);
    return buffer;
}

void runtimePlanBufferIsImmutableAndValidated(TestContext& ctx)
{
    static_assert(!std::is_copy_constructible_v<MediaTsMuxRuntimePlanBuffer>);
    static_assert(static_cast<int>(MediaBufferType::OutputByteSink) == 9);
    static_assert(static_cast<int>(MediaBufferType::TsMuxRuntimePlan) == 10);
    static_assert(static_cast<int>(MediaBufferType::TsAccessUnit) == 11);
    static_assert(static_cast<int>(MediaPayloadKind::OutputByteSink) == 14);
    static_assert(static_cast<int>(MediaPayloadKind::TsMuxRuntimePlan) == 15);
    static_assert(static_cast<int>(MediaPayloadKind::TsAccessUnit) == 16);
    const MediaPlaybackEpoch epoch{
        MediaRunningTime::fromNanoseconds(2'000'000'000),
        MediaRunningTime::fromNanoseconds(3'000'000'000), 17};
    auto created = MediaTsMuxRuntimePlanBuffer::create(
        muxPlan(), epoch, MediaAvSyncGroupKey("program-a"));
    EXPECT_TRUE(ctx, created);
    if (created) {
        auto* buffer = dynamic_cast<MediaTsMuxRuntimePlanBuffer*>(created.value().get());
        EXPECT_TRUE(ctx, buffer != nullptr);
        if (buffer) {
            EXPECT_EQ(ctx, buffer->type(), MediaBufferType::TsMuxRuntimePlan);
            EXPECT_EQ(ctx, buffer->payloadKind(), MediaPayloadKind::TsMuxRuntimePlan);
            EXPECT_EQ(ctx, buffer->streamKind(), MediaStreamKind::Metadata);
            EXPECT_EQ(ctx, buffer->plan().parameters().videoPid, std::uint16_t{0x101});
            EXPECT_EQ(ctx, buffer->epoch(), epoch);
            EXPECT_EQ(ctx, buffer->group(), MediaAvSyncGroupKey("program-a"));
        }
    }
    EXPECT_FALSE(ctx, MediaTsMuxRuntimePlanBuffer::create(
                          muxPlan(), epoch, MediaAvSyncGroupKey("")));
    EXPECT_FALSE(ctx, MediaTsMuxRuntimePlanBuffer::create(
                          muxPlan(),
                          MediaPlaybackEpoch{epoch.sourceStart, epoch.masterRelease, 0},
                          MediaAvSyncGroupKey("program-a")));
}

void accessUnitOwnsPacketAndExposesSynchronousView(TestContext& ctx)
{
    std::weak_ptr<MediaBuffer> lifetime;
    auto outer = packetBuffer(MediaStreamKind::Video, true);
    lifetime = outer;
    auto created = MediaTsAccessUnitBuffer::create(
        outer, MediaScheduledStream::Video, 17,
        MediaRunningTime::fromNanoseconds(250'000'000),
        MediaRunningTime::fromNanoseconds(220'000'000),
        MediaRunningTime::fromNanoseconds(120'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, created);
    outer.reset();
    EXPECT_FALSE(ctx, lifetime.expired());
    if (!created) return;

    auto* buffer = dynamic_cast<MediaTsAccessUnitBuffer*>(created.value().get());
    EXPECT_TRUE(ctx, buffer != nullptr);
    if (!buffer) return;
    EXPECT_EQ(ctx, buffer->type(), MediaBufferType::TsAccessUnit);
    EXPECT_EQ(ctx, buffer->payloadKind(), MediaPayloadKind::TsAccessUnit);
    const auto view = buffer->view();
    EXPECT_TRUE(ctx, view);
    if (!view) return;
    EXPECT_EQ(ctx, view.value().payload.size(), std::size_t{4});
    EXPECT_EQ(ctx, view.value().payload[0], std::uint8_t{1});
    EXPECT_EQ(ctx, view.value().stream, MediaScheduledStream::Video);
    EXPECT_EQ(ctx, view.value().generation, std::uint64_t{17});
    EXPECT_EQ(ctx, view.value().presentationOnMaster,
              MediaRunningTime::fromNanoseconds(250'000'000));
    EXPECT_EQ(ctx, view.value().dispatchOnMaster,
              MediaRunningTime::fromNanoseconds(220'000'000));
    EXPECT_EQ(ctx, view.value().emitOnMaster,
              MediaRunningTime::fromNanoseconds(120'000'000));
    EXPECT_TRUE(ctx, view.value().randomAccess);
}

void accessUnitDerivesStreamSpecificRandomAccess(TestContext& ctx)
{
    auto videoNonKey = MediaTsAccessUnitBuffer::create(
        packetBuffer(MediaStreamKind::Video, false), MediaScheduledStream::Video, 1,
        MediaRunningTime::fromNanoseconds(100), MediaRunningTime::fromNanoseconds(100),
        MediaRunningTime::fromNanoseconds(50), MediaRunningTime::fromNanoseconds(50));
    EXPECT_TRUE(ctx, videoNonKey);
    if (videoNonKey) {
        auto* buffer = dynamic_cast<MediaTsAccessUnitBuffer*>(videoNonKey.value().get());
        EXPECT_TRUE(ctx, buffer != nullptr);
        if (buffer) {
            auto view = buffer->view();
            EXPECT_TRUE(ctx, view);
            if (view) EXPECT_FALSE(ctx, view.value().randomAccess);
        }
    }

    auto audioKey = MediaTsAccessUnitBuffer::create(
        packetBuffer(MediaStreamKind::Audio, true), MediaScheduledStream::Audio, 1,
        MediaRunningTime::fromNanoseconds(100), MediaRunningTime::fromNanoseconds(100),
        MediaRunningTime::fromNanoseconds(50), MediaRunningTime::fromNanoseconds(50));
    EXPECT_TRUE(ctx, audioKey);
    if (audioKey) {
        auto* buffer = dynamic_cast<MediaTsAccessUnitBuffer*>(audioKey.value().get());
        EXPECT_TRUE(ctx, buffer != nullptr);
        if (buffer) {
            auto view = buffer->view();
            EXPECT_TRUE(ctx, view);
            if (view) EXPECT_FALSE(ctx, view.value().randomAccess);
        }
    }
}

void accessUnitRejectsInvalidOuterAndTiming(TestContext& ctx)
{
    const auto create = [](MediaBufferRef outer,
                           MediaScheduledStream stream,
                           std::uint64_t generation,
                           MediaRunningTime dispatch,
                           MediaRunningTime emission,
                           MediaRunningTime lead) {
        return MediaTsAccessUnitBuffer::create(
            std::move(outer), stream, generation, dispatch, dispatch,
            emission, lead);
    };
    EXPECT_FALSE(ctx, create(nullptr, MediaScheduledStream::Video, 1,
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(50),
                             MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(
        std::make_shared<MediaAvStartupClockBuffer>(MediaRunningTime::fromNanoseconds(0)),
        MediaScheduledStream::Video, 1, MediaRunningTime::fromNanoseconds(100),
        MediaRunningTime::fromNanoseconds(50), MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Audio, false),
                             MediaScheduledStream::Video, 1,
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(50),
                             MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Video, false),
                             static_cast<MediaScheduledStream>(2), 1,
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(50),
                             MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Video, false),
                             MediaScheduledStream::Video, 0,
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(50),
                             MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Video, false),
                             MediaScheduledStream::Video, 1,
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(51),
                             MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Video, false),
                             MediaScheduledStream::Video, 1,
                             MediaRunningTime::fromNanoseconds(50),
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Video, false),
                             MediaScheduledStream::Video, 1,
                             MediaRunningTime::fromNanoseconds(
                                 std::numeric_limits<std::int64_t>::max()),
                             MediaRunningTime::fromNanoseconds(-1),
                             MediaRunningTime::fromNanoseconds(50)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Video, false),
                             MediaScheduledStream::Video, 1,
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(0)));
    EXPECT_FALSE(ctx, create(packetBuffer(MediaStreamKind::Video, false, 0),
                             MediaScheduledStream::Video, 1,
                             MediaRunningTime::fromNanoseconds(100),
                             MediaRunningTime::fromNanoseconds(50),
                             MediaRunningTime::fromNanoseconds(50)));
}

void accessUnitViewFailsIfOuterPacketWasTransferred(TestContext& ctx)
{
    auto outer = packetBuffer(MediaStreamKind::Video, true);
    auto created = MediaTsAccessUnitBuffer::create(
        outer, MediaScheduledStream::Video, 1,
        MediaRunningTime::fromNanoseconds(200),
        MediaRunningTime::fromNanoseconds(150),
        MediaRunningTime::fromNanoseconds(100),
        MediaRunningTime::fromNanoseconds(50));
    EXPECT_TRUE(ctx, created);
    auto* packet = dynamic_cast<FFmpegPacketBuffer*>(outer.get());
    EXPECT_TRUE(ctx, packet != nullptr);
    if (!created || !packet) return;
    auto transferred = packet->takePacket();
    EXPECT_TRUE(ctx, transferred != nullptr);
    auto* accessUnit = dynamic_cast<MediaTsAccessUnitBuffer*>(created.value().get());
    EXPECT_TRUE(ctx, accessUnit != nullptr);
    if (accessUnit) EXPECT_FALSE(ctx, accessUnit->view());
}

} // namespace

void runMpegTsMuxRuntimeBufferTests(TestContext& ctx)
{
    runtimePlanBufferIsImmutableAndValidated(ctx);
    accessUnitOwnsPacketAndExposesSynchronousView(ctx);
    accessUnitDerivesStreamSpecificRandomAccess(ctx);
    accessUnitRejectsInvalidOuterAndTiming(ctx);
    accessUnitViewFailsIfOuterPacketWasTransferred(ctx);
}
