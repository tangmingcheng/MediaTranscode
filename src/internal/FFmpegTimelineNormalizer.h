#pragma once

#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

    class TimelineNormalizer {
    public:
        TimelineNormalizer() = default;

        void reset();

        bool hasStart() const;
        int64_t startUs() const;

        void initStart(int64_t timestampUs);
        void initStartFromFormat(const AVFormatContext* formatContext,
                                 const AVStream* videoStream,
                                 const AVStream* audioStream);

        int64_t normalizeUs(int64_t timestampUs);

        static int64_t toUs(int64_t timestamp, AVRational timeBase);
        static int64_t fromUs(int64_t timestampUs, AVRational timeBase);

    private:
        int64_t m_startUs = AV_NOPTS_VALUE;
    };

} // namespace media::ffmpeg
