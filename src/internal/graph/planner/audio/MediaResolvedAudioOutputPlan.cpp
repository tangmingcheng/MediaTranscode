#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

template <typename T>
::media::Result<T> resolveRequired(const std::optional<T>& request,
                                   const std::optional<T>& requirement,
                                   const T& source,
                                   const char* name)
{
    if (request && requirement && *request != *requirement) {
        return ::media::Result<T>::failure(
            ::media::ErrorInfo::unsupported(std::string("audio ") + name + " conflicts with output topology"));
    }
    return ::media::Result<T>::success(request ? *request : (requirement ? *requirement : source));
}

} // namespace

::media::Result<MediaResolvedAudioOutputPlan> MediaResolvedAudioOutputPlan::create(
    const MediaResolvedAudioSource& source,
    const MediaResolvedAudioRequest& request,
    const MediaAudioOutputRequirement& topologyRequirement,
    const std::optional<MediaSelectedAudioEncoder>& selectedEncoder)
{
    const std::string sourceCodec = canonicalCodecName(source.codecName);
    const std::optional<std::string> requestedCodec = request.codecName.empty()
        ? std::nullopt : std::optional<std::string>{canonicalCodecName(request.codecName)};
    auto codec = resolveRequired(requestedCodec, topologyRequirement.codecName, sourceCodec, "codec");
    auto profile = resolveRequired(request.profile, topologyRequirement.profile, source.profile, "profile");
    auto sampleRate = resolveRequired(request.sampleRate, topologyRequirement.sampleRate, source.sampleRate, "sample rate");
    auto channels = resolveRequired(request.channels, topologyRequirement.channels, source.channels, "channel count");
    if (!codec) return ::media::Result<MediaResolvedAudioOutputPlan>::failure(codec.error());
    if (!profile) return ::media::Result<MediaResolvedAudioOutputPlan>::failure(profile.error());
    if (!sampleRate) return ::media::Result<MediaResolvedAudioOutputPlan>::failure(sampleRate.error());
    if (!channels) return ::media::Result<MediaResolvedAudioOutputPlan>::failure(channels.error());
    if (codec.value().empty() || sampleRate.value() <= 0 || channels.value() <= 0) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("resolved audio output requires codec, sample rate, and channels"));
    }

    const bool bitrateMatches = !request.bitrateKbps ||
        (source.bitrateBitsPerSecond > 0 &&
         (source.bitrateBitsPerSecond + 999) / 1000 == *request.bitrateKbps);
    const bool encoderOnlyRequest = request.rateControl != MediaRateControlMode::Auto ||
        request.minBitrateKbps || request.maxBitrateKbps || request.bufferSizeKbits || request.quality ||
        !request.preset.empty();
    const bool copy = codec.value() == sourceCodec && profile.value() == source.profile &&
        sampleRate.value() == source.sampleRate && channels.value() == source.channels &&
        bitrateMatches && !encoderOnlyRequest;
    if (!copy && (!selectedEncoder || selectedEncoder->name.empty() || selectedEncoder->sampleFormat.empty())) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("transcode audio output requires complete selected encoder"));
    }

    MediaResolvedAudioOutputPlan plan;
    plan.m_codecName = codec.value();
    plan.m_profile = profile.value();
    plan.m_sampleRate = sampleRate.value();
    plan.m_channels = channels.value();
    plan.m_channelLayout = channels.value() == 1 ? "mono" : (channels.value() == 2 ? "stereo" : source.channelLayout);
    if (plan.m_channelLayout.empty()) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("resolved audio output requires channel layout"));
    }
    plan.m_sampleFormat = copy ? source.sampleFormat : selectedEncoder->sampleFormat;
    if (plan.m_sampleFormat.empty()) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("resolved audio output requires sample format"));
    }
    plan.m_branchMode = copy ? MediaBranchMode::CopyPacket : MediaBranchMode::TranscodeFrame;
    if (!copy) plan.m_encoderName = selectedEncoder->name;
    plan.m_rateControl = request.rateControl;
    plan.m_bitrateKbps = request.bitrateKbps;
    plan.m_minBitrateKbps = request.minBitrateKbps;
    plan.m_maxBitrateKbps = request.maxBitrateKbps;
    plan.m_bufferSizeKbits = request.bufferSizeKbits;
    plan.m_quality = request.quality;
    plan.m_preset = request.preset;
    return ::media::Result<MediaResolvedAudioOutputPlan>::success(std::move(plan));
}

const std::string& MediaResolvedAudioOutputPlan::codecName() const noexcept { return m_codecName; }
const MediaAudioProfile& MediaResolvedAudioOutputPlan::profile() const noexcept { return m_profile; }
int MediaResolvedAudioOutputPlan::sampleRate() const noexcept { return m_sampleRate; }
int MediaResolvedAudioOutputPlan::channels() const noexcept { return m_channels; }
const std::string& MediaResolvedAudioOutputPlan::channelLayout() const noexcept { return m_channelLayout; }
const std::string& MediaResolvedAudioOutputPlan::sampleFormat() const noexcept { return m_sampleFormat; }
MediaBranchMode MediaResolvedAudioOutputPlan::branchMode() const noexcept { return m_branchMode; }
const std::string& MediaResolvedAudioOutputPlan::encoderName() const noexcept { return m_encoderName; }
MediaRateControlMode MediaResolvedAudioOutputPlan::rateControl() const noexcept { return m_rateControl; }
const std::optional<int>& MediaResolvedAudioOutputPlan::bitrateKbps() const noexcept { return m_bitrateKbps; }
const std::optional<int>& MediaResolvedAudioOutputPlan::minBitrateKbps() const noexcept { return m_minBitrateKbps; }
const std::optional<int>& MediaResolvedAudioOutputPlan::maxBitrateKbps() const noexcept { return m_maxBitrateKbps; }
const std::optional<int>& MediaResolvedAudioOutputPlan::bufferSizeKbits() const noexcept { return m_bufferSizeKbits; }
const std::optional<int>& MediaResolvedAudioOutputPlan::quality() const noexcept { return m_quality; }
const std::string& MediaResolvedAudioOutputPlan::preset() const noexcept { return m_preset; }

} // namespace media::ffmpeg::graph
