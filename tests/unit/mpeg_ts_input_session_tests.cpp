#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

namespace {

class FragmentedOpener final : public FFmpegProtocolAvioOpener {
public:
    explicit FragmentedOpener(std::vector<uint8_t> bytes,
                              int terminalResult = AVERROR_EOF)
        : m_bytes(std::move(bytes)), m_terminalResult(terminalResult) {}

    ::media::Result<AVIOContext*> open(
        const std::string&, AVDictionary**, const AVIOInterruptCB*) override
    {
        auto* buffer = static_cast<unsigned char*>(av_malloc(7));
        auto* context = avio_alloc_context(buffer, 7, 0, this, &read, nullptr, nullptr);
        return context
            ? ::media::Result<AVIOContext*>::success(context)
            : ::media::Result<AVIOContext*>::failure(
                  ::media::ErrorInfo::allocationFailed("test AVIO allocation failed"));
    }

    void close(AVIOContext** context) noexcept override
    {
        if (context && *context) {
            av_freep(&(*context)->buffer);
            avio_context_free(context);
        }
    }

private:
    static int read(void* opaque, uint8_t* destination, int requested)
    {
        auto& self = *static_cast<FragmentedOpener*>(opaque);
        if (self.m_offset == self.m_bytes.size()) return self.m_terminalResult;
        const auto count = std::min<std::size_t>({3, self.m_bytes.size() - self.m_offset,
                                                  static_cast<std::size_t>(requested)});
        std::memcpy(destination, self.m_bytes.data() + self.m_offset, count);
        self.m_offset += count;
        return static_cast<int>(count);
    }

    std::vector<uint8_t> m_bytes;
    std::size_t m_offset = 0;
    int m_terminalResult = AVERROR_EOF;
};

class RecordingObserver final : public FFmpegObservedByteSink {
public:
    ::media::Status onBytes(std::uint64_t offset, std::span<const uint8_t> bytes) override
    {
        offsets.push_back(offset);
        lengths.push_back(bytes.size());
        observed.insert(observed.end(), bytes.begin(), bytes.end());
        return failureAfterFirst && offsets.size() == 1
            ? ::media::Status::failure(::media::ErrorInfo::invalidArgument("evidence rejected"))
            : ::media::Status::success();
    }

    bool failureAfterFirst = false;
    std::vector<std::uint64_t> offsets;
    std::vector<std::size_t> lengths;
    std::vector<uint8_t> observed;
};

void testObservedReadIsTransparent(TestContext& ctx)
{
    const std::vector<uint8_t> source{0, 1, 2, 3, 4, 5, 6};
    FragmentedOpener opener(source);
    RecordingObserver observer;
    FFmpegAvioInterruptState interrupt;
    auto opened = FFmpegObservedReadAvio::open(
        "test://source", nullptr, 16, observer, interrupt, opener);
    EXPECT_TRUE(ctx, opened);
    if (!opened) return;

    std::vector<uint8_t> received;
    uint8_t chunk[5]{};
    for (;;) {
        const int result = avio_read(opened.value()->outer(), chunk, sizeof(chunk));
        if (result == AVERROR_EOF) break;
        EXPECT_TRUE(ctx, result > 0);
        if (result <= 0) break;
        received.insert(received.end(), chunk, chunk + result);
    }
    EXPECT_EQ(ctx, received, source);
    EXPECT_EQ(ctx, observer.observed, source);
    EXPECT_FALSE(ctx, observer.offsets.empty());
    std::uint64_t expectedOffset = 0;
    for (std::size_t index = 0; index < observer.offsets.size(); ++index) {
        const auto offset = observer.offsets[index];
        EXPECT_EQ(ctx, offset, expectedOffset);
        expectedOffset += observer.lengths[index];
    }
}

void testObserverFailureDoesNotRewriteSuccessfulRead(TestContext& ctx)
{
    FragmentedOpener opener({9, 8, 7});
    RecordingObserver observer;
    observer.failureAfterFirst = true;
    FFmpegAvioInterruptState interrupt;
    auto opened = FFmpegObservedReadAvio::open(
        "test://source", nullptr, 16, observer, interrupt, opener);
    EXPECT_TRUE(ctx, opened);
    if (!opened) return;
    uint8_t bytes[3]{};
    EXPECT_EQ(ctx, avio_read(opened.value()->outer(), bytes, 3), 3);
    EXPECT_EQ(ctx, bytes[0], uint8_t{9});
    EXPECT_TRUE(ctx, opened.value()->observerFailure().has_value());
}

void testTerminalReadStatesRemainDistinct(TestContext& ctx)
{
    RecordingObserver observer;
    FFmpegAvioInterruptState interrupt;
    FragmentedOpener waiting({}, AVERROR(EAGAIN));
    auto waitingAvio = FFmpegObservedReadAvio::open(
        "test://waiting", nullptr, 16, observer, interrupt, waiting);
    EXPECT_TRUE(ctx, waitingAvio);
    if (waitingAvio) {
        uint8_t byte{};
        EXPECT_EQ(ctx, avio_read(waitingAvio.value()->outer(), &byte, 1), AVERROR(EAGAIN));
    }

    FFmpegAvioInterruptState eofInterrupt;
    FragmentedOpener closed({});
    auto eofAvio = FFmpegObservedReadAvio::open(
        "test://closed", nullptr, 16, observer, eofInterrupt, closed);
    EXPECT_TRUE(ctx, eofAvio);
    if (eofAvio) {
        uint8_t byte{};
        EXPECT_EQ(ctx, avio_read(eofAvio.value()->outer(), &byte, 1), AVERROR_EOF);
    }

    FFmpegAvioInterruptState cancelledInterrupt;
    FragmentedOpener cancelled({1});
    auto cancelledAvio = FFmpegObservedReadAvio::open(
        "test://cancelled", nullptr, 16, observer, cancelledInterrupt, cancelled);
    EXPECT_TRUE(ctx, cancelledAvio);
    if (cancelledAvio) {
        cancelledInterrupt.cancel();
        uint8_t byte{};
        EXPECT_EQ(ctx, avio_read(cancelledAvio.value()->outer(), &byte, 1), AVERROR_EXIT);
    }
}

void testCheckpointRetainsEveryObservedPcrPid(TestContext& ctx)
{
    auto created = MediaTsEvidenceTimeline::create(4, 376);
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    auto timeline = std::move(created.value());
    MediaTsEvidenceCheckpoint first;
    first.byteOffset = 188;
    first.pcrObservation = MediaTsRawPcrEvidence{
        .byteOffset = 188, .pid = 0x101, .pcr27Mhz = 27'000};
    MediaTsEvidenceCheckpoint second;
    second.byteOffset = 376;
    second.pcrObservation = MediaTsRawPcrEvidence{
        .byteOffset = 376, .pid = 0x301, .pcr27Mhz = 54'000};
    EXPECT_TRUE(ctx, timeline.append(std::move(first)));
    EXPECT_TRUE(ctx, timeline.append(std::move(second)));
    auto atFirst = timeline.atOrBefore(188);
    auto atSecond = timeline.atOrBefore(376);
    EXPECT_TRUE(ctx, atFirst);
    EXPECT_TRUE(ctx, atSecond);
    if (atFirst) EXPECT_EQ(ctx, atFirst.value().pcrObservation->pid, uint16_t{0x101});
    if (atSecond) EXPECT_EQ(ctx, atSecond.value().pcrObservation->pid, uint16_t{0x301});

    MediaTsEvidenceCheckpoint leaked;
    leaked.byteOffset = 564;
    leaked.pcrObservation = MediaTsRawPcrEvidence{
        .byteOffset = 752, .pid = 0x401, .pcr27Mhz = 81'000};
    EXPECT_FALSE(ctx, timeline.append(std::move(leaked)));
}

} // namespace

void runMpegTsInputSessionTests(TestContext& ctx)
{
    testObservedReadIsTransparent(ctx);
    testObserverFailureDoesNotRewriteSuccessfulRead(ctx);
    testTerminalReadStatesRemainDistinct(ctx);
    testCheckpointRetainsEveryObservedPcrPid(ctx);
}
