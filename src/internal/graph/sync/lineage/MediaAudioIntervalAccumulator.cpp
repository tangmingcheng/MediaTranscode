#include "internal/graph/sync/lineage/MediaAudioIntervalAccumulator.h"
#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaAudioIntervalAccumulator::fail(std::string message)
{
    m_terminalFailure = true;
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(std::move(message)));
}

::media::Status MediaAudioIntervalAccumulator::push(
    MediaAudioIntervalFragment fragment)
{
    if (m_terminalFailure) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio interval accumulator is terminal"));
    }
    const auto fragmentSamples = fragment.interval.sampleCount();
    if (!fragment.lineage || !fragmentSamples ||
        !validateMediaCanonicalLineage(*fragment.lineage)) {
        return fail("Audio interval accumulator requires a valid lineage interval");
    }
    if (!m_initialized) {
        m_generation = fragment.lineage->generation;
        m_sampleRate = fragment.interval.sampleRate;
        m_initialized = true;
    } else {
        if (fragment.lineage->generation != m_generation ||
            fragment.interval.sampleRate != m_sampleRate ||
            m_expectedNextBegin != fragment.interval.begin) {
            std::ostringstream message;
            message
                << "Audio interval accumulator requires contiguous same-generation intervals"
                << " expected_generation=" << m_generation
                << " actual_generation=" << fragment.lineage->generation
                << " expected_sample_rate=" << m_sampleRate
                << " actual_sample_rate=" << fragment.interval.sampleRate
                << " expected_begin=" << m_expectedNextBegin
                << " actual_begin=" << fragment.interval.begin
                << " actual_end=" << fragment.interval.end;
            return fail(message.str());
        }
    }
    const auto addedSamples = *fragmentSamples;
    if (addedSamples >
        std::numeric_limits<std::int64_t>::max() - m_queuedSamples) {
        return fail("Audio interval accumulator sample count overflow");
    }
    m_queuedSamples += addedSamples;
    m_expectedNextBegin = fragment.interval.end;
    m_fragments.push_back(std::move(fragment));
    return ::media::Status::success();
}

::media::Result<std::vector<MediaAudioIntervalFragment>>
MediaAudioIntervalAccumulator::take(int samples)
{
    if (m_terminalFailure || samples <= 0 || samples > m_queuedSamples) {
        m_terminalFailure = true;
        return ::media::Result<std::vector<MediaAudioIntervalFragment>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio interval accumulator cannot take the requested sample count"));
    }

    std::vector<MediaAudioIntervalFragment> result;
    std::int64_t remaining = samples;
    while (remaining > 0) {
        auto& front = m_fragments.front();
        const std::int64_t available = front.interval.end - front.interval.begin;
        const std::int64_t consumed = std::min(available, remaining);
        result.push_back(MediaAudioIntervalFragment{
            front.lineage,
            {front.interval.begin, front.interval.begin + consumed,
             front.interval.sampleRate}});
        front.interval.begin += consumed;
        remaining -= consumed;
        m_queuedSamples -= consumed;
        if (front.interval.begin == front.interval.end) m_fragments.pop_front();
    }
    return ::media::Result<std::vector<MediaAudioIntervalFragment>>::success(
        std::move(result));
}

::media::Status MediaAudioIntervalAccumulator::finish() const
{
    if (m_terminalFailure || m_queuedSamples != 0 || !m_fragments.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio interval accumulator finished with failure or residue"));
    }
    return ::media::Status::success();
}

::media::Status MediaAudioIntervalAccumulator::settleDroppedSamples(
    std::int64_t authorizedSamples)
{
    if (authorizedSamples < 0 || authorizedSamples != m_queuedSamples ||
        authorizedSamples > std::numeric_limits<int>::max()) {
        return fail(
            "Audio interval residue does not equal authorized dropped samples");
    }
    if (authorizedSamples > 0) {
        auto discarded = take(static_cast<int>(authorizedSamples));
        if (!discarded) return ::media::Status::failure(discarded.error());
    }
    return finish();
}

void MediaAudioIntervalAccumulator::reset() noexcept
{
    m_fragments.clear();
    m_generation = 0;
    m_sampleRate = 0;
    m_expectedNextBegin = 0;
    m_queuedSamples = 0;
    m_initialized = false;
    m_terminalFailure = false;
}

std::int64_t MediaAudioIntervalAccumulator::queuedSamples() const noexcept
{
    return m_queuedSamples;
}

std::size_t MediaAudioIntervalAccumulator::fragmentCount() const noexcept
{
    return m_fragments.size();
}

::media::Status MediaAudioIntervalAccumulator::observeLineageCapacity(
    MediaAudioLineageCapacity& capacity) const
{
    for (const auto& fragment : m_fragments) {
        if (auto status = capacity.observe(fragment.lineage); !status) {
            return status;
        }
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
