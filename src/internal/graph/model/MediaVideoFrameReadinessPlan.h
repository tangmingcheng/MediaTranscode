#pragma once
#include <cstddef>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {
enum class MediaCudaFrameProducer { NativeSafeDecode, CuvidDecode, ScaleCuda, HardwareTransfer };
// Supplied by the admitted producer, never selected from a frame at consumption.
struct MediaCudaFrameProducerFacts final {
    MediaCudaFrameProducer kind;
    void* context;
    void* stream;
    std::optional<std::size_t> maximumExtendedBufferReferences;
    friend bool operator==(const MediaCudaFrameProducerFacts&, const MediaCudaFrameProducerFacts&) = default;
};

enum class MediaSynchronousFrameProducer { RkmppDecode, RgaFilter };
// The admitted API has returned a consumable frame. This says nothing about
// independence from a decoder pool: AVFrame/AVBuffer ownership remains required.
struct MediaSynchronousFrameProducerFacts final {
    MediaSynchronousFrameProducer kind;
    std::size_t maximumExtendedBufferReferences;
    friend bool operator==(const MediaSynchronousFrameProducerFacts&,
        const MediaSynchronousFrameProducerFacts&) = default;
};
using MediaVideoFrameProducerFacts = std::variant<MediaCudaFrameProducerFacts,
    MediaSynchronousFrameProducerFacts>;
enum class MediaVideoFrameProducerRole { Decoder, Filter };

inline bool mediaVideoFrameProducerMatchesRole(const MediaVideoFrameProducerFacts& producer,
    MediaVideoFrameProducerRole role)
{
    if (role != MediaVideoFrameProducerRole::Decoder && role != MediaVideoFrameProducerRole::Filter) return false;
    const bool decode = role == MediaVideoFrameProducerRole::Decoder;
    if (const auto* cuda = std::get_if<MediaCudaFrameProducerFacts>(&producer))
        return cuda->context && (decode
            ? (cuda->kind == MediaCudaFrameProducer::NativeSafeDecode || cuda->kind == MediaCudaFrameProducer::CuvidDecode)
            : cuda->kind == MediaCudaFrameProducer::ScaleCuda);
    const auto& sync = std::get<MediaSynchronousFrameProducerFacts>(producer);
    return sync.kind == (decode ? MediaSynchronousFrameProducer::RkmppDecode : MediaSynchronousFrameProducer::RgaFilter);
}

inline std::optional<std::size_t> mediaVideoFrameProducerMaximumExtendedReferences(
    const MediaVideoFrameProducerFacts& producer)
{
    return std::visit([](const auto& facts) -> std::optional<std::size_t> {
        return facts.maximumExtendedBufferReferences;
    }, producer);
}
} // namespace media::ffmpeg::graph
