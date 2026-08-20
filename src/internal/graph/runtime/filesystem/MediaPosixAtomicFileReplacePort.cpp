#include "internal/graph/runtime/filesystem/MediaPosixAtomicFileReplacePort.h"

#if !defined(_WIN32)

#include <cerrno>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace media::ffmpeg::graph {
namespace {

class PosixAtomicFileReplaceTransaction final
    : public MediaAtomicFileReplaceTransaction {
public:
    PosixAtomicFileReplaceTransaction(std::string targetPath,
                                      std::string temporaryPath,
                                      int descriptor) noexcept
        : m_targetPath(std::move(targetPath)),
          m_temporaryPath(std::move(temporaryPath)),
          m_descriptor(descriptor)
    {
    }

    ~PosixAtomicFileReplaceTransaction() override
    {
        closeNoThrow();
        if (!m_temporaryPath.empty()) ::unlink(m_temporaryPath.c_str());
    }

    ::media::Status writeAll(std::span<const std::uint8_t> bytes) override
    {
        if (m_phase != Phase::Open || m_descriptor < 0) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Atomic file transaction is not writable"));
        }
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const ssize_t written = ::write(
                m_descriptor, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written < 0) {
                return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                    "Atomic publisher write failed", errno));
            }
            if (written == 0) {
                return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                    "Atomic publisher write made no progress"));
            }
            offset += static_cast<std::size_t>(written);
        }
        return ::media::Status::success();
    }

    ::media::Status flushAndClose() override
    {
        if (m_phase != Phase::Open || m_descriptor < 0) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Atomic file transaction is not open"));
        }
        if (::fsync(m_descriptor) != 0) {
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "Atomic publisher flush failed", errno));
        }
        if (::close(m_descriptor) != 0) {
            const int error = errno;
            m_descriptor = -1;
            m_phase = Phase::Failed;
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "Atomic publisher close failed", error));
        }
        m_descriptor = -1;
        m_phase = Phase::Flushed;
        return ::media::Status::success();
    }

    ::media::Status replaceTarget() override
    {
        if (m_phase != Phase::Flushed) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Atomic file transaction is not flushed"));
        }
        if (::rename(m_temporaryPath.c_str(), m_targetPath.c_str()) != 0) {
            m_phase = Phase::Failed;
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "Atomic publisher replace failed", errno));
        }
        m_temporaryPath.clear();
        m_phase = Phase::Committed;
        return ::media::Status::success();
    }

private:
    enum class Phase { Open, Flushed, Committed, Failed };

    void closeNoThrow() noexcept
    {
        if (m_descriptor >= 0) {
            ::close(m_descriptor);
            m_descriptor = -1;
        }
    }

    std::string m_targetPath;
    std::string m_temporaryPath;
    int m_descriptor = -1;
    Phase m_phase = Phase::Open;
};

} // namespace

::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>
MediaPosixAtomicFileReplacePort::begin(std::string_view targetPathUtf8)
{
    using Result =
        ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>>;
    if (targetPathUtf8.empty() ||
        targetPathUtf8.find('\0') != std::string_view::npos) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Atomic publisher requires a non-empty target path without NUL"));
    }
    std::string targetPath(targetPathUtf8);
    std::string temporaryTemplate = targetPath + ".tmp.XXXXXX";
    std::vector<char> temporaryPath(
        temporaryTemplate.begin(), temporaryTemplate.end());
    temporaryPath.push_back('\0');
    const int descriptor = ::mkstemp(temporaryPath.data());
    if (descriptor < 0) {
        return Result::failure(::media::ErrorInfo::ioFailure(
            "Atomic publisher could not create temporary file", errno));
    }
    try {
        std::unique_ptr<MediaAtomicFileReplaceTransaction> transaction =
            std::make_unique<PosixAtomicFileReplaceTransaction>(
                std::move(targetPath), std::string(temporaryPath.data()), descriptor);
        return Result::success(std::move(transaction));
    } catch (...) {
        ::close(descriptor);
        ::unlink(temporaryPath.data());
        throw;
    }
}

} // namespace media::ffmpeg::graph

#endif
