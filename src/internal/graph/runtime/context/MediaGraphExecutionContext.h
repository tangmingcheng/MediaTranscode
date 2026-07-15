#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaChannelRegistry.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/runtime/context/MediaAvSyncGroupRegistry.h"
#include "media_transcode/Result.h"

#include <vector>
#include <memory>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaGraphExecutionContext final {
public:
    MediaGraphExecutionContext() = default;
    ~MediaGraphExecutionContext();

    MediaGraphExecutionContext(const MediaGraphExecutionContext&) = delete;
    MediaGraphExecutionContext& operator=(const MediaGraphExecutionContext&) = delete;
    MediaGraphExecutionContext(MediaGraphExecutionContext&&) = default;
    MediaGraphExecutionContext& operator=(MediaGraphExecutionContext&&) = default;

    ::media::Status compile(const MediaGraph& graph);
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

private:
    ::media::Status buildChannels(const MediaGraph& graph);
    ::media::Status buildExecutionOrder(const MediaGraph& graph);

private:
    const MediaGraph* m_graph = nullptr;
    MediaChannelRegistry m_channels;
    std::vector<MediaNodeId> m_executionOrder;
    std::unordered_map<uint32_t, std::unique_ptr<MediaNodeWakeup>> m_nodeWakeups;
    MediaAvSyncGroupRegistry m_avSyncGroups;
    bool m_compiled = false;
    MediaGraphDiagnosticConfig m_diagnosticConfig;
};

} // namespace media::ffmpeg::graph
