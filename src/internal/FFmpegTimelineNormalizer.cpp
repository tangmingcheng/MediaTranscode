#include "internal/FFmpegTimelineNormalizer.h"

#include <algorithm>

namespace media::ffmpeg {

    void TimelineNormalizer::reset()
    {
        m_startUs = AV_NOPTS_VALUE;
    }

    bool TimelineNormalizer::hasStart() const
    {
        return m_startUs != AV_NOPTS_VALUE;
    }

    int64_t TimelineNormalizer::startUs() const
    {
        return m_startUs;
    }

    void TimelineNormalizer::initStart(int64_t timestampUs)
    {
        if (timestampUs == AV_NOPTS_VALUE) {
            return;
        }

        if (m_startUs == AV_NOPTS_VALUE) {
            m_startUs = timestampUs;
        }
    }

    void TimelineNormalizer::initStartFromFormat(const AVFormatContext* formatContext,
                                                 const AVStream* videoStream,
                                                 const AVStream* audioStream)
    {
        if (formatContext && formatContext->start_time != AV_NOPTS_VALUE) {
            initStart(formatContext->start_time);
        }

        if (videoStream && videoStream->start_time != AV_NOPTS_VALUE) {
            initStart(toUs(videoStream->start_time, videoStream->time_base));
        }

        if (audioStream && audioStream->start_time != AV_NOPTS_VALUE) {
            initStart(toUs(audioStream->start_time, audioStream->time_base));
        }
    }

    int64_t TimelineNormalizer::normalizeUs(int64_t timestampUs)
    {
        if (timestampUs == AV_NOPTS_VALUE) {
            return AV_NOPTS_VALUE;
        }

        initStart(timestampUs);

        if (m_startUs == AV_NOPTS_VALUE) {
            return AV_NOPTS_VALUE;
        }

        /*
         * 输出 MP4 不建议保留负时间戳。
         * 这里不是强制同步，而是统一将媒体起点归零。
         */
        return std::max<int64_t>(0, timestampUs - m_startUs);
    }

    int64_t TimelineNormalizer::toUs(int64_t timestamp, AVRational timeBase)
    {
        if (timestamp == AV_NOPTS_VALUE) {
            return AV_NOPTS_VALUE;
        }

        return av_rescale_q(timestamp, timeBase, AVRational{ 1, AV_TIME_BASE });
    }

    int64_t TimelineNormalizer::fromUs(int64_t timestampUs, AVRational timeBase)
    {
        if (timestampUs == AV_NOPTS_VALUE) {
            return AV_NOPTS_VALUE;
        }

        return av_rescale_q(timestampUs, AVRational{ 1, AV_TIME_BASE }, timeBase);
    }

} // namespace media::ffmpeg
