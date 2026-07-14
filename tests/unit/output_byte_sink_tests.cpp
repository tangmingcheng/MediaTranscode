#include "common/TestAssert.h"

#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSink.h"

extern "C" {
#include <libavformat/avio.h>
}

#include <array>
#include <algorithm>
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
        EXPECT_FALSE(ctx, sink->write({}));
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
    EXPECT_FALSE(ctx, created.value()->takeSink());
    created.value().reset();
    EXPECT_FALSE(ctx, destroyed);
    transferred.value().reset();
    EXPECT_TRUE(ctx, destroyed);
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
}
