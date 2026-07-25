#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"
#include "fixtures/MediaTsPesTimestampInspector.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;
using media_transcode::test::MediaTsPesTimestampInspector;

namespace {

class RecordingByteSink final : public MediaOutputByteSink {
public:
    ::media::Result<std::size_t> write(std::span<const std::uint8_t> bytes) override
    {
        ++writes;
        if (failWrite || (failWriteAt && writes == *failWriteAt)) {
            return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::ioFailure("injected write failure"));
        }
        storage.insert(storage.end(), bytes.begin(), bytes.end());
        return ::media::Result<std::size_t>::success(
            (shortWrite || (shortWriteAt && writes == *shortWriteAt))
                ? bytes.size() - 1 : bytes.size());
    }
    ::media::Status flush() override
    {
        ++flushes;
        return failFlush ? ::media::Status::failure(
            ::media::ErrorInfo::ioFailure("injected flush failure"))
            : ::media::Status::success();
    }
    ::media::Status close() override
    {
        ++closes;
        return failClose ? ::media::Status::failure(
            ::media::ErrorInfo::ioFailure("injected close failure"))
            : ::media::Status::success();
    }

    std::vector<std::uint8_t> storage;
    std::size_t writes = 0;
    std::size_t flushes = 0;
    std::size_t closes = 0;
    bool shortWrite = false;
    bool failWrite = false;
    std::optional<std::size_t> failWriteAt;
    std::optional<std::size_t> shortWriteAt;
    bool failFlush = false;
    bool failClose = false;
};

class FaultInjectingPacketCommitter final : public MediaTsPacketCommitter {
public:
    ::media::Status commit(MediaTsPacketCursor& cursor,
                           MediaTsPacketCommitToken token) override
    {
        ++commits;
        if (failAt && commits == *failAt) {
            return ::media::Status::failure(
                ::media::ErrorInfo::ioFailure("injected packet commit failure"));
        }
        return concrete.commit(cursor, std::move(token));
    }

    MediaTsPacketCursorCommitter concrete;
    std::optional<std::size_t> failAt;
    std::size_t commits = 0;
};

class InventorySink final : public MediaTsProgramInventorySink {
public:
    ::media::Status onProgramInventory(MediaTsProgramInventorySnapshot value) override
    {
        snapshot = std::move(value);
        return ::media::Status::success();
    }
    std::optional<MediaTsProgramInventorySnapshot> snapshot;
};

class VideoPesPayloadCollector final : public MediaTsPacketSink {
public:
    ::media::Status onPacket(const MediaTsPacketView& packet) override
    {
        if (packet.pid == 0x101) {
            bytes.insert(bytes.end(), packet.payloadSpan.begin(), packet.payloadSpan.end());
        }
        return ::media::Status::success();
    }

    ::media::Status onContinuityEvent(const MediaTsContinuityEvent&) override
    {
        return ::media::Status::success();
    }

    std::vector<std::uint8_t> bytes;
};

MediaTsMuxPlan plan(MediaTsH264InputLayout h264InputLayout,
                    MediaTsParameterSetPolicy parameterSetPolicy)
{
    return MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0, 0x100, 0x101, 0x102, 0x101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        h264InputLayout, 4, parameterSetPolicy,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        MediaRunningTime::fromNanoseconds(100'000'000), 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, 1024}).value();
}

MediaTsMuxSession::Binding bindingAt(RecordingByteSink*& observation,
                                     MediaPlaybackEpoch epoch)
{
    auto sink = std::make_unique<RecordingByteSink>();
    observation = sink.get();
    auto video = MediaTsMaterializedVideoConfig::create(
        MediaTsH264InputLayout::LengthPrefixed, 4,
        {0, 0, 0, 1, 0x67, 0x42}, {0, 0, 0, 1, 0x68, 0xCE}).value();
    return MediaTsMuxSession::Binding{
        plan(MediaTsH264InputLayout::LengthPrefixed,
             MediaTsParameterSetPolicy::BeforeRandomAccess), epoch,
        std::move(video), MediaTsMaterializedAudioConfig::create(2, 3, 2).value(),
        std::move(sink)};
}

MediaTsMuxSession::Binding binding(RecordingByteSink*& observation)
{
    return bindingAt(
        observation,
        MediaPlaybackEpoch{MediaRunningTime::fromNanoseconds(100'000'000),
                           MediaRunningTime::fromNanoseconds(100'000'000), 9});
}

std::uint16_t packetPid(const std::vector<std::uint8_t>& bytes,
                        std::size_t packetIndex)
{
    const std::size_t offset = packetIndex * 188;
    return static_cast<std::uint16_t>(
        ((bytes[offset + 1] & 0x1F) << 8) | bytes[offset + 2]);
}

void lifecycleAndDeadlines(TestContext& ctx)
{
    RecordingByteSink* sink = nullptr;
    auto session = MediaTsMuxSession::create(binding(sink));
    EXPECT_TRUE(ctx, session);
    EXPECT_TRUE(ctx, session.value()->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_EQ(ctx, sink->storage.size(), std::size_t{376});
    auto advanced = session.value()->advanceThrough(
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, advanced);
    if (advanced) {
        EXPECT_EQ(ctx, advanced.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(120'000'000));
        EXPECT_EQ(ctx, advanced.value().packetsWritten, std::size_t{3});
    }
    const std::size_t packetCount = sink->storage.size() / 188;
    EXPECT_TRUE(ctx, packetCount >= 3);
    if (packetCount >= 3) {
        EXPECT_EQ(ctx, packetPid(sink->storage, packetCount - 3), std::uint16_t{0});
        EXPECT_EQ(ctx, packetPid(sink->storage, packetCount - 2), std::uint16_t{0x100});
        EXPECT_EQ(ctx, packetPid(sink->storage, packetCount - 1), std::uint16_t{0x101});
    }
    EXPECT_TRUE(ctx, session.value()->finish());
    EXPECT_TRUE(ctx, session.value()->finish());
    EXPECT_EQ(ctx, sink->flushes, std::size_t{1});
    EXPECT_EQ(ctx, sink->closes, std::size_t{1});
    const std::array<std::uint8_t, 2> lateAudio{1, 2};
    EXPECT_FALSE(ctx, session.value()->writeAccessUnit(MediaTsAccessUnitView{
        lateAudio, MediaScheduledStream::Audio, 9,
        MediaRunningTime::fromNanoseconds(200'000'000),
        MediaRunningTime::fromNanoseconds(200'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000), false}));
    session.value()->abort();
    EXPECT_EQ(ctx, sink->closes, std::size_t{1});

    RecordingByteSink* duplicateSink = nullptr;
    auto duplicate = MediaTsMuxSession::create(binding(duplicateSink)).value();
    EXPECT_TRUE(ctx, duplicate->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_FALSE(ctx, duplicate->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_FALSE(ctx, duplicate->advanceThrough(MediaRunningTime::fromNanoseconds(0)));
}

void pollAdvancesTransportDeadlinesWithoutMedia(TestContext& ctx)
{
    RecordingByteSink* sink = nullptr;
    auto session = MediaTsMuxSession::create(binding(sink)).value();
    EXPECT_TRUE(ctx, session->start(MediaRunningTime::fromNanoseconds(0)));
    const auto initialBytes = sink->storage.size();

    auto before = session->poll(MediaRunningTime::fromNanoseconds(99'000'000));
    EXPECT_TRUE(ctx, before);
    if (before) {
        EXPECT_EQ(ctx, before.value().packetsWritten, std::size_t{0});
        EXPECT_EQ(ctx, before.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(100'000'000));
    }
    EXPECT_EQ(ctx, sink->storage.size(), initialBytes);

    auto due = session->poll(MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, due);
    if (due) {
        EXPECT_EQ(ctx, due.value().packetsWritten, std::size_t{3});
        EXPECT_EQ(ctx, due.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(120'000'000));
    }
    EXPECT_EQ(ctx, sink->storage.size(), initialBytes + 3 * 188);

    auto firstCatchUp = session->poll(
        MediaRunningTime::fromNanoseconds(500'000'000));
    EXPECT_TRUE(ctx, firstCatchUp);
    if (firstCatchUp) {
        EXPECT_EQ(ctx, firstCatchUp.value().packetsWritten, std::size_t{1});
        EXPECT_EQ(ctx, firstCatchUp.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(140'000'000));
    }
    const auto afterFirstCatchUp = sink->storage.size();
    auto secondCatchUp = session->poll(
        MediaRunningTime::fromNanoseconds(500'000'000));
    EXPECT_TRUE(ctx, secondCatchUp);
    if (secondCatchUp) {
        EXPECT_EQ(ctx, secondCatchUp.value().packetsWritten, std::size_t{1});
        EXPECT_EQ(ctx, secondCatchUp.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(160'000'000));
    }
    EXPECT_EQ(ctx, sink->storage.size(), afterFirstCatchUp + 188);

    EXPECT_TRUE(ctx, session->finish());
    EXPECT_FALSE(ctx, session->poll(
                          MediaRunningTime::fromNanoseconds(500'000'000)));

    RecordingByteSink* abortedSink = nullptr;
    auto aborted = MediaTsMuxSession::create(binding(abortedSink)).value();
    EXPECT_TRUE(ctx, aborted->start(MediaRunningTime::fromNanoseconds(0)));
    aborted->abort();
    auto abortedPoll = aborted->poll(
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_FALSE(ctx, abortedPoll);
    if (!abortedPoll) {
        EXPECT_EQ(ctx, abortedPoll.error().code, ::media::ErrorCode::Cancelled);
    }
}

void accessUnitsAndPoison(TestContext& ctx)
{
    RecordingByteSink* sink = nullptr;
    auto session = MediaTsMuxSession::create(binding(sink)).value();
    EXPECT_TRUE(ctx, session->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, session->advanceThrough(
        MediaRunningTime::fromNanoseconds(100'000'000)));
    const std::array<std::uint8_t, 7> video{0, 0, 0, 3, 0x65, 1, 2};
    auto videoWrite = session->writeAccessUnit(MediaTsAccessUnitView{
        video, MediaScheduledStream::Video, 9,
        MediaRunningTime::fromNanoseconds(240'000'000),
        MediaRunningTime::fromNanoseconds(220'000'000),
        MediaRunningTime::fromNanoseconds(120'000'000), true});
    EXPECT_TRUE(ctx, videoWrite);
    if (videoWrite) {
        EXPECT_EQ(ctx, videoWrite.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(140'000'000));
        EXPECT_TRUE(ctx, videoWrite.value().packetsWritten >= std::size_t{2});
    }
    const std::array<std::uint8_t, 3> audio{1, 2, 3};
    auto audioWrite = session->writeAccessUnit(MediaTsAccessUnitView{
        audio, MediaScheduledStream::Audio, 9,
        MediaRunningTime::fromNanoseconds(240'000'000),
        MediaRunningTime::fromNanoseconds(240'000'000),
        MediaRunningTime::fromNanoseconds(140'000'000), false});
    EXPECT_TRUE(ctx, audioWrite);
    if (audioWrite) {
        EXPECT_EQ(ctx, audioWrite.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(160'000'000));
        EXPECT_TRUE(ctx, audioWrite.value().packetsWritten >= std::size_t{2});
    }
    EXPECT_FALSE(ctx, session->writeAccessUnit(MediaTsAccessUnitView{
        audio, MediaScheduledStream::Audio, 8,
        MediaRunningTime::fromNanoseconds(260'000'000),
        MediaRunningTime::fromNanoseconds(260'000'000),
        MediaRunningTime::fromNanoseconds(160'000'000), false}));
    EXPECT_FALSE(ctx, session->advanceThrough(MediaRunningTime::fromNanoseconds(180'000'000)));

    MediaTsPesTimestampInspector inspector;
    auto parser = MediaTsPacketParser::create(188, inspector, nullptr);
    EXPECT_TRUE(ctx, parser);
    EXPECT_TRUE(ctx, parser.value()->push(sink->storage));
    EXPECT_EQ(ctx, inspector.continuityEventCount(), std::size_t{0});
    EXPECT_EQ(ctx, inspector.timestamps().size(), std::size_t{2});
    if (inspector.timestamps().size() == 2) {
        EXPECT_EQ(ctx, inspector.timestamps()[0].pid, std::uint16_t{0x101});
        EXPECT_EQ(ctx, inspector.timestamps()[0].pts, std::uint64_t{21'600});
        EXPECT_EQ(ctx, inspector.timestamps()[0].dts, std::uint64_t{19'800});
        EXPECT_EQ(ctx, inspector.timestamps()[1].pid, std::uint16_t{0x102});
        EXPECT_EQ(ctx, inspector.timestamps()[1].pts, std::uint64_t{21'600});
        EXPECT_EQ(ctx, inspector.timestamps()[1].dts, std::uint64_t{21'600});
    }
    EXPECT_FALSE(ctx, inspector.pcrValues().empty());
    EXPECT_TRUE(ctx, std::is_sorted(inspector.pcrValues().begin(),
                                    inspector.pcrValues().end()));
    for (std::size_t index = 1; index < inspector.pcrValues().size(); ++index) {
        EXPECT_TRUE(ctx, inspector.pcrValues()[index] - inspector.pcrValues()[index - 1]
                             <= std::uint64_t{2'700'000});
    }

    InventorySink inventory;
    MediaTsPsiSectionAssembler assembler(inventory);
    auto psiParser = MediaTsPacketParser::create(188, assembler, nullptr);
    EXPECT_TRUE(ctx, psiParser);
    EXPECT_TRUE(ctx, psiParser.value()->push(sink->storage));
    EXPECT_TRUE(ctx, inventory.snapshot.has_value());
    if (inventory.snapshot && !inventory.snapshot->programs.empty()) {
        const auto& program = inventory.snapshot->programs.front();
        EXPECT_EQ(ctx, program.programNumber, std::uint16_t{1});
        EXPECT_EQ(ctx, program.pmtPid, std::uint16_t{0x100});
        EXPECT_EQ(ctx, program.pcrPid, std::uint16_t{0x101});
        EXPECT_EQ(ctx, program.elementaryStreams.size(), std::size_t{2});
        if (program.elementaryStreams.size() == 2) {
            EXPECT_EQ(ctx, program.elementaryStreams[0].pid, std::uint16_t{0x101});
            EXPECT_EQ(ctx, program.elementaryStreams[0].streamType,
                      std::uint8_t{0x1B});
            EXPECT_EQ(ctx, program.elementaryStreams[1].pid, std::uint16_t{0x102});
            EXPECT_EQ(ctx, program.elementaryStreams[1].streamType,
                      std::uint8_t{0x0F});
        }
    }
}

void accessUnitBackingStorageMayExpireAfterWrite(TestContext& ctx)
{
    auto ownedSink = std::make_unique<RecordingByteSink>();
    RecordingByteSink* sink = ownedSink.get();
    auto video = MediaTsMaterializedVideoConfig::create(
        MediaTsH264InputLayout::AnnexB, 4,
        {0, 0, 0, 1, 0x67, 0x42}, {0, 0, 0, 1, 0x68, 0xCE}).value();
    auto session = MediaTsMuxSession::create(MediaTsMuxSession::Binding{
        plan(MediaTsH264InputLayout::AnnexB, MediaTsParameterSetPolicy::Never),
        MediaPlaybackEpoch{MediaRunningTime::fromNanoseconds(100'000'000),
                           MediaRunningTime::fromNanoseconds(100'000'000), 9},
        std::move(video), MediaTsMaterializedAudioConfig::create(2, 3, 2).value(),
        std::move(ownedSink)}).value();
    EXPECT_TRUE(ctx, session->start(MediaRunningTime::fromNanoseconds(0)));
    {
        const std::vector<std::uint8_t> scopedAnnexB{
            0, 0, 0, 1, 0x41, 0xA1, 0xB2, 0xC3};
        EXPECT_TRUE(ctx, session->writeAccessUnit(MediaTsAccessUnitView{
            scopedAnnexB, MediaScheduledStream::Video, 9,
            MediaRunningTime::fromNanoseconds(110'000'000),
            MediaRunningTime::fromNanoseconds(110'000'000),
            MediaRunningTime::fromNanoseconds(10'000'000), false}));
    }
    EXPECT_TRUE(ctx, session->advanceThrough(
        MediaRunningTime::fromNanoseconds(20'000'000)));
    EXPECT_TRUE(ctx, session->finish());

    VideoPesPayloadCollector collector;
    auto parser = MediaTsPacketParser::create(188, collector, nullptr);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;
    EXPECT_TRUE(ctx, parser.value()->push(sink->storage));
    EXPECT_TRUE(ctx, parser.value()->finish());
    const std::array<std::uint8_t, 8> expectedAnnexB{
        0, 0, 0, 1, 0x41, 0xA1, 0xB2, 0xC3};
    EXPECT_TRUE(ctx, std::search(collector.bytes.begin(), collector.bytes.end(),
                                 expectedAnnexB.begin(), expectedAnnexB.end()) !=
                         collector.bytes.end());
}

void sinkFailureIsTerminal(TestContext& ctx)
{
    RecordingByteSink* sink = nullptr;
    auto owned = binding(sink);
    sink->shortWrite = true;
    auto session = MediaTsMuxSession::create(std::move(owned)).value();
    EXPECT_FALSE(ctx, session->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_FALSE(ctx, session->advanceThrough(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_FALSE(ctx, session->finish());
    session->abort();
    session->abort();
    EXPECT_EQ(ctx, sink->closes, std::size_t{1});

    RecordingByteSink* failedSink = nullptr;
    auto failedBinding = binding(failedSink);
    failedSink->failWrite = true;
    auto failedSession = MediaTsMuxSession::create(std::move(failedBinding)).value();
    const auto first = failedSession->start(MediaRunningTime::fromNanoseconds(0));
    EXPECT_FALSE(ctx, first);
    const auto second = failedSession->finish();
    EXPECT_FALSE(ctx, second);
    if (!first && !second) EXPECT_EQ(ctx, first.error().message, second.error().message);
    EXPECT_EQ(ctx, failedSink->flushes, std::size_t{0});
    EXPECT_EQ(ctx, failedSink->closes, std::size_t{1});
}

void bindingAndClockFailures(TestContext& ctx)
{
    RecordingByteSink* sink = nullptr;
    auto mismatched = binding(sink);
    mismatched.audio = MediaTsMaterializedAudioConfig::create(2, 3, 1).value();
    EXPECT_FALSE(ctx, MediaTsMuxSession::create(std::move(mismatched)));

    RecordingByteSink* leadSink = nullptr;
    auto leadSession = MediaTsMuxSession::create(binding(leadSink)).value();
    EXPECT_TRUE(ctx, leadSession->start(MediaRunningTime::fromNanoseconds(0)));
    const std::array<std::uint8_t, 3> audio{1, 2, 3};
    EXPECT_FALSE(ctx, leadSession->writeAccessUnit(MediaTsAccessUnitView{
        audio, MediaScheduledStream::Audio, 9,
        MediaRunningTime::fromNanoseconds(200'000'000),
        MediaRunningTime::fromNanoseconds(200'000'000),
        MediaRunningTime::fromNanoseconds(101'000'000), false}));
}

void deadlineAndTerminalSemantics(TestContext& ctx)
{
    RecordingByteSink* advanceBeforeStartSink = nullptr;
    auto advanceBeforeStart = MediaTsMuxSession::create(
        binding(advanceBeforeStartSink)).value();
    const auto advanceFailure = advanceBeforeStart->advanceThrough(
        MediaRunningTime::fromNanoseconds(0));
    const auto startAfterAdvance = advanceBeforeStart->start(
        MediaRunningTime::fromNanoseconds(0));
    EXPECT_FALSE(ctx, advanceFailure);
    EXPECT_FALSE(ctx, startAfterAdvance);
    if (!advanceFailure && !startAfterAdvance) {
        EXPECT_EQ(ctx, advanceFailure.error().message, startAfterAdvance.error().message);
    }
    EXPECT_EQ(ctx, advanceBeforeStartSink->storage.size(), std::size_t{0});

    RecordingByteSink* writeBeforeStartSink = nullptr;
    auto writeBeforeStart = MediaTsMuxSession::create(
        binding(writeBeforeStartSink)).value();
    const std::array<std::uint8_t, 3> earlyAudio{1, 2, 3};
    const auto writeFailure = writeBeforeStart->writeAccessUnit(MediaTsAccessUnitView{
        earlyAudio, MediaScheduledStream::Audio, 9,
        MediaRunningTime::fromNanoseconds(100'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000),
        MediaRunningTime::fromNanoseconds(0), false});
    EXPECT_FALSE(ctx, writeFailure);
    EXPECT_FALSE(ctx, writeBeforeStart->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_EQ(ctx, writeBeforeStartSink->storage.size(), std::size_t{0});

    RecordingByteSink* wrongStartSink = nullptr;
    auto wrongStart = MediaTsMuxSession::create(binding(wrongStartSink)).value();
    EXPECT_FALSE(ctx, wrongStart->start(MediaRunningTime::fromNanoseconds(1)));
    EXPECT_EQ(ctx, wrongStartSink->storage.size(), std::size_t{0});

    RecordingByteSink* regressionSink = nullptr;
    auto regression = MediaTsMuxSession::create(binding(regressionSink)).value();
    EXPECT_TRUE(ctx, regression->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, regression->advanceThrough(
        MediaRunningTime::fromNanoseconds(10'000'000)));
    EXPECT_FALSE(ctx, regression->advanceThrough(
        MediaRunningTime::fromNanoseconds(9'000'000)));

    RecordingByteSink* exactGapSink = nullptr;
    auto exactGap = MediaTsMuxSession::create(binding(exactGapSink)).value();
    EXPECT_TRUE(ctx, exactGap->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, exactGap->advanceThrough(
        MediaRunningTime::fromNanoseconds(100'000'000)));

    RecordingByteSink* beyondGapSink = nullptr;
    auto beyondGap = MediaTsMuxSession::create(binding(beyondGapSink)).value();
    EXPECT_TRUE(ctx, beyondGap->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_FALSE(ctx, beyondGap->advanceThrough(
        MediaRunningTime::fromNanoseconds(100'000'001)));

    RecordingByteSink* overflowSink = nullptr;
    const auto nearMaximum = MediaRunningTime::fromNanoseconds(
        std::numeric_limits<std::int64_t>::max() - 100'000'000);
    auto overflow = MediaTsMuxSession::create(bindingAt(
        overflowSink,
        MediaPlaybackEpoch{MediaRunningTime::fromNanoseconds(0),
                           nearMaximum, 9})).value();
    const auto overflowOrigin = nearMaximum.checkedSubtract(
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, overflowOrigin);
    if (!overflowOrigin) return;
    EXPECT_TRUE(ctx, overflow->start(overflowOrigin.value()));
    EXPECT_TRUE(ctx, overflow->advanceThrough(nearMaximum));
    EXPECT_FALSE(ctx, overflow->advanceThrough(MediaRunningTime::fromNanoseconds(
        std::numeric_limits<std::int64_t>::max())));
    const std::size_t overflowPackets = overflowSink->storage.size() / 188;
    EXPECT_TRUE(ctx, overflowPackets >= 3);
    if (overflowPackets >= 3) {
        EXPECT_EQ(ctx, packetPid(overflowSink->storage, overflowPackets - 3),
                  std::uint16_t{0});
        EXPECT_EQ(ctx, packetPid(overflowSink->storage, overflowPackets - 2),
                  std::uint16_t{0x100});
        EXPECT_EQ(ctx, packetPid(overflowSink->storage, overflowPackets - 1),
                  std::uint16_t{0x101});
    }

    RecordingByteSink* finishSink = nullptr;
    auto finishSession = MediaTsMuxSession::create(binding(finishSink)).value();
    EXPECT_TRUE(ctx, finishSession->start(MediaRunningTime::fromNanoseconds(0)));
    finishSink->failFlush = true;
    finishSink->failClose = true;
    const auto firstFinish = finishSession->finish();
    const auto secondFinish = finishSession->finish();
    EXPECT_FALSE(ctx, firstFinish);
    EXPECT_FALSE(ctx, secondFinish);
    if (!firstFinish && !secondFinish) {
        EXPECT_EQ(ctx, firstFinish.error().message, std::string("injected flush failure"));
        EXPECT_EQ(ctx, firstFinish.error().message, secondFinish.error().message);
    }
    EXPECT_EQ(ctx, finishSink->flushes, std::size_t{1});
    EXPECT_EQ(ctx, finishSink->closes, std::size_t{1});

    RecordingByteSink* abortSink = nullptr;
    auto aborted = MediaTsMuxSession::create(binding(abortSink)).value();
    aborted->abort();
    aborted->abort();
    EXPECT_EQ(ctx, abortSink->flushes, std::size_t{0});
    EXPECT_EQ(ctx, abortSink->closes, std::size_t{1});
    EXPECT_FALSE(ctx, aborted->start(MediaRunningTime::fromNanoseconds(0)));
}

void secondBatchShortWritePoisonsSession(TestContext& ctx)
{
    RecordingByteSink* sink = nullptr;
    auto session = MediaTsMuxSession::create(binding(sink)).value();
    EXPECT_TRUE(ctx, session->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, session->advanceThrough(
        MediaRunningTime::fromNanoseconds(100'000'000)));
    sink->shortWriteAt = sink->writes + 2;
    std::vector<std::uint8_t> video(1'800, 0x55);
    const std::uint32_t nalBytes = static_cast<std::uint32_t>(video.size() - 4);
    video[0] = static_cast<std::uint8_t>(nalBytes >> 24);
    video[1] = static_cast<std::uint8_t>(nalBytes >> 16);
    video[2] = static_cast<std::uint8_t>(nalBytes >> 8);
    video[3] = static_cast<std::uint8_t>(nalBytes);
    video[4] = 0x65;
    const auto first = session->writeAccessUnit(MediaTsAccessUnitView{
        video, MediaScheduledStream::Video, 9,
        MediaRunningTime::fromNanoseconds(220'000'000),
        MediaRunningTime::fromNanoseconds(200'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000), true});
    EXPECT_FALSE(ctx, first);
    const auto second = session->finish();
    EXPECT_FALSE(ctx, second);
    if (!first && !second) EXPECT_EQ(ctx, first.error().message, second.error().message);

    RecordingByteSink* errorSink = nullptr;
    auto errorSession = MediaTsMuxSession::create(binding(errorSink)).value();
    EXPECT_TRUE(ctx, errorSession->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, errorSession->advanceThrough(
        MediaRunningTime::fromNanoseconds(100'000'000)));
    errorSink->failWriteAt = errorSink->writes + 2;
    EXPECT_FALSE(ctx, errorSession->writeAccessUnit(MediaTsAccessUnitView{
        video, MediaScheduledStream::Video, 9,
        MediaRunningTime::fromNanoseconds(220'000'000),
        MediaRunningTime::fromNanoseconds(200'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000), true}));
}

void commitAndFramerFailuresAreTerminal(TestContext& ctx)
{
    auto ownedSink = std::make_unique<RecordingByteSink>();
    auto* commitSink = ownedSink.get();
    auto committer = std::make_unique<FaultInjectingPacketCommitter>();
    auto* commitObservation = committer.get();
    committer->failAt = 1;
    auto writer = MediaTsPacketBatchWriter::create(
        7, std::move(ownedSink), std::move(committer));
    const auto muxPlan = plan(MediaTsH264InputLayout::LengthPrefixed,
                              MediaTsParameterSetPolicy::BeforeRandomAccess);
    auto packetizer = MediaTsTransportPacketizer::create(muxPlan);
    auto tables = MediaTsPsiSerializer::serialize(muxPlan);
    EXPECT_TRUE(ctx, writer);
    EXPECT_TRUE(ctx, packetizer);
    EXPECT_TRUE(ctx, tables);
    if (!writer || !packetizer || !tables) return;
    auto cursor = packetizer.value().beginPat(tables.value().pat());
    EXPECT_TRUE(ctx, cursor);
    if (!cursor) return;
    auto packetCursor = std::move(cursor).value();
    const auto commitFailure = writer.value().writeCursor(packetCursor);
    EXPECT_FALSE(ctx, commitFailure);
    EXPECT_EQ(ctx, commitObservation->commits, std::size_t{1});
    EXPECT_EQ(ctx, commitSink->storage.size(), std::size_t{188});
    EXPECT_FALSE(ctx, packetCursor.finished());
    writer.value().abort();

    RecordingByteSink* framerSink = nullptr;
    auto framerSession = MediaTsMuxSession::create(binding(framerSink)).value();
    EXPECT_TRUE(ctx, framerSession->start(MediaRunningTime::fromNanoseconds(0)));
    EXPECT_TRUE(ctx, framerSession->advanceThrough(
        MediaRunningTime::fromNanoseconds(100'000'000)));
    const std::array<std::uint8_t, 5> malformedVideo{0, 0, 0, 3, 0x65};
    const auto framerFailure = framerSession->writeAccessUnit(MediaTsAccessUnitView{
        malformedVideo, MediaScheduledStream::Video, 9,
        MediaRunningTime::fromNanoseconds(220'000'000),
        MediaRunningTime::fromNanoseconds(200'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000), true});
    EXPECT_FALSE(ctx, framerFailure);
    const auto framerTerminal = framerSession->finish();
    EXPECT_FALSE(ctx, framerTerminal);
    if (!framerFailure && !framerTerminal) {
        EXPECT_EQ(ctx, framerFailure.error().message, framerTerminal.error().message);
    }
}

void transportOriginAnchorsInitialPsiWithoutMovingPcr(TestContext& ctx)
{
    RecordingByteSink* sink = nullptr;
    auto session = MediaTsMuxSession::create(bindingAt(
        sink, MediaPlaybackEpoch{
                  MediaRunningTime::fromNanoseconds(0),
                  MediaRunningTime::fromNanoseconds(200'000'000), 9})).value();
    const auto origin = MediaRunningTime::fromNanoseconds(100'000'000);
    EXPECT_TRUE(ctx, session->start(origin));
    EXPECT_EQ(ctx, sink->storage.size(), std::size_t{376});

    const std::array<std::uint8_t, 7> video{0, 0, 0, 3, 0x65, 1, 2};
    auto first = session->writeAccessUnit(MediaTsAccessUnitView{
        video, MediaScheduledStream::Video, 9,
        MediaRunningTime::fromNanoseconds(220'000'000),
        MediaRunningTime::fromNanoseconds(200'000'000), origin, true});
    EXPECT_TRUE(ctx, first);
    if (first) {
        EXPECT_EQ(ctx, first.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(200'000'000));
    }
    const std::size_t afterFirstAccessUnit = sink->storage.size();
    auto beforeDeadline = session->advanceThrough(
        MediaRunningTime::fromNanoseconds(199'000'000));
    EXPECT_TRUE(ctx, beforeDeadline);
    if (beforeDeadline) {
        EXPECT_EQ(ctx, beforeDeadline.value().packetsWritten, std::size_t{0});
        EXPECT_EQ(ctx, beforeDeadline.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(200'000'000));
    }
    EXPECT_EQ(ctx, sink->storage.size(), afterFirstAccessUnit);

    auto atDeadline = session->advanceThrough(
        MediaRunningTime::fromNanoseconds(200'000'000));
    EXPECT_TRUE(ctx, atDeadline);
    if (atDeadline) {
        EXPECT_EQ(ctx, atDeadline.value().packetsWritten, std::size_t{3});
        EXPECT_EQ(ctx, atDeadline.value().nextDeadline,
                  MediaRunningTime::fromNanoseconds(220'000'000));
    }
    EXPECT_TRUE(ctx, session->finish());

    MediaTsPesTimestampInspector inspector;
    auto parser = MediaTsPacketParser::create(188, inspector, nullptr);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;
    EXPECT_TRUE(ctx, parser.value()->push(sink->storage));
    EXPECT_TRUE(ctx, parser.value()->finish());
    EXPECT_EQ(ctx, inspector.timestamps().size(), std::size_t{1});
    if (inspector.timestamps().size() == 1) {
        EXPECT_EQ(ctx, inspector.timestamps().front().pts, std::uint64_t{1'800});
        EXPECT_EQ(ctx, inspector.timestamps().front().dts, std::uint64_t{0});
    }
    EXPECT_EQ(ctx, inspector.pcrValues().size(), std::size_t{1});
    if (inspector.pcrValues().size() == 1) {
        EXPECT_EQ(ctx, inspector.pcrValues().front(), std::uint64_t{0});
    }

    RecordingByteSink* wrongSink = nullptr;
    auto wrong = MediaTsMuxSession::create(bindingAt(
        wrongSink, MediaPlaybackEpoch{
                       MediaRunningTime::fromNanoseconds(0),
                       MediaRunningTime::fromNanoseconds(200'000'000), 9})).value();
    EXPECT_FALSE(ctx, wrong->start(
                          MediaRunningTime::fromNanoseconds(100'000'001)));
    EXPECT_EQ(ctx, wrongSink->storage.size(), std::size_t{0});
}

void sessionTimestampsWrapOnTheWire(TestContext& ctx)
{
    constexpr std::int64_t wrapTicks = std::int64_t{1} << 33;
    auto sourceStart = MediaRunningTime::checkedFromTicks(
        wrapTicks - 9'900, 1, 90'000);
    EXPECT_TRUE(ctx, sourceStart);
    if (!sourceStart) return;
    auto shiftedSourceStart = sourceStart.value().checkedAdd(
        MediaRunningTime::fromNanoseconds(100'000'000));
    EXPECT_TRUE(ctx, shiftedSourceStart);
    if (!shiftedSourceStart) return;
    RecordingByteSink* sink = nullptr;
    auto session = MediaTsMuxSession::create(bindingAt(
        sink, MediaPlaybackEpoch{shiftedSourceStart.value(),
                                 MediaRunningTime::fromNanoseconds(100'000'000),
                                 9})).value();
    EXPECT_TRUE(ctx, session->start(MediaRunningTime::fromNanoseconds(0)));
    const std::array<std::uint8_t, 7> video{0, 0, 0, 3, 0x65, 1, 2};
    EXPECT_TRUE(ctx, session->writeAccessUnit(MediaTsAccessUnitView{
        video, MediaScheduledStream::Video, 9,
        MediaRunningTime::fromNanoseconds(105'000'000),
        MediaRunningTime::fromNanoseconds(100'000'000),
        MediaRunningTime::fromNanoseconds(0), true}));
    EXPECT_TRUE(ctx, session->writeAccessUnit(MediaTsAccessUnitView{
        video, MediaScheduledStream::Video, 9,
        MediaRunningTime::fromNanoseconds(115'000'000),
        MediaRunningTime::fromNanoseconds(110'000'000),
        MediaRunningTime::fromNanoseconds(10'000'000), false}));
    EXPECT_TRUE(ctx, session->advanceThrough(
        MediaRunningTime::fromNanoseconds(20'000'000)));
    EXPECT_TRUE(ctx, session->finish());

    MediaTsPesTimestampInspector inspector;
    auto parser = MediaTsPacketParser::create(188, inspector, nullptr);
    EXPECT_TRUE(ctx, parser);
    if (!parser) return;
    EXPECT_TRUE(ctx, parser.value()->push(sink->storage));
    EXPECT_TRUE(ctx, parser.value()->finish());
    EXPECT_EQ(ctx, inspector.timestamps().size(), std::size_t{2});
    if (inspector.timestamps().size() == 2) {
        EXPECT_EQ(ctx, inspector.timestamps()[0].dts,
                  static_cast<std::uint64_t>(wrapTicks - 900));
        EXPECT_EQ(ctx, inspector.timestamps()[0].pts,
                  static_cast<std::uint64_t>(wrapTicks - 450));
        EXPECT_EQ(ctx, inspector.timestamps()[1].dts, std::uint64_t{0});
        EXPECT_EQ(ctx, inspector.timestamps()[1].pts, std::uint64_t{450});
    }
}

} // namespace

void runMpegTsOutputSessionTests(TestContext& ctx)
{
    lifecycleAndDeadlines(ctx);
    pollAdvancesTransportDeadlinesWithoutMedia(ctx);
    accessUnitsAndPoison(ctx);
    accessUnitBackingStorageMayExpireAfterWrite(ctx);
    sinkFailureIsTerminal(ctx);
    bindingAndClockFailures(ctx);
    deadlineAndTerminalSemantics(ctx);
    secondBatchShortWritePoisonsSession(ctx);
    commitAndFramerFailuresAreTerminal(ctx);
    transportOriginAnchorsInitialPsiWithoutMovingPcr(ctx);
    sessionTimestampsWrapOnTheWire(ctx);
}
