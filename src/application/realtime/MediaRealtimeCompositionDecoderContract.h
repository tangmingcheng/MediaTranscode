#pragma once
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include "media_transcode/Result.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {
// The first opened decoder fixes the continuous preparation contract. Retain
// only the facts used by the existing equality check, not a full candidate plan.
class MediaRealtimeCompositionDecoderContract final {
    class ConstructionKey final {
        friend class MediaRealtimeCompositionDecoderContract;
        ConstructionKey() = default;
    };
public:
    static ::media::Result<std::shared_ptr<const MediaRealtimeCompositionDecoderContract>> create(
        const MediaVideoSourcePlan& candidate,
        const std::shared_ptr<MediaPreparationStorageBudget>& root);
    // Public only for allocate_shared; the key prevents uncharged construction.
    MediaRealtimeCompositionDecoderContract(ConstructionKey, const MediaVideoSourcePlan& candidate,
        std::optional<MediaPreparationStorageLease> metadataStorage);
    MediaRealtimeCompositionDecoderContract(const MediaRealtimeCompositionDecoderContract&) = delete;
    MediaRealtimeCompositionDecoderContract& operator=(const MediaRealtimeCompositionDecoderContract&) = delete;
    bool matches(const MediaVideoSourcePlan& candidate) const;
private:
    // Ordinary string requests remain charged until all owning fields are destroyed.
    std::optional<MediaPreparationStorageLease> m_metadataStorage;
    std::string m_decoderName;
    std::string m_hardwareName;
    std::optional<MediaDecoderInputRetention> m_decoderRetention;
    std::optional<MediaHardwareDescriptor> m_decoderFrame;
    MediaVideoLineagePropagation m_decoderLineage;
    bool m_filterActive;
    std::string m_filterName;
    std::optional<MediaHardwareDescriptor> m_filterInputFrame;
    std::optional<MediaHardwareDescriptor> m_filterOutputFrame;
    MediaHardwareTransferDirection m_transfer;
};
} // namespace media::ffmpeg::graph
