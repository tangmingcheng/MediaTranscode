#include "common/TestAssert.h"

#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSink.h"
#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSinkBackend.h"

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
}

#include <array>
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

class ShortWriteSink final : public MediaOutputByteSink {
public:
    ::media::Result<std::size_t> write(std::span<const std::uint8_t> bytes) override
    {
        return ::media::Result<std::size_t>::success(bytes.empty() ? 0 : bytes.size() - 1);
    }

    ::media::Status flush() override { return ::media::Status::success(); }
    ::media::Status close() override { return ::media::Status::success(); }
};

class DestructionObservedSink final : public MediaOutputByteSink {
public:
    explicit DestructionObservedSink(bool& destroyed) : m_destroyed(destroyed) {}
    ~DestructionObservedSink() override { m_destroyed = true; }

    ::media::Result<std::size_t> write(std::span<const std::uint8_t> bytes) override
    {
        return ::media::Result<std::size_t>::success(bytes.size());
    }

    ::media::Status flush() override { return ::media::Status::success(); }
    ::media::Status close() override { return ::media::Status::success(); }

private:
    bool& m_destroyed;
};

struct FakeAvioBackendState final {
    explicit FakeAvioBackendState(std::size_t maximumWriteBytes)
        : maximumWriteBytes(maximumWriteBytes)
    {
    }

    const std::size_t maximumWriteBytes;
    int writeCalls = 0;
    int flushCalls = 0;
    int closeCalls = 0;
    int writeFailure = 0;
    int flushFailure = 0;
    int closeFailure = 0;
    int currentError = 0;
};

class FakeAvioBackend final : public FFmpegAvioOutputByteSinkBackend {
public:
    explicit FakeAvioBackend(FakeAvioBackendState& state) : m_state(state) {}

    void write(std::span<const std::uint8_t>) override
    {
        ++m_state.writeCalls;
        if (m_state.writeFailure < 0) m_state.currentError = m_state.writeFailure;
    }

    void flush() override
    {
        ++m_state.flushCalls;
        if (m_state.currentError == 0 && m_state.flushFailure < 0) {
            m_state.currentError = m_state.flushFailure;
        }
    }

    int error() const noexcept override { return m_state.currentError; }
    std::size_t maximumWriteBytes() const noexcept override
    {
        return m_state.maximumWriteBytes;
    }

    int close() noexcept override
    {
        ++m_state.closeCalls;
        return m_state.closeFailure;
    }

private:
    FakeAvioBackendState& m_state;
};

std::unique_ptr<FFmpegAvioOutputByteSink> fakeSink(
    TestContext& ctx,
    FakeAvioBackendState& state)
{
    auto created = FFmpegAvioOutputByteSink::create(
        std::make_unique<FakeAvioBackend>(state));
    EXPECT_TRUE(ctx, created);
    return created ? std::move(created).value() : nullptr;
}

void expectSameError(TestContext& ctx,
                     const ::media::ErrorInfo& actual,
                     const ::media::ErrorInfo& expected)
{
    EXPECT_EQ(ctx, actual.code, expected.code);
    EXPECT_EQ(ctx, actual.nativeCode, expected.nativeCode);
    EXPECT_EQ(ctx, actual.message, expected.message);
}

std::filesystem::path uniqueOutputPath(const char* name)
{
    return std::filesystem::temp_directory_path() /
        (std::string("media_transcode_") + name + ".bin");
}

void testOpenRejectsInvalidArguments(TestContext& ctx)
{
    EXPECT_FALSE(ctx, FFmpegAvioOutputByteSink::open("", AVIO_FLAG_WRITE));
    EXPECT_FALSE(ctx, FFmpegAvioOutputByteSink::open("unused.bin", 0));
    EXPECT_FALSE(ctx, FFmpegAvioOutputByteSink::open("unused.bin", AVIO_FLAG_READ));
    EXPECT_FALSE(ctx, FFmpegAvioOutputByteSink::open(
        "unused.bin", AVIO_FLAG_READ_WRITE));
    EXPECT_FALSE(ctx, FFmpegAvioOutputByteSink::open(
        "unused.bin", AVIO_FLAG_WRITE | 0x40));
    auto protocolFailure = FFmpegAvioOutputByteSink::open(
        "invalid-protocol://byte-sink", AVIO_FLAG_WRITE);
    EXPECT_FALSE(ctx, protocolFailure);
    if (!protocolFailure) {
        EXPECT_EQ(ctx, protocolFailure.error().code, ::media::ErrorCode::FFmpegFailure);
        EXPECT_TRUE(ctx, protocolFailure.error().nativeCode < 0);
    }
}

void testWriteFlushCloseAndDestructorOwnership(TestContext& ctx)
{
    const auto path = uniqueOutputPath("byte_sink_exact_write");
    std::filesystem::remove(path);
    const std::array<std::uint8_t, 5> bytes{0x00, 0x47, 0x80, 0xFE, 0xFF};
    {
        auto opened = FFmpegAvioOutputByteSink::open(path.string(), AVIO_FLAG_WRITE);
        EXPECT_TRUE(ctx, opened);
        if (!opened) return;
        auto sink = std::move(opened).value();
        auto written = sink->write(bytes);
        EXPECT_TRUE(ctx, written);
        if (written) EXPECT_EQ(ctx, written.value(), bytes.size());
        EXPECT_TRUE(ctx, sink->flush());
        EXPECT_TRUE(ctx, sink->close());
        EXPECT_TRUE(ctx, sink->close());
        EXPECT_FALSE(ctx, sink->flush());
        auto afterClose = sink->write(bytes);
        EXPECT_FALSE(ctx, afterClose);
        if (!afterClose) {
            EXPECT_EQ(ctx, afterClose.error().code, ::media::ErrorCode::NotInitialized);
        }
    }

    std::ifstream input(path, std::ios::binary);
    const std::vector<std::uint8_t> actual{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    EXPECT_EQ(ctx, actual.size(), bytes.size());
    EXPECT_TRUE(ctx, std::equal(actual.begin(), actual.end(), bytes.begin(), bytes.end()));
    input.close();
    EXPECT_TRUE(ctx, std::filesystem::remove(path));

    const auto destructorPath = uniqueOutputPath("byte_sink_destructor");
    std::filesystem::remove(destructorPath);
    {
        auto opened = FFmpegAvioOutputByteSink::open(
            destructorPath.string(), AVIO_FLAG_WRITE);
        EXPECT_TRUE(ctx, opened);
        if (!opened) return;
        auto sink = std::move(opened).value();
        EXPECT_TRUE(ctx, sink->write(bytes));
    }
    EXPECT_TRUE(ctx, std::filesystem::remove(destructorPath));
}

void testBufferTransfersOwnershipExactlyOnce(TestContext& ctx)
{
    EXPECT_FALSE(ctx, MediaOutputByteSinkBuffer::create(nullptr));

    bool ownedSinkDestroyed = false;
    {
        auto owned = MediaOutputByteSinkBuffer::create(
            std::make_unique<DestructionObservedSink>(ownedSinkDestroyed));
        EXPECT_TRUE(ctx, owned);
    }
    EXPECT_TRUE(ctx, ownedSinkDestroyed);

    bool destroyed = false;
    auto created = MediaOutputByteSinkBuffer::create(
        std::make_unique<DestructionObservedSink>(destroyed));
    EXPECT_TRUE(ctx, created);
    if (!created) return;
    EXPECT_EQ(ctx, created.value()->type(), MediaBufferType::OutputByteSink);
    EXPECT_EQ(ctx, created.value()->payloadKind(), MediaPayloadKind::OutputByteSink);
    EXPECT_FALSE(ctx, destroyed);

    auto transferred = created.value()->takeSink();
    EXPECT_TRUE(ctx, transferred);
    auto secondTransfer = created.value()->takeSink();
    EXPECT_FALSE(ctx, secondTransfer);
    if (!secondTransfer) {
        EXPECT_EQ(ctx, secondTransfer.error().code, ::media::ErrorCode::NotInitialized);
    }
    created.value().reset();
    EXPECT_FALSE(ctx, destroyed);
    transferred.value().reset();
    EXPECT_TRUE(ctx, destroyed);
}

void testInputFailuresPoisonTheSink(TestContext& ctx)
{
    const std::array<std::uint8_t, 2> bytes{1, 2};
    FakeAvioBackendState emptyState{8};
    auto emptySink = fakeSink(ctx, emptyState);
    if (!emptySink) return;
    auto emptyFailure = emptySink->write({});
    EXPECT_FALSE(ctx, emptyFailure);
    auto writeAfterEmpty = emptySink->write(bytes);
    auto flushAfterEmpty = emptySink->flush();
    EXPECT_FALSE(ctx, writeAfterEmpty);
    EXPECT_FALSE(ctx, flushAfterEmpty);
    if (!emptyFailure && !writeAfterEmpty) {
        expectSameError(ctx, writeAfterEmpty.error(), emptyFailure.error());
    }
    if (!emptyFailure && !flushAfterEmpty) {
        expectSameError(ctx, flushAfterEmpty.error(), emptyFailure.error());
    }
    EXPECT_EQ(ctx, emptyState.writeCalls, 0);
    EXPECT_EQ(ctx, emptyState.flushCalls, 0);
    auto emptyClose = emptySink->close();
    auto repeatedEmptyClose = emptySink->close();
    EXPECT_FALSE(ctx, emptyClose);
    EXPECT_FALSE(ctx, repeatedEmptyClose);
    if (!emptyFailure && !emptyClose && !repeatedEmptyClose) {
        expectSameError(ctx, emptyClose.error(), emptyFailure.error());
        expectSameError(ctx, repeatedEmptyClose.error(), emptyFailure.error());
    }
    EXPECT_EQ(ctx, emptyState.closeCalls, 1);

    FakeAvioBackendState oversizedState{2};
    auto oversizedSink = fakeSink(ctx, oversizedState);
    if (!oversizedSink) return;
    const std::array<std::uint8_t, 3> oversized{1, 2, 3};
    auto oversizedFailure = oversizedSink->write(oversized);
    EXPECT_FALSE(ctx, oversizedFailure);
    auto writeAfterOversized = oversizedSink->write(bytes);
    auto flushAfterOversized = oversizedSink->flush();
    auto closeAfterOversized = oversizedSink->close();
    auto repeatedCloseAfterOversized = oversizedSink->close();
    EXPECT_FALSE(ctx, writeAfterOversized);
    EXPECT_FALSE(ctx, flushAfterOversized);
    EXPECT_FALSE(ctx, closeAfterOversized);
    EXPECT_FALSE(ctx, repeatedCloseAfterOversized);
    if (!oversizedFailure && !writeAfterOversized && !flushAfterOversized &&
        !closeAfterOversized && !repeatedCloseAfterOversized) {
        expectSameError(ctx, writeAfterOversized.error(), oversizedFailure.error());
        expectSameError(ctx, flushAfterOversized.error(), oversizedFailure.error());
        expectSameError(ctx, closeAfterOversized.error(), oversizedFailure.error());
        expectSameError(ctx, repeatedCloseAfterOversized.error(), oversizedFailure.error());
    }
    EXPECT_EQ(ctx, oversizedState.writeCalls, 0);
    EXPECT_EQ(ctx, oversizedState.flushCalls, 0);
    EXPECT_EQ(ctx, oversizedState.closeCalls, 1);
}

void testBackendFailuresAreTerminalAndPreserveFirstError(TestContext& ctx)
{
    const std::array<std::uint8_t, 3> bytes{1, 2, 3};

    FakeAvioBackendState writeState{8};
    writeState.writeFailure = AVERROR(EIO);
    writeState.closeFailure = AVERROR(ENOSPC);
    auto writeSink = fakeSink(ctx, writeState);
    if (!writeSink) return;
    auto writeFailure = writeSink->write(bytes);
    EXPECT_FALSE(ctx, writeFailure);
    EXPECT_EQ(ctx, writeState.writeCalls, 1);
    EXPECT_EQ(ctx, writeState.flushCalls, 1);
    auto secondWriteFailure = writeSink->write(bytes);
    auto flushAfterWriteFailure = writeSink->flush();
    EXPECT_FALSE(ctx, secondWriteFailure);
    EXPECT_FALSE(ctx, flushAfterWriteFailure);
    if (!writeFailure && !secondWriteFailure && !flushAfterWriteFailure) {
        expectSameError(ctx, secondWriteFailure.error(), writeFailure.error());
        expectSameError(ctx, flushAfterWriteFailure.error(), writeFailure.error());
    }
    EXPECT_EQ(ctx, writeState.writeCalls, 1);
    EXPECT_EQ(ctx, writeState.flushCalls, 1);
    auto writeClose = writeSink->close();
    EXPECT_FALSE(ctx, writeClose);
    if (!writeFailure && !writeClose) {
        expectSameError(ctx, writeClose.error(), writeFailure.error());
        EXPECT_EQ(ctx, writeFailure.error().nativeCode, AVERROR(EIO));
    }
    EXPECT_EQ(ctx, writeState.closeCalls, 1);
    EXPECT_FALSE(ctx, writeSink->close());
    EXPECT_EQ(ctx, writeState.closeCalls, 1);

    FakeAvioBackendState flushState{8};
    flushState.flushFailure = AVERROR(EPIPE);
    auto flushSink = fakeSink(ctx, flushState);
    if (!flushSink) return;
    auto flushFailure = flushSink->flush();
    EXPECT_FALSE(ctx, flushFailure);
    auto writeAfterFlush = flushSink->write(bytes);
    EXPECT_FALSE(ctx, writeAfterFlush);
    if (!flushFailure && !writeAfterFlush) {
        expectSameError(ctx, writeAfterFlush.error(), flushFailure.error());
    }
    EXPECT_EQ(ctx, flushState.writeCalls, 0);
    EXPECT_EQ(ctx, flushState.flushCalls, 1);

    FakeAvioBackendState closeState{8};
    closeState.closeFailure = AVERROR(ENOSPC);
    auto closeSink = fakeSink(ctx, closeState);
    if (!closeSink) return;
    auto closeFailure = closeSink->close();
    EXPECT_FALSE(ctx, closeFailure);
    EXPECT_FALSE(ctx, closeSink->close());
    EXPECT_EQ(ctx, closeState.closeCalls, 1);
    auto writeAfterCloseFailure = closeSink->write(bytes);
    auto flushAfterCloseFailure = closeSink->flush();
    EXPECT_FALSE(ctx, writeAfterCloseFailure);
    EXPECT_FALSE(ctx, flushAfterCloseFailure);
    if (!closeFailure && !writeAfterCloseFailure && !flushAfterCloseFailure) {
        expectSameError(ctx, writeAfterCloseFailure.error(), closeFailure.error());
        expectSameError(ctx, flushAfterCloseFailure.error(), closeFailure.error());
    }
}

void testBackendCloseOccursExactlyOnce(TestContext& ctx)
{
    FakeAvioBackendState destructorState{8};
    {
        auto sink = fakeSink(ctx, destructorState);
        if (!sink) return;
    }
    EXPECT_EQ(ctx, destructorState.closeCalls, 1);

    FakeAvioBackendState explicitState{8};
    auto sink = fakeSink(ctx, explicitState);
    if (!sink) return;
    EXPECT_TRUE(ctx, sink->close());
    EXPECT_TRUE(ctx, sink->close());
    EXPECT_EQ(ctx, explicitState.closeCalls, 1);
    const std::array<std::uint8_t, 1> byte{1};
    auto writeAfterClose = sink->write(byte);
    auto flushAfterClose = sink->flush();
    EXPECT_FALSE(ctx, writeAfterClose);
    EXPECT_FALSE(ctx, flushAfterClose);
    if (!writeAfterClose && !flushAfterClose) {
        EXPECT_EQ(ctx, writeAfterClose.error().code, ::media::ErrorCode::NotInitialized);
        EXPECT_EQ(ctx, flushAfterClose.error().code, ::media::ErrorCode::NotInitialized);
    }
}

void testShortWriteRemainsObservable(TestContext& ctx)
{
    ShortWriteSink sink;
    const std::array<std::uint8_t, 4> bytes{1, 2, 3, 4};
    auto written = sink.write(bytes);
    EXPECT_TRUE(ctx, written);
    if (written) EXPECT_EQ(ctx, written.value(), bytes.size() - 1);
}

} // namespace

void runOutputByteSinkTests(TestContext& ctx)
{
    testOpenRejectsInvalidArguments(ctx);
    testWriteFlushCloseAndDestructorOwnership(ctx);
    testBufferTransfersOwnershipExactlyOnce(ctx);
    testShortWriteRemainsObservable(ctx);
    testInputFailuresPoisonTheSink(ctx);
    testBackendFailuresAreTerminalAndPreserveFirstError(ctx);
    testBackendCloseOccursExactlyOnce(ctx);
}
