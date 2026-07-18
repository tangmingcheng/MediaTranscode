#pragma once

#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <filesystem>
#include <vector>

namespace media_transcode::test {

using namespace media::ffmpeg::graph;

struct ScheduledRtpDecodeAccessUnit final {
    MediaScheduledStream stream;
    ::media::ffmpeg::PacketPtr packet;
    MediaRunningTime dispatchOffset;
    MediaRunningTime presentationOnMaster;
};

class ScheduledRtpDecodeSampleFixture final {
public:
    static ::media::Result<ScheduledRtpDecodeSampleFixture> load(
        const std::filesystem::path& path,
        const MediaScheduledRtpPacketizationPlan& videoPacketization,
        const MediaScheduledRtpPacketizationPlan& audioPacketization);

    ScheduledRtpDecodeSampleFixture(
        ScheduledRtpDecodeSampleFixture&&) noexcept = default;
    ScheduledRtpDecodeSampleFixture& operator=(
        ScheduledRtpDecodeSampleFixture&&) noexcept = default;
    ScheduledRtpDecodeSampleFixture(
        const ScheduledRtpDecodeSampleFixture&) = delete;
    ScheduledRtpDecodeSampleFixture& operator=(
        const ScheduledRtpDecodeSampleFixture&) = delete;

    AVCodecContext& videoCodecContext() noexcept { return *m_videoCodec; }
    AVCodecContext& audioCodecContext() noexcept { return *m_audioCodec; }
    const std::vector<ScheduledRtpDecodeAccessUnit>& accessUnits() const noexcept
    {
        return m_accessUnits;
    }

private:
    ScheduledRtpDecodeSampleFixture(
        ::media::ffmpeg::CodecContextPtr videoCodec,
        ::media::ffmpeg::CodecContextPtr audioCodec,
        std::vector<ScheduledRtpDecodeAccessUnit> accessUnits) noexcept;

    ::media::ffmpeg::CodecContextPtr m_videoCodec;
    ::media::ffmpeg::CodecContextPtr m_audioCodec;
    std::vector<ScheduledRtpDecodeAccessUnit> m_accessUnits;
};

} // namespace media_transcode::test
