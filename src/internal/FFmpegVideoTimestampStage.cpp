#include "internal/FFmpegVideoTimestampStage.h"

#include "internal/FFmpegTimelineNormalizer.h"

namespace media::ffmpeg {
namespace {

bool isValidTimeBase(AVRational timeBase)
{
    return timeBase.num > 0 && timeBase.den > 0;
}

} // namespace

FFmpegVideoTimestampStage::~FFmpegVideoTimestampStage()
{
    reset();
}

void FFmpegVideoTimestampStage::reset()
{
    m_inputTimeBase = AVRational{ 0, 1 };
    m_timeline = nullptr;
}

bool FFmpegVideoTimestampStage::initialize(const Config& config, std::string* error)
{
    reset();

    if (!isValidTimeBase(config.inputMetadata.timeBase)) {
        if (error) {
            *error = "FFmpegVideoTimestampStage initialize failed: input time base is invalid";
        }
        return false;
    }

    if (!config.timeline) {
        if (error) {
            *error = "FFmpegVideoTimestampStage initialize failed: timeline is null";
        }
        return false;
    }

    m_inputTimeBase = config.inputMetadata.timeBase;
    m_timeline = config.timeline;

    return true;
}

bool FFmpegVideoTimestampStage::normalizeFramePts(AVFrame* frame, std::string* error) const
{
    if (!frame) {
        if (error) {
            *error = "normalize video frame pts failed: frame is null";
        }
        return false;
    }

    if (!isValidTimeBase(m_inputTimeBase) || !m_timeline) {
        if (error) {
            *error = "normalize video frame pts failed: timestamp stage is not initialized";
        }
        return false;
    }

    const int64_t inputVideoTs = decodedFrameTimestamp(frame);
    if (inputVideoTs == AV_NOPTS_VALUE) {
        if (error) {
            *error = "input video frame has no valid timestamp; refuse to synthesize PTS in normalized transcoder";
        }
        return false;
    }

    const int64_t inputVideoUs = TimelineNormalizer::toUs(inputVideoTs, m_inputTimeBase);
    const int64_t normalizedVideoUs = m_timeline->normalizeUs(inputVideoUs);

    if (normalizedVideoUs == AV_NOPTS_VALUE) {
        if (error) {
            *error = "failed to normalize input video timestamp";
        }
        return false;
    }

    frame->pts = TimelineNormalizer::fromUs(normalizedVideoUs, m_inputTimeBase);
    if (frame->pts == AV_NOPTS_VALUE) {
        if (error) {
            *error = "decoded video frame pts is invalid after normalization";
        }
        return false;
    }

    return true;
}

bool FFmpegVideoTimestampStage::isInitialized() const
{
    return isValidTimeBase(m_inputTimeBase) && m_timeline;
}

int64_t FFmpegVideoTimestampStage::decodedFrameTimestamp(const AVFrame* frame)
{
    if (!frame) {
        return AV_NOPTS_VALUE;
    }

    if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
        return frame->best_effort_timestamp;
    }

    if (frame->pts != AV_NOPTS_VALUE) {
        return frame->pts;
    }

    if (frame->pkt_dts != AV_NOPTS_VALUE) {
        return frame->pkt_dts;
    }

    return AV_NOPTS_VALUE;
}

} // namespace media::ffmpeg
