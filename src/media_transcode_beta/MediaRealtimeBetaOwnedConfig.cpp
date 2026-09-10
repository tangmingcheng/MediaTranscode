#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"
#include "media_transcode_beta/MediaRealtimeBetaRequestMapper.h"

#include <utility>

namespace media::beta {

::media::Result<MediaRealtimeBetaOwnedConfig> MediaRealtimeBetaOwnedConfig::create(
    const mt_beta_realtime_config* config,
    const mt_beta_realtime_callbacks* callbacks,
    mt_beta_realtime_session** session)
{
    using Result = ::media::Result<MediaRealtimeBetaOwnedConfig>;
    if (!config || !callbacks || !session || !callbacks->on_event) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "config, callbacks, session output, and event callback are required"));
    }
    auto request = MediaRealtimeBetaRequestMapper::map(*config);
    if (!request) return Result::failure(request.error());
    MediaRealtimeBetaOwnedConfig owned;
    owned.m_request = std::move(request).value();
    owned.m_eventCallback = callbacks->on_event;
    owned.m_eventUserData = callbacks->user_data;
    return Result::success(std::move(owned));
}

const ffmpeg::graph::MediaRealtimeRtpTranscodeRequest&
MediaRealtimeBetaOwnedConfig::request() const noexcept { return m_request; }
mt_beta_realtime_event_callback MediaRealtimeBetaOwnedConfig::eventCallback() const noexcept
{ return m_eventCallback; }
void* MediaRealtimeBetaOwnedConfig::eventUserData() const noexcept
{ return m_eventUserData; }

} // namespace media::beta
