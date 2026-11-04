#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/model/MediaFfmpegPayloadOwnershipLayout.h"
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {
class MediaGraphPayloadCreditLease;
// Pure allocator capability query: creates no frame, owner, or device object.
::media::Result<MediaFfmpegPayloadOwnershipLayout> describeMediaFfmpegPayloadOwnershipLayout();

// Snapshot precedes consumer submission and output wrapping. It never retains
// the new consumer wrapper, including when input and output alias one buffer.
class MediaFfmpegPayloadReferences final {
public:
    class ConstructionKey final {
        friend class MediaFfmpegPayloadReferences;
        ConstructionKey() = default;
    };
    static ::media::Result<std::shared_ptr<const MediaFfmpegPayloadReferences>> capture(
        const AVFrame& frame, const std::shared_ptr<MediaPreparationMemoryResource>& resource);
    MediaFfmpegPayloadReferences(ConstructionKey, const std::shared_ptr<MediaPreparationMemoryResource>& resource);
private:
    // Pixel release callbacks may dereference the frames context. Release it last.
    ::media::ffmpeg::BufferRefPtr m_framesContext;
    std::vector<::media::ffmpeg::BufferRefPtr, MediaLineageAllocator<::media::ffmpeg::BufferRefPtr>> m_references;
};

// One recorded producer operation; state must not own any wrapped allocation.
// Native completion is attempted once, including on the last-owner path.
class MediaFfmpegPayloadCompletion final {
public:
    class ConstructionKey final {
        friend class MediaFfmpegPayloadCompletion;
        ConstructionKey() = default;
    };
    using Complete = int (*)(void*) noexcept;
    static ::media::Result<std::shared_ptr<MediaFfmpegPayloadCompletion>> create(
        const std::shared_ptr<MediaPreparationMemoryResource>& resource,
        std::shared_ptr<void> operation, Complete complete, int successCode);
    ::media::Status ensureCompleted();
    void completeForRelease() noexcept;
    ~MediaFfmpegPayloadCompletion();
    // Public for allocate_shared; callers use create to validate the operation.
    MediaFfmpegPayloadCompletion(ConstructionKey, std::shared_ptr<void> operation, Complete complete, int successCode);
private:
    std::mutex m_mutex;
    std::shared_ptr<void> m_operation;
    Complete m_complete;
    int m_successCode;
    std::optional<int> m_result;
};

struct MediaFfmpegPayloadOwnership final {
    std::shared_ptr<MediaGraphPayloadCreditLease> credit;
    std::shared_ptr<MediaFfmpegPayloadCompletion> completion;
    MediaLineageAllocationScope allocation;
};

// Bind before producer publication. Actual allocation refs retain ownership
// through codec-held refs, clone and takeFrame, independently of wrappers.
// Explicit completion reports errors; free callbacks only diagnose them.
// The producer retains original aliases and the completion owner across any
// attachment failure, and must complete the operation before releasing aliases.
// Driver completion errors retain their failure status; cleanup does not claim
// safe device completion after an unproven driver error.
::media::Status retainMediaFfmpegPayload(
    AVPacket& packet, const MediaFfmpegPayloadOwnership& ownership);
::media::Status retainMediaFfmpegPayload(
    AVFrame& frame, const MediaFfmpegPayloadOwnership& ownership);
} // namespace media::ffmpeg::graph
