#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaOwnerThreadGenerationPurge.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationData.h"
#include "internal/graph/nodes/output/MediaMpegTsWireDatagramMaterializer.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

#include <memory>
#include <cstdint>
#include <optional>
#include <variant>
#include <deque>

namespace media::ffmpeg::graph {

struct MediaMpegTsMaterializerGenerationData final {
    using Materializer = std::variant<MediaMpegTsUdpWireDatagramMaterializer, MediaMpegTsRtpWireDatagramMaterializer>;
    MediaBufferRef protocolPlan;
    MediaBufferRef transportPlan;
    MediaBufferRef terminal;
    bool terminalReportPrepared = false;
    MediaBufferRef pendingProtocolBatch;
    std::deque<MediaBufferRef> pendingOutputs;
    std::optional<Materializer> materializer;
};

using MediaMpegTsMaterializerGenerationSession = MediaProtocolOutputGenerationData<MediaMpegTsMaterializerGenerationData>;

class MediaMpegTsDatagramMaterializerNode final : public FFmpegNodeRuntime {
public:
    static ::media::Result<
        std::unique_ptr<MediaMpegTsDatagramMaterializerNode>>
    create(MediaNodeId nodeId,
           std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);
    static MediaNodeKind staticKind() noexcept;
    std::shared_ptr<MediaAvGenerationPurgeTarget> generationPurgeTarget() const noexcept { return m_generationPurge; }
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;

    static constexpr std::string_view generationPurgeIdentity() noexcept { return "mpegts_materializer_generation_state"; }
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaOutputCommitReservation> reserveOutputCommit(const MediaBufferRef& buffer) const override;
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Status commitReservedOutput(
        const MediaBufferRef& buffer) override;

private:
    MediaMpegTsDatagramMaterializerNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);
    ::media::Status validatePorts(
        MediaGraphExecutionContext& context) const;
    ::media::Status tryCreateMaterializer(
        MediaGraphExecutionContext& context);
    void recordMaterialized(
        const MediaWireDatagramBatchCollection& batches,
        MediaRunningTime materializedAt) noexcept;
    void emitDiagnostics(const char* stage) noexcept;
    ::media::Result<MediaNodeProcessResult> finishProtocol(
        MediaGraphExecutionContext& context);
    void resetState() noexcept;
    std::shared_ptr<MediaMpegTsMaterializerGenerationSession> m_generationSession = std::make_shared<MediaMpegTsMaterializerGenerationSession>();
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::shared_ptr<MediaOwnerThreadGenerationPurge> m_generationPurge = std::make_shared<MediaOwnerThreadGenerationPurge>();
    std::optional<MediaAvGenerationPurge> m_completedPurge;

    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_authority;
    std::int64_t m_maximumMaterializedAfterReleaseNanoseconds = 0;
    std::int64_t m_worstMaterializedAtNanoseconds = 0;
    std::int64_t m_worstCanonicalReleaseNanoseconds = 0;
    std::int64_t m_worstCanonicalDeadlineNanoseconds = 0;
    std::uint64_t m_worstGlobalSequence = 0;
    bool m_diagnosticsEmitted = false;
};

} // namespace media::ffmpeg::graph
