#pragma once

#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

namespace media::ffmpeg::graph {

enum class MediaAvStartupVideoPreparationRole {
    ExtractorFeed,
    FilterReadiness,
    SequencerActivation
};

class MediaAvStartupVideoPreparationCapability final {
public:
    static ::media::Result<MediaAvStartupVideoPreparationCapability> issue(
        std::shared_ptr<MediaAvStartupVideoPreparationState> state,
        MediaAvStartupVideoPreparationRole role)
    {
        if (!state) {
            return ::media::Result<MediaAvStartupVideoPreparationCapability>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Video preparation capability requires shared state"));
        }
        return ::media::Result<MediaAvStartupVideoPreparationCapability>::success(
            MediaAvStartupVideoPreparationCapability(
                std::move(state), role));
    }

    MediaAvStartupVideoPreparationCapability(
        MediaAvStartupVideoPreparationCapability&&) noexcept = default;
    MediaAvStartupVideoPreparationCapability& operator=(
        MediaAvStartupVideoPreparationCapability&&) noexcept = default;
    MediaAvStartupVideoPreparationCapability(
        const MediaAvStartupVideoPreparationCapability&) = delete;
    MediaAvStartupVideoPreparationCapability& operator=(
        const MediaAvStartupVideoPreparationCapability&) = delete;

    const void* stateIdentity() const noexcept { return m_state.get(); }
    MediaAvStartupVideoPreparationSnapshot snapshot() const
    {
        return m_state->snapshot();
    }
    ::media::Status begin(std::uint64_t generation,
                          std::uint64_t releaseIdentity,
                          std::size_t count)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::ExtractorFeed)
            return wrongRole();
        return m_state->begin(generation, releaseIdentity, count);
    }
    MediaAvStartupVideoPreparationState::VideoReservation reserveNextVideoUnit(
        std::uint64_t generation, std::uint64_t releaseIdentity)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::ExtractorFeed) {
            return MediaAvStartupVideoPreparationState::VideoReservation::failure(
                wrongRole().error());
        }
        return m_state->reserveNextVideoUnit(generation, releaseIdentity);
    }
    ::media::Status commitVideoUnit(std::uint64_t generation,
                                    std::uint64_t releaseIdentity,
                                    std::size_t index)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::ExtractorFeed)
            return wrongRole();
        return m_state->commitVideoUnit(generation, releaseIdentity, index);
    }
    ::media::Status markFilterReady(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        MediaOutputCapacityReservationHandle reservation)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::FilterReadiness)
            return wrongRole();
        return m_state->markFilterReady(
            generation, releaseIdentity, std::move(reservation));
    }
    ::media::Status registerExtractorOutputs(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        MediaOutputCapacityReservationHandle reservation)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::ExtractorFeed)
            return wrongRole();
        return m_state->registerExtractorOutputs(
            generation, releaseIdentity, std::move(reservation));
    }
    ::media::Status authorizeRelease(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        const MediaReservedOutputTransaction::Authorization& activation)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::SequencerActivation)
            return wrongRole();
        return m_state->authorizeRelease(
            generation, releaseIdentity, activation);
    }
    ::media::Status publishInitialAnchor(
        std::uint64_t generation,
        std::uint64_t releaseIdentity,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::SequencerActivation)
            return wrongRole();
        return m_state->publishInitialAnchor(
            generation, releaseIdentity, epoch, audioOrigin);
    }
    ::media::Status acknowledgeExtractorReanchor(
        std::uint64_t generation,
        std::uint64_t releaseIdentity)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::ExtractorFeed)
            return wrongRole();
        return m_state->acknowledgeExtractorReanchor(
            generation, releaseIdentity);
    }
    ::media::Status cancel() { return m_state->cancel(); }
    ::media::Status bindSequencerWakeup(
        const std::shared_ptr<MediaNodeWakeup>& wakeup)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::SequencerActivation)
            return wrongRole();
        return m_state->bindSequencerWakeup(wakeup);
    }
    ::media::Status bindFilterWakeup(
        const std::shared_ptr<MediaNodeWakeup>& wakeup)
    {
        if (m_role != MediaAvStartupVideoPreparationRole::FilterReadiness)
            return wrongRole();
        return m_state->bindFilterWakeup(wakeup);
    }

private:
    MediaAvStartupVideoPreparationCapability(
        std::shared_ptr<MediaAvStartupVideoPreparationState> state,
        MediaAvStartupVideoPreparationRole role)
        : m_state(std::move(state)), m_role(role) {}
    static ::media::Status wrongRole()
    {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Video preparation capability rejects operation for this role"));
    }

    std::shared_ptr<MediaAvStartupVideoPreparationState> m_state;
    MediaAvStartupVideoPreparationRole m_role;
};

} // namespace media::ffmpeg::graph
