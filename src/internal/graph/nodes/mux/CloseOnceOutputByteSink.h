#pragma once

#include "internal/graph/runtime/io/MediaOutputByteSink.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class CloseOnceOutputByteSink final : public MediaOutputByteSink {
public:
    explicit CloseOnceOutputByteSink(std::unique_ptr<MediaOutputByteSink> inner);
    ~CloseOnceOutputByteSink() override;

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override;
    ::media::Status flush() override;
    ::media::Status close() override;

private:
    ::media::Status closedFailure(const char* message) const;
    ::media::Status terminalStatus() const;

    std::unique_ptr<MediaOutputByteSink> m_inner;
    std::optional<::media::ErrorInfo> m_failure;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
