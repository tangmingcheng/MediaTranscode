#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <filesystem>
#include <vector>

namespace media_transcode::test {

struct ScheduledMpegTsDecodeAccessUnit final {
    ::media::ffmpeg::graph::MediaScheduledStream stream;
    ::media::ffmpeg::PacketPtr packet;
    ::media::ffmpeg::graph::MediaRunningTime dispatchOffset;
    ::media::ffmpeg::graph::MediaRunningTime presentationOnMaster;
};

class ScheduledMpegTsDecodeSampleFixture final {
public:
    static ::media::Result<ScheduledMpegTsDecodeSampleFixture> load(
        const std::filesystem::path& path,
        const ::media::ffmpeg::graph::MediaTsMuxPlan& muxPlan);

    ScheduledMpegTsDecodeSampleFixture(
        ScheduledMpegTsDecodeSampleFixture&&) noexcept = default;
    ScheduledMpegTsDecodeSampleFixture& operator=(
        ScheduledMpegTsDecodeSampleFixture&&) noexcept = default;
    ScheduledMpegTsDecodeSampleFixture(
        const ScheduledMpegTsDecodeSampleFixture&) = delete;
    ScheduledMpegTsDecodeSampleFixture& operator=(
        const ScheduledMpegTsDecodeSampleFixture&) = delete;

    AVCodecContext& videoCodecContext() noexcept { return *m_videoCodec; }
    AVCodecContext& audioCodecContext() noexcept { return *m_audioCodec; }
    const std::vector<ScheduledMpegTsDecodeAccessUnit>& accessUnits() const noexcept
    {
        return m_accessUnits;
    }

private:
    ScheduledMpegTsDecodeSampleFixture(
        ::media::ffmpeg::CodecContextPtr videoCodec,
        ::media::ffmpeg::CodecContextPtr audioCodec,
        std::vector<ScheduledMpegTsDecodeAccessUnit> accessUnits) noexcept;

    ::media::ffmpeg::CodecContextPtr m_videoCodec;
    ::media::ffmpeg::CodecContextPtr m_audioCodec;
    std::vector<ScheduledMpegTsDecodeAccessUnit> m_accessUnits;
};

} // namespace media_transcode::test
