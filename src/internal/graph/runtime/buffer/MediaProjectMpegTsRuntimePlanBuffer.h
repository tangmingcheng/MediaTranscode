#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "media_transcode/Result.h"
#include "internal/graph/planner/realtime/MediaTsPrefixServicePlan.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaWireGlobalSequenceState;
class MediaDatagramTransportPlanBuffer;

class MediaProjectMpegTsRuntimePlanBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>
            outputPlan,
        MediaProtocolOutputSessionKey sessionKey,
        MediaTranscodeStreamSet streamSet,
        MediaProtocolOutputActivation activation,
        const MediaDatagramTransportPlanBuffer& transportPlan);

    MediaBufferType type() const noexcept override;
    const std::shared_ptr<MediaWireGlobalSequenceState>& transport() const noexcept
    { return m_transport; }
    const MediaTsPrefixServicePlan& prefixService() const noexcept { return m_prefixService; }
    const MediaTsMuxPlan& muxPlan() const noexcept;
    const MediaProjectMpegTsRuntimeOutputPlan& outputPlan() const noexcept;
    const std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>&
    sharedOutputPlan() const noexcept;
    const MediaProtocolOutputSessionKey& sessionKey() const noexcept;
    MediaTranscodeStreamSet streamSet() const noexcept;
    const MediaProtocolOutputActivation& activation() const noexcept;

private:
    MediaProjectMpegTsRuntimePlanBuffer(
        std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>
            outputPlan,
        MediaProtocolOutputSessionKey sessionKey,
        MediaTranscodeStreamSet streamSet,
        MediaProtocolOutputActivation activation,
        std::shared_ptr<MediaWireGlobalSequenceState> transport,
        MediaTsPrefixServicePlan prefixService);

    const std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>
        m_outputPlan;
    const MediaProtocolOutputSessionKey m_sessionKey;
    const MediaTranscodeStreamSet m_streamSet;
    const MediaProtocolOutputActivation m_activation;
    const std::shared_ptr<MediaWireGlobalSequenceState> m_transport;
    const MediaTsPrefixServicePlan m_prefixService;
};

} // namespace media::ffmpeg::graph
