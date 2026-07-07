#include "internal/graph/runtime/ffmpeg/FFmpegRealtimeInputOptions.h"

extern "C" {
#include <libavutil/dict.h>
}

#include <string>

namespace media::ffmpeg::graph {
namespace {

std::string positiveIntText(int value)
{
    return value > 0 ? std::to_string(value) : std::string();
}

std::string millisecondsAsMicrosecondsText(int milliseconds)
{
    if (milliseconds <= 0) {
        return {};
    }
    constexpr int kMicrosecondsPerMillisecond = 1000;
    return std::to_string(milliseconds * kMicrosecondsPerMillisecond);
}

void setDictionaryOption(AVDictionary** dictionary,
                         const std::string& key,
                         const std::string& value)
{
    if (!value.empty()) {
        av_dict_set(dictionary, key.c_str(), value.c_str(), 0);
    }
}

} // namespace

void applyFFmpegRealtimeInputOptions(AVDictionary** dictionary,
                                     const FFmpegRealtimeInputOptions& options)
{
    setDictionaryOption(dictionary, "rtsp_transport", options.rtspTransport);
    setDictionaryOption(dictionary, "stimeout", millisecondsAsMicrosecondsText(options.openTimeoutMs));
    setDictionaryOption(dictionary, "rw_timeout", millisecondsAsMicrosecondsText(options.readTimeoutMs));
    setDictionaryOption(dictionary, "timeout", millisecondsAsMicrosecondsText(options.readTimeoutMs));
    setDictionaryOption(dictionary, "analyzeduration", positiveIntText(options.analyzeDurationUs));
    setDictionaryOption(dictionary, "probesize", positiveIntText(options.probeSizeBytes));
    if (options.lowLatency) {
        setDictionaryOption(dictionary, "fflags", "nobuffer");
        setDictionaryOption(dictionary, "flags", "low_delay");
    }
    if (options.allowFileUdpRtpProtocols) {
        setDictionaryOption(dictionary, "protocol_whitelist", "file,udp,rtp");
    }
}

} // namespace media::ffmpeg::graph
