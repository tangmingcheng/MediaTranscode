#pragma once

namespace media::ffmpeg::graph {

// Allocation failures are terminal; pressure is retryable only within capacity.
enum class MediaStorageAdmission { RequiredAllocation, Backpressure };

} // namespace media::ffmpeg::graph
