#include "internal/graph/sync/MediaAvAggregateCoordination.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}
}

MediaAvAggregateCoordination::MediaAvAggregateCoordination(
    const MediaAvContinuousAggregateTopology& plan)
    : m_sources(plan.sources.size()), m_audioSource(plan.audioSource),
      m_sampleRate(plan.audio.sampleRate())
{
    for (const auto& source : plan.sources)
        m_sources.push_back({{plan.initialGeneration, std::nullopt, false},
            {plan.initialGeneration, std::nullopt, !source.audioIdentity.has_value()},
            source.videoProduct, std::nullopt});
}

::media::Status MediaAvAggregateCoordination::bindWakeup(
    MediaStreamKind owner, std::shared_ptr<MediaNodeWakeup> wakeup)
{
    if (!wakeup || (owner != MediaStreamKind::Video && owner != MediaStreamKind::Audio))
        return invalid("Aggregate coordination requires a media owner wakeup");
    {
        std::lock_guard lock(m_mutex);
        if (m_stopped) return invalid("Aggregate coordination is stopped");
        auto& slot = owner == MediaStreamKind::Video ? m_videoWakeup : m_audioWakeup;
        if (!slot.expired()) return invalid("Aggregate coordination owner is already bound");
        slot = wakeup;
    }
    // A notification is retained even if preparation preceded registration.
    wakeup->notify();
    return ::media::Status::success();
}

::media::Status MediaAvAggregateCoordination::publishAudioReady(MediaAvAggregateAudioReady ready)
{
    std::shared_ptr<MediaNodeWakeup> wakeup;
    {
        std::lock_guard lock(m_mutex);
        auto* selected = input(m_audioSource, MediaStreamKind::Audio);
        if (m_stopped || !selected ||
            ready.origin.generation != selected->minimumGeneration ||
            ready.origin.outputSampleRate != m_sampleRate || ready.interval.sampleRate != m_sampleRate ||
            ready.origin.epochOutputSampleIndex < 0 || ready.interval.begin < ready.origin.epochOutputSampleIndex ||
            ready.interval.end <= ready.interval.begin)
            return invalid("Aggregate audio readiness differs from its prepared source generation");
        if (m_audioReady) return *m_audioReady == ready ? ::media::Status::success()
            : invalid("Aggregate audio readiness must be invalidated before replacement");
        m_audioReady = ready;
        wakeup = m_videoWakeup.lock();
    }
    if (wakeup) wakeup->notify();
    return ::media::Status::success();
}

MediaAvAggregateInputCompletion* MediaAvAggregateCoordination::input(
    std::size_t source, MediaStreamKind stream) noexcept
{
    if (source >= m_sources.size()) return nullptr;
    if (stream == MediaStreamKind::Video) return &m_sources[source].video;
    if (stream == MediaStreamKind::Audio) return &m_sources[source].audio;
    return nullptr;
}

::media::Status MediaAvAggregateCoordination::observeInputEnd(
    std::size_t source, MediaStreamKind stream, std::uint64_t generation, MediaRunningTime end)
{
    std::lock_guard lock(m_mutex);
    auto* state = input(source, stream);
    if (m_stopped || !state || state->sealed || generation != state->minimumGeneration ||
        (stream == MediaStreamKind::Video &&
         m_sources[source].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint))
        return invalid("Aggregate input end requires an open current input");
    if (!state->end || end > *state->end) state->end = end;
    return ::media::Status::success();
}

::media::Status MediaAvAggregateCoordination::sealInput(std::size_t source, MediaStreamKind stream)
{
    std::shared_ptr<MediaNodeWakeup> video, audio;
    {
        std::lock_guard lock(m_mutex);
        auto* state = input(source, stream);
        if (m_stopped || !state || (stream == MediaStreamKind::Video &&
            m_sources[source].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint))
            return invalid("Aggregate interval seal requires its planned interval input");
        state->sealed = true;
        video = m_videoWakeup.lock();
        audio = m_audioWakeup.lock();
    }
    if (video) video->notify();
    if (audio) audio->notify();
    return ::media::Status::success();
}

::media::Status MediaAvAggregateCoordination::sealDisplayInput(std::size_t source,
    std::uint64_t generation, std::optional<MediaAvAggregateDisplayCommit> lastCommit)
{
    std::shared_ptr<MediaNodeWakeup> video, audio;
    {
        std::lock_guard lock(m_mutex);
        auto* state = input(source, MediaStreamKind::Video);
        if (m_stopped || !state || state->sealed || generation != state->minimumGeneration ||
            m_sources[source].videoProduct != MediaVideoFrameLineageProduct::SourceDisplayPoint ||
            state->end || (lastCommit && lastCommit->decodedSequence == 0))
            return invalid("Display completion requires a current unsealed display input and committed output evidence");
        m_sources[source].finalDisplayCommit = lastCommit;
        state->sealed = true;
        video = m_videoWakeup.lock();
        audio = m_audioWakeup.lock();
    }
    if (video) video->notify();
    if (audio) audio->notify();
    return ::media::Status::success();
}

::media::Status MediaAvAggregateCoordination::purgeInput(std::size_t source,
    MediaStreamKind stream, std::uint64_t oldGeneration, std::uint64_t nextGeneration)
{
    std::shared_ptr<MediaNodeWakeup> wakeup;
    {
        std::lock_guard lock(m_mutex);
        auto* state = input(source, stream);
        if (m_stopped || !state || state->sealed || !oldGeneration ||
            oldGeneration != state->minimumGeneration || nextGeneration <= oldGeneration)
            return invalid("Aggregate input purge requires an open advancing generation");
        state->minimumGeneration = nextGeneration;
        state->end.reset();
        if (stream == MediaStreamKind::Video) m_sources[source].finalDisplayCommit.reset();
        if (source == m_audioSource && stream == MediaStreamKind::Audio) {
            m_audioReady.reset();
            wakeup = m_videoWakeup.lock();
        }
    }
    if (wakeup) wakeup->notify();
    return ::media::Status::success();
}

MediaAvAggregateCoordinationSnapshot MediaAvAggregateCoordination::snapshot() const
{
    std::lock_guard lock(m_mutex);
    MediaAvAggregateCoordinationSnapshot result{m_audioReady, !m_stopped, std::nullopt};
    for (const auto& source : m_sources) {
        if (source.finalDisplayCommit && (!result.finalOutputEnd ||
            source.finalDisplayCommit->outputEnd > *result.finalOutputEnd))
            result.finalOutputEnd = source.finalDisplayCommit->outputEnd;
        for (const auto* state : {&source.video, &source.audio}) {
            result.allInputsSealed &= state->sealed;
            if (state->end && (!result.finalOutputEnd || *state->end > *result.finalOutputEnd))
                result.finalOutputEnd = state->end;
        }
    }
    if (!result.allInputsSealed) result.finalOutputEnd.reset();
    return result;
}

::media::Status MediaAvAggregateCoordination::requestDrain()
{
    std::shared_ptr<MediaNodeWakeup> video, audio;
    {
        std::lock_guard lock(m_mutex);
        if (m_stopped) return invalid("Aggregate drain requires a live coordination lifecycle");
        m_drain.requested = true;
        video = m_videoWakeup.lock();
        audio = m_audioWakeup.lock();
    }
    if (video) video->notify();
    if (audio) audio->notify();
    return ::media::Status::success();
}

::media::Status MediaAvAggregateCoordination::publishOutputClosed(MediaStreamKind stream)
{
    std::lock_guard lock(m_mutex);
    if (m_stopped || !m_drain.requested ||
        (stream != MediaStreamKind::Video && stream != MediaStreamKind::Audio))
        return invalid("Aggregate output closure requires its requested drain");
    (stream == MediaStreamKind::Video ? m_drain.videoClosed : m_drain.audioClosed) = true;
    return ::media::Status::success();
}

MediaAvAggregateDrainSnapshot MediaAvAggregateCoordination::drainSnapshot() const
{
    std::lock_guard lock(m_mutex);
    return m_drain;
}

void MediaAvAggregateCoordination::stop() noexcept
{
    std::shared_ptr<MediaNodeWakeup> video, audio;
    {
        std::lock_guard lock(m_mutex);
        m_stopped = true;
        m_audioReady.reset();
        video = m_videoWakeup.lock();
        audio = m_audioWakeup.lock();
        m_videoWakeup.reset();
        m_audioWakeup.reset();
    }
    if (video) video->notify();
    if (audio) audio->notify();
}

} // namespace media::ffmpeg::graph
