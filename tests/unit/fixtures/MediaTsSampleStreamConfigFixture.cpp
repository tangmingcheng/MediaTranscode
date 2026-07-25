#include "MediaTsSampleStreamConfigFixture.h"

#include <array>
#include <cstddef>
#include <utility>

namespace media_transcode::test {
namespace {

::media::Status readParameterSets(
    std::span<const std::uint8_t> bytes,
    std::size_t count,
    std::size_t& offset,
    std::vector<std::uint8_t>& firstParameterSet)
{
    for (std::size_t index = 0; index < count; ++index) {
        if (offset + 2 > bytes.size()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "sample AVCC parameter-set length is truncated"));
        }
        const std::size_t size =
            (std::size_t{bytes[offset]} << 8) | bytes[offset + 1];
        offset += 2;
        if (size == 0 || size > bytes.size() - offset) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "sample AVCC parameter-set payload is invalid"));
        }
        if (firstParameterSet.empty()) {
            firstParameterSet = {0, 0, 0, 1};
            firstParameterSet.insert(
                firstParameterSet.end(),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
        }
        offset += size;
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaTsSampleAvccConfig>
MediaTsSampleStreamConfigFixture::parseAvcc(
    std::span<const std::uint8_t> extradata)
{
    if (extradata.size() < 7 || extradata[0] != 1) {
        return ::media::Result<MediaTsSampleAvccConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "sample H.264 extradata is not a complete AVCC record"));
    }

    MediaTsSampleAvccConfig config{
        static_cast<std::uint8_t>((extradata[4] & 3) + 1), {}, {}};
    std::size_t offset = 6;
    const std::size_t spsCount = extradata[5] & 0x1F;
    auto status = readParameterSets(
        extradata, spsCount, offset, config.sequenceParameterSet);
    if (!status) {
        return ::media::Result<MediaTsSampleAvccConfig>::failure(status.error());
    }
    if (offset >= extradata.size()) {
        return ::media::Result<MediaTsSampleAvccConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "sample AVCC record has no PPS count"));
    }
    const std::size_t ppsCount = extradata[offset++];
    status = readParameterSets(
        extradata, ppsCount, offset, config.pictureParameterSet);
    if (!status) {
        return ::media::Result<MediaTsSampleAvccConfig>::failure(status.error());
    }
    if (config.sequenceParameterSet.empty() ||
        config.pictureParameterSet.empty()) {
        return ::media::Result<MediaTsSampleAvccConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "sample AVCC record must contain SPS and PPS"));
    }
    return ::media::Result<MediaTsSampleAvccConfig>::success(std::move(config));
}

::media::Result<std::uint8_t>
MediaTsSampleStreamConfigFixture::aacSamplingFrequencyIndex(int sampleRate)
{
    constexpr std::array<int, 13> rates{
        96000, 88200, 64000, 48000, 44100, 32000, 24000,
        22050, 16000, 12000, 11025, 8000, 7350};
    for (std::size_t index = 0; index < rates.size(); ++index) {
        if (rates[index] == sampleRate) {
            return ::media::Result<std::uint8_t>::success(
                static_cast<std::uint8_t>(index));
        }
    }
    return ::media::Result<std::uint8_t>::failure(
        ::media::ErrorInfo::unsupported(
            "sample AAC rate has no ADTS sampling-frequency index"));
}

} // namespace media_transcode::test
