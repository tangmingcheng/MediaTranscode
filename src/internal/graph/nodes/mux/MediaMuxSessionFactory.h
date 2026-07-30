#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/nodes/mux/MediaMuxSession.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaProtocolOutputGenerationState;
class MediaUdpDatagramSenderPortFactory;

class MediaMuxSessionFactory {
public:
    virtual ~MediaMuxSessionFactory() = default;

    virtual ::media::Result<std::unique_ptr<MediaMuxSession>> create(
        const MediaNodeOptions& options) const = 0;
};

class ExplicitMediaMuxSessionFactory final : public MediaMuxSessionFactory {
public:
    ExplicitMediaMuxSessionFactory() = default;
    ExplicitMediaMuxSessionFactory(
        std::shared_ptr<MediaProtocolOutputGenerationState> generationState,
        std::shared_ptr<MediaUdpDatagramSenderPortFactory>
            datagramPortFactory);

    ::media::Result<std::unique_ptr<MediaMuxSession>> create(
        const MediaNodeOptions& options) const override;

private:
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::shared_ptr<MediaUdpDatagramSenderPortFactory>
        m_datagramPortFactory;
};

} // namespace media::ffmpeg::graph
