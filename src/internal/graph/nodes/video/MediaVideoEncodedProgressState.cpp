#include "internal/graph/nodes/video/MediaVideoEncodedProgressState.h"
#include "internal/graph/planner/capability/MediaEncoderCompletionAdapter.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <limits>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }
}

void MediaVideoEncodedProgressState::revoke() noexcept
{
    suspendForReset();
    m_prepared = nullptr;
    m_context = nullptr;
    m_resetPending = false;
}

void MediaVideoEncodedProgressState::suspendForReset() noexcept
{
    m_generation.reset();
    m_timeline.reset();
    m_inputLowerBound.reset();
    m_submitted = m_completed = 0;
    m_resetPending = m_prepared != nullptr;
}

::media::Status MediaVideoEncodedProgressState::resumeAfterReset()
{
    if (!m_resetPending) return ::media::Status::success();
    if (!m_prepared || !m_context)
        return invalid("Encoded progress reset lost its prepared codec owner");
    auto valid = MediaEncoderCompletionAdapter::validateUnchanged(*m_context, *m_prepared);
    if (!valid) return valid;
    m_resetPending = false;
    return ::media::Status::success();
}

::media::Status MediaVideoEncodedProgressState::bind(
    const MediaVideoEncoderCompletionContract* prepared, const AVCodecContext& context)
{
    revoke();
    if (!prepared) return ::media::Status::success();
    if (prepared->lifetime != MediaEncoderCompletionLifetime::UnchangedPreparedContextUntilReset)
        return invalid("Encoded progress has an unsupported prepared lifetime");
    m_prepared = prepared;
    m_context = &context;
    auto valid = MediaEncoderCompletionAdapter::validateUnchanged(context, *prepared);
    if (!valid) revoke();
    return valid;
}

::media::Status MediaVideoEncodedProgressState::validateConfiguration(const AVCodecContext& c) const
{
    if (m_resetPending) return invalid("Encoded progress reset requires owner revalidation");
    if (!m_prepared || !m_generation) return ::media::Status::success();
    if (&c != m_context) return invalid("Encoded progress codec owner changed");
    return MediaEncoderCompletionAdapter::validateUnchanged(c, *m_prepared);
}

::media::Result<MediaVideoEncodeLineage> MediaVideoEncodedProgressState::prepare(
    std::shared_ptr<const MediaCanonicalLineage> canonical,
    std::optional<MediaVideoPresentationProgress> progress)
{
    using Result = ::media::Result<MediaVideoEncodeLineage>;
    if (m_resetPending) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Encoded progress input precedes reset completion"));
    if (!canonical) return Result::failure(::media::ErrorInfo::invalidArgument("Encoded progress lacks lineage"));
    if (!m_prepared) return Result::success({std::move(canonical), std::nullopt, std::nullopt});
    if (!progress && !m_generation)
        return Result::success({std::move(canonical), std::nullopt, std::nullopt});
    if (auto valid = MediaEncoderCompletionAdapter::validateUnchanged(*m_context, *m_prepared); !valid)
        return Result::failure(valid.error());
    if (canonical->decodeOrder != MediaDecodeOrderMode::PresentationOrderNoReorder ||
        (canonical->decode && *canonical->decode != canonical->presentation) ||
        (m_generation && *m_generation != canonical->generation) ||
        (m_timeline && !sameMediaCanonicalTimelineIdentity(*m_timeline, canonical->identity)) ||
        (m_inputLowerBound && canonical->presentation < *m_inputLowerBound) ||
        (progress && progress->followingFramesLowerBound < canonical->presentation) ||
        m_submitted == (std::numeric_limits<std::uint64_t>::max)())
        return Result::failure(::media::ErrorInfo::invalidArgument("Encoded producer progress violates its prepared timeline"));
    m_generation = canonical->generation;
    m_timeline = canonical->identity;
    if (progress) m_inputLowerBound = progress->followingFramesLowerBound;
    return Result::success({std::move(canonical), progress, ++m_submitted});
}

::media::Result<std::optional<MediaVideoDecodeProgress>> MediaVideoEncodedProgressState::complete(
    const MediaVideoEncodeLineage& lineage)
{
    using Result = ::media::Result<std::optional<MediaVideoDecodeProgress>>;
    if (m_resetPending) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Encoded progress output precedes reset completion"));
    if (!m_prepared) return Result::success(std::nullopt);
    // Inputs preceding the first producer assertion have no progress to publish.
    if (!lineage.completionOrder && !lineage.presentationProgress)
        return Result::success(std::nullopt);
    if (!lineage.canonical || !m_generation || lineage.canonical->generation != *m_generation ||
        !m_timeline || !sameMediaCanonicalTimelineIdentity(*m_timeline, lineage.canonical->identity) ||
        !lineage.completionOrder || m_completed == (std::numeric_limits<std::uint64_t>::max)() ||
        *lineage.completionOrder != m_completed + 1 || *lineage.completionOrder > m_submitted)
        return Result::failure(::media::ErrorInfo::invalidArgument("Encoded complete AU violates prepared input order"));
    ++m_completed;
    if (!lineage.presentationProgress) return Result::success(std::nullopt);
    return Result::success(MediaVideoDecodeProgress{lineage.presentationProgress->followingFramesLowerBound});
}
}
