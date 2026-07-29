#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"

#include <algorithm>

namespace media::ffmpeg::graph {

::media::Result<std::span<const std::uint8_t>> MediaTsAacAdtsFramer::frame(
    const MediaTsAacAdtsPlan& plan,
    std::span<const std::uint8_t> rawPayload,
    std::vector<std::uint8_t>& workspace)
{
    constexpr std::size_t HeaderSize = 7;
    constexpr std::size_t MaximumFrameSize = 8191;
    if (plan.mpegId > 1 || plan.audioObjectType < 1 ||
        plan.audioObjectType > 4 || plan.samplingFrequencyIndex > 12 ||
        plan.channelConfiguration < 1 || plan.channelConfiguration > 7) {
        return ::media::Result<std::span<const std::uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS AAC ADTS plan is invalid"));
    }
    if (rawPayload.empty() || rawPayload.size() > MaximumFrameSize - HeaderSize) {
        return ::media::Result<std::span<const std::uint8_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS AAC ADTS frame length is invalid"));
    }

    const std::size_t frameLength = HeaderSize + rawPayload.size();
    const std::uint8_t profile = static_cast<std::uint8_t>(plan.audioObjectType - 1);
    workspace.resize(frameLength);
    workspace[0] = 0xFF;
    workspace[1] = static_cast<std::uint8_t>(0xF1 | (plan.mpegId << 3));
    workspace[2] = static_cast<std::uint8_t>(
        (profile << 6) | (plan.samplingFrequencyIndex << 2) |
        (plan.channelConfiguration >> 2));
    workspace[3] = static_cast<std::uint8_t>(
        ((plan.channelConfiguration & 0x03) << 6) | (frameLength >> 11));
    workspace[4] = static_cast<std::uint8_t>(frameLength >> 3);
    workspace[5] = static_cast<std::uint8_t>(
        ((frameLength & 0x07) << 5) | 0x1F);
    workspace[6] = 0xFC;
    std::copy(
        rawPayload.begin(), rawPayload.end(), workspace.begin() + HeaderSize);
    return ::media::Result<std::span<const std::uint8_t>>::success(workspace);
}

} // namespace media::ffmpeg::graph
