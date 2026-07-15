#include "common/TestAssert.h"

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/mux/MediaMuxSessionFactory.h"
#include "internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/channel_layout.h>
}

#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaRunningTime ms(std::int64_t value)
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class ManualClock final : public MediaMasterClock {
public:
    explicit ManualClock(MediaRunningTime value) : nowValue(value) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(nowValue);
    }

    MediaRunningTime nowValue;
};

struct SinkState final {
    std::vector<std::uint8_t> bytes;
    std::size_t flushes = 0;
    std::size_t closes = 0;
};

class RecordingSink final : public MediaOutputByteSink {
public:
    explicit RecordingSink(std::shared_ptr<SinkState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override
    {
        m_state->bytes.insert(m_state->bytes.end(), bytes.begin(), bytes.end());
        return ::media::Result<std::size_t>::success(bytes.size());
    }

    ::media::Status flush() override
    {
        ++m_state->flushes;
        return ::media::Status::success();
    }

    ::media::Status close() override
    {
        ++m_state->closes;
        return ::media::Status::success();
    }

private:
    std::shared_ptr<SinkState> m_state;
};

MediaTsMuxPlan muxPlan()
{
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x100, 0x101, 0x102, 0x101, 0,
        ms(100), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, 4,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{ms(20), ms(100), ms(5), 1, 90'000},
        ms(100), 188, MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp}).value();
}

MediaAvSyncPlan avSyncPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "project-ts-adapter";
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 48'000;
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 64;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = ms(40);
    auto plan = MediaAvSyncPlanner::plan(request).value();
    plan.audioServo.commandLeadNs = ms(1'500);
    plan.audioServo.compensationWindowNs = ms(2'000);
    plan.audioServo.frequencyFilterTimeConstantNs = ms(5'000);
    return plan;
}

struct Fixture final {
    MediaGraphExecutionContext context;
    MediaAvSyncGroupKey group{"project-ts-group"};
    MediaPlaybackEpoch epoch{ms(0), ms(1'000), 7};
    std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>(epoch.masterRelease);

    bool activate(TestContext& ctx)
    {
        EXPECT_TRUE(ctx, context.registerAvSyncGroup(group, avSyncPlan(), clock));
        EXPECT_TRUE(ctx, context.activatePlaybackEpoch(group, epoch));
        return context.findAvSyncGroup(group) != nullptr;
    }

    MediaBufferRef planBuffer() const
    {
        return MediaTsMuxRuntimePlanBuffer::create(muxPlan(), epoch, group).value();
    }

    MediaBufferRef sinkBuffer(const std::shared_ptr<SinkState>& state) const
    {
        auto created = MediaOutputByteSinkBuffer::create(
            std::make_unique<RecordingSink>(state));
        return MediaBufferRef(std::move(created).value());
    }
};

MediaBufferRef codecParameters(MediaStreamKind kind)
{
    auto parameters = ::media::ffmpeg::makeCodecParameters();
    if (kind == MediaStreamKind::Video) {
        static constexpr std::array<std::uint8_t, 17> avcc{
            1, 0x42, 0, 0x1E, 0xFF, 0xE1, 0, 4, 0x67, 0x42, 0, 0x1E,
            1, 0, 2, 0x68, 0xCE};
        parameters->codec_type = AVMEDIA_TYPE_VIDEO;
        parameters->codec_id = AV_CODEC_ID_H264;
        parameters->extradata = static_cast<std::uint8_t*>(
            av_mallocz(avcc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        parameters->extradata_size = static_cast<int>(avcc.size());
        std::memcpy(parameters->extradata, avcc.data(), avcc.size());
    } else {
        static constexpr std::array<std::uint8_t, 2> asc{0x11, 0x90};
        parameters->codec_type = AVMEDIA_TYPE_AUDIO;
        parameters->codec_id = AV_CODEC_ID_AAC;
        parameters->sample_rate = 48'000;
        parameters->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        parameters->extradata = static_cast<std::uint8_t*>(
            av_mallocz(asc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        parameters->extradata_size = static_cast<int>(asc.size());
        std::memcpy(parameters->extradata, asc.data(), asc.size());
    }
    auto buffer = makeMediaBufferRef<FFmpegCodecParametersBuffer>(
        std::move(parameters));
    buffer->setStreamKind(kind);
    return buffer;
}

MediaBufferRef accessUnit(MediaScheduledStream stream,
                          std::uint64_t generation,
                          MediaRunningTime emission,
                          MediaRunningTime lead = ms(100))
{
    auto packet = ::media::ffmpeg::makePacket();
    av_new_packet(packet.get(), stream == MediaScheduledStream::Video ? 7 : 3);
    if (stream == MediaScheduledStream::Video) {
        const std::array<std::uint8_t, 7> payload{0, 0, 0, 3, 0x65, 1, 2};
        std::memcpy(packet->data, payload.data(), payload.size());
        packet->flags |= AV_PKT_FLAG_KEY;
    } else {
        const std::array<std::uint8_t, 3> payload{1, 2, 3};
        std::memcpy(packet->data, payload.data(), payload.size());
    }
    auto outer = std::make_shared<FFmpegPacketBuffer>(std::move(packet), std::nullopt);
    outer->setStreamKind(stream == MediaScheduledStream::Video
                             ? MediaStreamKind::Video
                             : MediaStreamKind::Audio);
    auto dispatch = emission.checkedAdd(lead).value();
    return MediaTsAccessUnitBuffer::create(
        outer, stream, generation, dispatch, dispatch, emission, lead).value();
}

void bindComplete(ProjectMpegTsMuxSessionAdapter& adapter,
                  Fixture& fixture,
                  const std::shared_ptr<SinkState>& sink,
                  TestContext& ctx,
                  bool reverse)
{
    if (reverse) {
        EXPECT_TRUE(ctx, adapter.bindStreamConfig(
                             fixture.context, codecParameters(MediaStreamKind::Audio)));
        EXPECT_TRUE(ctx, adapter.bindResource(fixture.context, fixture.sinkBuffer(sink)));
        EXPECT_TRUE(ctx, adapter.bindStreamConfig(
                             fixture.context, codecParameters(MediaStreamKind::Video)));
        EXPECT_TRUE(ctx, adapter.bindResource(fixture.context, fixture.planBuffer()));
    } else {
        EXPECT_TRUE(ctx, adapter.bindResource(fixture.context, fixture.planBuffer()));
        EXPECT_TRUE(ctx, adapter.bindStreamConfig(
                             fixture.context, codecParameters(MediaStreamKind::Video)));
        EXPECT_TRUE(ctx, adapter.bindStreamConfig(
                             fixture.context, codecParameters(MediaStreamKind::Audio)));
        EXPECT_TRUE(ctx, adapter.bindResource(fixture.context, fixture.sinkBuffer(sink)));
    }
}

void factoryRequiresBothStreams(TestContext& ctx)
{
    ExplicitMediaMuxSessionFactory factory;
    MediaNodeOptions options;
    options.set(MediaTranscodeOptionKey::MuxSessionKind, "project_mpegts");
    options.set(MediaTranscodeOptionKey::MuxExpectVideo, "1");
    options.set(MediaTranscodeOptionKey::MuxExpectAudio, "1");
    auto created = factory.create(options);
    EXPECT_TRUE(ctx, created);
    if (created) {
        EXPECT_TRUE(ctx, dynamic_cast<ProjectMpegTsMuxSessionAdapter*>(
                             created.value().get()) != nullptr);
    }
    options.set(MediaTranscodeOptionKey::MuxExpectAudio, "0");
    EXPECT_FALSE(ctx, factory.create(options));
    options.set(MediaTranscodeOptionKey::MuxExpectAudio, "1");
    options.set(MediaTranscodeOptionKey::MuxExpectVideo, "0");
    EXPECT_FALSE(ctx, factory.create(options));
}

void acquiringPollAndBindingOrder(TestContext& ctx)
{
    Fixture fixture;
    if (!fixture.activate(ctx)) return;
    ProjectMpegTsMuxSessionAdapter acquiring;
    auto waiting = acquiring.poll(fixture.context);
    EXPECT_TRUE(ctx, waiting);
    if (waiting) {
        EXPECT_FALSE(ctx, waiting.value().progressed);
        EXPECT_FALSE(ctx, waiting.value().nextWait.has_value());
    }

    for (bool reverse : {false, true}) {
        auto sink = std::make_shared<SinkState>();
        ProjectMpegTsMuxSessionAdapter adapter;
        bindComplete(adapter, fixture, sink, ctx, reverse);
        EXPECT_EQ(ctx, sink->bytes.size(), std::size_t{376});
        adapter.abort();
        adapter.abort();
        EXPECT_EQ(ctx, sink->closes, std::size_t{1});
    }
}

void rejectsMissingDuplicateAndWrongBindings(TestContext& ctx)
{
    Fixture fixture;
    if (!fixture.activate(ctx)) return;
    auto sink = std::make_shared<SinkState>();
    ProjectMpegTsMuxSessionAdapter missing;
    EXPECT_TRUE(ctx, missing.bindResource(fixture.context, fixture.sinkBuffer(sink)));
    auto missingFinish = missing.finish(fixture.context);
    EXPECT_FALSE(ctx, missingFinish);
    EXPECT_EQ(ctx, sink->closes, std::size_t{1});
    auto repeated = missing.poll(fixture.context);
    EXPECT_FALSE(ctx, repeated);
    if (!missingFinish && !repeated) {
        EXPECT_EQ(ctx, repeated.error().message, missingFinish.error().message);
    }

    ProjectMpegTsMuxSessionAdapter duplicate;
    EXPECT_TRUE(ctx, duplicate.bindResource(fixture.context, fixture.planBuffer()));
    EXPECT_FALSE(ctx, duplicate.bindResource(fixture.context, fixture.planBuffer()));

    ProjectMpegTsMuxSessionAdapter wrong;
    EXPECT_FALSE(ctx, wrong.bindResource(
                          fixture.context, codecParameters(MediaStreamKind::Video)));

    ProjectMpegTsMuxSessionAdapter duplicateConfig;
    EXPECT_TRUE(ctx, duplicateConfig.bindStreamConfig(
                         fixture.context, codecParameters(MediaStreamKind::Video)));
    EXPECT_FALSE(ctx, duplicateConfig.bindStreamConfig(
                          fixture.context, codecParameters(MediaStreamKind::Video)));
}

void pollUsesClockAndPublishesExactDeadline(TestContext& ctx)
{
    Fixture fixture;
    if (!fixture.activate(ctx)) return;
    auto sink = std::make_shared<SinkState>();
    ProjectMpegTsMuxSessionAdapter adapter;
    bindComplete(adapter, fixture, sink, ctx, false);

    fixture.clock->nowValue = fixture.epoch.masterRelease;
    auto due = adapter.poll(fixture.context);
    EXPECT_TRUE(ctx, due);
    if (due) {
        EXPECT_TRUE(ctx, due.value().progressed);
        EXPECT_TRUE(ctx, due.value().nextWait.has_value());
        if (due.value().nextWait) {
            EXPECT_EQ(ctx, due.value().nextWait->syncGroup, fixture.group);
            EXPECT_EQ(ctx, due.value().nextWait->masterDeadline, ms(1'020));
        }
    }
    const auto bytesAfterPcr = sink->bytes.size();
    fixture.clock->nowValue = ms(1'010);
    auto early = adapter.poll(fixture.context);
    EXPECT_TRUE(ctx, early);
    if (early) {
        EXPECT_FALSE(ctx, early.value().progressed);
        EXPECT_EQ(ctx, early.value().nextWait->masterDeadline, ms(1'020));
    }
    EXPECT_EQ(ctx, sink->bytes.size(), bytesAfterPcr);
}

void writeChecksTypeGenerationLeadAndActiveEpoch(TestContext& ctx)
{
    Fixture fixture;
    if (!fixture.activate(ctx)) return;
    auto sink = std::make_shared<SinkState>();
    ProjectMpegTsMuxSessionAdapter wrongType;
    bindComplete(wrongType, fixture, sink, ctx, false);
    EXPECT_FALSE(ctx, wrongType.write(
                          fixture.context, codecParameters(MediaStreamKind::Video)));

    ProjectMpegTsMuxSessionAdapter wrongGeneration;
    bindComplete(wrongGeneration, fixture, std::make_shared<SinkState>(), ctx, false);
    EXPECT_FALSE(ctx, wrongGeneration.write(
                          fixture.context,
                          accessUnit(MediaScheduledStream::Video, 8, ms(1'020))));

    ProjectMpegTsMuxSessionAdapter wrongLead;
    bindComplete(wrongLead, fixture, std::make_shared<SinkState>(), ctx, false);
    EXPECT_FALSE(ctx, wrongLead.write(
                          fixture.context,
                          accessUnit(MediaScheduledStream::Video, 7, ms(1'020), ms(90))));

    ProjectMpegTsMuxSessionAdapter active;
    auto activeSink = std::make_shared<SinkState>();
    bindComplete(active, fixture, activeSink, ctx, false);
    EXPECT_TRUE(ctx, active.write(
                         fixture.context,
                         accessUnit(MediaScheduledStream::Video, 7, ms(1'020))));
    EXPECT_TRUE(ctx, active.write(
                         fixture.context,
                         accessUnit(MediaScheduledStream::Audio, 7, ms(1'040))));

    fixture.context.findAvSyncGroup(fixture.group)->markAborted();
    auto epochFailure = active.poll(fixture.context);
    EXPECT_FALSE(ctx, epochFailure);
    auto sameFailure = active.finish(fixture.context);
    EXPECT_FALSE(ctx, sameFailure);
    if (!epochFailure && !sameFailure) {
        EXPECT_EQ(ctx, sameFailure.error().message, epochFailure.error().message);
    }
    active.abort();
    EXPECT_EQ(ctx, activeSink->closes, std::size_t{1});
}

void finishAndAbortCloseExactlyOnce(TestContext& ctx)
{
    Fixture fixture;
    if (!fixture.activate(ctx)) return;
    auto sink = std::make_shared<SinkState>();
    ProjectMpegTsMuxSessionAdapter adapter;
    bindComplete(adapter, fixture, sink, ctx, true);
    EXPECT_TRUE(ctx, adapter.finish(fixture.context));
    EXPECT_TRUE(ctx, adapter.finish(fixture.context));
    adapter.abort();
    EXPECT_EQ(ctx, sink->flushes, std::size_t{1});
    EXPECT_EQ(ctx, sink->closes, std::size_t{1});
}

} // namespace

void runProjectMpegTsMuxSessionAdapterTests(TestContext& ctx)
{
    factoryRequiresBothStreams(ctx);
    acquiringPollAndBindingOrder(ctx);
    rejectsMissingDuplicateAndWrongBindings(ctx);
    pollUsesClockAndPublishesExactDeadline(ctx);
    writeChecksTypeGenerationLeadAndActiveEpoch(ctx);
    finishAndAbortCloseExactlyOnce(ctx);
}
