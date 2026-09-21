#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"

#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/sync/MediaAudioSampleGrid.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Result<std::int64_t> addSamples(std::int64_t value, std::int64_t offset)
{
    if ((offset > 0 && value > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 && value < std::numeric_limits<std::int64_t>::min() - offset))
        return ::media::Result<std::int64_t>::failure(::media::ErrorInfo::invalidArgument(
            "Aggregate audio sample mapping is not representable"));
    return ::media::Result<std::int64_t>::success(value + offset);
}
}

::media::Result<std::int64_t> MediaAvContinuousAggregateNode::audioOffset(
    const MediaAudioPlaybackOrigin& source) const
{
    if (!m_audioOrigin || source.outputSampleRate != plan().audio.sampleRate() ||
        source.epochOutputSampleIndex < 0)
        return ::media::Result<std::int64_t>::failure(::media::ErrorInfo::invalidArgument(
            "Aggregate source audio origin does not match the normalized grid"));
    auto delta = source.masterRelease.checkedSubtract(m_audioOrigin->masterRelease);
    if (!delta) return ::media::Result<std::int64_t>::failure(delta.error());
    auto grid = MediaAudioSampleGrid::create(plan().audio.sampleRate());
    if (!grid) return ::media::Result<std::int64_t>::failure(grid.error());
    auto samples = grid.value().nearestSample(delta.value());
    if (!samples) return samples;
    auto relative = addSamples(samples.value(), -source.epochOutputSampleIndex);
    if (!relative) return relative;
    return addSamples(relative.value(), m_audioOrigin->epochOutputSampleIndex);
}

::media::Result<MediaBufferRef> MediaAvContinuousAggregateNode::buildAudio(
    MediaGraphExecutionContext& context)
{
    const auto* codec = static_cast<const FFmpegCodecContextBuffer*>(m_audioCodec.get())->context();
    const auto begin = m_nextAudioSample;
    const auto end = begin + plan().audio.codecFrameSamples();
    auto presentation = audioTime(begin);
    auto next = audioTime(end);
    if (!presentation || !next) return ::media::Result<MediaBufferRef>::failure(
        !presentation ? presentation.error() : next.error());
    auto duration = next.value().checkedSubtract(presentation.value());
    if (!duration) return ::media::Result<MediaBufferRef>::failure(duration.error());
    auto lineage = createMediaCanonicalOutputLineage(presentation.value(), std::nullopt,
        duration.value(), MediaDecodeOrderMode::PresentationOrderNoReorder,
        {plan().outputGroupKey.value(), MediaScheduledStream::Audio,
         MediaOutputAccessUnitSequence(static_cast<std::uint64_t>(
             (begin - plan().initialAudioSample) / plan().audio.codecFrameSamples()) + 1)},
        MediaTimeMappingConfidence::Locked, plan().initialGeneration, {});
    if (!lineage) return ::media::Result<MediaBufferRef>::failure(lineage.error());
    auto reservation = context.reservePayload(nodeId(), MediaStreamKind::Audio, MediaPayloadKind::Frame);
    if (!reservation) return ::media::Result<MediaBufferRef>::failure(reservation.error());
    ::media::ffmpeg::FramePtr frame(av_frame_alloc());
    if (!frame) return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
        "Aggregate audio frame allocation failed"));
    frame->sample_rate = codec->sample_rate;
    frame->format = codec->sample_fmt;
    frame->nb_samples = plan().audio.codecFrameSamples();
    frame->pts = begin;
    frame->duration = frame->nb_samples;
    frame->time_base = {1, codec->sample_rate};
    if (av_channel_layout_copy(&frame->ch_layout, &codec->ch_layout) < 0 ||
        av_frame_get_buffer(frame.get(), 0) < 0 ||
        av_samples_set_silence(frame->extended_data, 0, frame->nb_samples,
            frame->ch_layout.nb_channels, codec->sample_fmt) < 0)
        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
            "Aggregate audio allocation or silence initialization failed"));
    std::vector<MediaAudioIntervalFragment> fragments;
    std::vector<std::uint64_t> generations(m_sources.size());
    auto cursor = begin;
    auto append = [&](MediaCanonicalAudioContribution contribution) -> ::media::Status {
        if (fragments.size() >= plan().maximumAudioContributions)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Aggregate output contribution count exceeds its planner bound"));
        const auto interval = contribution.interval;
        fragments.push_back({lineage.value(), interval, std::move(contribution)});
        return ::media::Status::success();
    };
    auto silenceUntil = [&](std::int64_t stop) -> ::media::Status {
        if (stop <= cursor) return ::media::Status::success();
        auto status = append({MediaCanonicalAudioGeneratedSilence{}, {cursor, stop, codec->sample_rate}});
        if (status) cursor = stop;
        return status;
    };
    const auto sourceSnapshot = m_dependencies.sources[plan().audioSource].group->epochTransitionSnapshot();
    if (sourceSnapshot.outputPermitted && sourceSnapshot.playbackEpoch) {
        while (!m_audio.empty() && cursor < end) {
            const auto& candidate = m_audio.front();
            const auto& samples = *candidate->media();
            const auto candidateCount = *samples.interval().sampleCount();
            auto removeCandidate = [&] {
                m_audioCandidateSamples -= candidateCount;
                m_audio.pop_front();
            };
            if (candidate->audioOrigin().generation != sourceSnapshot.playbackEpoch->generation) {
                removeCandidate();
                continue;
            }
            auto offset = audioOffset(candidate->audioOrigin());
            if (!offset) return ::media::Result<MediaBufferRef>::failure(offset.error());
            auto mappedBegin = addSamples(samples.interval().begin, offset.value());
            auto mappedEnd = addSamples(samples.interval().end, offset.value());
            if (!mappedBegin || !mappedEnd) return ::media::Result<MediaBufferRef>::failure(
                !mappedBegin ? mappedBegin.error() : mappedEnd.error());
            auto endTime = audioTime(mappedEnd.value());
            if (!endTime) return ::media::Result<MediaBufferRef>::failure(endTime.error());
            auto masterEnd = presentationMaster(endTime.value());
            if (!masterEnd) return ::media::Result<MediaBufferRef>::failure(masterEnd.error());
            observeInputEnd(plan().audioSource, masterEnd.value());
            if (mappedEnd.value() <= cursor) { removeCandidate(); continue; }
            if (mappedBegin.value() >= end) break;
            const auto* input = FFmpegFrameView::frame(samples.media());
            if (!input || input->sample_rate != codec->sample_rate || input->format != codec->sample_fmt ||
                av_channel_layout_compare(&input->ch_layout, &codec->ch_layout) != 0 ||
                input->nb_samples != candidateCount)
                return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
                    "Aggregate source audio does not match its prepared output format"));
            if (auto status = silenceUntil(std::max(cursor, mappedBegin.value())); !status)
                return ::media::Result<MediaBufferRef>::failure(status.error());
            for (const auto& fragment : samples.fragments()) {
                auto partBegin = addSamples(fragment.interval.begin, offset.value());
                auto partEnd = addSamples(fragment.interval.end, offset.value());
                if (!partBegin || !partEnd) return ::media::Result<MediaBufferRef>::failure(
                    !partBegin ? partBegin.error() : partEnd.error());
                const auto left = std::max(cursor, partBegin.value());
                const auto right = std::min(end, partEnd.value());
                if (left >= right) continue;
                if (left != cursor) return ::media::Result<MediaBufferRef>::failure(
                    ::media::ErrorInfo::invalidArgument("Aggregate source audio contributions have a hole"));
                auto contribution = fragment.contribution;
                contribution.interval = {left, right, codec->sample_rate};
                auto* real = std::get_if<MediaCanonicalAudioRealSource>(&contribution.origin);
                if (!real) return ::media::Result<MediaBufferRef>::failure(
                    ::media::ErrorInfo::invalidArgument("Aggregate source input cannot claim generated output audio"));
                auto anchorBegin = addSamples(real->mappedInterval.begin, offset.value());
                auto anchorEnd = addSamples(real->mappedInterval.end, offset.value());
                if (!anchorBegin || !anchorEnd) return ::media::Result<MediaBufferRef>::failure(
                    !anchorBegin ? anchorBegin.error() : anchorEnd.error());
                real->mappedInterval = {anchorBegin.value(), anchorEnd.value(), codec->sample_rate};
                if (av_samples_copy(frame->extended_data, input->extended_data,
                    static_cast<int>(left - begin), static_cast<int>(left - mappedBegin.value()),
                    static_cast<int>(right - left), codec->ch_layout.nb_channels, codec->sample_fmt) < 0)
                    return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
                        "Aggregate source audio copy failed"));
                if (auto status = append(std::move(contribution)); !status)
                    return ::media::Result<MediaBufferRef>::failure(status.error());
                generations[plan().audioSource] = candidate->audioOrigin().generation;
                cursor = right;
            }
            if (mappedEnd.value() <= cursor) removeCandidate();
            else break;
        }
    }
    if (auto status = silenceUntil(end); !status) return ::media::Result<MediaBufferRef>::failure(status.error());
    auto footprint = MediaFramePayloadFootprint::logicalBytes(*frame, MediaStreamKind::Audio);
    if (!footprint) return ::media::Result<MediaBufferRef>::failure(footprint.error());
    if (auto status = reservation.value().shrinkToActual(footprint.value()); !status)
        return ::media::Result<MediaBufferRef>::failure(status.error());
    auto wrapped = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
    if (!wrapped) return wrapped;
    MediaTimeDescriptor time;
    time.timeBase = {1, codec->sample_rate};
    wrapped.value()->setTimeDescriptor(time);
    if (auto status = reservation.value().attachTo(*wrapped.value()); !status)
        return ::media::Result<MediaBufferRef>::failure(status.error());
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(std::move(wrapped).value(), std::move(fragments));
    if (!canonical) return canonical;
    auto output = MediaBoundCanonicalAudioBuffer::create(std::move(canonical).value(), *m_audioOrigin);
    if (!output) return output;
    m_pending = Pending{PendingKind::Audio, output.value(), std::move(generations)};
    if (auto status = validateMetadataBound(); !status) {
        m_pending.reset();
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    return output;
}

} // namespace media::ffmpeg::graph
