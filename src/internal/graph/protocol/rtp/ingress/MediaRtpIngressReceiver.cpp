#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressReceiver.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalidReceiver(const char* reason)
{
    return ::media::ErrorInfo::invalidArgument(reason);
}

::media::ErrorInfo unavailableAdapter()
{
    return ::media::ErrorInfo::notInitialized(
        "RTP ingress receiver has no platform adapter");
}

} // namespace

MediaRtpIngressReceiver::MediaRtpIngressReceiver(
    MediaRtpIngressStorage storage,
    std::unique_ptr<MediaRtpIngressAdapter> adapter) noexcept
    : m_storage(std::move(storage)), m_adapter(std::move(adapter))
{
}

::media::Result<MediaRtpIngressReceiver> MediaRtpIngressReceiver::create(
    const MediaRtpIngressPlan& plan,
    std::unique_ptr<MediaRtpIngressAdapter> adapter)
{
    if (auto status = plan.validateProduct(); !status) {
        return ::media::Result<MediaRtpIngressReceiver>::failure(
            status.error());
    }
    if (!adapter || adapter->kind() != plan.adapterKind()) {
        return ::media::Result<MediaRtpIngressReceiver>::failure(
            invalidReceiver(
                "RTP ingress adapter does not match the planner product"));
    }
    auto storage = MediaRtpIngressStorage::create(
        plan.batchByteCapacity(),
        plan.maximumDatagramBytes(),
        plan.descriptorCapacity(),
        plan.requiredBufferAlignmentBytes());
    if (!storage) {
        return ::media::Result<MediaRtpIngressReceiver>::failure(
            storage.error());
    }
    return ::media::Result<MediaRtpIngressReceiver>::success(
        MediaRtpIngressReceiver(
            std::move(storage).value(), std::move(adapter)));
}

::media::Result<MediaRtpIngressBatch>
MediaRtpIngressReceiver::receiveNext()
{
    if (!m_adapter) {
        return ::media::Result<MediaRtpIngressBatch>::failure(
            unavailableAdapter());
    }
    if (auto status = m_storage.reset(); !status) {
        return ::media::Result<MediaRtpIngressBatch>::failure(
            status.error());
    }
    auto received = m_adapter->receive(m_storage);
    if (!received) {
        if (m_storage.committedEntries() != 0) {
            return ::media::Result<MediaRtpIngressBatch>::failure(
                ::media::ErrorInfo::internalError(
                    "RTP ingress adapter failed after committing a partial batch"));
        }
        return ::media::Result<MediaRtpIngressBatch>::failure(
            received.error());
    }
    if (received.value() == 0 ||
        received.value() != m_storage.committedEntries()) {
        return ::media::Result<MediaRtpIngressBatch>::failure(
            ::media::ErrorInfo::internalError(
                "RTP ingress adapter reported inconsistent completion evidence"));
    }
    return m_storage.seal(received.value());
}

::media::Status MediaRtpIngressReceiver::interruptReceive() noexcept
{
    return m_adapter
        ? m_adapter->interruptReceive()
        : ::media::Status::failure(unavailableAdapter());
}

::media::Status MediaRtpIngressReceiver::stop() noexcept
{
    return m_adapter
        ? m_adapter->stop()
        : ::media::Status::failure(unavailableAdapter());
}

::media::Status MediaRtpIngressReceiver::abort() noexcept
{
    return m_adapter
        ? m_adapter->abort()
        : ::media::Status::failure(unavailableAdapter());
}

} // namespace media::ffmpeg::graph
