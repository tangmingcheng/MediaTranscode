#include "internal/graph/protocol/sdp/MediaAacLatmSdpCodecDescriptionFactory.h"

#include "internal/graph/protocol/rtp/MediaAacAudioSpecificConfig.h"

extern "C" {
#include <libavcodec/codec_par.h>
}

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

class BitWriter final {
public:
    void append(std::uint32_t value, int bitCount)
    {
        for (int bit = bitCount - 1; bit >= 0; --bit) {
            if (m_bitOffset == 0) m_bytes.push_back(0);
            m_bytes.back() |= static_cast<std::uint8_t>(
                ((value >> bit) & 1u) << (7 - m_bitOffset));
            m_bitOffset = (m_bitOffset + 1) % 8;
        }
    }

    const std::vector<std::uint8_t>& bytes() const noexcept { return m_bytes; }

private:
    std::vector<std::uint8_t> m_bytes;
    int m_bitOffset = 0;
};

std::string uppercaseHex(std::span<const std::uint8_t> bytes)
{
    constexpr char digits[] = "0123456789ABCDEF";
    std::string output;
    output.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        output.push_back(digits[byte >> 4]);
        output.push_back(digits[byte & 0x0f]);
    }
    return output;
}

} // namespace

::media::Result<MediaAacLatmSdpCodecDescription>
MediaAacLatmSdpCodecDescriptionFactory::create(const AVCodecParameters& parameters)
{
    if (parameters.codec_type != AVMEDIA_TYPE_AUDIO ||
        parameters.codec_id != AV_CODEC_ID_AAC || !parameters.extradata ||
        parameters.extradata_size != 2 || parameters.sample_rate <= 0 ||
        parameters.ch_layout.nb_channels <= 0) {
        return ::media::Result<MediaAacLatmSdpCodecDescription>::failure(
            ::media::ErrorInfo::unsupported("final codec parameters are not complete AAC-LC"));
    }
    const std::vector<std::uint8_t> asc(
        parameters.extradata,
        parameters.extradata + parameters.extradata_size);
    auto parsed = parseAacAudioSpecificConfig(asc);
    if (!parsed) {
        return ::media::Result<MediaAacLatmSdpCodecDescription>::failure(parsed.error());
    }
    if (parsed.value().sampleRate != parameters.sample_rate ||
        parsed.value().channels != parameters.ch_layout.nb_channels ||
        (parsed.value().channels != 1 && parsed.value().channels != 2) ||
        parsed.value().sampleRate > 48'000 || parsed.value().frameSamples != 1024) {
        return ::media::Result<MediaAacLatmSdpCodecDescription>::failure(
            ::media::ErrorInfo::unsupported(
                "AAC-LATM SDP supports matching AAC-LC mono or stereo up to 48 kHz"));
    }

    BitWriter config;
    config.append(0, 1);       // audioMuxVersion
    config.append(1, 1);       // allStreamsSameTimeFraming
    config.append(0, 6);       // numSubFrames
    config.append(0, 4);       // numProgram
    config.append(0, 3);       // numLayer
    config.append(asc[0], 8);  // AudioSpecificConfig
    config.append(asc[1], 8);
    config.append(0, 3);       // frameLengthType
    config.append(0xff, 8);    // latmBufferFullness
    config.append(0, 1);       // otherDataPresent
    config.append(0, 1);       // crcCheckPresent

    const int profileLevelId = parsed.value().sampleRate <= 24'000 ? 40 : 41;
    return ::media::Result<MediaAacLatmSdpCodecDescription>::success(
        MediaAacLatmSdpCodecDescription(
            parsed.value().sampleRate, parsed.value().channels, profileLevelId,
            uppercaseHex(config.bytes()), false));
}

} // namespace media::ffmpeg::graph
