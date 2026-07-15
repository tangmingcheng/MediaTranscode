#include "internal/graph/nodes/mux/CloseOnceOutputByteSink.h"

#include <utility>

namespace media::ffmpeg::graph {

CloseOnceOutputByteSink::CloseOnceOutputByteSink(
    std::unique_ptr<MediaOutputByteSink> inner)
    : m_inner(std::move(inner))
{
}

CloseOnceOutputByteSink::~CloseOnceOutputByteSink()
{
    close();
}

::media::Result<std::size_t> CloseOnceOutputByteSink::write(
    std::span<const std::uint8_t> bytes)
{
    if (m_failure) {
        return ::media::Result<std::size_t>::failure(*m_failure);
    }
    if (m_closed || !m_inner) {
        auto status = closedFailure("project MPEG-TS output sink is closed");
        return ::media::Result<std::size_t>::failure(status.error());
    }
    auto result = m_inner->write(bytes);
    if (!result) m_failure = result.error();
    return m_failure
        ? ::media::Result<std::size_t>::failure(*m_failure)
        : result;
}

::media::Status CloseOnceOutputByteSink::flush()
{
    if (m_failure) return terminalStatus();
    if (m_closed || !m_inner) {
        return closedFailure("project MPEG-TS output sink is closed");
    }
    auto status = m_inner->flush();
    if (!status) m_failure = status.error();
    return m_failure ? terminalStatus() : status;
}

::media::Status CloseOnceOutputByteSink::close()
{
    if (m_closed) {
        return m_failure ? terminalStatus() : ::media::Status::success();
    }
    m_closed = true;
    if (!m_inner) {
        return closedFailure("project MPEG-TS output sink is missing");
    }
    auto status = m_inner->close();
    if (!status && !m_failure) m_failure = status.error();
    return m_failure ? terminalStatus() : status;
}

::media::Status CloseOnceOutputByteSink::closedFailure(
    const char* message) const
{
    return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized(message));
}

::media::Status CloseOnceOutputByteSink::terminalStatus() const
{
    return ::media::Status::failure(*m_failure);
}

} // namespace media::ffmpeg::graph
