#include "internal/graph/runtime/ffmpeg/MediaVideoFrameReadiness.h"
#include <utility>
namespace media::ffmpeg::graph {
::media::Status validateMediaVideoFrameReferenceContract(
    const AVFrame& frame, const MediaCudaFrameProducerFacts& producer)
{
    if (producer.maximumExtendedBufferReferences &&
        (frame.nb_extended_buf < 0 ||
         static_cast<std::size_t>(frame.nb_extended_buf) > *producer.maximumExtendedBufferReferences ||
         (frame.nb_extended_buf && !frame.extended_buf)))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video frame references exceed the admitted producer contract"));
    return ::media::Status::success();
}
MediaVideoProducerFailureScope::MediaVideoProducerFailureScope(std::optional<MediaCudaFrameProducerFacts> producer) noexcept
    : m_producer(std::move(producer)) {}
MediaVideoProducerFailureScope::MediaVideoProducerFailureScope(MediaVideoProducerFailureScope&& other) noexcept
    : m_producer(std::exchange(other.m_producer, std::nullopt)) {}
MediaVideoProducerFailureScope::~MediaVideoProducerFailureScope()
{ if (m_producer) completeMediaCudaProducerForRelease(*m_producer); }

MediaVideoFrameReadiness::MediaVideoFrameReadiness(Kind kind,
    std::shared_ptr<MediaVideoFrameReadinessOperation> operation,
    std::shared_ptr<MediaFfmpegPayloadCompletion> completion)
    : m_kind(kind), m_operation(std::move(operation)), m_completion(std::move(completion)) {}
MediaVideoFrameReadiness MediaVideoFrameReadiness::completed()
{ return {Kind::Completed, nullptr, nullptr}; }
std::shared_ptr<MediaPreparationMemoryResource> MediaVideoFrameReadiness::resource() const noexcept
{ return m_operation ? m_operation->resource() : nullptr; }
MediaVideoReadinessObservation MediaVideoFrameReadiness::observeCompletion() const noexcept
{
    using State = MediaVideoReadinessObservation::State;
    if (m_kind == Kind::Completed) return {State::Completed, "not_queried_completed"};
    if (!m_operation) return {State::Unavailable, "missing_owner"};
    return m_operation->observeCompletion();
}
::media::Status MediaVideoFrameReadiness::ensureCompleted() const
{
    if (m_kind == Kind::Completed) return ::media::Status::success();
    if (!m_completion) return ::media::Status::failure(::media::ErrorInfo::notInitialized("Frame readiness completion owner missing"));
    return m_completion->ensureCompleted();
}
::media::Status MediaVideoFrameReadiness::enqueueCudaWait(void* context, void* stream) const
{
    if (m_kind == Kind::Completed) return ::media::Status::success();
    if (!m_operation) return ::media::Status::failure(::media::ErrorInfo::notInitialized("Frame readiness operation owner missing"));
    return m_operation->enqueueCudaWait(context, stream);
}
} // namespace media::ffmpeg::graph
