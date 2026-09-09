#pragma once

#include "application/realtime/MediaRealtimeVideoRunController.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include "internal/graph/model/MediaDatagramServiceScopeContract.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaGraphRuntime;

struct MediaRealtimeInitialOutputProducts final {
    std::uint64_t peakWireBytesPerSecond;
    MediaDatagramServiceScopeContract serviceScope;
    MediaRunningTime maximumStartupWait;
    std::uint64_t initialGeneration;
};

// Serializes output transactions; preparation alone executes off the control thread.
class MediaRealtimeOutputRunController final {
public:
    static ::media::Result<std::unique_ptr<MediaRealtimeOutputRunController>> create(
        const MediaRealtimeRtpTranscodeRequest& request,
        MediaRealtimeVideoSessionFacts facts,
        MediaRealtimeInitialOutputProducts initial,
        MediaGraphRuntime& runtime,
        MediaRealtimeVideoRunControl& control,
        const MediaRealtimeVideoRunObserver& observer,
        const MediaRealtimeVideoRunPolicy& policy,
        std::uint64_t initialOutputId);
    ~MediaRealtimeOutputRunController();

    ::media::Status startInitial();
    ::media::Status poll();
    void aggregate(MediaGraphRuntimeReport& report) const;
    bool hasOutputs() const noexcept;
    ::media::Status finish(const ::media::Status& cause);

private:
    class Impl;
    explicit MediaRealtimeOutputRunController(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};

} // namespace media::ffmpeg::graph
