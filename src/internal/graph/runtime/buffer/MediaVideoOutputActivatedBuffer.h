#pragma once

#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"

namespace media::ffmpeg::graph {

class MediaVideoOutputActivatedBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaProtocolOutputSessionKey sessionKey,
        MediaProtocolOutputActivation activation);

    MediaBufferType type() const noexcept override;
    const MediaProtocolOutputSessionKey& sessionKey() const noexcept;
    const MediaProtocolOutputActivation& activation() const noexcept;

private:
    MediaVideoOutputActivatedBuffer(
        MediaProtocolOutputSessionKey sessionKey,
        MediaProtocolOutputActivation activation) noexcept;

    MediaProtocolOutputSessionKey m_sessionKey;
    MediaProtocolOutputActivation m_activation;
};

} // namespace media::ffmpeg::graph
