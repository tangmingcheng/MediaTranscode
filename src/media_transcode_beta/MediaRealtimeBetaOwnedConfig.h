#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"
#include "media_transcode_beta/realtime.h"

namespace media::beta {

class MediaRealtimeBetaOwnedConfig final {
public:
    static ::media::Result<MediaRealtimeBetaOwnedConfig> create(
        const mt_beta_realtime_config* config,
        const mt_beta_realtime_callbacks* callbacks,
        mt_beta_realtime_session** session);

    const ffmpeg::graph::MediaRealtimeRtpTranscodeRequest& request() const noexcept;
    mt_beta_realtime_event_callback eventCallback() const noexcept;
    void* eventUserData() const noexcept;

private:
    ffmpeg::graph::MediaRealtimeRtpTranscodeRequest m_request;
    mt_beta_realtime_event_callback m_eventCallback = nullptr;
    void* m_eventUserData = nullptr;
};

} // namespace media::beta
