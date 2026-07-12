#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

using MediaRtpFmtpParameters = std::map<std::string, std::string>;

::media::Result<MediaRtpFmtpParameters> parseRtpFmtp(const std::string& text);
::media::Result<int> requiredRtpFmtpInt(const MediaRtpFmtpParameters& parameters,
                                        const std::string& key);
::media::Result<std::vector<uint8_t>> decodeRtpFmtpHex(const std::string& text);
::media::Result<std::vector<uint8_t>> decodeRtpFmtpBase64(const std::string& text);

} // namespace media::ffmpeg::graph
