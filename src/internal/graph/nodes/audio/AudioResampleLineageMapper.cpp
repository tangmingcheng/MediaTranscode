#include "internal/graph/nodes/audio/AudioResampleLineageMapper.h"

#include "internal/graph/nodes/audio/AudioResampleLineageState.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/lineage/MediaAudioLineageCapacity.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

AudioResampleLineageMapper::AudioResampleLineageMapper(
    std::shared_ptr<AudioResampleLineageState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Status AudioResampleLineageMapper::acceptInput(
    const MediaBoundCanonicalAudioBuffer& input,
    const AVFrame& frame,
    int outputSampleRate)
{
    if (!m_state || frame.nb_samples <= 0 || outputSampleRate <= 0 ||
        input.audioOrigin().outputSampleRate != outputSampleRate) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleLineageMapper requires planned rates and an exact frame"));
    }
    std::int64_t fragmentSamples = 0;
    for (const auto& fragment : input.media()->fragments()) {
        const auto samples = fragment.interval.end - fragment.interval.begin;
        if (samples <= 0 ||
            samples > std::numeric_limits<std::int64_t>::max() - fragmentSamples) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "AudioResampleLineageMapper fragment sample count overflow"));
        }
        fragmentSamples += samples;
    }
    if (fragmentSamples != frame.nb_samples) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleLineageMapper fragments do not match frame samples"));
    }
    if (m_state->activeOrigin &&
        *m_state->activeOrigin != input.audioOrigin()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "AudioResampleLineageMapper rejects changed playback origin"));
    }

    MediaAudioLineageCapacity leases(m_state->capacity());
    if (auto status = m_state->outputIntervals.observeLineageCapacity(leases);
        !status) {
        return status;
    }
    if (auto status = leases.observe(input.media()->fragments()); !status) {
        return status;
    }

    auto candidateProjection = m_state->sampleProjection;
    auto candidateIntervals = m_state->outputIntervals;
    auto candidateLastLineage = m_state->lastOutputLineage;
    for (const auto& fragment : input.media()->fragments()) {
        if (!candidateProjection) {
            auto projection = MediaAudioSampleProjection::create(
                input.audioOrigin().epochOutputSampleIndex,
                fragment.interval.sampleRate,
                outputSampleRate);
            if (!projection) return ::media::Status::failure(projection.error());
            candidateProjection = std::move(projection).value();
        } else if (candidateProjection->sourceSampleRate() !=
                       fragment.interval.sampleRate ||
                   candidateProjection->outputSampleRate() != outputSampleRate) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "AudioResampleLineageMapper rejects a changed source sample rate"));
        }
        auto projected = candidateProjection->append(
            fragment.interval.end - fragment.interval.begin);
        if (!projected) return ::media::Status::failure(projected.error());
        if (auto status = candidateIntervals.push(
                {fragment.lineage, projected.value()}); !status) {
            return status;
        }
        candidateLastLineage = fragment.lineage;
    }

    if (auto status = m_state->observe(input.audioOrigin().generation); !status) {
        return status;
    }
    if (!m_state->activeOrigin) {
        m_state->activeOrigin = input.audioOrigin();
        m_state->outputSampleIndex =
            input.audioOrigin().epochOutputSampleIndex;
        m_state->nextOutputPts =
            input.audioOrigin().epochOutputSampleIndex;
    }
    m_state->sampleProjection = std::move(candidateProjection);
    m_state->outputIntervals = std::move(candidateIntervals);
    m_state->lastOutputLineage = std::move(candidateLastLineage);
    return ::media::Status::success();
}

::media::Result<MediaBufferRef> AudioResampleLineageMapper::bindOutput(
    MediaBufferRef output,
    std::int64_t outputSamples)
{
    if (!m_state || !m_state->activeOrigin || !m_state->lastOutputLineage ||
        outputSamples <= 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized(
                "AudioResampleLineageMapper output requires active lineage"));
    }
    auto candidateProjection = m_state->sampleProjection;
    auto candidateIntervals = m_state->outputIntervals;
    const auto missing = outputSamples - candidateIntervals.queuedSamples();
    if (missing > 0) {
        if (!candidateProjection) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::notInitialized(
                    "AudioResampleLineageMapper correction requires a projection"));
        }
        auto extension = candidateProjection->extend(missing);
        if (!extension) {
            return ::media::Result<MediaBufferRef>::failure(extension.error());
        }
        MediaAudioLineageCapacity leases(m_state->capacity());
        if (auto status = candidateIntervals.observeLineageCapacity(leases);
            !status) {
            return ::media::Result<MediaBufferRef>::failure(status.error());
        }
        if (auto status = leases.observe(m_state->lastOutputLineage); !status) {
            return ::media::Result<MediaBufferRef>::failure(status.error());
        }
        if (auto status = candidateIntervals.push(
                {m_state->lastOutputLineage, extension.value()}); !status) {
            return ::media::Result<MediaBufferRef>::failure(status.error());
        }
    }
    auto fragments = candidateIntervals.take(outputSamples);
    if (!fragments) {
        return ::media::Result<MediaBufferRef>::failure(fragments.error());
    }
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        output, std::move(fragments).value());
    if (!canonical) {
        return ::media::Result<MediaBufferRef>::failure(canonical.error());
    }
    auto bound = MediaBoundCanonicalAudioBuffer::create(
        std::move(canonical).value(), *m_state->activeOrigin);
    if (!bound) {
        return ::media::Result<MediaBufferRef>::failure(bound.error());
    }
    m_state->sampleProjection = std::move(candidateProjection);
    m_state->outputIntervals = std::move(candidateIntervals);
    return bound;
}

::media::Status AudioResampleLineageMapper::settleDroppedSamples(
    std::int64_t authorizedDroppedSamples)
{
    if (!m_state) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "AudioResampleLineageMapper requires planned state"));
    }
    auto candidateIntervals = m_state->outputIntervals;
    if (auto status = candidateIntervals.settleDroppedSamples(
            authorizedDroppedSamples); !status) {
        return status;
    }
    m_state->outputIntervals = std::move(candidateIntervals);
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
