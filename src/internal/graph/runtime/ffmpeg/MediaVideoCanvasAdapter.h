#pragma once

#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "internal/graph/runtime/ffmpeg/MediaVideoFrameReadiness.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"
#include <memory>
#include <span>
#include "internal/graph/diagnostics/MediaCanvasComposeObservation.h"

namespace media::ffmpeg::graph {

// Borrowed together from the same owning frame. A missing tile has no dependency.
struct MediaVideoCanvasTile final {
    const AVFrame* frame = nullptr;
    const MediaVideoFrameReadiness* readiness = nullptr;
};

// Borrowed frames remain owned by the caller until synchronous completion.
struct MediaVideoCanvasCopyBatch final {
    const AVFrame& black;
    AVFrame& target;
    MediaVideoCanvasRectangle full;
    std::span<const MediaVideoCanvasTile> tiles;
    std::span<const MediaVideoCanvasRectangle> rectangles;

    template<class Function>
    ::media::Status visit(Function&& operation) const
    {
        using Observation = MediaCanvasComposeObservation;
        Observation::setTile(std::nullopt);
        const auto completed = MediaVideoFrameReadiness::completed();
        auto status = Observation::measure(Observation::Stage::Black,
            [&] { return operation(black, full, completed); });
        for (std::size_t i = 0; status && i < tiles.size(); ++i) {
            if (!tiles[i].frame) continue;
            Observation::setTile(i);
            status = Observation::measure(Observation::Stage::Tile,
                [&] { return operation(*tiles[i].frame, rectangles[i], *tiles[i].readiness); });
        }
        Observation::setTile(std::nullopt);
        return status;
    }
};

// Synchronous platform operation: success means all submitted work completed.
class MediaVideoCanvasAdapter {
public:
    virtual ~MediaVideoCanvasAdapter() = default;
    virtual ::media::Status upload(const AVFrame& source, AVFrame& target) = 0;
    virtual ::media::Status download(const AVFrame& source, AVFrame& target) = 0;
    ::media::Status copyBatch(const MediaVideoCanvasCopyBatch& batch)
    {
        if (batch.tiles.size() != batch.rectangles.size())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Canvas batch geometry count differs"));
        auto status = validate(batch.black, batch.target, batch.full);
        if (!status) return status;
        for (std::size_t i = 0; i < batch.tiles.size(); ++i) {
            if (!batch.tiles[i].frame) continue;
            if (!batch.tiles[i].readiness)
                return ::media::Status::failure(::media::ErrorInfo::notInitialized("Canvas tile lacks producer readiness"));
            status = validate(*batch.tiles[i].frame, batch.target, batch.rectangles[i]);
            if (!status) return status;
        }
        return executeBatch(batch);
    }
    virtual ::media::Status validate(const AVFrame& source, const AVFrame& target,
        const MediaVideoCanvasRectangle& targetRectangle) = 0;
    static ::media::Result<std::unique_ptr<MediaVideoCanvasAdapter>> create(AVBufferRef* frames);
protected:
    virtual ::media::Status executeBatch(const MediaVideoCanvasCopyBatch& batch) = 0;
};

::media::Result<MediaVideoCanvasAllocation> readMediaVideoCanvasAllocation(const AVFrame& frame);

} // namespace media::ffmpeg::graph
