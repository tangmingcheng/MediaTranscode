#include "internal/graph/protocol/mpegts/MediaTsScheduledDatagramSink.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaTsScheduledDatagramSink::MediaTsScheduledDatagramSink(
    std::shared_ptr<MediaScheduledDatagramBatchBuilder> builder,
    std::uint16_t packetSizeBytes) noexcept
    : m_builder(std::move(builder)), m_packetSizeBytes(packetSizeBytes)
{
}

::media::Result<std::unique_ptr<MediaTsScheduledDatagramSink>>
MediaTsScheduledDatagramSink::create(
    std::shared_ptr<MediaScheduledDatagramBatchBuilder> builder,
    std::uint16_t packetSizeBytes)
{
    using Result = ::media::Result<std::unique_ptr<MediaTsScheduledDatagramSink>>;
    if (!builder || builder->released() || packetSizeBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled MPEG-TS sink requires an active batch builder"));
    }
    auto sink = std::unique_ptr<MediaTsScheduledDatagramSink>(
        new (std::nothrow) MediaTsScheduledDatagramSink(
            std::move(builder), packetSizeBytes));
    if (!sink) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaTsScheduledDatagramSink"));
    }
    return Result::success(std::move(sink));
}

::media::Status MediaTsScheduledDatagramSink::fail(::media::ErrorInfo error)
{
    if (!m_failure) m_failure = std::move(error);
    return ::media::Status::failure(*m_failure);
}

::media::Result<std::size_t> MediaTsScheduledDatagramSink::write(
    std::span<const std::uint8_t> completeTsPackets,
    const MediaTsDatagramEnqueueWindow& enqueueWindow)
{
    if (m_failure) return ::media::Result<std::size_t>::failure(*m_failure);
    if (m_closed || !m_builder) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "scheduled MPEG-TS sink is closed"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    if (completeTsPackets.empty() ||
        completeTsPackets.size() % m_packetSizeBytes != 0) {
        auto status = fail(::media::ErrorInfo::invalidArgument(
            "scheduled MPEG-TS sink requires complete TS packets"));
        return ::media::Result<std::size_t>::failure(status.error());
    }
    auto appended = m_builder->append(
        completeTsPackets, enqueueWindow.notBefore(), enqueueWindow.deadline(),
        enqueueWindow.serviceDuration());
    if (!appended) {
        auto status = fail(appended.error());
        return ::media::Result<std::size_t>::failure(status.error());
    }
    return ::media::Result<std::size_t>::success(completeTsPackets.size());
}

::media::Status MediaTsScheduledDatagramSink::flush()
{
    return m_failure ? ::media::Status::failure(*m_failure)
                     : ::media::Status::success();
}

::media::Status MediaTsScheduledDatagramSink::close()
{
    m_closed = true;
    return m_failure ? ::media::Status::failure(*m_failure)
                     : ::media::Status::success();
}

void MediaTsScheduledDatagramSink::abort() noexcept
{
    m_closed = true;
}

} // namespace media::ffmpeg::graph
