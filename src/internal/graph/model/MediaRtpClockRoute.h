#pragma once
#include <array>
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"

namespace media::ffmpeg::graph {
enum class MediaRtpClockRouteRole { VideoInput, AudioInput, Group, Fanout, VideoBinder, AudioBinder, StateAdapter };
struct MediaRtpClockRouteShape final {
    MediaTranscodeStreamSet members;
    bool has(MediaRtpClockRouteRole role) const noexcept {
        return (role != MediaRtpClockRouteRole::AudioInput && role != MediaRtpClockRouteRole::AudioBinder) ||
            members == MediaTranscodeStreamSet::AudioVideo;
    }
    static MediaNodeKind nodeKind(MediaRtpClockRouteRole role) noexcept {
        using R = MediaRtpClockRouteRole;
        switch (role) {
        case R::VideoInput: case R::AudioInput: return MediaNodeKind::RawRtpInput;
        case R::Group: return MediaNodeKind::RtpClockGroup;
        case R::Fanout: return MediaNodeKind::RtpClockSnapshotFanout;
        case R::VideoBinder: case R::AudioBinder: return MediaNodeKind::RtpPacketClockBinder;
        case R::StateAdapter: return MediaNodeKind::RtpSourceClockStateAdapter;
        }
        return MediaNodeKind::Unknown;
    }
};
enum class MediaRtpClockRoutePayload { Evidence, Snapshot };
struct MediaRtpClockRouteConnection final {
    MediaRtpClockRouteRole from;
    const char* output;
    MediaRtpClockRouteRole to;
    const char* input;
    const char* label;
    MediaRtpClockRoutePayload payload;
    bool audioOnly;
};
// The builder and storage planner consume this same route. Scalar state
// outputs are deliberately absent: they do not retain owning CNAME bytes.
inline constexpr std::array MediaRtpClockRouteConnections{
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::VideoInput, "clock", MediaRtpClockRouteRole::Group, "video_clock", "video RTP clock -> RTP clock group", MediaRtpClockRoutePayload::Evidence, false},
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::VideoInput, "event", MediaRtpClockRouteRole::Group, "video_event", "video RTP event -> RTP clock group", MediaRtpClockRoutePayload::Evidence, false},
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::AudioInput, "clock", MediaRtpClockRouteRole::Group, "audio_clock", "audio RTP clock -> RTP clock group", MediaRtpClockRoutePayload::Evidence, true},
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::AudioInput, "event", MediaRtpClockRouteRole::Group, "audio_event", "audio RTP event -> RTP clock group", MediaRtpClockRoutePayload::Evidence, true},
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::Group, "clock_group", MediaRtpClockRouteRole::Fanout, "clock", "RTP group clock -> snapshot fanout", MediaRtpClockRoutePayload::Snapshot, false},
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::Fanout, "video", MediaRtpClockRouteRole::VideoBinder, "clock", "RTP snapshot -> video binder", MediaRtpClockRoutePayload::Snapshot, false},
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::Fanout, "audio", MediaRtpClockRouteRole::AudioBinder, "clock", "RTP snapshot -> audio binder", MediaRtpClockRoutePayload::Snapshot, true},
    MediaRtpClockRouteConnection{MediaRtpClockRouteRole::Fanout, "startup", MediaRtpClockRouteRole::StateAdapter, "clock", "RTP snapshot -> source clock adapter", MediaRtpClockRoutePayload::Snapshot, false}
};
} // namespace media::ffmpeg::graph
