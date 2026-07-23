#pragma once

#include "media_transcode/Result.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace media_transcode::test {

class PreparedAvFixture final {
public:
    static ::media::Result<PreparedAvFixture> create(
        const std::filesystem::path& ffmpeg,
        const std::filesystem::path& source,
        int videoWidth,
        int videoHeight);

    PreparedAvFixture(PreparedAvFixture&& other) noexcept;
    PreparedAvFixture& operator=(PreparedAvFixture&& other) noexcept;
    PreparedAvFixture(const PreparedAvFixture&) = delete;
    PreparedAvFixture& operator=(const PreparedAvFixture&) = delete;
    ~PreparedAvFixture();

    const std::filesystem::path& path() const noexcept { return m_path; }

private:
    explicit PreparedAvFixture(std::filesystem::path path);
    void remove() noexcept;

    std::filesystem::path m_path;
};

class UdpDatagramReceiver final {
public:
    static ::media::Result<UdpDatagramReceiver> bindLoopback(
        std::uint16_t port);

    UdpDatagramReceiver(UdpDatagramReceiver&& other) noexcept;
    UdpDatagramReceiver& operator=(UdpDatagramReceiver&& other) noexcept;
    UdpDatagramReceiver(const UdpDatagramReceiver&) = delete;
    UdpDatagramReceiver& operator=(const UdpDatagramReceiver&) = delete;
    ~UdpDatagramReceiver();

    ::media::Result<std::size_t> receiveBytes(
        std::chrono::milliseconds timeout);

private:
    explicit UdpDatagramReceiver(std::uintptr_t socket) noexcept;
    void close() noexcept;

    std::uintptr_t m_socket = 0;
};

class FfmpegRealtimeFeeder final {
public:
    static ::media::Result<FfmpegRealtimeFeeder> startMpegTs(
        const std::filesystem::path& ffmpeg,
        const std::filesystem::path& sample,
        std::uint16_t destinationPort);
    static ::media::Result<FfmpegRealtimeFeeder> startSeparateRtp(
        const std::filesystem::path& ffmpeg,
        const std::filesystem::path& sample,
        std::uint16_t videoRtpPort,
        std::uint16_t audioRtpPort,
        std::string cname);

    FfmpegRealtimeFeeder(FfmpegRealtimeFeeder&& other) noexcept;
    FfmpegRealtimeFeeder& operator=(FfmpegRealtimeFeeder&& other) noexcept;
    FfmpegRealtimeFeeder(const FfmpegRealtimeFeeder&) = delete;
    FfmpegRealtimeFeeder& operator=(const FfmpegRealtimeFeeder&) = delete;
    ~FfmpegRealtimeFeeder();

private:
    explicit FfmpegRealtimeFeeder(void* process) noexcept;
    static ::media::Result<FfmpegRealtimeFeeder> startProcess(
        const std::filesystem::path& executable,
        std::wstring command);
    void stop() noexcept;

    void* m_process = nullptr;
};

::media::Result<std::uint16_t> findAvailableLoopbackUdpPort();

} // namespace media_transcode::test
