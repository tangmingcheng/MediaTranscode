#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/nodes/MediaNodeRuntime.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "media_transcode/Result.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace media::ffmpeg::graph {

enum class RouteMatchPolicy {
    RequireMatch,
    AllowDrop
};

class FFmpegNodeRuntime : public MediaNodeRuntime {
public:
    FFmpegNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name);
    ~FFmpegNodeRuntime() override = default;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Result<MediaNodeProcessResult> process(MediaGraphExecutionContext& context) override;

protected:
    bool canFinishProcess() const noexcept override;
    ::media::Result<MediaNodeProcessResult> processProgress(::media::Status status = ::media::Status::success());
    ::media::Result<MediaNodeProcessResult> processFinished(::media::Status status = ::media::Status::success());
    std::size_t pendingOutputBufferCount() const noexcept;
    bool retainsPendingOutput(const MediaBufferRef& buffer) const noexcept;
    void cancelPendingOutputTransfer() noexcept;
    virtual bool pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept;
    virtual ::media::Result<
        std::optional<MediaProtocolOutputGenerationCommitReservation>>
    reserveOutputCommit(const MediaBufferRef& buffer) const;
    virtual ::media::Status commitReservedOutput(
        const MediaBufferRef& buffer);
    virtual ::media::Status cancelReservedOutput(
        const MediaBufferRef& buffer);
    struct PoppedChannelBuffer {
        MediaChannel* channel = nullptr;
        MediaBufferRef buffer;
    };
    const MediaNodeOptions* nodeOptions(MediaGraphExecutionContext& context) const noexcept;
    std::string nodeOption(MediaGraphExecutionContext& context,
                           const std::string& key,
                           std::string missingValue = {}) const;

    ::media::Result<MediaBufferRef> popInput(MediaGraphExecutionContext& context,
                                             const std::string& portName);
    ::media::Result<MediaBufferRef> tryPopFirstInput(MediaGraphExecutionContext& context);
    ::media::Result<std::optional<MediaBufferRef>> tryPopFirstInputOptional(MediaGraphExecutionContext& context);
    ::media::Result<std::optional<PoppedChannelBuffer>> tryPopFirstInputWithChannelOptional(MediaGraphExecutionContext& context);
    ::media::Result<std::optional<PoppedChannelBuffer>> tryPopFirstInputWithChannelOptional(
        MediaGraphExecutionContext& context,
        std::span<const std::string_view> eligiblePortNames);
    ::media::Result<std::optional<MediaBufferRef>> tryPopInputOptional(MediaGraphExecutionContext& context,
                                                                        const std::string& portName);

    ::media::Status emitOutput(MediaGraphExecutionContext& context,
                               const std::string& portName,
                               const MediaBufferRef& buffer);
    ::media::Status pushToAllOutputs(MediaGraphExecutionContext& context,
                                      const MediaBufferRef& buffer);
    ::media::Status broadcastControlToAllOutputs(MediaGraphExecutionContext& context,
                                                  const MediaBufferRef& buffer);
    ::media::Status pushToMatchingOutputs(MediaGraphExecutionContext& context,
                                           const MediaBufferRef& buffer,
                                           MediaStreamKind streamKind,
                                           int streamIndex = invalidMediaStreamIndex,
                                           RouteMatchPolicy policy = RouteMatchPolicy::RequireMatch);

    std::vector<MediaChannel*> outputChannels(MediaGraphExecutionContext& context);

private:
    enum class AtomicTransferResult {
        NotApplicable,
        Waiting,
        Published
    };
    struct PendingTransfer {
        MediaBufferRef buffer;
        std::vector<MediaChannel*> channels;
        std::size_t nextChannel = 0;
        bool atomic = false;
    };
    ::media::Result<AtomicTransferResult> publishAtomicOutput(
        MediaGraphExecutionContext& context,
        const std::vector<MediaChannel*>& channels,
        const MediaBufferRef& buffer,
        const char* action);
    ::media::Status transferOrDefer(MediaGraphExecutionContext& context,
                                    const std::vector<MediaChannel*>& channels,
                                    const MediaBufferRef& buffer,
                                    const char* action);
    ::media::Status drainPendingTransfers(MediaGraphExecutionContext& context, bool& waiting);
    ::media::Result<MediaNodeProcessResult> processImpl(
        MediaGraphExecutionContext& context);
    std::size_t m_nextInputIndex = 0;
    std::optional<PendingTransfer> m_pendingTransfer;
    bool m_finishPending = false;
    bool m_finished = false;
};

#define MEDIA_FFMPEG_GRAPH_DECLARE_FFMPEG_NODE(ClassName) \
class ClassName final : public FFmpegNodeRuntime { \
public: \
    explicit ClassName(MediaNodeId nodeId); \
    static MediaNodeKind staticKind() noexcept; \
protected: \
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override; \
};

#define MEDIA_FFMPEG_GRAPH_DEFINE_FFMPEG_NODE(ClassName, KindValue) \
ClassName::ClassName(MediaNodeId nodeId) \
    : FFmpegNodeRuntime(nodeId, staticKind(), #ClassName) \
{ \
} \
MediaNodeKind ClassName::staticKind() noexcept \
{ \
    return KindValue; \
} \
::media::Result<MediaNodeProcessResult> ClassName::onProcess(MediaGraphExecutionContext&) \
{ \
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished()); \
}

} // namespace media::ffmpeg::graph
