#include "internal/graph/sync/MediaRtpIngressFrontiers.h"

namespace media::ffmpeg::graph {
namespace {

bool valid(const MediaAvGenerationPurge& purge) noexcept
{
    return purge.publishedGeneration != 0 &&
        purge.oldGeneration >= purge.publishedGeneration &&
        purge.nextGeneration > purge.oldGeneration &&
        purge.transitionSequence != 0;
}

bool same(const MediaAvGenerationPurge& a, const MediaAvGenerationPurge& b) noexcept
{
    return a.oldGeneration == b.oldGeneration &&
        a.nextGeneration == b.nextGeneration &&
        a.transitionSequence == b.transitionSequence &&
        a.publishedGeneration == b.publishedGeneration;
}

} // namespace

MediaRtpIngressFrontiers::MediaRtpIngressFrontiers(
    MediaTranscodeStreamSet members) noexcept : m_members(members)
{
}

bool MediaRtpIngressFrontiers::contains(MediaStreamKind stream) const noexcept
{
    return (m_members == MediaTranscodeStreamSet::VideoOnly ||
            m_members == MediaTranscodeStreamSet::AudioVideo) &&
        (stream == MediaStreamKind::Video ||
         (stream == MediaStreamKind::Audio &&
          m_members == MediaTranscodeStreamSet::AudioVideo));
}

::media::Status MediaRtpIngressFrontiers::publish(
    MediaStreamKind stream, const MediaAvGenerationPurge& purge,
    std::uint64_t cutoff)
{
    std::lock_guard lock(m_mutex);
    if (!contains(stream) || !valid(purge))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP ingress frontier requires a planned stream and exact transition"));
    auto& slot = stream == MediaStreamKind::Video ? m_video : m_audio;
    if (slot && same(slot->purge, purge)) {
        return slot->cutoff == cutoff ? ::media::Status::success()
            : ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                  "RTP ingress frontier cannot change within a transition"));
    }
    if (slot && (purge.transitionSequence <= slot->purge.transitionSequence ||
                 purge.oldGeneration != slot->purge.nextGeneration ||
                 cutoff < slot->cutoff))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP ingress frontier requires contiguous generations and monotonic sequence"));
    slot = Frontier{purge, cutoff};
    return ::media::Status::success();
}

::media::Result<std::optional<std::uint64_t>> MediaRtpIngressFrontiers::find(
    MediaStreamKind stream, const MediaAvGenerationPurge& purge) const
{
    using Result = ::media::Result<std::optional<std::uint64_t>>;
    std::lock_guard lock(m_mutex);
    if (!contains(stream) || !valid(purge))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP ingress frontier lookup requires a planned stream and exact transition"));
    const auto& slot = stream == MediaStreamKind::Video ? m_video : m_audio;
    if (!slot) return Result::success(std::nullopt);
    if (same(slot->purge, purge)) return Result::success(slot->cutoff);
    if (purge.transitionSequence > slot->purge.transitionSequence &&
        purge.oldGeneration == slot->purge.nextGeneration)
        return Result::success(std::nullopt);
    return Result::failure(::media::ErrorInfo::invalidArgument(
        "RTP ingress frontier lookup rejects a conflicting or retired transition"));
}

} // namespace media::ffmpeg::graph
