#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"

#include <algorithm>

namespace media::ffmpeg::graph {

::media::Result<std::vector<std::uint8_t>> MediaTsAacAdtsFramer::frame(
    const MediaTsAacAdtsPlan& plan,
    std::span<const std::uint8_t> rawPayload)
{
    constexpr std::size_t HeaderSize = 7;
    constexpr std::size_t MaximumFrameSize = 8191;
    if (plan.mpegId > 1 || plan.audioObjectType < 1 ||
        plan.audioObjectType > 4 || plan.samplingFrequencyIndex > 12 ||
        plan.channelConfiguration < 1 || plan.channelConfiguration > 7) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS AAC ADTS plan is invalid"));
    }
    if (rawPayload.empty() || rawPayload.size() > MaximumFrameSize - HeaderSize) {
        return ::media::Result<std::vector<std::uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS AAC ADTS frame length is invalid"));
    }

    const std::size_t frameLength = HeaderSize + rawPayload.size();
    const std::uint8_t profile = static_cast<std::uint8_t>(plan.audioObjectType - 1);
    std::vector<std::uint8_t> output(frameLength);
    output[0] = 0xFF;
    output[1] = static_cast<std::uint8_t>(0xF1 | (plan.mpegId << 3));
    output[2] = static_cast<std::uint8_t>(
        (profile << 6) | (plan.samplingFrequencyIndex << 2) |
        (plan.channelConfiguration >> 2));
    output[3] = static_cast<std::uint8_t>(
        ((plan.channelConfiguration & 0x03) << 6) | (frameLength >> 11));
    output[4] = static_cast<std::uint8_t>(frameLength >> 3);
    output[5] = static_cast<std::uint8_t>(((frameLength & 0x07) << 5) | 0x1F);
    output[6] = 0xFC;
    std::copy(rawPayload.begin(), rawPayload.end(), output.begin() + HeaderSize);
    return ::media::Result<std::vector<std::uint8_t>>::success(std::move(output));
}

} // namespace media::ffmpeg::graph
