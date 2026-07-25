#include "internal/graph/protocol/rtp/MediaAacAudioSpecificConfig.h"
#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"

namespace media::ffmpeg::graph {
::media::Result<MediaAacAudioSpecificConfig> parseAacAudioSpecificConfig(
    const std::vector<uint8_t>& bytes)
{
    auto parsed = parseMediaAacAudioSpecificConfig(bytes);
    if (!parsed) {
        return ::media::Result<MediaAacAudioSpecificConfig>::failure(
            parsed.error());
    }
    return ::media::Result<MediaAacAudioSpecificConfig>::success({
        parsed.value().audioObjectType,
        parsed.value().sampleRate,
        parsed.value().channels,
        parsed.value().frameSamples
    });
}

} // namespace media::ffmpeg::graph
