#include "internal/graph/runtime/buffer/MediaAvReleasedAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaDecodedAudioTrimInputBuffer.h"
#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/nodes/audio/MediaAudioDecodeInputView.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#include <cassert>
#include <iostream>
#include <memory>

using namespace media::ffmpeg::graph;

namespace {

MediaRunningTime ns(std::int64_t value)
{
    return MediaRunningTime::fromNanoseconds(value);
}

std::shared_ptr<const MediaCanonicalLineage> lineage()
{
    return std::make_shared<const MediaCanonicalLineage>(MediaCanonicalLineage{
        ns(0), std::nullopt, ns(10'000'000),
        MediaDecodeOrderMode::PresentationOrderNoReorder, "audio-source",
        MediaSourceAccessUnitSequence(1), MediaTimeMappingConfidence::Locked, 7});
}

MediaBufferRef canonicalPacket()
{
    auto packet = ::media::ffmpeg::makePacket();
    assert(packet);
    packet->size = 1;
    auto raw = FFmpegBufferFactory::wrapPacket(
        std::move(packet), MediaStreamKind::Audio, std::nullopt);
    assert(raw);
    auto canonical = MediaCanonicalAccessUnitBuffer::create(
        raw.value(), lineage(),
        MediaCanonicalAudioSampleInterval{0, 480, 48'000});
    assert(canonical);
    return std::move(canonical).value();
}

MediaBufferRef canonicalFrame()
{
    auto frame = ::media::ffmpeg::makeFrame();
    assert(frame);
    frame->format = AV_SAMPLE_FMT_FLTP;
    frame->sample_rate = 48'000;
    frame->nb_samples = 480;
    av_channel_layout_default(&frame->ch_layout, 2);
    assert(av_frame_get_buffer(frame.get(), 0) == 0);
    auto raw = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
    assert(raw);
    auto canonical = MediaCanonicalAudioSamplesBuffer::create(
        raw.value(), lineage(), {0, 480, 48'000});
    assert(canonical);
    return std::move(canonical).value();
}

void originAndTrimHaveOneWayTypedOwnership()
{
    const MediaAudioPlaybackOrigin origin{7, ns(5'000'000), ns(8'000'000), 120, 48'000};
    auto released = MediaAvReleasedAudioBuffer::create(canonicalPacket(), 240, origin);
    assert(released);
    const auto* releasedAudio = dynamic_cast<const MediaAvReleasedAudioBuffer*>(
        released.value().get());
    assert(releasedAudio && releasedAudio->audioOrigin() == origin);

    auto decoded = MediaDecodedAudioTrimInputBuffer::create(
        canonicalFrame(), releasedAudio->audioOrigin(), releasedAudio->trimLeadingSamples());
    assert(decoded);
    const auto* trimInput = dynamic_cast<const MediaDecodedAudioTrimInputBuffer*>(
        decoded.value().get());
    assert(trimInput && trimInput->audioOrigin() == origin);
    assert(trimInput->trimLeadingSamples() == 240);

    auto bound = MediaBoundCanonicalAudioBuffer::create(
        trimInput->media(), trimInput->audioOrigin());
    assert(bound);
    const auto* boundAudio = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
        bound.value().get());
    assert(boundAudio && boundAudio->audioOrigin() == origin);
    assert(boundAudio->media() == trimInput->media());
}

void mismatchedGenerationAndUntypedPayloadAreRejected()
{
    const MediaAudioPlaybackOrigin wrong{8, ns(0), ns(0), 0, 48'000};
    assert(!MediaAvReleasedAudioBuffer::create(canonicalPacket(), 0, wrong));
    assert(!MediaDecodedAudioTrimInputBuffer::create(canonicalPacket(), wrong, 0));
    assert(!MediaBoundCanonicalAudioBuffer::create(canonicalPacket(), wrong));
}

void factoryRequiresOneExactAudioExecutionMode()
{
    MediaGraph graph;
    const auto missingId = graph.addNode(MediaNodeKind::AudioDecode, "missing");
    const MediaNode* missing = graph.findNode(missingId);
    assert(missing && !MediaRuntimeNodeFactory::create(*missing));

    const auto unknownId = graph.addNode(MediaNodeKind::AudioDecode, "unknown");
    assert(graph.setNodeOption(
        unknownId, std::string(MediaAudioLineageModeOptionKey), "guessed"));
    const MediaNode* unknown = graph.findNode(unknownId);
    assert(unknown && !MediaRuntimeNodeFactory::create(*unknown));

    const auto legacyId = graph.addNode(MediaNodeKind::AudioDecode, "legacy");
    assert(graph.setNodeOption(
        legacyId, std::string(MediaAudioLineageModeOptionKey),
        std::string(mediaAudioLineageExecutionModeName(
            MediaAudioLineageExecutionMode::LegacyPlainPacket))));
    const MediaNode* legacy = graph.findNode(legacyId);
    assert(legacy && MediaRuntimeNodeFactory::create(*legacy));

    const auto synchronizedId = graph.addNode(MediaNodeKind::AudioDecode, "synchronized");
    assert(graph.setNodeOption(
        synchronizedId, std::string(MediaAudioLineageModeOptionKey),
        std::string(mediaAudioLineageExecutionModeName(
            MediaAudioLineageExecutionMode::SynchronizedReleasedAudio))));
    assert(graph.setNodeOption(
        synchronizedId, "audio.lineage.identity",
        std::string(MediaAudioDecodeLineageIdentity)));
    assert(graph.setNodeOption(
        synchronizedId, "audio.lineage.capacity", "8"));
    const MediaNode* synchronized = graph.findNode(synchronizedId);
    assert(synchronized && MediaRuntimeNodeFactory::create(*synchronized));
}

void encodedEnvelopeRemainsAConsumablePacket()
{
    auto packet = ::media::ffmpeg::makePacket();
    assert(packet && av_new_packet(packet.get(), 1) == 0);
    auto raw = FFmpegBufferFactory::wrapPacket(
        std::move(packet), MediaStreamKind::Audio, std::nullopt);
    assert(raw);
    const MediaAudioPlaybackOrigin origin{7, ns(0), ns(0), 0, 48'000};
    auto encoded = MediaEncodedAudioLineageBuffer::create(
        raw.value(), {{lineage(), {0, 480, 48'000}}}, origin);
    assert(encoded);
    assert(FFmpegPacketView::packet(encoded.value()) != nullptr);
}

void decodeInputModeRejectsCrossedBufferTypes()
{
    const MediaAudioPlaybackOrigin origin{7, ns(0), ns(0), 0, 48'000};
    auto released = MediaAvReleasedAudioBuffer::create(canonicalPacket(), 0, origin);
    assert(released);
    assert(resolveMediaAudioDecodeInput(
        released.value(), MediaAudioLineageExecutionMode::SynchronizedReleasedAudio));
    assert(!resolveMediaAudioDecodeInput(
        canonicalPacket(), MediaAudioLineageExecutionMode::SynchronizedReleasedAudio));
    assert(resolveMediaAudioDecodeInput(
        canonicalPacket(), MediaAudioLineageExecutionMode::LegacyPlainPacket));
    assert(!resolveMediaAudioDecodeInput(
        released.value(), MediaAudioLineageExecutionMode::LegacyPlainPacket));
}

} // namespace

int main()
{
    originAndTrimHaveOneWayTypedOwnership();
    mismatchedGenerationAndUntypedPayloadAreRejected();
    factoryRequiresOneExactAudioExecutionMode();
    decodeInputModeRejectsCrossedBufferTypes();
    encodedEnvelopeRemainsAConsumablePacket();
    std::cout << "audio origin envelope tests passed\n";
    return 0;
}
