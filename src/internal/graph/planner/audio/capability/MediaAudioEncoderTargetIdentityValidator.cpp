#include "internal/graph/planner/audio/capability/MediaAudioEncoderTargetIdentityValidator.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/rational.h>
}

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Status mismatch(const char* field)
{
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            std::string("opened audio encoder changed exact target ") + field));
}

#if LIBAVUTIL_VERSION_MAJOR >= 57
class ChannelLayoutOwner final {
public:
    ~ChannelLayoutOwner() { av_channel_layout_uninit(&m_layout); }

    AVChannelLayout& get() noexcept { return m_layout; }
    const AVChannelLayout& get() const noexcept { return m_layout; }

private:
    AVChannelLayout m_layout{};
};
#endif

} // namespace

::media::Status MediaAudioEncoderTargetIdentityValidator::validate(
    const MediaResolvedAudioTargetDecision& expected,
    AVSampleFormat expectedSampleFormat,
    const AVCodecContext& openedContext)
{
    if (openedContext.sample_rate != expected.sampleRate()) return mismatch("sample rate");
    if (openedContext.sample_fmt != expectedSampleFormat) return mismatch("sample format");
    if (openedContext.profile != expected.profile().ffmpegProfileId()) return mismatch("profile");

    const AVRational expectedTimeBase{1, expected.sampleRate()};
    if (openedContext.time_base.num <= 0 || openedContext.time_base.den <= 0 ||
        expectedTimeBase.num <= 0 || expectedTimeBase.den <= 0 ||
        av_cmp_q(openedContext.time_base, expectedTimeBase) != 0) {
        return mismatch("time base");
    }

#if LIBAVUTIL_VERSION_MAJOR >= 57
    ChannelLayoutOwner expectedLayout;
    const int layoutStatus = av_channel_layout_from_string(
        &expectedLayout.get(), expected.channelLayout().c_str());
    if (layoutStatus < 0 || expectedLayout.get().nb_channels != expected.channels()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "planned audio encoder target has invalid channel layout identity"));
    }
    if (av_channel_layout_check(&openedContext.ch_layout) != 1 ||
        av_channel_layout_compare(&openedContext.ch_layout, &expectedLayout.get()) != 0) {
        return mismatch("channel layout");
    }
#else
    const std::uint64_t expectedMask = av_get_channel_layout(expected.channelLayout().c_str());
    if (expectedMask == 0 ||
        av_get_channel_layout_nb_channels(expectedMask) != expected.channels()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "planned audio encoder target has invalid channel layout identity"));
    }
    if (openedContext.channels != expected.channels() ||
        openedContext.channel_layout != expectedMask) {
        return mismatch("channel layout");
    }
#endif
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
