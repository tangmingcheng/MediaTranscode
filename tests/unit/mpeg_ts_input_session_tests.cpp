#include "common/TestAssert.h"

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshotFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.h"

extern "C" {
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

namespace {

uint32_t crc32Mpeg(std::span<const uint8_t> bytes)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (const uint8_t byte : bytes) {
        crc ^= static_cast<uint32_t>(byte) << 24;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80000000U) ? (crc << 1) ^ 0x04C11DB7U : crc << 1;
        }
    }
    return crc;
}

void appendCrc(std::vector<uint8_t>& section)
{
    const auto crc = crc32Mpeg(section);
    section.insert(section.end(), {static_cast<uint8_t>(crc >> 24),
                                   static_cast<uint8_t>(crc >> 16),
                                   static_cast<uint8_t>(crc >> 8),
                                   static_cast<uint8_t>(crc)});
}

std::array<uint8_t, 188> sectionPacket(uint16_t pid, std::vector<uint8_t> section)
{
    std::array<uint8_t, 188> packet;
    packet.fill(0xFF);
    packet[0] = 0x47;
    packet[1] = static_cast<uint8_t>(0x40 | (pid >> 8));
    packet[2] = static_cast<uint8_t>(pid);
    packet[3] = 0x10;
    packet[4] = 0;
    std::copy(section.begin(), section.end(), packet.begin() + 5);
    return packet;
}

std::vector<uint8_t> validMpegTsBytes(std::size_t extraPesPackets = 0)
{
    std::vector<uint8_t> pat{0x00, 0xB0, 0x0D, 0x00, 0x01, 0xC1, 0, 0,
                             0, 1, 0xE1, 0x00};
    appendCrc(pat);
    std::vector<uint8_t> pmt{0x02, 0xB0, 0x12, 0, 1, 0xC1, 0, 0,
                             0xE1, 0x01, 0xF0, 0,
                             0x0F, 0xE1, 0x01, 0xF0, 0};
    appendCrc(pmt);
    std::array<uint8_t, 188> pes;
    pes.fill(0xFF);
    pes[0] = 0x47; pes[1] = 0x41; pes[2] = 0x01; pes[3] = 0x10;
    const std::array<uint8_t, 27> payload{
        0,0,1,0xC0,0,21,0x80,0x80,5,0x21,0,1,0,1,
        0xFF,0xF1,0x50,0x80,0x01,0xBF,0xFC,
        0x21,0x10,0x04,0x60,0x8C,0x1C};
    std::copy(payload.begin(), payload.end(), pes.begin() + 4);
    std::array<uint8_t, 188> nullPacket;
    nullPacket.fill(0xFF);
    nullPacket[0] = 0x47; nullPacket[1] = 0x1F; nullPacket[2] = 0xFF; nullPacket[3] = 0x10;
    auto secondPes = pes;
    secondPes[3] = 0x11;
    std::vector<uint8_t> bytes;
    const auto patPacket = sectionPacket(0, std::move(pat));
    const auto pmtPacket = sectionPacket(0x100, std::move(pmt));
    for (const auto& packet : {patPacket, pmtPacket, pes, secondPes}) {
        bytes.insert(bytes.end(), packet.begin(), packet.end());
    }
    for (std::size_t index = 0; index < extraPesPackets; ++index) {
        auto extra = pes;
        extra[3] = static_cast<uint8_t>(0x10 | ((index + 2) & 0x0F));
        bytes.insert(bytes.end(), extra.begin(), extra.end());
    }
    bytes.insert(bytes.end(), nullPacket.begin(), nullPacket.end());
    return bytes;
}

class FragmentedOpener final : public FFmpegProtocolAvioOpener {
public:
    explicit FragmentedOpener(std::vector<uint8_t> bytes,
                              int terminalResult = AVERROR_EOF)
        : m_bytes(std::move(bytes)), m_terminalResult(terminalResult) {}

    ::media::Result<AVIOContext*> open(
        const std::string&, AVDictionary** options, const AVIOInterruptCB*) override
    {
        sawProtocolOption = av_dict_get(*options, "protocol_only", nullptr, 0) != nullptr;
        sawDemuxOption = av_dict_get(*options, "scan_all_pmts", nullptr, 0) != nullptr;
        auto* buffer = static_cast<unsigned char*>(av_malloc(7));
        auto* context = avio_alloc_context(buffer, 7, 0, this, &read, nullptr, nullptr);
        return context
            ? ::media::Result<AVIOContext*>::success(context)
            : ::media::Result<AVIOContext*>::failure(
                  ::media::ErrorInfo::allocationFailed("test AVIO allocation failed"));
    }

    void close(AVIOContext** context) noexcept override
    {
        ++m_closeCount;
        if (context && *context) {
            av_freep(&(*context)->buffer);
            avio_context_free(context);
        }
    }

    std::size_t readCount() const noexcept { return m_readCount; }
    std::size_t closeCount() const noexcept { return m_closeCount; }
    bool sawProtocolOption = false;
    bool sawDemuxOption = false;

private:
    static int read(void* opaque, uint8_t* destination, int requested)
    {
        auto& self = *static_cast<FragmentedOpener*>(opaque);
        ++self.m_readCount;
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
    std::size_t m_readCount = 0;
    std::size_t m_closeCount = 0;
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

class BlockingOpener final : public FFmpegProtocolAvioOpener {
public:
    ::media::Result<AVIOContext*> open(
        const std::string&, AVDictionary**, const AVIOInterruptCB* interrupt) override
    {
        m_interrupt = *interrupt;
        auto* buffer = static_cast<unsigned char*>(av_malloc(16));
        auto* context = avio_alloc_context(buffer, 16, 0, this, &read, nullptr, nullptr);
        return context
            ? ::media::Result<AVIOContext*>::success(context)
            : ::media::Result<AVIOContext*>::failure(
                  ::media::ErrorInfo::allocationFailed("blocking AVIO allocation failed"));
    }

    void requestInterrupt(AVIOContext*) noexcept override
    {
        std::lock_guard lock(m_mutex);
        m_interrupted = true;
        m_changed.notify_all();
    }

    void close(AVIOContext** context) noexcept override
    {
        if (context && *context) {
            av_freep(&(*context)->buffer);
            avio_context_free(context);
        }
    }

    void waitUntilReadStarted()
    {
        std::unique_lock lock(m_mutex);
        m_changed.wait(lock, [this] { return m_readStarted; });
    }

private:
    static int read(void* opaque, uint8_t*, int)
    {
        auto& self = *static_cast<BlockingOpener*>(opaque);
        std::unique_lock lock(self.m_mutex);
        self.m_readStarted = true;
        self.m_changed.notify_all();
        self.m_changed.wait(lock, [&self] { return self.m_interrupted; });
        return self.m_interrupt.callback(self.m_interrupt.opaque) ? AVERROR_EXIT : AVERROR(EIO);
    }

    AVIOInterruptCB m_interrupt{};
    std::mutex m_mutex;
    std::condition_variable m_changed;
    bool m_readStarted = false;
    bool m_interrupted = false;
};

class FailingOpener final : public FFmpegProtocolAvioOpener {
public:
    ::media::Result<AVIOContext*> open(
        const std::string&, AVDictionary**, const AVIOInterruptCB*) override
    {
        return ::media::Result<AVIOContext*>::failure(
            ::media::ErrorInfo::ioFailure("planned protocol open failure", AVERROR(EIO)));
    }
    void close(AVIOContext**) noexcept override { ++closeCount; }
    std::size_t closeCount = 0;
};

class ThrowingObserver final : public FFmpegObservedByteSink {
public:
    ::media::Status onBytes(std::uint64_t, std::span<const uint8_t>) override
    {
        throw std::runtime_error("observer failure");
    }
};

class BlockingByteOpener final : public FFmpegProtocolAvioOpener {
public:
    explicit BlockingByteOpener(std::vector<uint8_t> bytes,
                                int interruptResult = AVERROR_EXIT)
        : m_bytes(std::move(bytes)), m_interruptResult(interruptResult) {}
    ::media::Result<AVIOContext*> open(
        const std::string&, AVDictionary**, const AVIOInterruptCB* interrupt) override
    {
        m_interrupt = *interrupt;
        auto* buffer = static_cast<unsigned char*>(av_malloc(64));
        auto* context = avio_alloc_context(buffer, 64, 0, this, &read, nullptr, nullptr);
        return context ? ::media::Result<AVIOContext*>::success(context)
                       : ::media::Result<AVIOContext*>::failure(
                             ::media::ErrorInfo::allocationFailed("blocking byte AVIO allocation failed"));
    }
    void requestInterrupt(AVIOContext*) noexcept override
    {
        std::lock_guard lock(m_mutex);
        m_interrupted = true;
        m_changed.notify_all();
    }
    void close(AVIOContext** context) noexcept override
    {
        ++closeCount;
        if (context && *context) {
            av_freep(&(*context)->buffer);
            avio_context_free(context);
        }
    }
    void waitUntilBlocked()
    {
        std::unique_lock lock(m_mutex);
        m_changed.wait(lock, [this] { return m_blocked; });
    }
    std::size_t readCount()
    {
        std::lock_guard lock(m_mutex);
        return m_readCount;
    }
    std::size_t closeCount = 0;
private:
    static int read(void* opaque, uint8_t* destination, int requested)
    {
        auto& self = *static_cast<BlockingByteOpener*>(opaque);
        std::unique_lock lock(self.m_mutex);
        ++self.m_readCount;
        if (self.m_offset < self.m_bytes.size()) {
            const auto count = std::min<std::size_t>(
                self.m_bytes.size() - self.m_offset, static_cast<std::size_t>(requested));
            std::memcpy(destination, self.m_bytes.data() + self.m_offset, count);
            self.m_offset += count;
            return static_cast<int>(count);
        }
        self.m_blocked = true;
        self.m_changed.notify_all();
        self.m_changed.wait(lock, [&self] { return self.m_interrupted; });
        return self.m_interrupt.callback(self.m_interrupt.opaque)
            ? self.m_interruptResult : AVERROR(EIO);
    }
    std::vector<uint8_t> m_bytes;
    std::size_t m_offset = 0;
    AVIOInterruptCB m_interrupt{};
    std::mutex m_mutex;
    std::condition_variable m_changed;
    bool m_blocked = false;
    bool m_interrupted = false;
    std::size_t m_readCount = 0;
    int m_interruptResult = AVERROR_EXIT;
};

class ReentrantCloseObserver final : public FFmpegObservedByteSink {
public:
    ::media::Status onBytes(std::uint64_t, std::span<const uint8_t>) override
    {
        closeStatus = owner->close();
        return closeStatus;
    }
    FFmpegObservedReadAvio* owner = nullptr;
    ::media::Status closeStatus = ::media::Status::success();
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
    const auto readsBeforeFailureBoundary = opener.readCount();
    EXPECT_EQ(ctx, avio_read(opened.value()->outer(), bytes, 1), AVERROR_INVALIDDATA);
    EXPECT_EQ(ctx, opener.readCount(), readsBeforeFailureBoundary);
}

void testBorrowedSnapshotFailureDoesNotOwnFormatContext(TestContext& ctx)
{
    AVFormatContext* format = avformat_alloc_context();
    EXPECT_TRUE(ctx, format != nullptr);
    if (!format) return;
    AVStream* first = avformat_new_stream(format, nullptr);
    AVStream* second = avformat_new_stream(format, nullptr);
    EXPECT_TRUE(ctx, first != nullptr);
    EXPECT_TRUE(ctx, second != nullptr);
    if (!first || !second) {
        avformat_free_context(format);
        return;
    }
    avcodec_parameters_free(&second->codecpar);
    auto snapshots = FFmpegInputStreamSnapshotFactory::fromFormatContext(*format);
    EXPECT_FALSE(ctx, snapshots);
    EXPECT_EQ(ctx, format->nb_streams, 2U);
    EXPECT_TRUE(ctx, format->streams[0] == first);
    avformat_free_context(format);
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

void testCloseWaitsForActiveReadCallback(TestContext& ctx)
{
    BlockingOpener opener;
    RecordingObserver observer;
    FFmpegAvioInterruptState interrupt;
    auto opened = FFmpegObservedReadAvio::open(
        "test://blocking", nullptr, 16, observer, interrupt, opener);
    EXPECT_TRUE(ctx, opened);
    if (!opened) return;
    AVIOContext* outer = opened.value()->outer();
    int readResult = 0;
    std::thread reader([&] {
        uint8_t byte{};
        readResult = avio_read(outer, &byte, 1);
    });
    opener.waitUntilReadStarted();
    opened.value()->close();
    reader.join();
    EXPECT_EQ(ctx, readResult, AVERROR_EXIT);
    EXPECT_TRUE(ctx, interrupt.cancelled());
}

void testObserverExceptionBecomesStructuredFailure(TestContext& ctx)
{
    FragmentedOpener opener({1, 2});
    ThrowingObserver observer;
    FFmpegAvioInterruptState interrupt;
    auto opened = FFmpegObservedReadAvio::open(
        "test://throwing", nullptr, 16, observer, interrupt, opener);
    EXPECT_TRUE(ctx, opened);
    if (!opened) return;
    uint8_t bytes[2]{};
    EXPECT_EQ(ctx, avio_read(opened.value()->outer(), bytes, 2), 2);
    auto status = opened.value()->status();
    EXPECT_FALSE(ctx, status);
    if (!status) EXPECT_EQ(ctx, status.error().code, ::media::ErrorCode::InternalError);
    EXPECT_EQ(ctx, avio_read(opened.value()->outer(), bytes, 1), AVERROR_INVALIDDATA);
}

void testReentrantCloseFailsWithoutDeadlock(TestContext& ctx)
{
    FragmentedOpener opener({1});
    ReentrantCloseObserver observer;
    FFmpegAvioInterruptState interrupt;
    auto opened = FFmpegObservedReadAvio::open(
        "test://reentrant", nullptr, 16, observer, interrupt, opener);
    EXPECT_TRUE(ctx, opened);
    if (!opened) return;
    observer.owner = opened.value().get();
    uint8_t byte{};
    EXPECT_EQ(ctx, avio_read(opened.value()->outer(), &byte, 1), 1);
    EXPECT_FALSE(ctx, observer.closeStatus);
}

void testSessionProbeAndPreparedTransfer(TestContext& ctx)
{
    FragmentedOpener opener(validMpegTsBytes());
    MediaTsInputSessionOptions options;
    options.protocolUrl = "test://mpegts";
    options.avioBufferBytes = 64;
    options.packetStride = 188;
    options.evidenceCapacity = 32;
    options.maximumPositionRegressionBytes = 188 * 8;
    AVDictionary* protocolOptions = nullptr;
    AVDictionary* demuxOptions = nullptr;
    av_dict_set(&protocolOptions, "protocol_only", "preserved", 0);
    av_dict_set(&demuxOptions, "scan_all_pmts", "1", 0);
    options.protocolOptions = protocolOptions;
    options.demuxOptions = demuxOptions;
    auto session = MediaTsInputSession::open(options, opener);
    EXPECT_TRUE(ctx, av_dict_get(protocolOptions, "protocol_only", nullptr, 0) != nullptr);
    EXPECT_TRUE(ctx, av_dict_get(demuxOptions, "scan_all_pmts", nullptr, 0) != nullptr);
    EXPECT_TRUE(ctx, opener.sawProtocolOption);
    EXPECT_FALSE(ctx, opener.sawDemuxOption);
    av_dict_free(&protocolOptions);
    av_dict_free(&demuxOptions);
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    EXPECT_EQ(ctx, session.value()->programInventory().programs.size(), std::size_t{1});
    EXPECT_FALSE(ctx, session.value()->streamSnapshots().empty());
    auto prepared = MediaTsPreparedInputBuffer::create(std::move(session.value()));
    EXPECT_TRUE(ctx, prepared);
    if (!prepared) return;
    auto taken = prepared.value()->takeSession();
    EXPECT_TRUE(ctx, taken);
    EXPECT_FALSE(ctx, prepared.value()->takeSession());
    taken.value().reset();
    EXPECT_EQ(ctx, opener.closeCount(), std::size_t{1});
}

void testSessionRejectsUnsupportedAndIncompleteInput(TestContext& ctx)
{
    FragmentedOpener opener(validMpegTsBytes());
    MediaTsInputSessionOptions unsupported;
    unsupported.protocolUrl = "test://mpegts";
    unsupported.avioBufferBytes = 64;
    unsupported.packetStride = 192;
    unsupported.evidenceCapacity = 8;
    EXPECT_FALSE(ctx, MediaTsInputSession::open(unsupported, opener));
    EXPECT_EQ(ctx, opener.readCount(), std::size_t{0});

    FragmentedOpener incomplete(std::vector<uint8_t>(188 * 3, 0x47));
    MediaTsInputSessionOptions options;
    options.protocolUrl = "test://incomplete";
    options.avioBufferBytes = 64;
    options.packetStride = 188;
    options.evidenceCapacity = 8;
    auto failed = MediaTsInputSession::open(options, incomplete);
    EXPECT_FALSE(ctx, failed);
    EXPECT_EQ(ctx, incomplete.closeCount(), std::size_t{1});

    FailingOpener failing;
    options.protocolUrl = "test://open-failure";
    auto openFailed = MediaTsInputSession::open(options, failing);
    EXPECT_FALSE(ctx, openFailed);
    EXPECT_EQ(ctx, failing.closeCount, std::size_t{0});
}

void testPreparedDestructionBeforeTransferClosesOnce(TestContext& ctx)
{
    FragmentedOpener opener(validMpegTsBytes());
    MediaTsInputSessionOptions options;
    options.protocolUrl = "test://prepared-destruction";
    options.avioBufferBytes = 64;
    options.packetStride = 188;
    options.evidenceCapacity = 32;
    auto session = MediaTsInputSession::open(options, opener);
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    auto prepared = MediaTsPreparedInputBuffer::create(std::move(session.value()));
    EXPECT_TRUE(ctx, prepared);
    prepared.value().reset();
    EXPECT_EQ(ctx, opener.closeCount(), std::size_t{1});
}

void testSessionCloseRejectsNewReads(TestContext& ctx)
{
    FragmentedOpener opener(validMpegTsBytes());
    MediaTsInputSessionOptions options;
    options.protocolUrl = "test://session-close";
    options.avioBufferBytes = 64;
    options.packetStride = 188;
    options.evidenceCapacity = 32;
    auto session = MediaTsInputSession::open(options, opener);
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    EXPECT_TRUE(ctx, session.value()->close());
    auto packet = ::media::ffmpeg::makePacket();
    EXPECT_TRUE(ctx, packet != nullptr);
    if (packet) {
        auto read = session.value()->readFrame(*packet);
        EXPECT_FALSE(ctx, read);
        if (!read) EXPECT_EQ(ctx, read.error().code, ::media::ErrorCode::Cancelled);
    }
    EXPECT_EQ(ctx, opener.closeCount(), std::size_t{1});
}

void testSessionCloseInterruptsBlockedRead(TestContext& ctx)
{
    BlockingByteOpener opener(validMpegTsBytes(64));
    AVDictionary* demuxOptions = nullptr;
    av_dict_set(&demuxOptions, "probesize", "512", 0);
    av_dict_set(&demuxOptions, "analyzeduration", "0", 0);
    MediaTsInputSessionOptions options;
    options.protocolUrl = "test://session-blocking";
    options.demuxOptions = demuxOptions;
    options.avioBufferBytes = 64;
    options.packetStride = 188;
    options.evidenceCapacity = 128;
    auto session = MediaTsInputSession::open(options, opener);
    av_dict_free(&demuxOptions);
    EXPECT_TRUE(ctx, session);
    if (!session) return;
    ::media::Result<MediaTsReadFrameState> read =
        ::media::Result<MediaTsReadFrameState>::success(MediaTsReadFrameState::Waiting);
    std::thread reader([&] {
        auto packet = ::media::ffmpeg::makePacket();
        for (;;) {
            av_packet_unref(packet.get());
            read = session.value()->readFrame(*packet);
            if (!read || read.value() != MediaTsReadFrameState::Frame) return;
        }
    });
    opener.waitUntilBlocked();
    std::mutex statusMutex;
    std::condition_variable statusChanged;
    bool initialStatusRead = false;
    bool closeFinished = false;
    std::array<::media::Status, 3> statuses{
        ::media::Status::success(), ::media::Status::success(), ::media::Status::success()};
    std::thread statusReader([&] {
        statuses[0] = session.value()->status();
        {
            std::lock_guard lock(statusMutex);
            initialStatusRead = true;
        }
        statusChanged.notify_all();
        {
            std::unique_lock lock(statusMutex);
            statusChanged.wait(lock, [&] { return closeFinished; });
        }
        statuses[1] = session.value()->status();
        statuses[2] = session.value()->status();
    });
    {
        std::unique_lock lock(statusMutex);
        statusChanged.wait(lock, [&] { return initialStatusRead; });
    }
    EXPECT_TRUE(ctx, statuses[0]);
    auto inventoryWhileReading = session.value()->programInventory();
    EXPECT_EQ(ctx, inventoryWhileReading.programs.size(), std::size_t{1});
    auto evidenceWhileReading = session.value()->evidenceAtOrBefore(188);
    EXPECT_TRUE(ctx, evidenceWhileReading);
    auto evidenceSnapshot = session.value()->evidenceSnapshotAfter(std::nullopt);
    EXPECT_TRUE(ctx, evidenceSnapshot);
    EXPECT_FALSE(ctx, evidenceSnapshot.value().empty());
    auto noNewEvidence = session.value()->evidenceSnapshotAfter(
        evidenceSnapshot.value().back().byteOffset);
    EXPECT_TRUE(ctx, noNewEvidence);
    EXPECT_TRUE(ctx, noNewEvidence.value().empty());
    const auto readsBeforeSecondReader = opener.readCount();
    auto secondPacket = ::media::ffmpeg::makePacket();
    auto secondRead = session.value()->readFrame(*secondPacket);
    EXPECT_FALSE(ctx, secondRead);
    if (!secondRead) EXPECT_EQ(ctx, secondRead.error().code, ::media::ErrorCode::InvalidArgument);
    EXPECT_EQ(ctx, opener.readCount(), readsBeforeSecondReader);
    EXPECT_TRUE(ctx, session.value()->close());
    {
        std::lock_guard lock(statusMutex);
        closeFinished = true;
    }
    statusChanged.notify_all();
    reader.join();
    statusReader.join();
    EXPECT_FALSE(ctx, read);
    if (!read) EXPECT_EQ(ctx, read.error().code, ::media::ErrorCode::Cancelled);
    EXPECT_EQ(ctx, opener.closeCount, std::size_t{1});
    EXPECT_FALSE(ctx, statuses[1]);
    EXPECT_FALSE(ctx, statuses[2]);
    if (!statuses[1] && !statuses[2]) {
        EXPECT_EQ(ctx, statuses[1].error().code, ::media::ErrorCode::Cancelled);
        EXPECT_EQ(ctx, statuses[2].error().code, ::media::ErrorCode::Cancelled);
        EXPECT_EQ(ctx, statuses[1].error().message, statuses[2].error().message);
    }
}

void testInterruptedTerminalResultsAreCancelled(TestContext& ctx)
{
    for (const int terminalResult : {AVERROR_EOF, AVERROR(EAGAIN)}) {
        BlockingByteOpener opener(validMpegTsBytes(64), terminalResult);
        AVDictionary* demuxOptions = nullptr;
        av_dict_set(&demuxOptions, "probesize", "512", 0);
        av_dict_set(&demuxOptions, "analyzeduration", "0", 0);
        MediaTsInputSessionOptions options;
        options.protocolUrl = "test://cancel-classification";
        options.demuxOptions = demuxOptions;
        options.avioBufferBytes = 64;
        options.packetStride = 188;
        options.evidenceCapacity = 128;
        auto session = MediaTsInputSession::open(options, opener);
        av_dict_free(&demuxOptions);
        EXPECT_TRUE(ctx, session);
        if (!session) continue;
        ::media::Result<MediaTsReadFrameState> read =
            ::media::Result<MediaTsReadFrameState>::success(MediaTsReadFrameState::Waiting);
        std::thread reader([&] {
            auto packet = ::media::ffmpeg::makePacket();
            for (;;) {
                av_packet_unref(packet.get());
                read = session.value()->readFrame(*packet);
                if (!read || read.value() != MediaTsReadFrameState::Frame) return;
            }
        });
        opener.waitUntilBlocked();
        EXPECT_TRUE(ctx, session.value()->close());
        reader.join();
        EXPECT_FALSE(ctx, read);
        if (!read) EXPECT_EQ(ctx, read.error().code, ::media::ErrorCode::Cancelled);
    }
}

} // namespace

void runMpegTsInputSessionTests(TestContext& ctx)
{
    testObservedReadIsTransparent(ctx);
    testObserverFailureDoesNotRewriteSuccessfulRead(ctx);
    testBorrowedSnapshotFailureDoesNotOwnFormatContext(ctx);
    testTerminalReadStatesRemainDistinct(ctx);
    testCloseWaitsForActiveReadCallback(ctx);
    testObserverExceptionBecomesStructuredFailure(ctx);
    testReentrantCloseFailsWithoutDeadlock(ctx);
    testCheckpointRetainsEveryObservedPcrPid(ctx);
    testSessionProbeAndPreparedTransfer(ctx);
    testSessionRejectsUnsupportedAndIncompleteInput(ctx);
    testPreparedDestructionBeforeTransferClosesOnce(ctx);
    testSessionCloseRejectsNewReads(ctx);
    testSessionCloseInterruptsBlockedRead(ctx);
    testInterruptedTerminalResultsAreCancelled(ctx);
}
