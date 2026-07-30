#include "internal/graph/protocol/mpegts/MediaTsUdpDatagramSink.h"

#include "internal/graph/runtime/io/MediaOutputByteSink.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaTsUdpDatagramSink::MediaTsUdpDatagramSink(
    std::unique_ptr<MediaOutputByteSink> sink) noexcept
    : m_sink(std::move(sink))
{
}

MediaTsUdpDatagramSink::~MediaTsUdpDatagramSink()
{
    (void)close();
}

::media::Result<std::unique_ptr<MediaTsUdpDatagramSink>>
MediaTsUdpDatagramSink::create(std::unique_ptr<MediaOutputByteSink> sink)
{
    if (!sink) {
        return ::media::Result<
            std::unique_ptr<MediaTsUdpDatagramSink>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS UDP datagram sink requires an owned byte sink"));
    }
    auto adapter = std::unique_ptr<MediaTsUdpDatagramSink>(
        new (std::nothrow) MediaTsUdpDatagramSink(std::move(sink)));
    if (!adapter) {
        return ::media::Result<
            std::unique_ptr<MediaTsUdpDatagramSink>>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaTsUdpDatagramSink"));
    }
    return ::media::Result<
        std::unique_ptr<MediaTsUdpDatagramSink>>::success(
        std::move(adapter));
}

::media::Status MediaTsUdpDatagramSink::terminalStatus() const
{
    return ::media::Status::failure(*m_failure);
}

::media::Status MediaTsUdpDatagramSink::fail(::media::ErrorInfo error)
{
    if (!m_failure) m_failure = std::move(error);
    return terminalStatus();
}

::media::Result<std::size_t> MediaTsUdpDatagramSink::write(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime emitOnMaster)
{
    if (m_failure) {
        return ::media::Result<std::size_t>::failure(*m_failure);
    }
    if (m_closed || !m_sink) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "MPEG-TS UDP datagram sink is closed"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    if (completeTsPackets.empty() ||
        (completeTsPackets.size() % std::size_t{188}) != 0 ||
        (m_lastEmitOnMaster &&
         emitOnMaster < *m_lastEmitOnMaster)) {
        auto status = fail(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP datagram sink requires ordered complete TS packets"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    auto written = m_sink->write(completeTsPackets);
    if (!written) {
        auto status = fail(written.error());
        return ::media::Result<std::size_t>::failure(status.error());
    }
    if (written.value() != completeTsPackets.size()) {
        auto status = fail(::media::ErrorInfo::ioFailure(
            "MPEG-TS UDP byte sink returned a short datagram write"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    m_lastEmitOnMaster = emitOnMaster;
    return written;
}

::media::Status MediaTsUdpDatagramSink::flush()
{
    if (m_failure) return terminalStatus();
    if (m_closed || !m_sink) {
        return fail(::media::ErrorInfo::notInitialized(
            "MPEG-TS UDP datagram sink cannot flush after close"));
    }
    auto status = m_sink->flush();
    return status ? status : fail(status.error());
}

::media::Status MediaTsUdpDatagramSink::close()
{
    if (m_closed) {
        return m_failure ? terminalStatus() : ::media::Status::success();
    }
    m_closed = true;
    if (!m_sink) {
        return fail(::media::ErrorInfo::notInitialized(
            "MPEG-TS UDP datagram sink lost its byte sink"));
    }
    auto status = m_sink->close();
    if (!status) fail(status.error());
    return m_failure ? terminalStatus() : ::media::Status::success();
}

} // namespace media::ffmpeg::graph
