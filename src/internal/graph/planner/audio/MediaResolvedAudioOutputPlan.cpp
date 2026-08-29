#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<MediaResolvedAudioOutputPlan> MediaResolvedAudioOutputPlan::create(
    const MediaResolvedAudioTargetDecision& target,
    const std::optional<MediaSelectedAudioEncoder>& selectedEncoder,
    std::optional<int> copiedAccessUnitSamples)
{
    const bool copy = target.branchMode() == MediaBranchMode::CopyPacket;
    if (copy && selectedEncoder) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "copy audio output must not bind an encoder"));
    }
    if (!copy && (!selectedEncoder || selectedEncoder->name.empty() || selectedEncoder->sampleFormat.empty())) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("transcode audio output requires complete selected encoder"));
    }
    if (!copy && !selectedEncoder->supportedSampleRates.empty() &&
        std::find(selectedEncoder->supportedSampleRates.begin(),
                  selectedEncoder->supportedSampleRates.end(), target.sampleRate()) ==
            selectedEncoder->supportedSampleRates.end()) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::unsupported("selected audio encoder does not support resolved sample rate"));
    }
    if (!copy && target.profile().knowledge() == MediaAudioProfileKnowledge::Known &&
        selectedEncoder->supportedProfileIds.empty()) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "selected audio encoder has no executable profile capability"));
    }
    if (!copy && target.profile().knowledge() == MediaAudioProfileKnowledge::Known &&
        std::find(selectedEncoder->supportedProfileIds.begin(),
                  selectedEncoder->supportedProfileIds.end(), target.profile().ffmpegProfileId()) ==
            selectedEncoder->supportedProfileIds.end()) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::unsupported("selected audio encoder does not support resolved profile"));
    }

    if ((copy && (!copiedAccessUnitSamples || *copiedAccessUnitSamples <= 0)) ||
        (!copy && copiedAccessUnitSamples)) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "resolved audio output requires branch-owned access-unit timing"));
    }
    MediaResolvedAudioOutputPlan plan;
    plan.m_codecName = target.codecName();
    plan.m_profile = target.profile();
    plan.m_sampleRate = target.sampleRate();
    plan.m_channels = target.channels();
    plan.m_channelLayout = target.channelLayout();
    plan.m_sampleFormat = copy ? target.sourceSampleFormat() : selectedEncoder->sampleFormat;
    if (plan.m_sampleFormat.empty()) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument("resolved audio output requires sample format"));
    }
    plan.m_branchMode = copy ? MediaBranchMode::CopyPacket : MediaBranchMode::TranscodeFrame;
    if (!copy) plan.m_encoderName = selectedEncoder->name;
    plan.m_codecFrameSamples = copy
        ? *copiedAccessUnitSamples
        : selectedEncoder->frameSizeSamples;
    plan.m_encoderDelaySamples = copy ? 0 : selectedEncoder->delaySamples;
    if ((!copy && plan.m_codecFrameSamples <= 0) ||
        plan.m_encoderDelaySamples < 0) {
        return ::media::Result<MediaResolvedAudioOutputPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "resolved audio output lacks bounded codec timing facts"));
    }
    plan.m_rateControl = target.rateControl();
    plan.m_bitrateKbps = target.bitrateKbps();
    plan.m_minBitrateKbps = target.minBitrateKbps();
    plan.m_maxBitrateKbps = target.maxBitrateKbps();
    plan.m_bufferSizeKbits = copy
        ? std::nullopt
        : selectedEncoder->bufferSizeKbits;
    plan.m_quality = target.quality();
    plan.m_preset = target.preset();
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
int MediaResolvedAudioOutputPlan::codecFrameSamples() const noexcept { return m_codecFrameSamples; }
int MediaResolvedAudioOutputPlan::encoderDelaySamples() const noexcept { return m_encoderDelaySamples; }

} // namespace media::ffmpeg::graph
