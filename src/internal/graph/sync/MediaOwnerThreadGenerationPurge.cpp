#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"

namespace media::ffmpeg::graph {
namespace {
bool samePurge(const MediaAvGenerationPurge& a, const MediaAvGenerationPurge& b)
{
    return a.oldGeneration == b.oldGeneration && a.nextGeneration == b.nextGeneration &&
        a.transitionSequence == b.transitionSequence;
}
}

::media::Status MediaOwnerThreadGenerationPurge::start(std::shared_ptr<MediaNodeWakeup> ownerWakeup)
{
    std::lock_guard lock(m_mutex);
    if (!ownerWakeup || !m_stopped) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("Owner purge requires a wakeup and a stopped lifecycle"));
    m_ownerWakeup = std::move(ownerWakeup);
    m_request.reset();
    m_result.reset();
    m_waitStop = std::stop_source{};
    m_stopped = false;
    return ::media::Status::success();
}

::media::Status MediaOwnerThreadGenerationPurge::purge(const MediaAvGenerationPurge& purge)
{
    std::unique_lock lock(m_mutex);
    if (m_stopped) return ::media::Status::failure(
        ::media::ErrorInfo::cancelled("Owner purge lifecycle stopped"));
    if (m_request && samePurge(*m_request, purge)) {
        return m_result ? *m_result : ::media::Status::failure(
            ::media::ErrorInfo::wouldBlock("Owner purge awaits worker completion"));
    }
    if (purge.oldGeneration == 0 || purge.nextGeneration <= purge.oldGeneration ||
        purge.transitionSequence == 0 ||
        (m_request && (!m_result || !*m_result ||
            purge.transitionSequence <= m_request->transitionSequence ||
            purge.oldGeneration != m_request->nextGeneration)))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Owner purge requires one exact fresh generation transition"));
    m_request = purge;
    m_result.reset();
    auto wakeup = m_ownerWakeup;
    auto cancellation = m_waitStop;
    lock.unlock();
    cancellation.request_stop();
    wakeup->notify();
    return ::media::Status::failure(
        ::media::ErrorInfo::wouldBlock("Owner purge was queued for its worker"));
}

std::optional<MediaAvGenerationPurge> MediaOwnerThreadGenerationPurge::pending() const
{
    std::lock_guard lock(m_mutex);
    return !m_stopped && !m_result ? m_request : std::nullopt;
}

::media::Status MediaOwnerThreadGenerationPurge::complete(
    const MediaAvGenerationPurge& purge, ::media::Status result)
{
    {
        std::lock_guard lock(m_mutex);
        if (m_stopped || !m_request || !samePurge(*m_request, purge) || m_result ||
            (!result && result.error().code == ::media::ErrorCode::WouldBlock))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Owner purge completion requires its exact unfinished transaction"));
        m_result = std::move(result);
        m_waitStop = std::stop_source{};
    }
    notifyPurgeProgress();
    return ::media::Status::success();
}

std::stop_token MediaOwnerThreadGenerationPurge::waitStopToken() const
{
    std::lock_guard lock(m_mutex);
    return m_waitStop.get_token();
}

void MediaOwnerThreadGenerationPurge::stop() noexcept
{
    std::stop_source cancellation;
    {
        std::lock_guard lock(m_mutex);
        m_stopped = true;
        cancellation = m_waitStop;
    }
    cancellation.request_stop();
    notifyPurgeProgress();
}

} // namespace media::ffmpeg::graph
