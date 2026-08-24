#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/output/MediaMpegTsWireDatagramMaterializer.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include <memory>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

class MediaMpegTsDatagramMaterializerNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<
        std::unique_ptr<MediaMpegTsDatagramMaterializerNode>>
    create(MediaNodeId nodeId,
           std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);
    static MediaNodeKind staticKind() noexcept;

    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Status commitReservedOutput(
        const MediaBufferRef& buffer) override;

private:
    using Materializer = std::variant<
        MediaMpegTsUdpWireDatagramMaterializer,
        MediaMpegTsRtpWireDatagramMaterializer>;

    MediaMpegTsDatagramMaterializerNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority) noexcept;
    ::media::Status validatePorts(
        MediaGraphExecutionContext& context) const;
    ::media::Status tryCreateMaterializer();
    void resetState() noexcept;

    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_authority;
    MediaBufferRef m_protocolPlan;
    MediaBufferRef m_transportPlan;
    MediaBufferRef m_pendingOutput;
    std::optional<Materializer> m_materializer;
};

} // namespace media::ffmpeg::graph
