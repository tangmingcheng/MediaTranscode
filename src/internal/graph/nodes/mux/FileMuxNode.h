#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/mux/MediaMuxSessionFactory.h"
#include "internal/graph/runtime/lifecycle/MediaInputTerminalTracker.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaAvGenerationPurgeTarget;
class MediaProtocolOutputGenerationState;

class FileMuxNode final : public FFmpegNodeRuntime {
public:
    explicit FileMuxNode(MediaNodeId nodeId);
    FileMuxNode(MediaNodeId nodeId, bool projectMpegTsGenerationTarget);
    FileMuxNode(MediaNodeId nodeId,
                std::unique_ptr<MediaMuxSessionFactory> sessionFactory);
    static MediaNodeKind staticKind() noexcept;
    static constexpr std::string_view generationPurgeIdentity() noexcept
    {
        return "project_mpegts_mux_generation_state";
    }
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;

    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;

private:
    enum class Phase : std::uint8_t {
        AcquiringBindings,
        Streaming
    };

    ::media::Status ensureSession(MediaGraphExecutionContext& context);
    struct BindingInputState final {
        std::string portName;
        bool satisfied = false;
    };

    ::media::Status bindInputTracking(MediaGraphExecutionContext& context);
    ::media::Status validateAcquiringBindingChannels(
        MediaGraphExecutionContext& context);
    bool isUnsatisfiedBindingChannel(const MediaChannel& channel) const noexcept;
    bool allBindingChannelsSatisfied() const noexcept;
    void observeClosedInputs(MediaGraphExecutionContext& context);
    ::media::Status handleBuffer(MediaGraphExecutionContext& context,
                                 const PoppedChannelBuffer& input);
    ::media::Result<MediaNodeProcessResult> finishIfReady(
        MediaGraphExecutionContext& context,
        const MediaBufferRef& terminalBuffer = {});
    ::media::Result<MediaNodeProcessResult> pollOrWait(
        MediaGraphExecutionContext& context);
    ::media::Status remember(::media::Status status);
    ::media::Result<MediaNodeProcessResult> terminalResult() const;
    ::media::Status forwardIfOutputsExist(MediaGraphExecutionContext& context,
                                          const MediaBufferRef& buffer);
    void releaseSession() noexcept;

    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::unique_ptr<MediaMuxSessionFactory> m_sessionFactory;
    std::unique_ptr<MediaMuxSession> m_session;
    std::unique_ptr<MediaInputTerminalTracker> m_completion;
    std::unordered_map<std::uint32_t, BindingInputState> m_bindingInputs;
    std::optional<::media::ErrorInfo> m_terminalFailure;
    Phase m_phase = Phase::AcquiringBindings;
    bool m_abortForwarded = false;
};

} // namespace media::ffmpeg::graph
