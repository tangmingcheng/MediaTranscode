#include "internal/graph/runtime/filesystem/MediaAtomicUtf8FilePublisher.h"
#include "internal/graph/protocol/MediaUtf8TextValidator.h"

#include <exception>
#include <string>

namespace media::ffmpeg::graph {

MediaAtomicUtf8FilePublisher::MediaAtomicUtf8FilePublisher(
    MediaAtomicFileReplacePort& port) noexcept
    : m_port(port)
{
}

::media::Status MediaAtomicUtf8FilePublisher::publish(
    std::string_view targetPathUtf8,
    std::string_view contentUtf8)
{
    if (targetPathUtf8.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Atomic publisher requires a target path"));
    }
    if (targetPathUtf8.find('\0') != std::string_view::npos) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Atomic publisher target path contains NUL"));
    }
    if (contentUtf8.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Atomic publisher requires non-empty content"));
    }
    if (auto validContent = MediaUtf8TextValidator::validateWellFormed(
            contentUtf8, MediaUtf8ControlPolicy::AllowCrLf);
        !validContent) {
        return validContent;
    }

    try {
        auto transaction = m_port.begin(targetPathUtf8);
        if (!transaction) {
            return ::media::Status::failure(transaction.error());
        }
        if (!transaction.value()) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "Atomic publisher port returned an empty transaction"));
        }

        const auto* begin = reinterpret_cast<const std::uint8_t*>(contentUtf8.data());
        const std::span<const std::uint8_t> bytes(begin, contentUtf8.size());
        if (auto status = transaction.value()->writeAll(bytes); !status) {
            return status;
        }
        if (auto status = transaction.value()->flushAndClose(); !status) {
            return status;
        }
        return transaction.value()->replaceTarget();
    } catch (const std::exception& error) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            std::string("Atomic publisher operation threw: ") + error.what()));
    } catch (...) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ioFailure("Atomic publisher operation threw"));
    }
}

} // namespace media::ffmpeg::graph
