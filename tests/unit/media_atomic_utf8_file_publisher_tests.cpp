#include "common/TestAssert.h"

#include "internal/graph/runtime/filesystem/MediaAtomicUtf8FilePublisher.h"
#include "internal/graph/runtime/filesystem/MediaWin32AtomicFileReplacePort.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

enum class FailureStage {
    None,
    Begin,
    Write,
    Flush,
    Replace,
    ThrowOnBegin,
    ThrowOnWrite,
    ThrowOnFlush,
    ThrowOnReplace,
    EmptyTransaction
};

struct FakeState final {
    FailureStage failure = FailureStage::None;
    int beginCalls = 0;
    int writeCalls = 0;
    int flushCalls = 0;
    int replaceCalls = 0;
    int destroyedTransactions = 0;
    std::string target;
    std::string content;
};

class FakeTransaction final : public MediaAtomicFileReplaceTransaction {
public:
    explicit FakeTransaction(FakeState& state) noexcept : m_state(state) {}
    ~FakeTransaction() override { ++m_state.destroyedTransactions; }

    ::media::Status writeAll(std::span<const std::uint8_t> bytes) override
    {
        ++m_state.writeCalls;
        if (m_state.failure == FailureStage::ThrowOnWrite) {
            throw std::runtime_error("injected write exception");
        }
        if (m_state.failure == FailureStage::Write) {
            return failure(::media::ErrorCode::IoFailure,
                           "injected write failure", 204);
        }
        m_state.content.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return ::media::Status::success();
    }

    ::media::Status flushAndClose() override
    {
        ++m_state.flushCalls;
        if (m_state.failure == FailureStage::ThrowOnFlush) {
            throw std::runtime_error("injected flush exception");
        }
        return m_state.failure == FailureStage::Flush
            ? failure(::media::ErrorCode::FFmpegFailure,
                      "injected flush failure", 302)
            : ::media::Status::success();
    }

    ::media::Status replaceTarget() override
    {
        ++m_state.replaceCalls;
        if (m_state.failure == FailureStage::ThrowOnReplace) {
            throw std::runtime_error("injected replace exception");
        }
        return m_state.failure == FailureStage::Replace
            ? failure(::media::ErrorCode::Cancelled,
                      "injected replace failure", 403)
            : ::media::Status::success();
    }

private:
    static ::media::Status failure(
        ::media::ErrorCode code,
        std::string message,
        int nativeCode)
    {
        return ::media::Status::failure(
            ::media::ErrorInfo::make(code, std::move(message), nativeCode));
    }

    FakeState& m_state;
};

class FakePort final : public MediaAtomicFileReplacePort {
public:
    explicit FakePort(FakeState& state) noexcept : m_state(state) {}

    ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>> begin(
        std::string_view targetPathUtf8) override
    {
        ++m_state.beginCalls;
        m_state.target.assign(targetPathUtf8);
        if (m_state.failure == FailureStage::ThrowOnBegin) {
            throw std::runtime_error("injected begin exception");
        }
        if (m_state.failure == FailureStage::Begin) {
            return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::failure(
                ::media::ErrorInfo::make(
                    ::media::ErrorCode::NotInitialized,
                    "injected begin failure", 101));
        }
        if (m_state.failure == FailureStage::EmptyTransaction) {
            return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::success(
                nullptr);
        }
        return ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>::success(
            std::make_unique<FakeTransaction>(m_state));
    }

private:
    FakeState& m_state;
};

void testPublisherSequenceAndInputContract(TestContext& ctx)
{
    FakeState state;
    FakePort port(state);
    MediaAtomicUtf8FilePublisher publisher(port);
    EXPECT_TRUE(ctx, publisher.publish("output.sdp", "v=0\r\n"));
    EXPECT_EQ(ctx, state.beginCalls, 1);
    EXPECT_EQ(ctx, state.writeCalls, 1);
    EXPECT_EQ(ctx, state.flushCalls, 1);
    EXPECT_EQ(ctx, state.replaceCalls, 1);
    EXPECT_EQ(ctx, state.destroyedTransactions, 1);
    EXPECT_EQ(ctx, state.target, std::string("output.sdp"));
    EXPECT_EQ(ctx, state.content, std::string("v=0\r\n"));

    EXPECT_FALSE(ctx, publisher.publish({}, "v=0\r\n"));
    EXPECT_FALSE(ctx, publisher.publish("output.sdp", {}));
    const std::string nulPath("bad\0path", 8);
    EXPECT_FALSE(ctx, publisher.publish(nulPath, "v=0\r\n"));
    EXPECT_EQ(ctx, state.beginCalls, 1);

    for (const std::string invalid : {
             std::string("\xC0\xAF", 2),
             std::string("\xED\xA0\x80", 3),
             std::string("\xF0\x9F\x92", 3),
             std::string("valid\0invalid", 13)}) {
        EXPECT_FALSE(ctx, publisher.publish("output.sdp", invalid));
    }
    EXPECT_EQ(ctx, state.beginCalls, 1);
    EXPECT_TRUE(ctx, publisher.publish(
        "output.sdp", "v=0\r\ns=\xE5\xAA\x92\xE4\xBD\x93\r\n"));
    EXPECT_EQ(ctx, state.beginCalls, 2);
}

void testFailuresStopBeforeReplacementAndDestroyTransaction(TestContext& ctx)
{
    for (const auto failure : {FailureStage::Begin, FailureStage::Write,
                               FailureStage::Flush, FailureStage::Replace,
                               FailureStage::ThrowOnBegin,
                               FailureStage::ThrowOnWrite,
                               FailureStage::ThrowOnFlush,
                               FailureStage::ThrowOnReplace,
                               FailureStage::EmptyTransaction}) {
        FakeState state;
        state.failure = failure;
        FakePort port(state);
        MediaAtomicUtf8FilePublisher publisher(port);
        EXPECT_FALSE(ctx, publisher.publish("output.sdp", "new"));
        if (failure == FailureStage::Begin ||
            failure == FailureStage::ThrowOnBegin ||
            failure == FailureStage::EmptyTransaction) {
            EXPECT_EQ(ctx, state.destroyedTransactions, 0);
            EXPECT_EQ(ctx, state.writeCalls, 0);
        } else {
            EXPECT_EQ(ctx, state.destroyedTransactions, 1);
            EXPECT_EQ(ctx, state.writeCalls, 1);
        }
        if (failure == FailureStage::Write || failure == FailureStage::ThrowOnWrite ||
            failure == FailureStage::Begin || failure == FailureStage::ThrowOnBegin ||
            failure == FailureStage::EmptyTransaction) {
            EXPECT_EQ(ctx, state.flushCalls, 0);
            EXPECT_EQ(ctx, state.replaceCalls, 0);
        }
        if (failure == FailureStage::Flush || failure == FailureStage::ThrowOnFlush) {
            EXPECT_EQ(ctx, state.replaceCalls, 0);
        }
    }
}

void testStructuredStageErrorsArePreserved(TestContext& ctx)
{
    struct Expected final {
        FailureStage stage;
        ::media::ErrorCode code;
        const char* message;
        int nativeCode;
    };
    for (const auto& expected : {
             Expected{FailureStage::Begin, ::media::ErrorCode::NotInitialized,
                      "injected begin failure", 101},
             Expected{FailureStage::Write, ::media::ErrorCode::IoFailure,
                      "injected write failure", 204},
             Expected{FailureStage::Flush, ::media::ErrorCode::FFmpegFailure,
                      "injected flush failure", 302},
             Expected{FailureStage::Replace, ::media::ErrorCode::Cancelled,
                      "injected replace failure", 403}}) {
        FakeState state;
        state.failure = expected.stage;
        FakePort port(state);
        MediaAtomicUtf8FilePublisher publisher(port);
        const auto published = publisher.publish("output.sdp", "v=0\r\n");
        EXPECT_FALSE(ctx, published);
        if (!published) {
            EXPECT_EQ(ctx, published.error().code, expected.code);
            EXPECT_EQ(ctx, published.error().message, std::string(expected.message));
            EXPECT_EQ(ctx, published.error().nativeCode, expected.nativeCode);
        }
    }
}

void testThrownAndEmptyTransactionBoundaries(TestContext& ctx)
{
    for (const auto stage : {FailureStage::ThrowOnBegin,
                             FailureStage::ThrowOnWrite,
                             FailureStage::ThrowOnFlush,
                             FailureStage::ThrowOnReplace}) {
        FakeState state;
        state.failure = stage;
        FakePort port(state);
        MediaAtomicUtf8FilePublisher publisher(port);
        const auto published = publisher.publish("output.sdp", "v=0\r\n");
        EXPECT_FALSE(ctx, published);
        if (!published) {
            EXPECT_EQ(ctx, published.error().code, ::media::ErrorCode::IoFailure);
            EXPECT_TRUE(ctx, published.error().message.starts_with(
                "Atomic publisher operation threw: injected "));
            EXPECT_EQ(ctx, published.error().nativeCode, 0);
        }
    }

    FakeState state;
    state.failure = FailureStage::EmptyTransaction;
    FakePort port(state);
    MediaAtomicUtf8FilePublisher publisher(port);
    const auto published = publisher.publish("output.sdp", "v=0\r\n");
    EXPECT_FALSE(ctx, published);
    if (!published) {
        EXPECT_EQ(ctx, published.error().code, ::media::ErrorCode::InternalError);
        EXPECT_EQ(ctx, published.error().message,
                  std::string("Atomic publisher port returned an empty transaction"));
        EXPECT_EQ(ctx, published.error().nativeCode, 0);
    }
    EXPECT_EQ(ctx, state.beginCalls, 1);
    EXPECT_EQ(ctx, state.writeCalls, 0);
    EXPECT_EQ(ctx, state.destroyedTransactions, 0);
}

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void removeMatchingTemporaryFiles(const std::filesystem::path& target)
{
    const auto parent = target.parent_path().empty()
        ? std::filesystem::current_path()
        : target.parent_path();
    const auto prefix = target.filename().wstring() + L".tmp.";
    for (const auto& entry : std::filesystem::directory_iterator(parent)) {
        if (entry.path().filename().wstring().starts_with(prefix)) {
            std::filesystem::remove(entry.path());
        }
    }
}

std::size_t matchingTemporaryFileCount(const std::filesystem::path& target)
{
    const auto parent = target.parent_path().empty()
        ? std::filesystem::current_path()
        : target.parent_path();
    const auto prefix = target.filename().wstring() + L".tmp.";
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(parent)) {
        if (entry.path().filename().wstring().starts_with(prefix)) ++count;
    }
    return count;
}

void testWin32PublisherCreatesAndReplacesUnicodeTarget(TestContext& ctx)
{
#if defined(_WIN32)
    const std::filesystem::path target =
        std::filesystem::current_path() / std::filesystem::path(L"task10_3_原子发布.sdp");
    std::filesystem::remove(target);
    removeMatchingTemporaryFiles(target);

    MediaWin32AtomicFileReplacePort port;
    MediaAtomicUtf8FilePublisher publisher(port);
    const auto targetUtf8 = target.u8string();
    const std::string path(reinterpret_cast<const char*>(targetUtf8.data()), targetUtf8.size());
    EXPECT_TRUE(ctx, publisher.publish(path, "old\r\n"));
    EXPECT_EQ(ctx, readFile(target), std::string("old\r\n"));
    EXPECT_TRUE(ctx, publisher.publish(path, "new\r\ncomplete\r\n"));
    EXPECT_EQ(ctx, readFile(target), std::string("new\r\ncomplete\r\n"));
    EXPECT_EQ(ctx, matchingTemporaryFileCount(target), static_cast<std::size_t>(0));

    std::filesystem::remove(target);
#endif
}

void testWin32PublisherRejectsInvalidPathAndCleansFailedReplace(TestContext& ctx)
{
#if defined(_WIN32)
    MediaWin32AtomicFileReplacePort port;
    MediaAtomicUtf8FilePublisher publisher(port);
    const std::string invalidUtf8("\xC3\x28", 2);
    EXPECT_FALSE(ctx, publisher.publish(invalidUtf8, "content"));

    const auto directory = std::filesystem::current_path() / "task10_3_target_directory";
    std::filesystem::create_directories(directory);
    removeMatchingTemporaryFiles(directory);
    const auto directoryUtf8 = directory.u8string();
    const std::string path(reinterpret_cast<const char*>(directoryUtf8.data()),
                           directoryUtf8.size());
    EXPECT_FALSE(ctx, publisher.publish(path, "content"));
    EXPECT_TRUE(ctx, std::filesystem::is_directory(directory));
    EXPECT_EQ(ctx, matchingTemporaryFileCount(directory), static_cast<std::size_t>(0));
    std::filesystem::remove(directory);
#endif
}

void testWin32FailedReplacementPreservesExistingFile(TestContext& ctx)
{
#if defined(_WIN32)
    const auto target = std::filesystem::current_path() /
                        "task10_3_locked_target.sdp";
    std::filesystem::remove(target);
    removeMatchingTemporaryFiles(target);
    {
        std::ofstream output(target, std::ios::binary);
        output << "old-complete\r\n";
    }
    HANDLE lock = CreateFileW(
        target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    EXPECT_TRUE(ctx, lock != INVALID_HANDLE_VALUE);
    if (lock == INVALID_HANDLE_VALUE) return;

    MediaWin32AtomicFileReplacePort port;
    MediaAtomicUtf8FilePublisher publisher(port);
    const auto targetUtf8 = target.u8string();
    const std::string path(reinterpret_cast<const char*>(targetUtf8.data()),
                           targetUtf8.size());
    EXPECT_FALSE(ctx, publisher.publish(path, "new-complete\r\n"));
    CloseHandle(lock);
    EXPECT_EQ(ctx, readFile(target), std::string("old-complete\r\n"));
    EXPECT_EQ(ctx, matchingTemporaryFileCount(target), static_cast<std::size_t>(0));
    std::filesystem::remove(target);
#endif
}

} // namespace

int main()
{
    TestContext ctx;
    testPublisherSequenceAndInputContract(ctx);
    testFailuresStopBeforeReplacementAndDestroyTransaction(ctx);
    testStructuredStageErrorsArePreserved(ctx);
    testThrownAndEmptyTransactionBoundaries(ctx);
    testWin32PublisherCreatesAndReplacesUnicodeTarget(ctx);
    testWin32PublisherRejectsInvalidPathAndCleansFailedReplace(ctx);
    testWin32FailedReplacementPreservesExistingFile(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
