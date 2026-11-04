#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/sync/MediaAvGenerationTransition.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <mutex>
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

inline constexpr std::string_view MediaRtpVideoIngressGenerationIdentity = "rtp_video_ingress";
inline constexpr std::string_view MediaRtpAudioIngressGenerationIdentity = "rtp_audio_ingress";
inline constexpr std::string_view MediaRtpVideoBinderGenerationIdentity = "rtp_video_binder";
inline constexpr std::string_view MediaRtpAudioBinderGenerationIdentity = "rtp_audio_binder";

struct MediaRtpIngressBinding final {
    MediaAvSyncGroupKey group;
    MediaStreamKind stream;
};

// Each stream has one durable owner-produced cutoff. A sequence <= cutoff
// belongs to the media retired by that exact source-domain transaction.
class MediaRtpIngressFrontiers final {
public:
    explicit MediaRtpIngressFrontiers(MediaTranscodeStreamSet members) noexcept;
    ::media::Status publish(MediaStreamKind stream,
                            const MediaAvGenerationPurge& purge,
                            std::uint64_t cutoff);
    ::media::Result<std::optional<std::uint64_t>> find(
        MediaStreamKind stream, const MediaAvGenerationPurge& purge) const;

private:
    struct Frontier final {
        MediaAvGenerationPurge purge;
        std::uint64_t cutoff;
    };
    bool contains(MediaStreamKind stream) const noexcept;
    const MediaTranscodeStreamSet m_members;
    mutable std::mutex m_mutex;
    std::optional<Frontier> m_video;
    std::optional<Frontier> m_audio;
};

} // namespace media::ffmpeg::graph
