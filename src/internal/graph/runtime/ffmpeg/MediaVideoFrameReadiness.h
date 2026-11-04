#pragma once
#include "internal/graph/diagnostics/MediaVideoReadinessObservation.h"
#include "internal/graph/model/MediaVideoFrameReadinessPlan.h"
#include "internal/graph/runtime/ffmpeg/MediaFfmpegPayloadOwnership.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"

namespace media::ffmpeg::graph {
// Place after frame owners and before a producer call. Release only after the
// frame has an allocation-bound completion owner or the operation succeeded.
class MediaVideoProducerFailureScope final {
public:
    explicit MediaVideoProducerFailureScope(std::optional<MediaVideoFrameProducerFacts> producer) noexcept;
    MediaVideoProducerFailureScope(MediaVideoProducerFailureScope&& other) noexcept;
    MediaVideoProducerFailureScope(const MediaVideoProducerFailureScope&) = delete;
    MediaVideoProducerFailureScope& operator=(const MediaVideoProducerFailureScope&) = delete;
    ~MediaVideoProducerFailureScope();
    void release() noexcept { m_producer.reset(); }
private:
    std::optional<MediaVideoFrameProducerFacts> m_producer;
};
void completeMediaCudaProducerForRelease(const MediaCudaFrameProducerFacts& producer) noexcept;
class MediaVideoFrameReadinessOperation {
public:
    virtual ~MediaVideoFrameReadinessOperation() = default;
    virtual const std::shared_ptr<MediaPreparationMemoryResource>& resource() const noexcept = 0;
    virtual MediaVideoReadinessObservation observeCompletion() noexcept = 0;
    virtual ::media::Status enqueueCudaWait(void* context, void* stream) = 0;
};
class MediaVideoFrameReadiness final {
public:
    enum class Kind { Completed, CudaEvent };
    static MediaVideoFrameReadiness completed();
    std::shared_ptr<MediaPreparationMemoryResource> resource() const noexcept;
    Kind kind() const noexcept { return m_kind; }
    ::media::Status ensureCompleted() const;
    MediaVideoReadinessObservation observeCompletion() const noexcept;
    ::media::Status enqueueCudaWait(void* context, void* stream) const;
private:
    friend ::media::Result<MediaVideoFrameReadiness> captureMediaCudaFrameReadiness(
        AVFrame&, const std::shared_ptr<MediaPreparationMemoryResource>&, const MediaCudaFrameProducerFacts&, std::shared_ptr<const MediaFfmpegPayloadReferences>);
    MediaVideoFrameReadiness(Kind kind, std::shared_ptr<MediaVideoFrameReadinessOperation> operation,
        std::shared_ptr<MediaFfmpegPayloadCompletion> completion);
    Kind m_kind;
    std::shared_ptr<MediaVideoFrameReadinessOperation> m_operation;
    std::shared_ptr<MediaFfmpegPayloadCompletion> m_completion;
};
::media::Result<MediaCudaFrameProducerFacts> inspectMediaCudaFrameProducer(
    const AVFrame& frame, MediaCudaFrameProducer authority);

// Exact engine-owned shared operation request; excludes the driver event and
// completion/payload owners, which have separate allocation products.
::media::Result<MediaSharedAllocationGeometry> mediaCudaFrameReadinessAllocationGeometry();
::media::Status validateMediaVideoFrameReferenceContract(
    const AVFrame& frame, const MediaVideoFrameProducerFacts& producer);

::media::Result<MediaVideoFrameReadiness> captureMediaVideoFrameReadiness(
    AVFrame& frame, const std::shared_ptr<MediaPreparationMemoryResource>& resource,
    const MediaVideoFrameProducerFacts& producer,
    std::shared_ptr<const MediaFfmpegPayloadReferences> consumedInputs);

// The caller retains the unpublished frame and all pre-existing aliases until
// this call returns. Attachment failure drains the recorded producer operation.
::media::Result<MediaVideoFrameReadiness> captureMediaCudaFrameReadiness(
    AVFrame& frame, const std::shared_ptr<MediaPreparationMemoryResource>& resource,
    const MediaCudaFrameProducerFacts& producer,
    std::shared_ptr<const MediaFfmpegPayloadReferences> consumedInputs);
} // namespace media::ffmpeg::graph
