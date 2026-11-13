#pragma once
namespace media::ffmpeg::graph {
// Protocol ownership established by preparation, never inferred downstream.
enum class MediaRtpControlDelivery { RawProtocolOwner, SharedPreparedSession };
} // namespace media::ffmpeg::graph
