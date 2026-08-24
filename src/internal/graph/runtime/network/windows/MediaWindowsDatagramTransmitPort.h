#pragma once

#include "internal/graph/runtime/network/MediaDatagramTransmitPort.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"

namespace media::ffmpeg::graph {

class MediaWindowsDatagramTransmitPortFactory final
    : public MediaDatagramTransmitPortFactory {
public:
    explicit MediaWindowsDatagramTransmitPortFactory(
        std::shared_ptr<MediaSocketRuntime> runtime) noexcept;

    ::media::Result<std::unique_ptr<MediaDatagramTransmitPort>> create() override;

private:
    std::shared_ptr<MediaSocketRuntime> m_runtime;
};

} // namespace media::ffmpeg::graph
