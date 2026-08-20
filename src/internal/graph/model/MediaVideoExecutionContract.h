#pragma once

#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaVideoLineagePropagation {
    Unknown,
    CodecCopyOpaque,
    SubmissionOrder
};

enum class MediaVideoFilterImplementation {
    Unknown,
    None,
    Generic,
    Rga
};

enum class MediaVideoEncoderAbortPolicy {
    Unknown,
    Immediate,
    DrainThenAbort
};

const char* mediaVideoLineagePropagationName(
    MediaVideoLineagePropagation value) noexcept;
const char* mediaVideoFilterImplementationName(
    MediaVideoFilterImplementation value) noexcept;
const char* mediaVideoEncoderAbortPolicyName(
    MediaVideoEncoderAbortPolicy value) noexcept;

bool parseMediaVideoFilterImplementation(
    std::string_view text,
    MediaVideoFilterImplementation& value) noexcept;
bool parseMediaVideoEncoderAbortPolicy(
    std::string_view text,
    MediaVideoEncoderAbortPolicy& value) noexcept;

} // namespace media::ffmpeg::graph
