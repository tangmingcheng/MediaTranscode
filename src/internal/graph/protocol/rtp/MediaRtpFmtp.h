#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

using MediaRtpFmtpParameters = std::map<std::string, std::string>;

::media::Result<MediaRtpFmtpParameters> parseRtpFmtp(const std::string& text);
::media::Result<int> requiredRtpFmtpInt(const MediaRtpFmtpParameters& parameters,
                                        const std::string& key);
::media::Status validateHevcNonInterleavedRtpFmtp(
    const MediaRtpFmtpParameters& parameters);
::media::Result<std::vector<uint8_t>> decodeRtpFmtpHex(const std::string& text);
::media::Result<std::vector<uint8_t>> decodeRtpFmtpBase64(const std::string& text);
::media::Result<std::string> encodeRtpFmtpBase64(
    std::span<const std::uint8_t> bytes);

} // namespace media::ffmpeg::graph
