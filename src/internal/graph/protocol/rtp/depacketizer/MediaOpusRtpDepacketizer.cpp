#include "internal/graph/protocol/rtp/depacketizer/MediaOpusRtpDepacketizer.h"
#include "internal/graph/protocol/rtp/MediaOpusRtpCapability.h"

namespace media::ffmpeg::graph {
namespace {

constexpr std::size_t MaxOpusFrameBytes = 1275;
constexpr int MaxOpusFramesPerPacket = 48;

int opusFrameSamples(int config) noexcept
{
    if (config < 12) {
        constexpr int SilkSamples[] = {480, 960, 1920, 2880};
        return SilkSamples[config & 3];
    }
    if (config < 16) return (config & 1) == 0 ? 480 : 960;
    constexpr int CeltSamples[] = {120, 240, 480, 960};
    return CeltSamples[config & 3];
}

::media::Result<std::size_t> readFrameSize(const std::vector<uint8_t>& payload,
                                          std::size_t& offset)
{
    if (offset >= payload.size()) return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::invalidArgument("Opus frame-size field is truncated"));
    const uint8_t first = payload[offset++];
    if (first < 252) return ::media::Result<std::size_t>::success(first);
    if (offset >= payload.size()) return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::invalidArgument("Opus two-byte frame-size field is truncated"));
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(first) + 4 * payload[offset++]);
}

::media::Status validateFrameSize(std::size_t size)
{
    if (size == 0 || size > MaxOpusFrameBytes) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("Opus packet contains an invalid frame size"));
    return ::media::Status::success();
}

::media::Result<int64_t> parseOpusPacketDuration(const std::vector<uint8_t>& payload)
{
    if (payload.empty()) return ::media::Result<int64_t>::failure(
        ::media::ErrorInfo::invalidArgument("Opus RTP payload is empty"));
    const uint8_t toc = payload[0];
    const int config = toc >> 3;
    const int frameSamples = opusFrameSamples(config);
    const uint8_t frameCode = toc & 3;
    int frames = frameCode == 0 ? 1 : 2;
    std::size_t offset = 1;
    if (frameCode == 0) {
        if (auto status = validateFrameSize(payload.size() - offset); !status)
            return ::media::Result<int64_t>::failure(status.error());
    } else if (frameCode == 1) {
        const std::size_t dataBytes = payload.size() - offset;
        if ((dataBytes % 2) != 0) return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("Opus code-1 CBR frames have unequal sizes"));
        if (auto status = validateFrameSize(dataBytes / 2); !status)
            return ::media::Result<int64_t>::failure(status.error());
    } else if (frameCode == 2) {
        auto firstSize = readFrameSize(payload, offset);
        if (!firstSize) return ::media::Result<int64_t>::failure(firstSize.error());
        if (auto status = validateFrameSize(firstSize.value()); !status)
            return ::media::Result<int64_t>::failure(status.error());
        if (firstSize.value() >= payload.size() - offset) return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("Opus code-2 VBR sizes exceed packet data"));
        if (auto status = validateFrameSize(payload.size() - offset - firstSize.value()); !status)
            return ::media::Result<int64_t>::failure(status.error());
    } else {
        if (offset >= payload.size()) return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("Opus code-3 packet lacks frame count"));
        const uint8_t control = payload[offset++];
        frames = control & 0x3f;
        if (frames == 0 || frames > MaxOpusFramesPerPacket) return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("Opus code-3 packet frame count is outside 1..48"));

        std::size_t padding = 0;
        if ((control & 0x40) != 0) {
            uint8_t byte = 0;
            do {
                if (offset >= payload.size()) return ::media::Result<int64_t>::failure(
                    ::media::ErrorInfo::invalidArgument("Opus code-3 padding length is truncated"));
                byte = payload[offset++];
                const std::size_t contribution = byte == 255 ? 254 : byte;
                if (padding > payload.size() - contribution) return ::media::Result<int64_t>::failure(
                    ::media::ErrorInfo::invalidArgument("Opus code-3 padding length overflows packet"));
                padding += contribution;
            } while (byte == 255);
        }
        if (padding > payload.size() - offset) return ::media::Result<int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("Opus code-3 padding exceeds packet data"));
        const std::size_t dataEnd = payload.size() - padding;

        if ((control & 0x80) != 0) {
            std::size_t declaredBytes = 0;
            for (int index = 0; index < frames - 1; ++index) {
                auto size = readFrameSize(payload, offset);
                if (!size) return ::media::Result<int64_t>::failure(size.error());
                if (auto status = validateFrameSize(size.value()); !status)
                    return ::media::Result<int64_t>::failure(status.error());
                if (offset > dataEnd || declaredBytes > dataEnd - offset ||
                    size.value() > dataEnd - offset - declaredBytes)
                    return ::media::Result<int64_t>::failure(
                        ::media::ErrorInfo::invalidArgument("Opus code-3 VBR sizes exceed packet data"));
                declaredBytes += size.value();
            }
            if (offset > dataEnd || declaredBytes >= dataEnd - offset) return ::media::Result<int64_t>::failure(
                ::media::ErrorInfo::invalidArgument("Opus code-3 VBR final frame is empty or truncated"));
            if (auto status = validateFrameSize(dataEnd - offset - declaredBytes); !status)
                return ::media::Result<int64_t>::failure(status.error());
        } else {
            if (offset > dataEnd) return ::media::Result<int64_t>::failure(
                ::media::ErrorInfo::invalidArgument("Opus code-3 CBR header exceeds packet data"));
            const std::size_t dataBytes = dataEnd - offset;
            if ((dataBytes % static_cast<std::size_t>(frames)) != 0) return ::media::Result<int64_t>::failure(
                ::media::ErrorInfo::invalidArgument("Opus code-3 CBR frames have unequal sizes"));
            if (auto status = validateFrameSize(dataBytes / static_cast<std::size_t>(frames)); !status)
                return ::media::Result<int64_t>::failure(status.error());
        }
    }
    const int64_t duration = static_cast<int64_t>(frameSamples) * frames;
    if (duration > MaximumOpusRtpAccessUnitSamples) return ::media::Result<int64_t>::failure(
        ::media::ErrorInfo::invalidArgument("Opus RTP packet duration exceeds 120 ms"));
    return ::media::Result<int64_t>::success(duration);
}

} // namespace

MediaOpusRtpDepacketizer::MediaOpusRtpDepacketizer(MediaRtpDepacketizerConfig config)
    : m_config(std::move(config))
{
}

::media::Result<MediaRtpDepacketizerResult> MediaOpusRtpDepacketizer::push(const MediaRtpPacket& packet)
{
    if (packet.payloadType != m_config.payloadType) return ::media::Result<MediaRtpDepacketizerResult>::failure(
        ::media::ErrorInfo::invalidArgument("Opus RTP payload type changed"));
    auto duration = parseOpusPacketDuration(packet.payload);
    if (!duration) return ::media::Result<MediaRtpDepacketizerResult>::failure(duration.error());
    auto unit = makeRtpAccessUnit(packet.payload, packet.timestamp, m_config.clockRate, duration.value(), true);
    if (!unit) return ::media::Result<MediaRtpDepacketizerResult>::failure(unit.error());
    MediaRtpDepacketizerResult result; result.accessUnits.push_back(std::move(unit).value());
    return ::media::Result<MediaRtpDepacketizerResult>::success(std::move(result));
}

void MediaOpusRtpDepacketizer::discontinuity(MediaRtpDiscontinuityReason) noexcept
{
}

} // namespace media::ffmpeg::graph
