#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/output/MediaMpegTsWireDatagramMaterializer.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include <memory>
#include <cstdint>
#include <optional>
#include <variant>
#include <deque>

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
    ::media::Status tryCreateMaterializer(
        MediaGraphExecutionContext& context);
    void recordMaterialized(
        const MediaWireDatagramBatchCollection& batches,
        MediaRunningTime materializedAt) noexcept;
    void emitDiagnostics(const char* stage) noexcept;
    void resetState() noexcept;

    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_authority;
    MediaBufferRef m_protocolPlan;
    MediaBufferRef m_transportPlan;
    MediaBufferRef m_pendingProtocolBatch;
    std::deque<MediaBufferRef> m_pendingOutputs;
    std::optional<Materializer> m_materializer;
    std::int64_t m_maximumMaterializedAfterReleaseNanoseconds = 0;
    std::int64_t m_worstMaterializedAtNanoseconds = 0;
    std::int64_t m_worstCanonicalReleaseNanoseconds = 0;
    std::int64_t m_worstCanonicalDeadlineNanoseconds = 0;
    std::uint64_t m_worstGlobalSequence = 0;
    bool m_diagnosticsEmitted = false;
};

} // namespace media::ffmpeg::graph
