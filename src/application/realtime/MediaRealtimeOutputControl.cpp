#include "application/realtime/MediaRealtimeVideoRunController.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<std::uint64_t> MediaRealtimeVideoRunControl::addOutput(
    const MediaRealtimeVideoOutputRequest& request)
{
    using Result = ::media::Result<std::uint64_t>;
    std::lock_guard lock(m_waitMutex);
    if (!m_outputChangesEnabled || stopRequested()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Output changes require a running video session"));
    }
    if (m_outputChangeInProgress || m_pendingOutputChange) {
        return Result::failure(::media::ErrorInfo::wouldBlock(
            "An output change is already in progress"));
    }
    if (m_nextOutputId == std::numeric_limits<std::uint64_t>::max()) {
        return Result::failure(::media::ErrorInfo::unsupported(
            "The session output identifier space is exhausted"));
    }
    MediaRealtimeOutputChange change{m_nextOutputId, request};
    m_outputSnapshots.push_back({
        change.outputId, MediaRealtimeOutputState::Preparing,
        MediaRealtimeOutputFailureStage::Planning, {}, {}, std::nullopt});
    m_pendingOutputChange.emplace(std::move(change));
    const auto outputId = m_nextOutputId++;
    m_waitCondition.notify_all();
    return Result::success(outputId);
}

::media::Status MediaRealtimeVideoRunControl::removeOutput(
    std::uint64_t outputId)
{
    std::lock_guard lock(m_waitMutex);
    if (!m_outputChangesEnabled || stopRequested()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Output changes require a running video session"));
    }
    if (m_outputChangeInProgress || m_pendingOutputChange) {
        return ::media::Status::failure(::media::ErrorInfo::wouldBlock(
            "An output change is already in progress"));
    }
    const auto output = std::find_if(
        m_outputSnapshots.begin(), m_outputSnapshots.end(),
        [outputId](const auto& snapshot) {
            return snapshot.outputId == outputId;
        });
    if (output == m_outputSnapshots.end()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "The output identifier does not belong to an active output"));
    }
    m_pendingOutputChange.emplace(
        MediaRealtimeOutputChange{outputId, std::nullopt});
    m_waitCondition.notify_all();
    return ::media::Status::success();
}

std::vector<MediaRealtimeOutputSnapshot>
MediaRealtimeVideoRunControl::outputSnapshots() const
{
    std::lock_guard lock(m_waitMutex);
    return m_outputSnapshots;
}

std::optional<MediaRealtimeOutputChange>
MediaRealtimeVideoRunControl::takeOutputChange()
{
    std::lock_guard lock(m_waitMutex);
    if (m_outputChangeInProgress || !m_pendingOutputChange) {
        return std::nullopt;
    }
    auto result = std::move(m_pendingOutputChange);
    m_pendingOutputChange.reset();
    m_outputChangeInProgress = true;
    return result;
}

void MediaRealtimeVideoRunControl::completeOutputChange() noexcept
{
    std::lock_guard lock(m_waitMutex);
    m_outputChangeInProgress = false;
}

void MediaRealtimeVideoRunControl::publishOutputSnapshot(
    MediaRealtimeOutputSnapshot snapshot)
{
    std::lock_guard lock(m_waitMutex);
    const auto output = std::find_if(
        m_outputSnapshots.begin(), m_outputSnapshots.end(),
        [&snapshot](const auto& current) {
            return current.outputId == snapshot.outputId;
        });
    if (output == m_outputSnapshots.end()) {
        return;
    }
    if (snapshot.state == MediaRealtimeOutputState::Retired) {
        m_outputSnapshots.erase(output);
    } else {
        *output = std::move(snapshot);
    }
}

::media::Result<std::uint64_t>
MediaRealtimeVideoRunControl::registerInitialOutput(
    const std::string& descriptionPath)
{
    using Result = ::media::Result<std::uint64_t>;
    std::lock_guard lock(m_waitMutex);
    if (!m_outputSnapshots.empty() || m_nextOutputId != 1) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "An input session can register its initial output only once"));
    }
    const auto outputId = m_nextOutputId;
    m_outputSnapshots.push_back({
        outputId, MediaRealtimeOutputState::Preparing,
        MediaRealtimeOutputFailureStage::Preparation, {}, descriptionPath,
        std::nullopt});
    ++m_nextOutputId;
    return Result::success(outputId);
}

void MediaRealtimeVideoRunControl::setOutputChangesEnabled(bool enabled)
{
    std::lock_guard lock(m_waitMutex);
    m_outputChangesEnabled = enabled;
}

} // namespace media::ffmpeg::graph
