#include "internal/graph/nodes/sync/MediaContinuousAudioAggregateNode.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {
struct Envelope final {
    std::uint64_t count = 0;
    std::uint64_t sum = 0;
    std::optional<std::int64_t> begin;
    std::optional<std::int64_t> end;
    bool incomplete = false;

    void observe(std::int64_t left, std::int64_t right) noexcept
    {
        if (right < left) { incomplete = true; return; }
        const auto size = static_cast<std::uint64_t>(right) - static_cast<std::uint64_t>(left);
        if (sum > std::numeric_limits<std::uint64_t>::max() - size ||
            count == std::numeric_limits<std::uint64_t>::max()) {
            incomplete = true;
            return;
        }
        ++count;
        sum += size;
        begin = begin ? std::min(*begin, left) : left;
        end = end ? std::max(*end, right) : right;
    }
    void write(std::ostringstream& out, const char* name) const
    {
        out << ' ' << name << "_count=" << count << ' ' << name << "_sample_sum=" << sum
            << ' ' << name << "_incomplete=" << incomplete;
        if (begin) out << ' ' << name << "_envelope_begin=" << *begin
            << ' ' << name << "_envelope_end=" << *end;
        else out << ' ' << name << "_envelope=unavailable";
    }
};
bool shifted(std::int64_t value, std::int64_t offset, std::int64_t& result) noexcept
{
    if ((offset > 0 && value > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 && value < std::numeric_limits<std::int64_t>::min() - offset)) return false;
    result = value + offset;
    return true;
}
}

void MediaContinuousAudioAggregateNode::logSourcePurge(const MediaAvGenerationPurge& request) const noexcept
{
    if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
    try {
        Envelope original, mapped, pending;
        for (const auto& candidate : m_audio) {
            if (candidate->audioOrigin().generation != request.oldGeneration) continue;
            const auto interval = candidate->media()->interval();
            original.observe(interval.begin, interval.end);
            const auto offset = audioOffset(candidate->audioOrigin());
            std::int64_t left = 0, right = 0;
            if (!offset || !shifted(interval.begin, offset.value(), left) ||
                !shifted(interval.end, offset.value(), right)) { mapped.incomplete = true; continue; }
            mapped.observe(left, right);
        }
        const bool pendingCancelled = m_pending &&
            m_pending->sourceGenerations[plan().audioSource] != 0 &&
            m_pending->sourceGenerations[plan().audioSource] <= request.oldGeneration;
        if (pendingCancelled && m_pending->kind == PendingKind::Audio) {
            const auto* audio = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(m_pending->buffer.get());
            if (!audio) pending.incomplete = true;
            else for (const auto& fragment : audio->media()->fragments()) {
                if (std::holds_alternative<MediaCanonicalAudioRealSource>(fragment.contribution.origin))
                    pending.observe(fragment.interval.begin, fragment.interval.end);
            }
        }
        std::ostringstream out;
        out << "continuous_audio_source_purge stage=before_clear node=" << nodeId().value
            << " source=" << plan().sources[plan().audioSource].groupKey.value()
            << " old_generation=" << request.oldGeneration << " next_generation=" << request.nextGeneration
            << " transition=" << request.transitionSequence << " published_generation=" << request.publishedGeneration
            << " owner_candidates=" << m_audio.size() << " owner_candidate_samples=" << m_audioCandidateSamples
            << " output_sample_rate=" << plan().audio.sampleRate() << " next_output_sample=" << m_nextAudioSample
            << " committed_real_samples=" << m_committedRealSamples << " pending_cancelled=" << pendingCancelled
            << " interval_semantics=envelope_not_continuity pending_may_overlap_candidates=1";
        original.write(out, "old_original");
        mapped.write(out, "old_mapped");
        pending.write(out, "pending_real_output");
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) { /* Diagnostic failure must not alter source purge. */ }
}
} // namespace media::ffmpeg::graph
