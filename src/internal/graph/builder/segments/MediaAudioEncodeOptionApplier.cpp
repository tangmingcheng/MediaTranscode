#include "internal/graph/builder/segments/MediaAudioEncodeOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaAudioEncodeOptionApplier";

::media::Result<void> setOption(MediaGraph& graph,
                                 MediaNodeId nodeId,
                                 const std::string& key,
                                 const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setIfPresent(MediaGraph& graph,
                                    MediaNodeId nodeId,
                                    const std::string& key,
                                    const std::optional<int>& value)
{
    if (!value) {
        return ::media::Result<void>::success();
    }
    return setOption(graph, nodeId, key, std::to_string(*value));
}

::media::Result<void> setIfNotEmpty(MediaGraph& graph,
                                     MediaNodeId nodeId,
                                     const std::string& key,
                                     const std::string& value)
{
    if (value.empty()) {
        return ::media::Result<void>::success();
    }
    return setOption(graph, nodeId, key, value);
}

::media::Result<void> validateOptionalNonNegative(const std::optional<int>& value,
                                                   const std::string& name)
{
    if (value && *value < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeOptionApplier requires non-negative " + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateOptionalPositive(const std::optional<int>& value,
                                                const std::string& name)
{
    if (value && *value <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeOptionApplier requires positive " + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateAudioOptions(const MediaAudioTranscodeParameters& audio)
{
    if (auto status = validateOptionalNonNegative(audio.bitrateKbps, "audio bitrate"); !status) return status;
    if (auto status = validateOptionalNonNegative(audio.minBitrateKbps, "audio min bitrate"); !status) return status;
    if (auto status = validateOptionalNonNegative(audio.maxBitrateKbps, "audio max bitrate"); !status) return status;
    if (auto status = validateOptionalPositive(audio.bufferSizeKbits, "audio buffer size"); !status) return status;
    if (auto status = validateOptionalPositive(audio.sampleRate, "audio sample rate"); !status) return status;
    if (auto status = validateOptionalPositive(audio.channels, "audio channels"); !status) return status;
    if (auto status = validateOptionalNonNegative(audio.quality, "audio quality"); !status) return status;
    if (audio.minBitrateKbps && audio.maxBitrateKbps && *audio.minBitrateKbps > *audio.maxBitrateKbps) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaAudioEncodeOptionApplier requires audio min bitrate <= max bitrate"));
    }
    return ::media::Result<void>::success();
}

} // namespace

::media::Result<void> MediaAudioEncodeOptionApplier::applyCodecResolverOptions(
    MediaGraph& graph,
    MediaNodeId codecResolver,
    const MediaAudioTranscodeParameters& audio)
{
    if (auto status = validateAudioOptions(audio); !status) return status;
    if (auto status = setOption(graph, codecResolver, MediaTranscodeOptionKey::AudioRateControl, mediaRateControlModeName(audio.rateControl)); !status) return status;
    if (auto status = setIfPresent(graph, codecResolver, MediaTranscodeOptionKey::AudioBitrateKbps, audio.bitrateKbps); !status) return status;
    if (auto status = setIfPresent(graph, codecResolver, MediaTranscodeOptionKey::AudioMinBitrateKbps, audio.minBitrateKbps); !status) return status;
    if (auto status = setIfPresent(graph, codecResolver, MediaTranscodeOptionKey::AudioMaxBitrateKbps, audio.maxBitrateKbps); !status) return status;
    if (auto status = setIfPresent(graph, codecResolver, MediaTranscodeOptionKey::AudioBufferSizeKbits, audio.bufferSizeKbits); !status) return status;
    if (auto status = setIfPresent(graph, codecResolver, MediaTranscodeOptionKey::AudioSampleRate, audio.sampleRate); !status) return status;
    if (auto status = setIfPresent(graph, codecResolver, MediaTranscodeOptionKey::AudioChannels, audio.channels); !status) return status;
    if (auto status = setIfPresent(graph, codecResolver, MediaTranscodeOptionKey::AudioQuality, audio.quality); !status) return status;
    if (auto status = setIfNotEmpty(graph, codecResolver, MediaTranscodeOptionKey::AudioPreset, audio.preset); !status) return status;
    return setIfNotEmpty(graph, codecResolver, MediaTranscodeOptionKey::AudioProfile, audio.profile);
}

} // namespace media::ffmpeg::graph
