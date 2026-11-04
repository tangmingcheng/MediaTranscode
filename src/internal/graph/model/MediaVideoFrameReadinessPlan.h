#pragma once
#include <cstddef>
#include <optional>

namespace media::ffmpeg::graph {
enum class MediaCudaFrameProducer { NativeSafeDecode, CuvidDecode, ScaleCuda, HardwareTransfer };
// Supplied by the admitted producer, never selected from a frame at consumption.
struct MediaCudaFrameProducerFacts final {
    MediaCudaFrameProducer kind;
    void* context;
    void* stream;
    std::optional<std::size_t> maximumExtendedBufferReferences;
};
} // namespace media::ffmpeg::graph
