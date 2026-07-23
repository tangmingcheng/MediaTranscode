#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaSynchronizedAudioDecodeInput final {
    std::shared_ptr<const MediaCanonicalLineage> lineage;
    MediaCanonicalAudioSampleInterval sourceInterval;
    MediaAudioPlaybackOrigin origin;
    std::uint32_t trimLeadingSamples;
};

struct MediaAudioDecodeInputView final {
    MediaBufferRef packet;
    std::optional<MediaSynchronizedAudioDecodeInput> synchronized;
};

::media::Result<MediaAudioDecodeInputView> resolveMediaAudioDecodeInput(
    const MediaBufferRef& input,
    MediaAudioLineageExecutionMode mode);

} // namespace media::ffmpeg::graph
