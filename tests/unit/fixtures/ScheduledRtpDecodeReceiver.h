#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace media_transcode::test {

struct ScheduledRtpFrameMd5Timeline final {
    std::vector<::media::ffmpeg::graph::MediaRunningTime> presentations;
};

class ScheduledRtpDecodeReceiver final {
public:
    static ::media::Status preflightPlatformApis();
    static ::media::Result<std::uint16_t> findAvailableIpv4PortBlock();

    static ::media::Status preflightExecutable(
        const std::filesystem::path& ffmpeg);
    static ::media::Status validateGeneratedSdp(
        const std::filesystem::path& sdp);

    static ::media::Result<ScheduledRtpDecodeReceiver> start(
        const std::filesystem::path& ffmpeg,
        const std::filesystem::path& sdp,
        const std::filesystem::path& videoFrameMd5,
        const std::filesystem::path& audioFrameMd5,
        const std::filesystem::path& log);

    ScheduledRtpDecodeReceiver(ScheduledRtpDecodeReceiver&& other) noexcept;
    ScheduledRtpDecodeReceiver& operator=(
        ScheduledRtpDecodeReceiver&& other) noexcept;
    ScheduledRtpDecodeReceiver(const ScheduledRtpDecodeReceiver&) = delete;
    ScheduledRtpDecodeReceiver& operator=(
        const ScheduledRtpDecodeReceiver&) = delete;
    ~ScheduledRtpDecodeReceiver();

    ::media::Status waitUntilPortsBound(
        std::span<const std::uint16_t> ports,
        std::chrono::milliseconds timeout);
    ::media::Status waitForSuccess(std::chrono::milliseconds timeout);
    std::string diagnostics() const;

private:
    ScheduledRtpDecodeReceiver(
        void* process,
        std::uint32_t processId,
        std::filesystem::path log) noexcept;

    void stop() noexcept;

    void* m_process = nullptr;
    std::uint32_t m_processId = 0;
    std::filesystem::path m_log;
};

::media::Result<ScheduledRtpFrameMd5Timeline>
readScheduledRtpFrameMd5Timeline(const std::filesystem::path& path);

} // namespace media_transcode::test
