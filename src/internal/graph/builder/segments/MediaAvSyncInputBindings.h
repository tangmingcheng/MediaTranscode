#pragma once
#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/runtime/factory/MediaAvRuntimeRegistrationPlan.h"
#include "internal/graph/model/MediaStartupRoutePlan.h"
#include "internal/graph/model/MediaRtpClockRoute.h"
#include "internal/graph/planner/realtime/MediaSourceInputConnectionsPlanner.h"
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {
// Populated only from actual node creation and explicit upstream endpoints.
struct MediaAvSyncSharedNodeBindings final {
    MediaNodeId sourceClock;
    MediaNodeId videoGenerationGate;
    std::optional<MediaNodeId> audioGenerationGate;
    MediaNodeId videoStartup;
    std::optional<MediaNodeId> audioCanonical;
    MediaNodeId coordinator;
    MediaNodeId startupClock;
    MediaNodeId epochBinder;
    MediaNodeId activationSequencer;
    MediaNodeId releaseExtractor;
    std::optional<MediaNodeId> routeNode(MediaStartupRouteRole role) const noexcept {
        switch (role) {
        case MediaStartupRouteRole::VideoInput: return videoStartup;
        case MediaStartupRouteRole::AudioInput: return audioCanonical;
        case MediaStartupRouteRole::Coordinator: return coordinator;
        case MediaStartupRouteRole::Binder: return epochBinder;
        case MediaStartupRouteRole::Sequencer: return activationSequencer;
        case MediaStartupRouteRole::Extractor: return releaseExtractor;
        }
        return std::nullopt;
    }
};

struct MediaRtpProtocolNodeBindings final {
    MediaAvRtpClockRegistration registered;
    MediaNodeId snapshotFanout;
    MediaNodeId stateAdapter;
    MediaNodeId routeNode(MediaRtpClockRouteRole role) const noexcept {
        using R = MediaRtpClockRouteRole;
        switch (role) {
        case R::VideoInput: return registered.videoRaw;
        case R::AudioInput: return registered.audioRaw.value_or(MediaNodeId::invalid());
        case R::Group: return registered.clockGroup;
        case R::Fanout: return snapshotFanout;
        case R::VideoBinder: return registered.videoBinder;
        case R::AudioBinder: return registered.audioBinder.value_or(MediaNodeId::invalid());
        case R::StateAdapter: return stateAdapter;
        case R::VideoControl: return registered.videoControl.value_or(MediaNodeId::invalid());
        case R::AudioControl: return registered.audioControl.value_or(MediaNodeId::invalid());
        }
        return MediaNodeId::invalid();
    }
};
struct MediaRealtimeAvSyncProtocolInputEndpoints final {
    MediaEndpoint video;
    MediaEndpoint audio;
    MediaEndpoint sourceClock;
    std::optional<MediaAvDemuxClockRegistration> demuxClock;
    std::vector<MediaNodeId> sourceMembers;
    std::optional<MediaRtpProtocolNodeBindings> rtp;
};
struct MediaAvSyncInputBindings final {
    MediaAvSyncSharedNodeBindings shared;
    MediaRealtimeAvSyncProtocolInputEndpoints protocol;
    MediaEndpoint endpoint(MediaSourceInputRole role, const MediaEndpoint& decoderFrame) const {
        using R = MediaSourceInputRole;
        switch (role) {
        case R::RawVideo: return {protocol.rtp ? protocol.rtp->registered.videoRaw : MediaNodeId::invalid(), "packet"};
        case R::RawAudio: return {protocol.rtp ? protocol.rtp->registered.audioRaw.value_or(MediaNodeId::invalid()) : MediaNodeId::invalid(), "packet"};
        case R::ProtocolVideo: return protocol.video;
        case R::ProtocolAudio: return protocol.audio;
        case R::ProtocolClock: return protocol.sourceClock;
        case R::SourceClock: return {shared.sourceClock, ""};
        case R::VideoGate: return {shared.videoGenerationGate, ""};
        case R::AudioGate: return {shared.audioGenerationGate.value_or(MediaNodeId::invalid()), ""};
        case R::VideoStartup: return {shared.videoStartup, ""};
        case R::AudioStartup: return {shared.audioCanonical.value_or(MediaNodeId::invalid()), ""};
        case R::StartupClock: return {shared.startupClock, ""};
        case R::Coordinator: return {shared.coordinator, ""};
        case R::DecoderFrame: return decoderFrame;
        }
        return {};
    }

};
} // namespace media::ffmpeg::graph
