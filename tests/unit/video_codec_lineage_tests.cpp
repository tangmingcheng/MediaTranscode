#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/MediaFfmpegCopyOpaqueCapability.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/nodes/video/VideoDecodeNode.h"
#include "internal/graph/nodes/video/VideoEncodeNode.h"
#include "internal/graph/nodes/video/VideoFilterNode.h"
#include "internal/graph/nodes/video/VideoFrameRateNode.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"
#include "internal/graph/sync/lineage/MediaVideoLineageDerivation.h"
#include "internal/graph/sync/lineage/MediaVideoFrameRateState.h"
#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#endif

using namespace media::ffmpeg::graph;

#undef assert
#define assert(condition)                                                        \
    do {                                                                         \
        if (!(condition)) {                                                      \
            std::cerr << "CHECK failed: " #condition << " at " << __LINE__      \
                      << '\n';                                                   \
            std::exit(1);                                                        \
        }                                                                        \
    } while (false)

namespace {

std::shared_ptr<const MediaCanonicalLineage> lineage(std::uint64_t generation,
                                                      std::uint64_t sequence)
{
    const auto timestamp = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(sequence) * 40'000'000);
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        timestamp,
        timestamp,
        MediaRunningTime::fromNanoseconds(40'000'000),
        MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
        "video-source",
        MediaSourceAccessUnitSequence(sequence),
        MediaTimeMappingConfidence::Locked,
        generation});
}

::media::Result<AVBufferRef*> submitOpaque(
    MediaCodecLineageRegistry& registry,
    std::shared_ptr<const MediaCanonicalLineage> value)
{
    auto submitted = registry.submit(std::move(value));
    if (!submitted) {
        return ::media::Result<AVBufferRef*>::failure(submitted.error());
    }
    return makeMediaFfmpegLineageOpaque(std::move(submitted).value());
}

void boundedRegistryRejectsInvalidAndUnresolvedTokens()
{
    auto created = MediaCodecLineageRegistry::create(2);
    assert(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(
        std::move(created).value());

    auto first = submitOpaque(*registry, lineage(7, 1));
    auto second = submitOpaque(*registry, lineage(7, 2));
    assert(first && second);
    assert(!submitOpaque(*registry, lineage(7, 3)));
    assert(!registry->resolve(first.value(), 8));
    assert(registry->resolve(first.value(), 7));
    AVBufferRef* firstCopy = av_buffer_ref(first.value());
    assert(firstCopy);
    assert(registry->resolve(firstCopy, 7)); // codec one-to-many
    av_buffer_unref(&first.value());
    assert(!submitOpaque(*registry, lineage(7, 3))); // copy still owns lease
    av_buffer_unref(&firstCopy);
    auto replacement = submitOpaque(*registry, lineage(7, 3));
    assert(replacement); // final ref release reclaimed one slot
    assert(registry->resolve(second.value(), 7));
    assert(registry->resolve(replacement.value(), 7));
    av_buffer_unref(&second.value());
    av_buffer_unref(&replacement.value());
    assert(registry->finishGeneration(7));
}

void sustainedResolvedLeasesRemainBounded()
{
    auto created = MediaCodecLineageRegistry::create(2);
    assert(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(
        std::move(created).value());
    for (std::uint64_t sequence = 1; sequence <= 1'000; ++sequence) {
        auto submitted = submitOpaque(*registry, lineage(17, sequence));
        assert(submitted);
        AVBufferRef* copiedOutput = av_buffer_ref(submitted.value());
        assert(copiedOutput);
        assert(registry->resolve(submitted.value(), 17));
        assert(registry->resolve(copiedOutput, 17));
        av_buffer_unref(&submitted.value());
        av_buffer_unref(&copiedOutput);
    }
    assert(registry->finishGeneration(17));
}

void purgeIsExactByGeneration()
{
    auto created = MediaCodecLineageRegistry::create(4);
    assert(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(
        std::move(created).value());
    auto oldToken = submitOpaque(*registry, lineage(3, 1));
    auto currentToken = submitOpaque(*registry, lineage(4, 2));
    assert(oldToken && currentToken);
    MediaAvGenerationPurge purge{3, 4, 1};
    assert(registry->purge(purge));
    assert(!registry->retainedOutputIsCurrent(*lineage(3, 3)));
    assert(registry->retainedOutputIsCurrent(*lineage(4, 4)));
    assert(!registry->resolve(oldToken.value(), 3));
    assert(registry->resolve(currentToken.value(), 4));
    av_buffer_unref(&oldToken.value());
    av_buffer_unref(&currentToken.value());
    assert(registry->finishGeneration(4));
}

void lateOpaqueReleaseIsSafeAfterPurgeAndRegistryDestruction()
{
    AVBufferRef* purgedOpaque = nullptr;
    {
        auto created = MediaCodecLineageRegistry::create(1);
        assert(created);
        auto registry = std::make_shared<MediaCodecLineageRegistry>(
            std::move(created).value());
        auto submitted = submitOpaque(*registry, lineage(61, 1));
        assert(submitted);
        purgedOpaque = submitted.value();
        assert(registry->purge({61, 62, 1}));
        assert(!registry->resolve(purgedOpaque, 61));

        auto blockedReplacement = submitOpaque(*registry, lineage(62, 2));
        assert(!blockedReplacement); // purge invalidates lookup but cannot reclaim a live lease
        av_buffer_unref(&purgedOpaque);
        auto replacement = submitOpaque(*registry, lineage(62, 2));
        assert(replacement);
        av_buffer_unref(&replacement.value());
        assert(registry->finishGeneration(62));
    }
    assert(!purgedOpaque);

    AVBufferRef* orphanedOpaque = nullptr;
    {
        auto created = MediaCodecLineageRegistry::create(1);
        assert(created);
        auto registry = std::make_shared<MediaCodecLineageRegistry>(
            std::move(created).value());
        auto submitted = submitOpaque(*registry, lineage(63, 1));
        assert(submitted);
        orphanedOpaque = submitted.value();
    }
    av_buffer_unref(&orphanedOpaque);
    assert(!orphanedOpaque);
}

void deterministicCodecSequencesPreserveStrictOwnership()
{
    auto created = MediaCodecLineageRegistry::create(8);
    assert(created);
    auto registry = std::make_shared<MediaCodecLineageRegistry>(
        std::move(created).value());

    // Decoder delay and reorder: the second submitted AU is emitted first.
    auto decodeFirst = submitOpaque(*registry, lineage(21, 1));
    auto decodeSecond = submitOpaque(*registry, lineage(21, 2));
    assert(decodeFirst && decodeSecond);
    auto resolvedSecond = registry->resolve(decodeSecond.value(), 21);
    auto resolvedFirst = registry->resolve(decodeFirst.value(), 21);
    assert(resolvedSecond && resolvedSecond.value()->sourceSequence.value() == 2);
    assert(resolvedFirst && resolvedFirst.value()->sourceSequence.value() == 1);
    av_buffer_unref(&decodeFirst.value());
    av_buffer_unref(&decodeSecond.value());
    assert(registry->finishGeneration(21));

    // Encoder delay and one-to-many packetization retain one owned token.
    auto encodeDelayed = submitOpaque(*registry, lineage(22, 3));
    assert(encodeDelayed);
    assert(!registry->finishGeneration(22));
    assert(registry->resolve(encodeDelayed.value(), 22));
    assert(registry->resolve(encodeDelayed.value(), 22));
    av_buffer_unref(&encodeDelayed.value());
    assert(registry->finishGeneration(22));

    // Interleaved generations remain explicit; residue in either generation
    // cannot be hidden by whichever generation arrived last.
    auto oldGeneration = submitOpaque(*registry, lineage(23, 4));
    auto nextGeneration = submitOpaque(*registry, lineage(24, 5));
    assert(oldGeneration && nextGeneration);
    assert(registry->resolve(nextGeneration.value(), 24));
    av_buffer_unref(&nextGeneration.value());
    assert(registry->finishGeneration(24));
    assert(!registry->finishGeneration(23));
    assert(registry->resolve(oldGeneration.value(), 23));
    av_buffer_unref(&oldGeneration.value());
    assert(registry->finishGeneration(23));
}

void opaqueTokensRejectMissingDuplicateAndGenerationMismatch()
{
    assert(!mediaFfmpegLineageToken(nullptr));
    auto created = MediaCodecLineageRegistry::create(2);
    assert(created);
    auto submitted = submitOpaque(created.value(), lineage(31, 1));
    assert(submitted);
    assert(!created.value().resolve(submitted.value(), 32));
    av_buffer_unref(&submitted.value());
}

void explicitRateConversionDerivesDeterministicSequences()
{
    const auto source = lineage(41, 9);
    const MediaRational milliseconds{1, 1'000};
    auto first = deriveMediaVideoLineage(*source, 100, 100, 20, milliseconds);
    auto second = deriveMediaVideoLineage(*source, 100, 120, 20, milliseconds);
    auto third = deriveMediaVideoLineage(*source, 100, 140, 20, milliseconds);
    assert(first && second && third);
    assert(first.value()->sourceSequence == source->sourceSequence);
    assert(second.value()->sourceSequence == source->sourceSequence);
    assert(third.value()->sourceSequence == source->sourceSequence);
    assert(first.value()->presentation == source->presentation);
    assert(second.value()->presentation.nanoseconds() -
               first.value()->presentation.nanoseconds() ==
           20'000'000);
    assert(third.value()->presentation.nanoseconds() -
               second.value()->presentation.nanoseconds() ==
           20'000'000);
    assert(first.value()->duration ==
           MediaRunningTime::fromNanoseconds(20'000'000));
}

void copyOpaqueCapabilityRejectsMissingOrIncompatibleRuntime()
{
    assert(!validateMediaFfmpegCopyOpaqueCapability(false, 61, 61));
    assert(!validateMediaFfmpegCopyOpaqueCapability(true, 61, 60));
    assert(validateMediaFfmpegCopyOpaqueCapability(true, 61, 61));
    assert(requireMediaFfmpegCopyOpaqueCapability());
}

void packetViewUnwrapsCanonicalPayloadWithoutCopy()
{
    auto packet = ::media::ffmpeg::makePacket();
    assert(packet);
    assert(av_new_packet(packet.get(), 16) == 0);
    AVPacket* identity = packet.get();
    auto wrapped = FFmpegBufferFactory::wrapPacket(
        std::move(packet), MediaStreamKind::Video, std::nullopt);
    assert(wrapped);
    MediaBufferRef raw = wrapped.value();
    auto expectedLineage = lineage(9, 1);
    auto canonical = MediaCanonicalAccessUnitBuffer::create(raw, expectedLineage);
    assert(canonical);
    assert(FFmpegPacketView::packet(canonical.value()) == identity);
    assert(FFmpegPacketView::writablePacket(canonical.value()) == identity);
    assert(FFmpegPacketView::canonicalLineage(canonical.value()) ==
           expectedLineage);
}

void frameViewUnwrapsCanonicalPayloadWithoutCopy()
{
    auto frame = ::media::ffmpeg::makeFrame();
    assert(frame);
    AVFrame* identity = frame.get();
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(frame), MediaStreamKind::Video);
    assert(wrapped);
    auto expectedLineage = lineage(10, 1);
    auto canonical = MediaCanonicalVideoFrameBuffer::create(
        wrapped.value(), expectedLineage);
    assert(canonical);
    assert(FFmpegFrameView::frame(canonical.value()) == identity);
    assert(FFmpegFrameView::writableFrame(canonical.value()) == identity);
    assert(FFmpegFrameView::canonicalLineage(canonical.value()) ==
           expectedLineage);
}

MediaNode lineageNode(MediaNodeKind kind,
                      std::string identity,
                      bool capacity = true)
{
    MediaNode node;
    node.id = MediaNodeId::fromValue(static_cast<std::uint32_t>(kind));
    node.kind = kind;
    node.name = "lineage.stage";
    if (capacity) node.options.set("video.lineage.capacity", "8");
    if (!identity.empty()) {
        node.options.set("video.lineage.identity", std::move(identity));
    }
    return node;
}

template <typename Node>
void assertStableTarget(MediaNodeKind kind, std::string_view identity)
{
    auto created = MediaRuntimeNodeFactory::create(
        lineageNode(kind, std::string(identity)));
    assert(created);
    auto* runtime = dynamic_cast<Node*>(created.value().get());
    assert(runtime);
    assert(runtime->generationPurgeIdentity() == identity);
    auto first = runtime->generationPurgeTarget();
    auto second = runtime->generationPurgeTarget();
    assert(first);
    assert(first.get() == second.get());
}

void productionStagesExposeExactStablePurgeTargets()
{
    assertStableTarget<VideoDecodeNode>(
        MediaNodeKind::VideoDecode, "video_decode");
    assertStableTarget<VideoFrameRateNode>(
        MediaNodeKind::VideoFrameRate, "video_frame_rate");
    assertStableTarget<VideoFilterNode>(
        MediaNodeKind::VideoFilter, "video_filter");
    assertStableTarget<VideoEncodeNode>(
        MediaNodeKind::VideoEncode, "video_encode");

    assert(!MediaRuntimeNodeFactory::create(lineageNode(
        MediaNodeKind::VideoDecode, {})));
    assert(!MediaRuntimeNodeFactory::create(lineageNode(
        MediaNodeKind::VideoDecode,
        "video_decode", false)));
    assert(!MediaRuntimeNodeFactory::create(lineageNode(
        MediaNodeKind::VideoDecode,
        "video_decoder_lineage_registry")));
}

void participantGroupRejectsMissingAndDuplicateStageTargets()
{
    MediaAvGenerationParticipantPlan plan{
        MediaAvGenerationParticipant::CanonicalLineage,
        {"video_decode", "video_frame_rate"}};
    auto created = MediaAvGenerationParticipantGroup::create(plan);
    assert(created);
    auto decode = MediaRuntimeNodeFactory::create(lineageNode(
        MediaNodeKind::VideoDecode, "video_decode"));
    assert(decode);
    auto* runtime = dynamic_cast<VideoDecodeNode*>(decode.value().get());
    assert(runtime);
    assert(created.value().registerChild(
        std::string(runtime->generationPurgeIdentity()),
        runtime->generationPurgeTarget()));
    assert(!created.value().registerChild(
        std::string(runtime->generationPurgeIdentity()),
        runtime->generationPurgeTarget()));
    assert(!created.value().seal());
}

void copyOpaqueOptionIsStrictAndSymmetric()
{
    MediaNodeOptions legacy;
    auto legacyResult = parseMediaVideoLineageCopyOpaqueOption(&legacy);
    assert(legacyResult && !legacyResult.value());

    MediaNodeOptions copyOnly;
    copyOnly.set("video.lineage.copy_opaque", "1");
    assert(!parseMediaVideoLineageCopyOpaqueOption(&copyOnly));

    MediaNodeOptions missingCopy;
    missingCopy.set("video.lineage.capacity", "8");
    assert(!parseMediaVideoLineageCopyOpaqueOption(&missingCopy));

    for (const std::string value : {"0", "false", "yes", "garbage"}) {
        MediaNodeOptions conflicting;
        conflicting.set("video.lineage.capacity", "8");
        conflicting.set("video.lineage.copy_opaque", value);
        assert(!parseMediaVideoLineageCopyOpaqueOption(&conflicting));
    }

    MediaNodeOptions complete;
    complete.set("video.lineage.capacity", "8");
    complete.set("video.lineage.copy_opaque", "1");
    auto enabled = parseMediaVideoLineageCopyOpaqueOption(&complete);
    assert(enabled && enabled.value());
}

void frameRateTargetOwnsAndPurgesLineageState()
{
    auto runtime = MediaRuntimeNodeFactory::create(lineageNode(
        MediaNodeKind::VideoFrameRate, "video_frame_rate"));
    assert(runtime);
    auto* node = dynamic_cast<VideoFrameRateNode*>(runtime.value().get());
    assert(node);
    auto state = std::dynamic_pointer_cast<MediaVideoFrameRateState>(
        node->generationPurgeTarget());
    assert(state);
    assert(state->activateGeneration(51));
    {
        auto guard = state->lock();
        auto& data = state->data();
        data.pendingFrames.push_back({MediaBufferRef{}, 51});
        data.lastInputFrame = {MediaBufferRef{}, 51};
        data.initialized = true;
        data.started = true;
        data.startPts = 900;
        data.nextOutputIndex = 4;
        data.lastOutputPts = 1'020;
    }
    assert(state->purge({51, 52, 1}));
    {
        auto guard = state->lock();
        const auto& data = state->data();
        assert(data.pendingFrames.empty());
        assert(!data.lastInputFrame.buffer);
        assert(!data.initialized && !data.started);
        assert(data.nextOutputIndex == 0);
        assert(data.lastOutputPts == AV_NOPTS_VALUE);
        assert(data.expectedGeneration == 52);
    }
    assert(!state->activateGeneration(51));
    assert(state->activateGeneration(52));
}

} // namespace

int main()
{
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    boundedRegistryRejectsInvalidAndUnresolvedTokens();
    sustainedResolvedLeasesRemainBounded();
    purgeIsExactByGeneration();
    lateOpaqueReleaseIsSafeAfterPurgeAndRegistryDestruction();
    deterministicCodecSequencesPreserveStrictOwnership();
    opaqueTokensRejectMissingDuplicateAndGenerationMismatch();
    explicitRateConversionDerivesDeterministicSequences();
    copyOpaqueCapabilityRejectsMissingOrIncompatibleRuntime();
    copyOpaqueOptionIsStrictAndSymmetric();
    packetViewUnwrapsCanonicalPayloadWithoutCopy();
    frameViewUnwrapsCanonicalPayloadWithoutCopy();
    productionStagesExposeExactStablePurgeTargets();
    participantGroupRejectsMissingAndDuplicateStageTargets();
    frameRateTargetOwnsAndPurgesLineageState();
    std::cout << "video codec lineage tests passed\n";
    return 0;
}
