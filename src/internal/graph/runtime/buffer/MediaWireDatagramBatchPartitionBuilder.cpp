#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaWireDatagramBatchPartitionBuilder::
MediaWireDatagramBatchPartitionBuilder(
    std::string sessionKey,
    std::string serviceScopeId,
    std::uint64_t generation,
    MediaDatagramBatchPlan batchPlan) noexcept
    : m_sessionKey(std::move(sessionKey)),
      m_serviceScopeId(std::move(serviceScopeId)),
      m_generation(generation),
      m_batchPlan(batchPlan)
{
}

::media::Result<MediaWireDatagramBatchPartitionBuilder>
MediaWireDatagramBatchPartitionBuilder::create(
    const std::string& sessionKey,
    const std::string& serviceScopeId,
    std::uint64_t generation,
    const MediaDatagramBatchPlan& batchPlan)
{
    using Result = ::media::Result<MediaWireDatagramBatchPartitionBuilder>;
    if (sessionKey.empty() || serviceScopeId.empty() || generation == 0 ||
        batchPlan.maximumDatagrams == 0 || batchPlan.maximumBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire batch partition builder requires service identity and planner batch bounds"));
    }
    try {
        return Result::success(MediaWireDatagramBatchPartitionBuilder(
            sessionKey, serviceScopeId, generation, batchPlan));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "wire batch partition builder identity"));
    }
}

::media::Status MediaWireDatagramBatchPartitionBuilder::beginPartition()
{
    auto created = MediaWireDatagramBatchBuilder::create(
        m_sessionKey, m_serviceScopeId, m_generation);
    if (!created) return ::media::Status::failure(created.error());
    m_current.emplace(std::move(created).value());
    m_currentDatagrams = 0;
    m_currentBytes = 0;
    m_currentDeadline.reset();
    return ::media::Status::success();
}

::media::Status MediaWireDatagramBatchPartitionBuilder::finishPartition()
{
    if (!m_current || m_currentDatagrams == 0 || m_currentBytes == 0 ||
        !m_currentDeadline) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "wire batch partition builder has no complete partition"));
    }
    auto finished = m_current->finish();
    if (!finished) return ::media::Status::failure(finished.error());
    try {
        m_partitions.push_back(std::move(finished).value());
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "wire batch partition collection"));
    }
    m_current.reset();
    m_currentDatagrams = 0;
    m_currentBytes = 0;
    m_currentDeadline.reset();
    return ::media::Status::success();
}

::media::Status MediaWireDatagramBatchPartitionBuilder::append(
    std::span<const std::uint8_t> bytes,
    std::uint64_t endpointId,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline,
    std::uint64_t globalSequence,
    MediaDatagramSubmitCommitLease commitLease)
{
    if (m_finished || bytes.empty() ||
        bytes.size() > m_batchPlan.maximumBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "wire datagram cannot fit one planner-owned batch partition"));
    }
    const auto bytes64 = static_cast<std::uint64_t>(bytes.size());
    const bool deadlineChanged =
        m_currentDeadline && *m_currentDeadline != canonicalDeadline;
    const bool countFull =
        m_currentDatagrams == m_batchPlan.maximumDatagrams;
    const bool bytesFull = m_currentBytes >
        m_batchPlan.maximumBytes - bytes64;
    if (m_current && (deadlineChanged || countFull || bytesFull)) {
        auto finished = finishPartition();
        if (!finished) return finished;
    }
    if (!m_current) {
        auto begun = beginPartition();
        if (!begun) return begun;
    }
    auto appended = m_current->append(
        bytes, endpointId, canonicalRelease, canonicalDeadline,
        globalSequence, std::move(commitLease));
    if (!appended) return appended;
    ++m_currentDatagrams;
    m_currentBytes += bytes64;
    m_currentDeadline = canonicalDeadline;
    return ::media::Status::success();
}

::media::Result<MediaWireDatagramBatchCollection>
MediaWireDatagramBatchPartitionBuilder::finish()
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    if (m_finished || (!m_current && m_partitions.empty())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "wire batch partition builder can finish one nonempty collection"));
    }
    if (m_current) {
        auto finished = finishPartition();
        if (!finished) return Result::failure(finished.error());
    }
    m_finished = true;
    return Result::success(std::move(m_partitions));
}

} // namespace media::ffmpeg::graph
