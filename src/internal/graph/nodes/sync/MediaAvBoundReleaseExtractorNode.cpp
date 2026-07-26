#include "internal/graph/nodes/sync/MediaAvBoundReleaseExtractorNode.h"

#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <array>
#include <span>
#include <sstream>

namespace media::ffmpeg::graph {

MediaAvBoundReleaseExtractorNode::MediaAvBoundReleaseExtractorNode(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey groupKey)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvBoundReleaseExtractorNode")
    , m_groupKey(std::move(groupKey))
{
}

MediaAvBoundReleaseExtractorNode::MediaAvBoundReleaseExtractorNode(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey groupKey,
    MediaAvStartupVideoPreparationCapability capability)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvBoundReleaseExtractorNode")
    , m_groupKey(std::move(groupKey))
    , m_preparationCapability(std::move(capability))
{
}

MediaNodeKind MediaAvBoundReleaseExtractorNode::staticKind() noexcept
{
    return MediaNodeKind::AvBoundReleaseExtractor;
}

::media::Status MediaAvBoundReleaseExtractorNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "video"), {}},
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "audio"), {}}};
    auto transaction = MediaAtomicOutputTransaction::acquire(
        "A/V bound release extractor", batches);
    if (!transaction) return ::media::Status::failure(transaction.error());
    if (!transaction.value()) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "A/V bound release extractor output validation would block"));
    }
    transaction.value().reset();
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaAvBoundReleaseExtractorNode::stop(
    MediaGraphExecutionContext& context)
{
    if (m_preparationCapability) m_preparationCapability->cancel();
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAvBoundReleaseExtractorNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    if (m_preparationCapability) m_preparationCapability->cancel();
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaAvBoundReleaseExtractorNode::resetState() noexcept
{
    m_pending.reset();
    m_preparationTransaction.reset();
    m_stagedVideo.clear();
    m_stagedAudio.clear();
    m_releaseStaged = false;
    m_prefixReservationPending = false;
    m_initialOutputReservation.reset();
    m_firstReleaseDiagnosticEmitted = false;
    m_firstCommitDiagnosticEmitted = false;
}

::media::Status MediaAvBoundReleaseExtractorNode::stageRelease(
    const MediaAvStartupReleaseBuffer& release,
    std::size_t firstVideoIndex,
    std::optional<MediaAudioPlaybackOrigin> audioOrigin)
{
    if (!m_firstReleaseDiagnosticEmitted) {
        std::ostringstream out;
        out << "av_release_trace stage=first_release generation="
            << release.epoch().generation
            << " video_count=" << release.video().size()
            << " audio_count=" << release.audio().size();
        if (!release.video().empty()) {
            const auto* first = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
                release.video().front().media.get());
            out << " first_video_sequence="
                << (first ? std::to_string(first->sourceSequence().value()) : "untyped");
        } else {
            out << " first_video_sequence=none";
        }
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                out.str());
        m_firstReleaseDiagnosticEmitted = true;
    }
    m_stagedVideo.clear();
    if (firstVideoIndex > release.video().size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "A/V bound release extractor rejects an invalid video prefix"));
    }
    m_stagedVideo.reserve(release.video().size() - firstVideoIndex);
    for (std::size_t index = firstVideoIndex;
         index < release.video().size(); ++index) {
        const auto& unit = release.video()[index];
        if (!unit.media) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor rejects null video units"));
        }
        m_stagedVideo.push_back(unit.media);
    }

    m_stagedAudio.clear();
    m_stagedAudio.reserve(release.audio().size());
    for (const auto& unit : release.audio()) {
        auto staged = MediaAvReleasedAudioBuffer::create(
            unit.media, unit.trimLeadingSamples,
            audioOrigin.value_or(release.audioOrigin()));
        if (!staged) {
            m_stagedVideo.clear();
            m_stagedAudio.clear();
            return ::media::Status::failure(staged.error());
        }
        m_stagedAudio.push_back(std::move(staged.value()));
    }
    m_releaseStaged = true;
    return ::media::Status::success();
}

::media::Result<MediaAvStartupReleaseDisposition>
MediaAvBoundReleaseExtractorNode::classifyRelease(
    MediaGraphExecutionContext& context,
    const MediaAvStartupReleaseBuffer& release) const
{
    if (release.groupKey() != m_groupKey) {
        return ::media::Result<
            MediaAvStartupReleaseDisposition>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor rejects a mismatched sync group"));
    }
    auto group = context.findAvSyncGroup(m_groupKey);
    if (!group || group->key() != m_groupKey) {
        return ::media::Result<
            MediaAvStartupReleaseDisposition>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V bound release extractor requires its planned sync group"));
    }
    return group->classifyStartupRelease(
        release.releaseKind(),
        release.epoch().generation,
        release.completedTransitionSequence());
}

::media::Result<MediaAvStartupReleaseDisposition>
MediaAvBoundReleaseExtractorNode::commit(
    MediaGraphExecutionContext& context,
    const MediaAvStartupReleaseBuffer& release)
{
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "video"), m_stagedVideo},
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "audio"), m_stagedAudio}};
    auto transaction = MediaAtomicOutputTransaction::acquire(
        "A/V bound release extractor", batches);
    if (!transaction) {
        return ::media::Result<
            MediaAvStartupReleaseDisposition>::failure(
            transaction.error());
    }
    if (!transaction.value()) {
        return ::media::Result<
            MediaAvStartupReleaseDisposition>::failure(
            ::media::ErrorInfo::wouldBlock(
                "A/V bound release extractor outputs are full"));
    }
    auto group = context.findAvSyncGroup(m_groupKey);
    if (!group || group->key() != m_groupKey) {
        return ::media::Result<
            MediaAvStartupReleaseDisposition>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V bound release extractor requires its planned sync group"));
    }
    auto publication = group->reserveStartupReleasePublication(
        release.releaseKind(),
        release.epoch().generation,
        release.completedTransitionSequence());
    if (!publication) {
        return ::media::Result<
            MediaAvStartupReleaseDisposition>::failure(
            publication.error());
    }
    const auto disposition = publication.value().disposition();
    if (disposition == MediaAvStartupReleaseDisposition::Publish) {
        transaction.value()->commitReserved();
        publication.value().completePublished();
    }
    return ::media::Result<MediaAvStartupReleaseDisposition>::success(
        disposition);
}

void MediaAvBoundReleaseExtractorNode::discardPendingRelease() noexcept
{
    m_pending.reset();
    m_preparationTransaction.reset();
    m_stagedVideo.clear();
    m_stagedAudio.clear();
    m_releaseStaged = false;
}

::media::Result<MediaNodeProcessResult>
MediaAvBoundReleaseExtractorNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_preparationCapability) {
        const auto preparation = m_preparationCapability->snapshot();
        const auto phase = preparation.phase;
        if (phase == MediaAvStartupVideoPreparationPhase::Awaiting ||
            phase == MediaAvStartupVideoPreparationPhase::Feeding ||
            m_prefixReservationPending ||
            (phase == MediaAvStartupVideoPreparationPhase::FilterReady &&
             !preparation.extractorOutputsReserved)) {
            return processPreparation(context);
        }
        if (phase == MediaAvStartupVideoPreparationPhase::Cancelled) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::cancelled(
                    "A/V bound release extractor preparation was cancelled"));
        }
        if (phase == MediaAvStartupVideoPreparationPhase::ReleaseCommitted &&
            m_initialOutputReservation) {
            const auto* transaction = dynamic_cast<
                const MediaStartupReleaseTransactionBuffer*>(
                    m_preparationTransaction.get());
            const auto* release =
                transaction ? transaction->release() : nullptr;
            if (!release) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "A/V bound release extractor lost its initial release authorization"));
            }
            std::optional<MediaAvStartupReleasePublicationReservation>
                publication;
            if (auto committed = m_initialOutputReservation->commit([&] {
                    auto group = context.findAvSyncGroup(m_groupKey);
                    if (!group || group->key() != m_groupKey) {
                        return ::media::Status::failure(
                            ::media::ErrorInfo::notInitialized(
                                "A/V bound release extractor requires its planned sync group"));
                    }
                    auto reserved =
                        group->reserveStartupReleasePublication(
                            release->releaseKind(),
                            release->epoch().generation,
                            release->completedTransitionSequence());
                    if (!reserved) {
                        return ::media::Status::failure(
                            reserved.error());
                    }
                    publication.emplace(std::move(reserved).value());
                    return publication->disposition() ==
                            MediaAvStartupReleaseDisposition::Publish
                        ? ::media::Status::success()
                        : ::media::Status::failure(
                              ::media::ErrorInfo::cancelled(
                                  "A/V bound release extractor initial release lost live playback authorization"));
                });
                !committed)
                return ::media::Result<MediaNodeProcessResult>::failure(
                    committed.error());
            publication->completePublished();
            logFirstCommit();
            m_initialOutputReservation.reset();
            m_stagedVideo.clear();
            m_stagedAudio.clear();
            m_releaseStaged = false;
            return processProgress();
        }
        if (phase == MediaAvStartupVideoPreparationPhase::FilterReady &&
            preparation.anchoredEpoch && preparation.anchoredAudioOrigin &&
            !preparation.extractorOutputsReanchored &&
            m_initialOutputReservation && m_preparationTransaction) {
            const auto* transaction = dynamic_cast<
                const MediaStartupReleaseTransactionBuffer*>(
                    m_preparationTransaction.get());
            const auto* release = transaction ? transaction->release() : nullptr;
            if (!release) return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V bound release extractor lost its anchored transaction"));
            if (auto staged = stageRelease(
                    *release, preparation.committedVideoUnits,
                    preparation.anchoredAudioOrigin); !staged) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    staged.error());
            }
            const std::array<MediaAtomicOutputBatch, 2> outputBatches{
                MediaAtomicOutputBatch{
                    context.findOutputChannel(nodeId(), "video"), m_stagedVideo},
                MediaAtomicOutputBatch{
                    context.findOutputChannel(nodeId(), "audio"), m_stagedAudio}};
            if (auto replaced = m_initialOutputReservation->replacePendingBatches(
                    outputBatches); !replaced) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    replaced.error());
            }
            if (auto acknowledged =
                    m_preparationCapability->acknowledgeExtractorReanchor(
                        release->epoch().generation,
                        transaction->releaseIdentity()); !acknowledged) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    acknowledged.error());
            }
            return processWaiting();
        }
        return processBoundRelease(context);
    }

    if (!m_pending) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "in"),
            "A/V bound release extractor", "in");
        if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
        if (!input.value()) return processWaiting();
        m_pending = std::move(*input.value());
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            m_pending.get())) {
        if (control->controlKind() == MediaControlBufferKind::Unknown) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V bound release extractor rejects unknown control"));
        }
        const std::array<MediaAtomicOutputBatch, 2> batches{
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "video"),
                std::span(&m_pending, 1)},
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "audio"),
                std::span(&m_pending, 1)}};
        auto transaction = MediaAtomicOutputTransaction::acquire(
            "A/V bound release extractor", batches);
        if (!transaction) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                transaction.error());
        }
        if (!transaction.value()) return processWaiting();
        if (auto committed = transaction.value()->commit(); !committed) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                committed.error());
        }
        const bool finished = control->controlKind() == MediaControlBufferKind::Eof ||
                              control->controlKind() == MediaControlBufferKind::Abort;
        m_pending.reset();
        return finished ? processFinished() : processProgress();
    }
    const auto* release = dynamic_cast<const MediaAvStartupReleaseBuffer*>(m_pending.get());
    if (!release) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor requires a startup release"));
    }
    auto classified = classifyRelease(context, *release);
    if (!classified) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            classified.error());
    }
    if (classified.value() == MediaAvStartupReleaseDisposition::DropOld) {
        discardPendingRelease();
        return processProgress();
    }
    if (classified.value() == MediaAvStartupReleaseDisposition::Withhold) {
        return processWaiting();
    }
    if (classified.value() == MediaAvStartupReleaseDisposition::Reject) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor rejects a release outside the live playback epoch"));
    }
    if (!m_releaseStaged) {
        if (auto status = stageRelease(*release); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }
    auto committed = commit(context, *release);
    if (!committed) {
        if (committed.error().code == ::media::ErrorCode::WouldBlock) {
            return processWaiting();
        }
        return ::media::Result<MediaNodeProcessResult>::failure(committed.error());
    }
    if (committed.value() == MediaAvStartupReleaseDisposition::DropOld) {
        discardPendingRelease();
        return processProgress();
    }
    if (committed.value() == MediaAvStartupReleaseDisposition::Withhold) {
        return processWaiting();
    }
    if (committed.value() == MediaAvStartupReleaseDisposition::Reject) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor lost live playback authorization"));
    }
    logFirstCommit();
    discardPendingRelease();
    return processProgress();
}

void MediaAvBoundReleaseExtractorNode::logFirstCommit()
{
    if (m_firstCommitDiagnosticEmitted) return;
    std::ostringstream out;
    out << "av_release_trace stage=first_commit video_count="
        << m_stagedVideo.size()
        << " audio_count=" << m_stagedAudio.size();
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            out.str());
    m_firstCommitDiagnosticEmitted = true;
}

::media::Result<MediaNodeProcessResult>
MediaAvBoundReleaseExtractorNode::processPreparation(
    MediaGraphExecutionContext& context)
{
    if (!m_preparationTransaction) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "preparation"),
            "A/V bound release extractor", "preparation");
        if (!input) return ::media::Result<MediaNodeProcessResult>::failure(
            input.error());
        if (!input.value()) return processWaiting();
        m_preparationTransaction = std::move(*input.value());
        const auto* transaction = dynamic_cast<
            const MediaStartupReleaseTransactionBuffer*>(
                m_preparationTransaction.get());
        const auto* release = transaction ? transaction->release() : nullptr;
        if (!release || release->releaseKind() !=
                MediaAvStartupReleaseKind::InitialAtomicRelease) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V bound release extractor preparation requires an initial transaction"));
        }
        if (auto begun = m_preparationCapability->begin(
                release->epoch().generation, transaction->releaseIdentity(),
                release->video().size()); !begun) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                begun.error());
        }
    }

    const auto* transaction = dynamic_cast<
        const MediaStartupReleaseTransactionBuffer*>(
            m_preparationTransaction.get());
    const auto* release = transaction ? transaction->release() : nullptr;
    if (!release) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor lost its preparation transaction"));
    }
    auto reservation = m_preparationCapability->reserveNextVideoUnit(
        release->epoch().generation, transaction->releaseIdentity());
    if (!reservation) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            reservation.error());
    }
    if (reservation.value().kind ==
            MediaAvStartupVideoReservationKind::NoReservation) {
        const auto snapshot = m_preparationCapability->snapshot();
        if (snapshot.phase != MediaAvStartupVideoPreparationPhase::FilterReady)
            return processWaiting();
        if (!m_releaseStaged) {
            if (auto staged = stageRelease(
                    *release, snapshot.committedVideoUnits); !staged) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    staged.error());
            }
        }
        const std::array<MediaAtomicOutputBatch, 2> outputBatches{
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "video"), m_stagedVideo},
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "audio"), m_stagedAudio}};
        auto outputs = MediaReservedOutputTransaction::reserve(
            "A/V bound release extractor initial outputs", outputBatches);
        if (!outputs) return ::media::Result<MediaNodeProcessResult>::failure(
            outputs.error());
        if (!outputs.value()) return processWaiting();
        m_initialOutputReservation.emplace(std::move(*outputs.value()));
        if (auto registered = m_preparationCapability->registerExtractorOutputs(
                release->epoch().generation, transaction->releaseIdentity(),
                m_initialOutputReservation->handle()); !registered) {
            m_initialOutputReservation.reset();
            return ::media::Result<MediaNodeProcessResult>::failure(
                registered.error());
        }
        return processWaiting();
    }
    const std::size_t index = *reservation.value().index;
    m_prefixReservationPending = true;
    if (index >= release->video().size() || !release->video()[index].media) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor preparation index is invalid"));
    }
    const MediaBufferRef reserved = release->video()[index].media;
    const std::array<MediaAtomicOutputBatch, 1> batches{
        MediaAtomicOutputBatch{
            context.findOutputChannel(nodeId(), "video"),
            std::span(&reserved, 1)}};
    auto output = MediaAtomicOutputTransaction::acquire(
        "A/V bound release extractor preparation", batches);
    if (!output) return ::media::Result<MediaNodeProcessResult>::failure(
        output.error());
    if (!output.value()) return processWaiting();
    if (auto ownership = m_preparationCapability->commitVideoUnit(
            release->epoch().generation, transaction->releaseIdentity(),
            index); !ownership) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ownership.error());
    }
    m_prefixReservationPending = false;
    if (auto committed = output.value()->commit(); !committed) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            committed.error());
    }
    return processProgress();
}

::media::Result<MediaNodeProcessResult>
MediaAvBoundReleaseExtractorNode::processBoundRelease(
    MediaGraphExecutionContext& context)
{
    if (!m_pending) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "bound_release"),
            "A/V bound release extractor", "bound_release");
        if (!input) return ::media::Result<MediaNodeProcessResult>::failure(
            input.error());
        if (!input.value()) return processWaiting();
        m_pending = std::move(*input.value());
    }
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            m_pending.get())) {
        if (control->controlKind() == MediaControlBufferKind::Unknown) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V bound release extractor rejects unknown bound control"));
        }
        const std::array<MediaAtomicOutputBatch, 2> batches{
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "video"),
                std::span(&m_pending, 1)},
            MediaAtomicOutputBatch{
                context.findOutputChannel(nodeId(), "audio"),
                std::span(&m_pending, 1)}};
        auto atomic = MediaAtomicOutputTransaction::acquire(
            "A/V bound release extractor bound control", batches);
        if (!atomic) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                atomic.error());
        }
        if (!atomic.value()) return processWaiting();
        if (auto committed = atomic.value()->commit(); !committed) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                committed.error());
        }
        const bool finished =
            control->controlKind() == MediaControlBufferKind::Eof ||
            control->controlKind() == MediaControlBufferKind::Abort;
        m_pending.reset();
        return finished ? processFinished() : processProgress();
    }
    const auto* transaction = dynamic_cast<
        const MediaStartupReleaseTransactionBuffer*>(m_pending.get());
    const auto* release = transaction ? transaction->release() : nullptr;
    if (!release) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor bound input requires a release transaction"));
    }
    std::size_t firstVideoIndex = 0;
    if (release->releaseKind() ==
        MediaAvStartupReleaseKind::InitialAtomicRelease) {
        const auto snapshot = m_preparationCapability->snapshot();
        if (snapshot.phase !=
                MediaAvStartupVideoPreparationPhase::ReleaseCommitted ||
            snapshot.generation != release->epoch().generation ||
            snapshot.releaseIdentity != transaction->releaseIdentity()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V bound release extractor rejects mismatched initial release identity"));
        }
        m_pending.reset();
        m_preparationTransaction.reset();
        return processProgress();
    }
    auto classified = classifyRelease(context, *release);
    if (!classified) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            classified.error());
    }
    if (classified.value() == MediaAvStartupReleaseDisposition::DropOld) {
        discardPendingRelease();
        return processProgress();
    }
    if (classified.value() == MediaAvStartupReleaseDisposition::Withhold) {
        return processWaiting();
    }
    if (classified.value() == MediaAvStartupReleaseDisposition::Reject) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor rejects an unbound playback generation"));
    }
    if (!m_releaseStaged) {
        if (auto status = stageRelease(
                *release, firstVideoIndex); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                status.error());
        }
    }
    auto committed = commit(context, *release);
    if (!committed) {
        return committed.error().code == ::media::ErrorCode::WouldBlock
            ? processWaiting()
            : ::media::Result<MediaNodeProcessResult>::failure(
                  committed.error());
    }
    if (committed.value() == MediaAvStartupReleaseDisposition::DropOld) {
        discardPendingRelease();
        return processProgress();
    }
    if (committed.value() == MediaAvStartupReleaseDisposition::Withhold) {
        return processWaiting();
    }
    if (committed.value() == MediaAvStartupReleaseDisposition::Reject) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V bound release extractor lost live playback authorization"));
    }
    discardPendingRelease();
    return processProgress();
}

} // namespace media::ffmpeg::graph
