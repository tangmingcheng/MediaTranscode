#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/context/MediaRuntimeSegmentOutputBinding.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaChannelRegistry.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/runtime/threading/MediaGraphWorkerExitToken.h"
#include "internal/graph/runtime/context/MediaAvSyncGroupRegistry.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadReservation.h"
#include "media_transcode/Result.h"

#include <vector>
#include <memory>
#include <span>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaGraphPayloadCreditLedger;
class MediaInputActivity;

class MediaGraphExecutionContext final {
public:
    MediaGraphExecutionContext() = default;
    ~MediaGraphExecutionContext();

    MediaGraphExecutionContext(const MediaGraphExecutionContext&) = delete;
    MediaGraphExecutionContext& operator=(const MediaGraphExecutionContext&) = delete;
    MediaGraphExecutionContext(MediaGraphExecutionContext&&) = default;
    MediaGraphExecutionContext& operator=(MediaGraphExecutionContext&&) = default;

    ::media::Status compile(const MediaGraph& graph);
    ::media::Status compileSegment(
        std::shared_ptr<const MediaGraph> graph,
        std::span<const MediaNodeId> nodes,
        MediaGraphExecutionContext& session,
        std::span<const MediaRuntimeSegmentOutputBinding> upstreamInputs);
    ::media::Result<MediaRuntimeSegmentOutputBinding> exportOutput(MediaPortId port) const;
    void detachExecutionNodes(std::span<const MediaNodeId> nodes);
    void cancelUnstartedExecution() noexcept;
    void cancelSessionPayloadWaiters() noexcept;
    void reset();
    void rebindCompiledGraph(const MediaGraph& graph) noexcept;

    void setDiagnosticsEnabled(bool enabled) noexcept;
    bool diagnosticsEnabled() const noexcept;
    void setDiagnosticConfig(MediaGraphDiagnosticConfig config) noexcept;
    const MediaGraphDiagnosticConfig& diagnosticConfig() const noexcept;

    bool compiled() const noexcept;

    const MediaGraph* graph() const noexcept;
    MediaChannelRegistry& channels() noexcept;
    const MediaChannelRegistry& channels() const noexcept;

    const std::vector<MediaNodeId>& executionOrder() const noexcept;

    MediaChannel* findInputChannel(MediaNodeId nodeId, const std::string& portName);
    const MediaChannel* findInputChannel(MediaNodeId nodeId, const std::string& portName) const;

    MediaChannel* findOutputChannel(MediaNodeId nodeId, const std::string& portName);
    const MediaChannel* findOutputChannel(MediaNodeId nodeId, const std::string& portName) const;

    std::vector<MediaChannel*> inputChannels(MediaNodeId nodeId);
    std::vector<MediaChannel*> outputChannels(MediaNodeId nodeId);
    MediaNodeWakeup& nodeWakeup(MediaNodeId nodeId);
    std::shared_ptr<MediaNodeWakeup> sharedNodeWakeup(MediaNodeId nodeId);
    std::shared_ptr<MediaNodeWakeup> findNodeWakeup(MediaNodeId nodeId) const noexcept;
    std::shared_ptr<MediaGraphWorkerExitToken> nodeExitToken(MediaNodeId nodeId) const noexcept;
    void interruptNodeWakeups() noexcept;
    void shutdownAvSyncGroups() noexcept;
    ::media::Status registerAvSyncGroup(
        MediaAvSyncGroupKey key,
        MediaAvSyncPlan plan,
        std::shared_ptr<MediaMasterClock> clock,
        std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch,
        std::shared_ptr<MediaAvEpochTransitionService> transitionService);
    std::shared_ptr<MediaAvSyncGroupRuntime> findAvSyncGroup(
        const MediaAvSyncGroupKey& key) const noexcept;
    std::shared_ptr<MediaGraphPayloadCreditLedger> payloadCreditLedger()
        const noexcept;
    bool payloadCreditsRequired() const noexcept;
    std::shared_ptr<MediaInputActivity> inputActivity() const noexcept;
    ::media::Result<MediaGraphPayloadReservation> reservePayload(
        MediaNodeId producer,
        MediaStreamKind streamKind,
        MediaPayloadKind payloadKind) noexcept;
    ::media::Result<std::vector<MediaGraphPayloadReservation>>
    reservePayloadBatch(
        MediaNodeId producer,
        MediaStreamKind streamKind,
        MediaPayloadKind payloadKind,
        std::span<const std::uint64_t> actualBytes) noexcept;

private:
    ::media::Status buildChannels(const MediaGraph& graph);
    ::media::Status buildExecutionOrder(const MediaGraph& graph);

private:
    const MediaGraph* m_graph = nullptr;
    std::shared_ptr<const MediaGraph> m_graphOwner;
    bool m_ownsPayloadLedger = true;
    bool m_ownsGlobalDiagnostics = true;
    MediaChannelRegistry m_channels;
    std::vector<MediaNodeId> m_executionOrder;
    std::unordered_map<uint32_t, std::shared_ptr<MediaNodeWakeup>> m_nodeWakeups;
    std::unordered_map<uint32_t, std::shared_ptr<MediaGraphWorkerExitToken>> m_nodeExitTokens;
    MediaAvSyncGroupRegistry m_avSyncGroups;
    std::shared_ptr<MediaGraphPayloadCreditLedger> m_payloadCreditLedger;
    std::shared_ptr<MediaInputActivity> m_inputActivity;
    bool m_compiled = false;
    MediaGraphDiagnosticConfig m_diagnosticConfig;
};

} // namespace media::ffmpeg::graph
