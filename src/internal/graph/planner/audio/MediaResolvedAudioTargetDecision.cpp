#include "internal/graph/planner/audio/MediaResolvedAudioTargetDecision.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <limits>
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
            ::media::ErrorInfo::unsupported(std::string("audio ") + name +
                                            " conflicts with output topology"));
    }
    return ::media::Result<T>::success(request ? *request : (requirement ? *requirement : source));
}

::media::Status validateCodecProfile(const std::string& codec,
                                     const MediaAudioProfile& profile,
                                     bool allowUnknownAac)
{
    if (codec == "aac") {
        if (profile.knowledge() == MediaAudioProfileKnowledge::Unknown && allowUnknownAac) {
            return ::media::Status::success();
        }
        if (profile.knowledge() != MediaAudioProfileKnowledge::Known ||
            profile != MediaAudioProfile::knownAacLow()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::unsupported(
                    "AAC output profile must be executable AAC-LC"));
        }
        return ::media::Status::success();
    }
    if (profile.knowledge() != MediaAudioProfileKnowledge::NotApplicable) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported(
                "non-AAC audio codec requires a not-applicable profile"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Result<MediaResolvedAudioTargetDecision> MediaResolvedAudioTargetDecision::create(
    const MediaResolvedAudioSource& source,
    const MediaResolvedAudioRequest& request,
    const MediaAudioOutputRequirement& topologyRequirement)
{
    const std::string sourceCodec = canonicalCodecName(source.codecName);
    if (sourceCodec.empty() || source.sampleRate <= 0 || source.channels <= 0 ||
        source.channelLayout.empty() || source.sampleFormat.empty()) {
        return ::media::Result<MediaResolvedAudioTargetDecision>::failure(
            ::media::ErrorInfo::invalidArgument("resolved audio source facts are incomplete"));
    }
    if ((sourceCodec == "aac" &&
         source.profile.knowledge() == MediaAudioProfileKnowledge::NotApplicable) ||
        (sourceCodec != "aac" &&
         source.profile.knowledge() != MediaAudioProfileKnowledge::NotApplicable)) {
        return ::media::Result<MediaResolvedAudioTargetDecision>::failure(
            ::media::ErrorInfo::unsupported("audio source codec and profile are inconsistent"));
    }

    const std::optional<std::string> requestedCodec = request.codecName.empty()
        ? std::nullopt : std::optional<std::string>{canonicalCodecName(request.codecName)};
    const std::optional<std::string> requiredCodec = topologyRequirement.codecName
        ? std::optional<std::string>{canonicalCodecName(*topologyRequirement.codecName)}
        : std::nullopt;
    auto codec = resolveRequired(requestedCodec, requiredCodec, sourceCodec, "codec");
    auto profile = resolveRequired(request.profile, topologyRequirement.profile,
                                   source.profile, "profile");
    auto sampleRate = resolveRequired(request.sampleRate, topologyRequirement.sampleRate,
                                      source.sampleRate, "sample rate");
    auto channels = resolveRequired(request.channels, topologyRequirement.channels,
                                    source.channels, "channel count");
    if (!codec) return ::media::Result<MediaResolvedAudioTargetDecision>::failure(codec.error());
    if (!profile) return ::media::Result<MediaResolvedAudioTargetDecision>::failure(profile.error());
    if (!sampleRate) return ::media::Result<MediaResolvedAudioTargetDecision>::failure(sampleRate.error());
    if (!channels) return ::media::Result<MediaResolvedAudioTargetDecision>::failure(channels.error());
    if (codec.value().empty() || sampleRate.value() <= 0 || channels.value() <= 0) {
        return ::media::Result<MediaResolvedAudioTargetDecision>::failure(
            ::media::ErrorInfo::invalidArgument(
                "resolved audio target requires codec, sample rate, and channels"));
    }

    const bool bitrateMatches = !request.bitrateKbps ||
        (source.bitrateBitsPerSecond > 0 &&
         (source.bitrateBitsPerSecond + 999) / 1000 == *request.bitrateKbps);
    const bool encoderOnlyRequest = request.rateControl != MediaRateControlMode::Auto ||
        request.minBitrateKbps || request.maxBitrateKbps || request.bufferSizeKbits ||
        request.quality || !request.preset.empty();
    const bool copy = codec.value() == sourceCodec && profile.value() == source.profile &&
        sampleRate.value() == source.sampleRate && channels.value() == source.channels &&
        bitrateMatches && !encoderOnlyRequest &&
        !topologyRequirement.requireFrameTranscode;
    if (auto status = validateCodecProfile(codec.value(), profile.value(), copy); !status) {
        return ::media::Result<MediaResolvedAudioTargetDecision>::failure(status.error());
    }

    MediaResolvedAudioTargetDecision decision;
    decision.m_codecName = codec.value();
    decision.m_profile = profile.value();
    decision.m_sampleRate = sampleRate.value();
    decision.m_channels = channels.value();
    decision.m_channelLayout = channels.value() == 1
        ? "mono" : (channels.value() == 2 ? "stereo" : source.channelLayout);
    if (decision.m_channelLayout.empty()) {
        return ::media::Result<MediaResolvedAudioTargetDecision>::failure(
            ::media::ErrorInfo::invalidArgument("resolved audio target requires channel layout"));
    }
    decision.m_sourceSampleFormat = source.sampleFormat;
    decision.m_branchMode = copy ? MediaBranchMode::CopyPacket : MediaBranchMode::TranscodeFrame;
    decision.m_rateControl = request.rateControl;
    decision.m_bitrateKbps = request.bitrateKbps;
    if (!decision.m_bitrateKbps && source.bitrateBitsPerSecond > 0) {
        const int64_t sourceBitrateKbps =
            (source.bitrateBitsPerSecond + 999) / 1000;
        if (sourceBitrateKbps > std::numeric_limits<int>::max()) {
            return ::media::Result<MediaResolvedAudioTargetDecision>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "resolved source audio bitrate exceeds integer range"));
        }
        decision.m_bitrateKbps = static_cast<int>(sourceBitrateKbps);
    }
    decision.m_minBitrateKbps = request.minBitrateKbps;
    decision.m_maxBitrateKbps = request.maxBitrateKbps;
    decision.m_bufferSizeKbits = request.bufferSizeKbits;
    decision.m_quality = request.quality;
    decision.m_preset = request.preset;
    return ::media::Result<MediaResolvedAudioTargetDecision>::success(std::move(decision));
}

const std::string& MediaResolvedAudioTargetDecision::codecName() const noexcept { return m_codecName; }
const MediaAudioProfile& MediaResolvedAudioTargetDecision::profile() const noexcept { return m_profile; }
int MediaResolvedAudioTargetDecision::sampleRate() const noexcept { return m_sampleRate; }
int MediaResolvedAudioTargetDecision::channels() const noexcept { return m_channels; }
const std::string& MediaResolvedAudioTargetDecision::channelLayout() const noexcept { return m_channelLayout; }
const std::string& MediaResolvedAudioTargetDecision::sourceSampleFormat() const noexcept { return m_sourceSampleFormat; }
MediaBranchMode MediaResolvedAudioTargetDecision::branchMode() const noexcept { return m_branchMode; }
MediaRateControlMode MediaResolvedAudioTargetDecision::rateControl() const noexcept { return m_rateControl; }
const std::optional<int>& MediaResolvedAudioTargetDecision::bitrateKbps() const noexcept { return m_bitrateKbps; }
const std::optional<int>& MediaResolvedAudioTargetDecision::minBitrateKbps() const noexcept { return m_minBitrateKbps; }
const std::optional<int>& MediaResolvedAudioTargetDecision::maxBitrateKbps() const noexcept { return m_maxBitrateKbps; }
const std::optional<int>& MediaResolvedAudioTargetDecision::bufferSizeKbits() const noexcept { return m_bufferSizeKbits; }
const std::optional<int>& MediaResolvedAudioTargetDecision::quality() const noexcept { return m_quality; }
const std::string& MediaResolvedAudioTargetDecision::preset() const noexcept { return m_preset; }

} // namespace media::ffmpeg::graph
