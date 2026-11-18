#pragma once

#include "internal/graph/model/MediaDecoderInputRetention.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "media_transcode/Result.h"
#include <optional>

namespace media::ffmpeg::graph {

// Fixed live owners from opened codecs and the real protocol completion contract.
// Candidate AU/startup capacities are deliberately not part of these facts.
struct MediaCompositionStreamHolderFacts final {
    MediaDecoderInputRetention decoder;
    std::uint64_t maximumAtomicInputCompletion;
};

struct MediaCompositionProducerHolderFacts final {
    MediaCompositionStreamHolderFacts video;
    std::optional<MediaCompositionStreamHolderFacts> audio;
    std::optional<std::uint64_t> filterRetainedInputFrames;
};

struct MediaCompositionPacketResidenceCapacity final {
    std::uint64_t startupPackets;
    std::uint64_t acquiringPackets;
};

struct MediaCompositionProducerResidence final {
    std::uint64_t additionalRetainedObjects;
    std::uint64_t minimumProgressObjects;
};

class MediaCompositionProducerResidencePlanner final {
public:
    static ::media::Result<MediaCompositionProducerResidence> plan(
        const MediaCompositionProducerHolderFacts& facts, MediaNodeKind producer,
        MediaStreamKind stream, MediaPayloadKind payload,
        const MediaCompositionPacketResidenceCapacity& capacity);
};

} // namespace media::ffmpeg::graph
