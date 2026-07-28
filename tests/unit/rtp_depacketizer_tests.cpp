#include "common/TestAssert.h"

#include "internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpCodecDescriptor.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

namespace {

MediaRtpPacket packet(uint16_t sequence, uint32_t timestamp, bool marker,
                      std::vector<uint8_t> payload, uint8_t payloadType = 96,
                      uint32_t ssrc = 0x10203040)
{
    MediaRtpPacket result{};
    result.version = 2;
    result.sequenceNumber = sequence;
    result.timestamp = timestamp;
    result.marker = marker;
    result.payloadType = payloadType;
    result.ssrc = ssrc;
    result.payload = std::move(payload);
    return result;
}

MediaRtpDepacketizerConfig config(std::string codec, std::string fmtp = {})
{
    MediaRtpDepacketizerConfig result;
    result.streamKind = codec == "aac" || codec == "opus" ? MediaStreamKind::Audio : MediaStreamKind::Video;
    result.codecName = std::move(codec);
    result.fmtp = std::move(fmtp);
    result.payloadType = result.streamKind == MediaStreamKind::Audio ? 97 : 96;
    result.clockRate = result.codecName == "opus" ? 48000 : (result.streamKind == MediaStreamKind::Audio ? 44100 : 90000);
    result.channels = result.streamKind == MediaStreamKind::Audio ? 2 : 0;
    result.accessUnitDurationRtpTicks = result.codecName == "aac" ? 1024 : 0;
    return result;
}

void testReorder(TestContext& ctx)
{
    MediaRtpReorderBuffer reorder({4, std::chrono::milliseconds(10), 96});
    const auto now = std::chrono::steady_clock::time_point{};
    auto first = reorder.push(packet(65535, 90, false, {1}), now);
    EXPECT_TRUE(ctx, first);
    EXPECT_EQ(ctx, first.value().packets.size(), static_cast<std::size_t>(1));
    auto ahead = reorder.push(packet(1, 90, true, {3}), now);
    EXPECT_TRUE(ctx, ahead);
    EXPECT_EQ(ctx, ahead.value().packets.size(), static_cast<std::size_t>(0));
    auto wrap = reorder.push(packet(0, 90, false, {2}), now);
    EXPECT_TRUE(ctx, wrap);
    EXPECT_EQ(ctx, wrap.value().packets.size(), static_cast<std::size_t>(2));
    auto duplicate = reorder.push(packet(0, 90, false, {2}), now);
    EXPECT_TRUE(ctx, duplicate);
    EXPECT_TRUE(ctx, duplicate.value().duplicate);

    EXPECT_TRUE(ctx, reorder.push(packet(4, 180, true, {4}), now));
    auto gap = reorder.push(packet(5, 180, true, {5}), now + std::chrono::milliseconds(20));
    EXPECT_TRUE(ctx, gap);
    EXPECT_EQ(ctx, gap.value().discontinuities.size(), static_cast<std::size_t>(1));

    auto changed = reorder.push(packet(6, 180, true, {6}, 97), now + std::chrono::milliseconds(21));
    EXPECT_TRUE(ctx, changed);
    EXPECT_EQ(ctx, changed.value().discontinuities.size(), static_cast<std::size_t>(1));

    auto ssrcChanged = reorder.push(packet(6, 180, true, {6}, 96, 0x55667788), now + std::chrono::milliseconds(22));
    EXPECT_TRUE(ctx, ssrcChanged);
    EXPECT_EQ(ctx, ssrcChanged.value().discontinuities.size(), static_cast<std::size_t>(1));
}

void testH264(TestContext& ctx)
{
    auto created = MediaRtpDepacketizerFactory::create(config(
        "h264", "packetization-mode=1;sprop-parameter-sets=Z0IAH5WoFAFuQA==,aM4xUg==;profile-level-id=42001f"));
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto single = created.value()->push(packet(1, 9000, true, {0x65, 0x11}));
    EXPECT_TRUE(ctx, single);
    EXPECT_EQ(ctx, single.value().accessUnits.size(), static_cast<std::size_t>(1));
    if (single && single.value().accessUnits.size() == 1) {
        EXPECT_EQ(ctx, single.value().accessUnits[0].rtpTimestamp, static_cast<uint32_t>(9000));
        EXPECT_EQ(ctx, single.value().accessUnits[0].timeBase.den, 90000);
        EXPECT_TRUE(ctx, (single.value().accessUnits[0].packet->flags & AV_PKT_FLAG_KEY) != 0);
    }

    auto stap = created.value()->push(packet(2, 18000, true, {24, 0, 2, 0x61, 1, 0, 2, 0x65, 2}));
    EXPECT_TRUE(ctx, stap);
    EXPECT_EQ(ctx, stap.value().accessUnits.size(), static_cast<std::size_t>(1));

    EXPECT_TRUE(ctx, created.value()->push(packet(3, 27000, false, {28, 0x85, 0xAA})));
    auto fu = created.value()->push(packet(4, 27000, true, {28, 0x45, 0xBB}));
    EXPECT_TRUE(ctx, fu);
    EXPECT_EQ(ctx, fu.value().accessUnits.size(), static_cast<std::size_t>(1));

    EXPECT_FALSE(ctx, created.value()->push(packet(5, 36000, true, {25, 0, 1, 0x61})));
    EXPECT_FALSE(ctx, created.value()->push(packet(6, 36000, true, {28, 0x45})));
    created.value()->discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    auto orphanedFuContinuation = created.value()->push(
        packet(8, 45000, true, {28, 0x45, 1}));
    EXPECT_TRUE(ctx, orphanedFuContinuation);
    if (orphanedFuContinuation) {
        EXPECT_TRUE(ctx, orphanedFuContinuation.value().accessUnits.empty());
    }
    auto recovered = created.value()->push(packet(9, 54000, true, {0x65, 3}));
    EXPECT_TRUE(ctx, recovered);
    EXPECT_EQ(ctx, recovered.value().accessUnits.size(), static_cast<std::size_t>(1));

    EXPECT_FALSE(ctx, created.value()->push(packet(10, 63000, true, {0xE5, 1})));
    EXPECT_FALSE(ctx, created.value()->push(packet(11, 72000, true, {24, 0, 2, 0xE1, 1})));

    auto aggregateState = MediaRtpDepacketizerFactory::create(config(
        "h264", "packetization-mode=1;sprop-parameter-sets=Z0IAH5WoFAFuQA==,aM4xUg==;profile-level-id=42001f"));
    EXPECT_TRUE(ctx, aggregateState);
    if (aggregateState) {
        EXPECT_TRUE(ctx, aggregateState.value()->push(packet(20, 81000, false, {0x61, 1})));
        EXPECT_FALSE(ctx, aggregateState.value()->push(packet(21, 81000, true, {24, 0, 2, 0x61})));
        auto recoveredOnly = aggregateState.value()->push(packet(22, 81000, true, {0x61, 2}));
        EXPECT_TRUE(ctx, recoveredOnly);
        if (recoveredOnly) EXPECT_EQ(ctx, recoveredOnly.value().accessUnits[0].packet->size, 6);
    }
    auto fuIdentity = MediaRtpDepacketizerFactory::create(config(
        "h264", "packetization-mode=1;sprop-parameter-sets=Z0IAH5WoFAFuQA==,aM4xUg==;profile-level-id=42001f"));
    EXPECT_TRUE(ctx, fuIdentity);
    if (fuIdentity) {
        EXPECT_TRUE(ctx, fuIdentity.value()->push(packet(30, 90000, false, {0x7C, 0x85, 1})));
        EXPECT_FALSE(ctx, fuIdentity.value()->push(packet(31, 90000, true, {0x5C, 0x45, 2})));
        auto recoveredOnly = fuIdentity.value()->push(packet(32, 90000, true, {0x61, 3}));
        EXPECT_TRUE(ctx, recoveredOnly);
        if (recoveredOnly) EXPECT_EQ(ctx, recoveredOnly.value().accessUnits[0].packet->size, 6);
    }
    auto truncatedFu = MediaRtpDepacketizerFactory::create(config(
        "h264", "packetization-mode=1;sprop-parameter-sets=Z0IAH5WoFAFuQA==,aM4xUg==;profile-level-id=42001f"));
    EXPECT_TRUE(ctx, truncatedFu);
    if (truncatedFu) {
        EXPECT_TRUE(ctx, truncatedFu.value()->push(packet(40, 99000, false, {0x7C, 0x85, 1})));
        EXPECT_FALSE(ctx, truncatedFu.value()->push(packet(41, 99000, true, {0x7C, 0x45})));
        auto recoveredOnly = truncatedFu.value()->push(packet(42, 99000, true, {0x61, 4}));
        EXPECT_TRUE(ctx, recoveredOnly);
        if (recoveredOnly) EXPECT_EQ(ctx, recoveredOnly.value().accessUnits[0].packet->size, 6);
    }
    EXPECT_FALSE(ctx, created.value()->push(packet(12, 81000, true, {28, 0x80, 1})));
}

MediaRealtimeRtpInputMetadata aacMetadata(std::string asc, int clockRate)
{
    MediaRealtimeRtpInputMetadata metadata;
    metadata.url = "rtp://127.0.0.1:5004";
    metadata.codecName = "aac";
    metadata.payloadType = 97;
    metadata.clockRate = clockRate;
    metadata.channels = 2;
    metadata.fmtp = "streamtype=5;profile-level-id=1;mode=AAC-hbr;config=" + std::move(asc) +
        ";sizeLength=13;indexLength=3;indexDeltaLength=3";
    return metadata;
}

void testSparseReorderExpiresWithoutSubsequentRtp(TestContext& ctx)
{
    MediaRtpReorderBuffer reorder({4, std::chrono::milliseconds(10), 96});
    const auto now = std::chrono::steady_clock::time_point{};
    auto first = reorder.push(packet(10, 90, true, {1}), now);
    EXPECT_TRUE(ctx, first);
    EXPECT_EQ(ctx, first.value().packets.size(), static_cast<std::size_t>(1));

    auto sparse = reorder.push(packet(12, 180, true, {3}), now);
    EXPECT_TRUE(ctx, sparse);
    EXPECT_EQ(ctx, sparse.value().packets.size(), static_cast<std::size_t>(0));
    EXPECT_TRUE(ctx, reorder.nextDeadline().has_value());
    if (!reorder.nextDeadline()) return;
    EXPECT_EQ(ctx, *reorder.nextDeadline(), now + std::chrono::milliseconds(10));

    auto early = reorder.expire(now + std::chrono::milliseconds(9));
    EXPECT_TRUE(ctx, early);
    EXPECT_TRUE(ctx, early.value().packets.empty());
    EXPECT_TRUE(ctx, early.value().discontinuities.empty());

    auto expired = reorder.expire(now + std::chrono::milliseconds(10));
    EXPECT_TRUE(ctx, expired);
    EXPECT_EQ(ctx, expired.value().discontinuities.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(ctx, expired.value().packets.size(), static_cast<std::size_t>(1));
    if (!expired.value().discontinuities.empty()) {
        EXPECT_EQ(ctx, expired.value().discontinuities[0].reason, MediaRtpDiscontinuityReason::SequenceGap);
        EXPECT_EQ(ctx, expired.value().discontinuities[0].firstMissingSequence, static_cast<uint16_t>(11));
        EXPECT_EQ(ctx, expired.value().discontinuities[0].resumedSequence, static_cast<uint16_t>(12));
    }
    EXPECT_FALSE(ctx, reorder.nextDeadline().has_value());

    MediaRtpReorderBuffer oldestDeadline({4, std::chrono::milliseconds(10), 96});
    EXPECT_TRUE(ctx, oldestDeadline.push(packet(20, 270, true, {1}), now));
    EXPECT_TRUE(ctx, oldestDeadline.push(packet(23, 360, true, {4}), now));
    EXPECT_TRUE(ctx, oldestDeadline.push(packet(22, 360, true, {3}), now + std::chrono::milliseconds(5)));
    EXPECT_TRUE(ctx, oldestDeadline.nextDeadline().has_value());
    if (oldestDeadline.nextDeadline()) {
        EXPECT_EQ(ctx, *oldestDeadline.nextDeadline(), now + std::chrono::milliseconds(10));
    }
    auto oldestExpired = oldestDeadline.expire(now + std::chrono::milliseconds(10));
    EXPECT_TRUE(ctx, oldestExpired);
    if (oldestExpired) {
        EXPECT_EQ(ctx, oldestExpired.value().packets.size(), static_cast<std::size_t>(2));
    }
}

void testHevc(TestContext& ctx)
{
    auto created = MediaRtpDepacketizerFactory::create(config(
        "hevc", "sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwB4;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA=="));
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto single = created.value()->push(packet(1, 90, true, {0x26, 1, 2}));
    EXPECT_TRUE(ctx, single);
    EXPECT_EQ(ctx, single.value().accessUnits.size(), static_cast<std::size_t>(1));
    auto ap = created.value()->push(packet(2, 180, true, {0x60, 1, 0, 2, 0x02, 1, 0, 2, 0x26, 1}));
    EXPECT_TRUE(ctx, ap);
    EXPECT_EQ(ctx, ap.value().accessUnits.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, created.value()->push(packet(3, 270, false, {0x62, 1, 0x93, 0xAA})));
    auto fu = created.value()->push(packet(4, 270, true, {0x62, 1, 0x53, 0xBB}));
    EXPECT_TRUE(ctx, fu);
    EXPECT_EQ(ctx, fu.value().accessUnits.size(), static_cast<std::size_t>(1));
    EXPECT_FALSE(ctx, created.value()->push(packet(5, 360, true, {0x64, 1, 0})));
    EXPECT_FALSE(ctx, created.value()->push(packet(6, 360, true, {0x60, 1, 0, 8, 1})));
    EXPECT_FALSE(ctx, created.value()->push(packet(7, 450, true, {0xA6, 1, 2})));
    EXPECT_FALSE(ctx, created.value()->push(packet(8, 540, true, {0x26, 0, 2})));
    EXPECT_FALSE(ctx, created.value()->push(packet(9, 630, true, {0x60, 1, 0, 2, 0x02, 0})));
    EXPECT_FALSE(ctx, created.value()->push(packet(10, 720, true, {0x62, 1, 0xB1, 1})));

    created.value()->discontinuity(MediaRtpDiscontinuityReason::SequenceGap);
    auto orphanedFuContinuation = created.value()->push(
        packet(11, 810, true, {0x62, 1, 0x53, 1}));
    EXPECT_TRUE(ctx, orphanedFuContinuation);
    if (orphanedFuContinuation) {
        EXPECT_TRUE(ctx, orphanedFuContinuation.value().accessUnits.empty());
    }

    EXPECT_FALSE(ctx, MediaRtpDepacketizerFactory::create(config(
        "hevc", "sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwB4;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA==;sprop-max-don-diff=1")));
    auto aggregateState = MediaRtpDepacketizerFactory::create(config(
        "hevc", "sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwB4;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA=="));
    EXPECT_TRUE(ctx, aggregateState);
    if (aggregateState) {
        EXPECT_TRUE(ctx, aggregateState.value()->push(packet(20, 810, false, {0x02, 1, 1})));
        EXPECT_FALSE(ctx, aggregateState.value()->push(packet(21, 810, true, {0x60, 1, 0, 3, 0x02, 1})));
        auto recoveredOnly = aggregateState.value()->push(packet(22, 810, true, {0x02, 1, 2}));
        EXPECT_TRUE(ctx, recoveredOnly);
        if (recoveredOnly) EXPECT_EQ(ctx, recoveredOnly.value().accessUnits[0].packet->size, 7);
    }
    auto fuIdentity = MediaRtpDepacketizerFactory::create(config(
        "hevc", "sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwB4;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA=="));
    EXPECT_TRUE(ctx, fuIdentity);
    if (fuIdentity) {
        EXPECT_TRUE(ctx, fuIdentity.value()->push(packet(30, 900, false, {0x62, 1, 0x93, 1})));
        EXPECT_FALSE(ctx, fuIdentity.value()->push(packet(31, 900, true, {0x62, 2, 0x53, 2})));
        auto recoveredOnly = fuIdentity.value()->push(packet(32, 900, true, {0x02, 1, 3}));
        EXPECT_TRUE(ctx, recoveredOnly);
        if (recoveredOnly) EXPECT_EQ(ctx, recoveredOnly.value().accessUnits[0].packet->size, 7);
    }
    auto truncatedFu = MediaRtpDepacketizerFactory::create(config(
        "hevc", "sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwB4;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA=="));
    EXPECT_TRUE(ctx, truncatedFu);
    if (truncatedFu) {
        EXPECT_TRUE(ctx, truncatedFu.value()->push(packet(40, 990, false, {0x62, 1, 0x93, 1})));
        EXPECT_FALSE(ctx, truncatedFu.value()->push(packet(41, 990, true, {0x62, 1, 0x53})));
        auto recoveredOnly = truncatedFu.value()->push(packet(42, 990, true, {0x02, 1, 4}));
        EXPECT_TRUE(ctx, recoveredOnly);
        if (recoveredOnly) EXPECT_EQ(ctx, recoveredOnly.value().accessUnits[0].packet->size, 7);
    }
}

void testAacAndOpus(TestContext& ctx)
{
    const std::string aacFmtp = "streamtype=5;profile-level-id=1;mode=AAC-hbr;config=1210;sizeLength=13;indexLength=3;indexDeltaLength=3";
    auto aac = MediaRtpDepacketizerFactory::create(config("aac", aacFmtp));
    EXPECT_TRUE(ctx, aac);
    if (aac) {
        auto au = aac.value()->push(packet(1, 1024, true, {0, 16, 0, 0x10, 0x11, 0x22}, 97));
        EXPECT_TRUE(ctx, au);
        EXPECT_EQ(ctx, au.value().accessUnits.size(), static_cast<std::size_t>(1));
        if (au && au.value().accessUnits.size() == 1) EXPECT_EQ(ctx, au.value().accessUnits[0].packet->duration, static_cast<int64_t>(1024));

        auto aggregate = aac.value()->push(packet(
            2, 4096, true, {0, 32, 0, 0x10, 0, 0x18, 0x11, 0x22, 0x31, 0x32, 0x33}, 97));
        EXPECT_TRUE(ctx, aggregate);
        EXPECT_EQ(ctx, aggregate.value().accessUnits.size(), static_cast<std::size_t>(2));
        if (aggregate && aggregate.value().accessUnits.size() == 2) {
            const auto& first = aggregate.value().accessUnits[0];
            const auto& second = aggregate.value().accessUnits[1];
            EXPECT_EQ(ctx, first.rtpTimestamp, static_cast<uint32_t>(4096));
            EXPECT_EQ(ctx, second.rtpTimestamp, static_cast<uint32_t>(5120));
            EXPECT_EQ(ctx, first.packet->duration, static_cast<int64_t>(1024));
            EXPECT_EQ(ctx, second.packet->duration, static_cast<int64_t>(1024));
            EXPECT_EQ(ctx, first.packet->size, 2);
            EXPECT_EQ(ctx, second.packet->size, 3);
            EXPECT_EQ(ctx, first.packet->data[0], static_cast<uint8_t>(0x11));
            EXPECT_EQ(ctx, second.packet->data[0], static_cast<uint8_t>(0x31));
        }

        auto fragmentedFirst = aac.value()->push(packet(
            3, 6144, false, {0, 16, 0, 0x30, 1, 2, 3, 4}, 97));
        EXPECT_TRUE(ctx, fragmentedFirst);
        if (fragmentedFirst) {
            EXPECT_TRUE(ctx, fragmentedFirst.value().accessUnits.empty());
        }
        auto fragmentedLast = aac.value()->push(packet(
            4, 6144, true, {0, 16, 0, 0x30, 5, 6}, 97));
        EXPECT_TRUE(ctx, fragmentedLast);
        if (fragmentedLast) {
            EXPECT_EQ(ctx, fragmentedLast.value().accessUnits.size(),
                      static_cast<std::size_t>(1));
            EXPECT_EQ(ctx, fragmentedLast.value().accessUnits[0].packet->size, 6);
            EXPECT_EQ(ctx, fragmentedLast.value().accessUnits[0].packet->data[5],
                      static_cast<std::uint8_t>(6));
        }

        std::vector<uint8_t> nineAus{0, 144};
        for (int index = 0; index < 9; ++index) {
            nineAus.push_back(0);
            nineAus.push_back(8);
        }
        for (int index = 0; index < 9; ++index) nineAus.push_back(static_cast<uint8_t>(0x40 + index));
        auto expandedIndexes = aac.value()->push(packet(5, 8192, true, std::move(nineAus), 97));
        EXPECT_TRUE(ctx, expandedIndexes);
        if (expandedIndexes) {
            EXPECT_EQ(ctx, expandedIndexes.value().accessUnits.size(), static_cast<std::size_t>(9));
        }
        if (expandedIndexes && expandedIndexes.value().accessUnits.size() == 9) {
            EXPECT_EQ(ctx, expandedIndexes.value().accessUnits[8].rtpTimestamp, static_cast<uint32_t>(16384));
            EXPECT_EQ(ctx, expandedIndexes.value().accessUnits[8].packet->data[0], static_cast<uint8_t>(0x48));
        }
        EXPECT_FALSE(ctx, aac.value()->push(packet(6, 2048, true, {0, 15, 0, 0}, 97)));
        EXPECT_FALSE(ctx, aac.value()->push(packet(7, 2048, true, {0, 16, 0, 0x60, 1}, 97)));
        EXPECT_FALSE(ctx, aac.value()->push(packet(8, 2048, true, {0, 32, 0, 8, 1}, 97)));
        EXPECT_FALSE(ctx, aac.value()->push(packet(9, 2048, true, {0, 32, 0, 0x10, 0, 0x18, 1, 2}, 97)));
    }
    EXPECT_FALSE(ctx, MediaRtpDepacketizerFactory::create(config("aac", "mode=AAC-lbr;config=1210;sizeLength=13;indexLength=3;indexDeltaLength=3")));
    EXPECT_FALSE(ctx, MediaRtpDepacketizerFactory::create(config(
        "aac", "streamtype=5;profile-level-id=1;mode=AAC-hbr;config=1210ff;sizeLength=13;indexLength=3;indexDeltaLength=3")));
    auto wrongAacClock = config("aac", aacFmtp);
    wrongAacClock.clockRate = 48000;
    EXPECT_FALSE(ctx, MediaRtpDepacketizerFactory::create(wrongAacClock));
    for (const char* optionalHeader : {
             "CTSDeltaLength=1", "DTSDeltaLength=1", "randomAccessIndication=1",
             "streamStateIndication=1", "auxiliaryDataSizeLength=1"}) {
        EXPECT_FALSE(ctx, MediaRtpDepacketizerFactory::create(config(
            "aac", aacFmtp + ";" + optionalHeader)));
    }

    auto opus = MediaRtpDepacketizerFactory::create(config("opus"));
    EXPECT_TRUE(ctx, opus);
    if (opus) {
        auto frame = opus.value()->push(packet(1, 48000, true, {0xF8, 1}, 97));
        EXPECT_TRUE(ctx, frame);
        EXPECT_EQ(ctx, frame.value().accessUnits.size(), static_cast<std::size_t>(1));
        if (frame && frame.value().accessUnits.size() == 1) EXPECT_EQ(ctx, frame.value().accessUnits[0].packet->duration, static_cast<int64_t>(960));
        EXPECT_TRUE(ctx, opus.value()->push(packet(2, 48960, false, {0xF8, 1}, 97)));
        EXPECT_FALSE(ctx, opus.value()->push(packet(3, 48960, true, {}, 97)));
        EXPECT_FALSE(ctx, opus.value()->push(packet(4, 48960, true, {0xF8}, 97)));
        EXPECT_FALSE(ctx, opus.value()->push(packet(5, 48960, true, {0xF9, 1, 2, 3}, 97)));
        EXPECT_FALSE(ctx, opus.value()->push(packet(6, 48960, true, {0xFA, 5, 1}, 97)));
        auto code3Cbr = opus.value()->push(packet(7, 48960, true, {0xFB, 2, 1, 2}, 97));
        EXPECT_TRUE(ctx, code3Cbr);
        if (code3Cbr) EXPECT_EQ(ctx, code3Cbr.value().accessUnits[0].packet->duration, static_cast<int64_t>(1920));
        EXPECT_FALSE(ctx, opus.value()->push(packet(8, 48960, true, {0xFB, 0x42, 5, 1}, 97)));
        EXPECT_FALSE(ctx, opus.value()->push(packet(9, 48960, true, {0xFB, 0x82, 5, 1, 2}, 97)));
        auto code3Vbr = opus.value()->push(packet(10, 48960, true, {0xFB, 0x82, 1, 0x11, 0x22}, 97));
        EXPECT_TRUE(ctx, code3Vbr);
        auto code3Padded = opus.value()->push(packet(11, 48960, true, {0xFB, 0x42, 1, 0x11, 0x22, 0}, 97));
        EXPECT_TRUE(ctx, code3Padded);
        EXPECT_FALSE(ctx, opus.value()->push(packet(12, 48960, true, {0xFB, 0, 1}, 97)));
        EXPECT_FALSE(ctx, opus.value()->push(packet(13, 48960, true, {0xFB, 2}, 97)));
        EXPECT_FALSE(ctx, opus.value()->push(packet(14, 48960, true, {0x1B, 3, 1, 2, 3}, 97)));
        for (const uint8_t toc : {static_cast<uint8_t>(0x18), static_cast<uint8_t>(0x38), static_cast<uint8_t>(0x58)}) {
            auto sixtyMs = opus.value()->push(packet(15, 48960, true, {toc, 1}, 97));
            EXPECT_TRUE(ctx, sixtyMs);
            if (sixtyMs) EXPECT_EQ(ctx, sixtyMs.value().accessUnits[0].packet->duration, static_cast<int64_t>(2880));
        }
        EXPECT_FALSE(ctx, opus.value()->push(packet(16, 48960, true, {0x83, 49, 1}, 97)));
    }
}

void testAacPlannerClockAndDuration(TestContext& ctx)
{
    EXPECT_FALSE(ctx, MediaRealtimeRtpCodecRegistry::describe(
        MediaStreamKind::Audio, aacMetadata("1210", 88200)));
    EXPECT_FALSE(ctx, MediaRealtimeRtpCodecRegistry::describe(
        MediaStreamKind::Audio, aacMetadata("1192", 48000)));
    EXPECT_FALSE(ctx, MediaRealtimeRtpCodecRegistry::describe(
        MediaStreamKind::Audio, aacMetadata("1191", 48000)));
    EXPECT_FALSE(ctx, MediaRealtimeRtpCodecRegistry::describe(
        MediaStreamKind::Audio, aacMetadata("1210ff", 44100)));

    auto longFrame = MediaRealtimeRtpCodecRegistry::describe(
        MediaStreamKind::Audio, aacMetadata("1190", 48000));
    EXPECT_TRUE(ctx, longFrame);
    if (longFrame) EXPECT_EQ(ctx, longFrame.value().accessUnitDurationRtpTicks, 1024);

    auto shortFrame = MediaRealtimeRtpCodecRegistry::describe(
        MediaStreamKind::Audio, aacMetadata("1194", 48000));
    EXPECT_TRUE(ctx, shortFrame);
    if (shortFrame) EXPECT_EQ(ctx, shortFrame.value().accessUnitDurationRtpTicks, 960);
}

void testImmutableDescriptors(TestContext& ctx)
{
    auto h264 = MediaRawRtpStreamDescriptorFactory::create(config(
        "h264", "packetization-mode=1;sprop-parameter-sets=Z0IAH5WoFAFuQA==,aM4xUg==;profile-level-id=42001f"));
    EXPECT_TRUE(ctx, h264);
    if (h264) {
        EXPECT_TRUE(ctx, h264.value()->inputSnapshotComplete());
        EXPECT_TRUE(ctx, h264.value()->context() == nullptr);
        const auto* stream = h264.value()->inputStreamSnapshot(0);
        EXPECT_TRUE(ctx, stream != nullptr);
        if (stream) {
            EXPECT_EQ(ctx, stream->time.timeBase.num, 1);
            EXPECT_EQ(ctx, stream->time.timeBase.den, 90000);
            auto parameters = stream->cloneCodecParameters();
            EXPECT_TRUE(ctx, parameters);
            if (parameters) EXPECT_TRUE(ctx, parameters.value()->extradata_size > 0);
        }
    }
}

} // namespace

void runRtpDepacketizerTests(TestContext& ctx)
{
    testReorder(ctx);
    testSparseReorderExpiresWithoutSubsequentRtp(ctx);
    testH264(ctx);
    testHevc(ctx);
    testAacAndOpus(ctx);
    testAacPlannerClockAndDuration(ctx);
    testImmutableDescriptors(ctx);
}
