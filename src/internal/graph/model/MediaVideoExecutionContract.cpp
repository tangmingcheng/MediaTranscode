#include "internal/graph/model/MediaVideoExecutionContract.h"

namespace media::ffmpeg::graph {

const char* mediaVideoLineagePropagationName(
    MediaVideoLineagePropagation value) noexcept
{
    switch (value) {
    case MediaVideoLineagePropagation::Unknown: return "unknown";
    case MediaVideoLineagePropagation::CodecCopyOpaque: return "codec_copy_opaque";
    case MediaVideoLineagePropagation::SubmissionOrder: return "submission_order";
    }
    return "unknown";
}

const char* mediaVideoFilterImplementationName(
    MediaVideoFilterImplementation value) noexcept
{
    switch (value) {
    case MediaVideoFilterImplementation::Unknown: return "unknown";
    case MediaVideoFilterImplementation::None: return "none";
    case MediaVideoFilterImplementation::Generic: return "generic";
    case MediaVideoFilterImplementation::Rga: return "rga";
    }
    return "unknown";
}

const char* mediaVideoEncoderAbortPolicyName(
    MediaVideoEncoderAbortPolicy value) noexcept
{
    switch (value) {
    case MediaVideoEncoderAbortPolicy::Unknown: return "unknown";
    case MediaVideoEncoderAbortPolicy::Immediate: return "immediate";
    case MediaVideoEncoderAbortPolicy::DrainThenAbort: return "drain_then_abort";
    }
    return "unknown";
}

bool parseMediaVideoFilterImplementation(
    std::string_view text,
    MediaVideoFilterImplementation& value) noexcept
{
    if (text == "none") value = MediaVideoFilterImplementation::None;
    else if (text == "generic") value = MediaVideoFilterImplementation::Generic;
    else if (text == "rga") value = MediaVideoFilterImplementation::Rga;
    else return false;
    return true;
}

bool parseMediaVideoEncoderAbortPolicy(
    std::string_view text,
    MediaVideoEncoderAbortPolicy& value) noexcept
{
    if (text == "immediate") value = MediaVideoEncoderAbortPolicy::Immediate;
    else if (text == "drain_then_abort") value = MediaVideoEncoderAbortPolicy::DrainThenAbort;
    else return false;
    return true;
}

} // namespace media::ffmpeg::graph
