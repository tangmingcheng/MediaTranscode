#pragma once

#include "media_transcode/Result.h"

#include <filesystem>

namespace media_transcode::test {

class ScheduledRtpDecodePrerequisites final {
public:
    static ::media::Status check(
        const std::filesystem::path& ffmpeg,
        const std::filesystem::path& sample);

    static int externalFailureExit(const ::media::ErrorInfo& error);
    static int pipelineFailureExit(const ::media::ErrorInfo& error);

private:
    ScheduledRtpDecodePrerequisites() = delete;
};

} // namespace media_transcode::test
