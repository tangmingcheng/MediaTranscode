#pragma once

#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "media_transcode/Result.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAudioCorrectionQuantizer.h"
#include <algorithm>
extern "C" {
#include <libavutil/samplefmt.h>
}
#include <cstddef>
#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {

struct MediaAudioEncoderFifoRetentionPlan final {
    int maximumInputSamples;
    int maximumSamples;
    int maximumBytes;
    std::size_t maximumFragments;
    int frameSamples;

    friend bool operator==(const MediaAudioEncoderFifoRetentionPlan&,
                           const MediaAudioEncoderFifoRetentionPlan&) = default;

    static ::media::Result<MediaAudioEncoderFifoRetentionPlan> create(
        const MediaResolvedAudioOutputPlan& output, std::int64_t resamplerBlock,
        const MediaAvSyncAudioServoPolicy& servo)
    {
        if (resamplerBlock <= 0 || !servo.compensationWindowNs || !servo.commandLeadNs ||
            !servo.outputSampleRate || *servo.outputSampleRate != output.sampleRate() ||
            !servo.normalCorrectionLimitPpm || !servo.recoveryCorrectionLimitPpm) {
            return ::media::Result<MediaAudioEncoderFifoRetentionPlan>::failure(
                ::media::ErrorInfo::invalidArgument("audio FIFO requires complete prepared correction bounds"));
        }
        auto quantizer = MediaAudioCorrectionQuantizer::create(
            *servo.compensationWindowNs, *servo.commandLeadNs, *servo.outputSampleRate);
        if (!quantizer) return ::media::Result<MediaAudioEncoderFifoRetentionPlan>::failure(quantizer.error());
        auto distance = quantizer.value().maximumCompensationDistance(
            std::max(*servo.normalCorrectionLimitPpm, *servo.recoveryCorrectionLimitPpm));
        if (!distance) return ::media::Result<MediaAudioEncoderFifoRetentionPlan>::failure(distance.error());
        const auto maximumInput = std::max<std::int64_t>(resamplerBlock, distance.value());
        const int frame = output.codecFrameSamples();
        if (frame <= 0 || maximumInput <= 0 ||
            maximumInput > std::numeric_limits<int>::max() - frame + 1) {
            return ::media::Result<MediaAudioEncoderFifoRetentionPlan>::failure(
                ::media::ErrorInfo::invalidArgument("audio encoder FIFO sample bound is not representable"));
        }
        // AudioEncodeNode drains full frames before admitting another input.
        const int samples = static_cast<int>(maximumInput) + (frame - 1);
        const int bytes = av_samples_get_buffer_size(nullptr, output.channels(),
            samples, av_get_sample_fmt(output.sampleFormat().c_str()), 1);
        if (bytes <= 0) {
            return ::media::Result<MediaAudioEncoderFifoRetentionPlan>::failure(
                ::media::ErrorInfo::invalidArgument("audio encoder FIFO byte bound is not representable"));
        }
        // Each canonical fragment owns at least one sample, regardless of origin.
        return ::media::Result<MediaAudioEncoderFifoRetentionPlan>::success({
            static_cast<int>(maximumInput), samples, bytes,
            static_cast<std::size_t>(samples), frame});
    }
};

} // namespace media::ffmpeg::graph
