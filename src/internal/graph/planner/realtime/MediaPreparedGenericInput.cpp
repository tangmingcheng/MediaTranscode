#include "internal/graph/planner/realtime/MediaPreparedGenericInput.h"

#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshotFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketPayloadFootprint.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <chrono>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

struct PreparedReadDeadline final {
    std::chrono::steady_clock::time_point value;
};

int interruptPreparedRead(void* opaque)
{
    const auto* deadline = static_cast<const PreparedReadDeadline*>(opaque);
    return deadline && std::chrono::steady_clock::now() >= deadline->value;
}

std::string ffmpegError(int code)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, text, sizeof(text));
    return text;
}

bool completeTiming(const AVPacket& packet) noexcept
{
    return packet.pts != AV_NOPTS_VALUE && packet.dts != AV_NOPTS_VALUE &&
        packet.duration > 0;
}

bool whollyUntimed(const AVPacket& packet) noexcept
{
    return packet.pts == AV_NOPTS_VALUE && packet.dts == AV_NOPTS_VALUE &&
        packet.duration > 0;
}

::media::Status addBounded(
    std::size_t& packets,
    std::uint64_t& bytes,
    std::uint64_t packetBytes,
    std::size_t packetCapacity,
    std::uint64_t byteCapacity,
    std::uint64_t maximumPacketBytes,
    const char* stream)
{
    const std::string prefix = std::string("prepared generic input exceeds planned ") +
        stream + " replay ";
    if (packetBytes == 0 || packetBytes > maximumPacketBytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            prefix + "maximum packet bytes"));
    }
    if (packets >= packetCapacity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            prefix + "packet capacity current=" + std::to_string(packets) +
            " capacity=" + std::to_string(packetCapacity)));
    }
    if (bytes > byteCapacity || packetBytes > byteCapacity - bytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            prefix + "byte capacity current=" + std::to_string(bytes) +
            " packet=" + std::to_string(packetBytes) +
            " capacity=" + std::to_string(byteCapacity)));
    }
    ++packets;
    bytes += packetBytes;
    return ::media::Status::success();
}

::media::Result<MediaRunningTime> presentation(
    const MediaPreparedDemuxFirstPacketEvidence& evidence)
{
    return MediaRunningTime::checkedFromTicks(
        evidence.pts, evidence.timeBase.num, evidence.timeBase.den);
}

} // namespace

struct MediaPreparedGenericInput::CaptureState final {
    struct InterruptState final {
        std::atomic_bool stopRequested{false};
        std::chrono::steady_clock::time_point deadline;
        AVIOInterruptCB previous{};
    };

    CaptureState(
        ::media::ffmpeg::InputFormatContextPtr ownedContext,
        std::deque<MediaDemuxPreparedPacket> preparedReplay,
        const MediaPreparedGenericInputPlan& preparedPlan,
        std::uint64_t nextPreparedOrdinal)
        : context(std::move(ownedContext)),
          replay(std::move(preparedReplay)),
          plan(preparedPlan),
          nextOrdinal(nextPreparedOrdinal)
    {
        interrupt.previous = context->interrupt_callback;
        interrupt.deadline = std::chrono::steady_clock::now() +
            std::chrono::nanoseconds(
                plan.maximumPreparedHandoffDuration.nanoseconds());
        context->interrupt_callback = AVIOInterruptCB{
            [](void* opaque) {
                const auto* state = static_cast<const InterruptState*>(opaque);
                if (!state) return 1;
                if (state->stopRequested.load(std::memory_order_acquire) ||
                    std::chrono::steady_clock::now() >= state->deadline) {
                    return 1;
                }
                return state->previous.callback
                    ? state->previous.callback(state->previous.opaque)
                    : 0;
            },
            &interrupt};
    }

    ~CaptureState()
    {
        stopAndJoin();
    }

    void start()
    {
        worker = std::jthread([this] { capture(); });
    }

    void stopAndJoin() noexcept
    {
        interrupt.stopRequested.store(true, std::memory_order_release);
        if (worker.joinable()) worker.join();
        if (context) context->interrupt_callback = interrupt.previous;
    }

    ::media::Result<MediaDemuxInputSession> takeSession()
    {
        stopAndJoin();
        std::scoped_lock lock(mutex);
        if (error) {
            return ::media::Result<MediaDemuxInputSession>::failure(*error);
        }
        return ::media::Result<MediaDemuxInputSession>::success(
            MediaDemuxInputSession{
                std::move(context), std::move(replay), nextOrdinal});
    }

    void fail(::media::ErrorInfo failure)
    {
        std::scoped_lock lock(mutex);
        if (!error) error = std::move(failure);
    }

    void capture() noexcept
    {
        while (!interrupt.stopRequested.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() >= interrupt.deadline) {
                fail(::media::ErrorInfo::wouldBlock(
                    "prepared generic handoff deadline expired"));
                return;
            }
            auto packet = ::media::ffmpeg::makePacket();
            if (!packet) {
                fail(::media::ErrorInfo::allocationFailed(
                    "prepared generic handoff packet allocation failed"));
                return;
            }
            const int read = av_read_frame(context.get(), packet.get());
            if (read < 0) {
                if (interrupt.stopRequested.load(std::memory_order_acquire)) {
                    return;
                }
                const bool expired =
                    std::chrono::steady_clock::now() >= interrupt.deadline;
                fail(::media::ErrorInfo::ffmpegFailure(
                    expired
                        ? "prepared generic handoff deadline expired"
                        : "prepared generic handoff av_read_frame: " +
                            ffmpegError(read),
                    read));
                return;
            }

            const std::uint64_t currentOrdinal = nextOrdinal++;
            const bool video =
                packet->stream_index == plan.videoStreamIndex;
            const bool audio =
                packet->stream_index == plan.audioStreamIndex;
            if (!video && !audio) continue;
            if (!completeTiming(*packet)) {
                fail(::media::ErrorInfo::invalidArgument(
                    audio
                        ? "prepared generic handoff rejects untimed or partial audio"
                        : "prepared generic handoff rejects untimed or partial video after first evidence"));
                return;
            }
            const auto footprint = ffmpegPacketPayloadFootprintBytes(*packet);
            if (!footprint) {
                fail(::media::ErrorInfo::invalidArgument(
                    "prepared generic handoff packet footprint is malformed"));
                return;
            }
            auto bounded = addBounded(
                video ? videoPackets : audioPackets,
                video ? videoBytes : audioBytes,
                *footprint,
                video ? plan.handoffVideoPacketCapacity
                      : plan.handoffAudioPacketCapacity,
                video ? plan.handoffVideoByteCapacity
                      : plan.handoffAudioByteCapacity,
                video ? plan.maximumVideoPacketBytes
                      : plan.maximumAudioPacketBytes,
                video ? "video" : "audio");
            if (!bounded) {
                fail(bounded.error());
                return;
            }
            std::scoped_lock lock(mutex);
            replay.push_back(MediaDemuxPreparedPacket{
                std::move(packet),
                MediaDemuxPacketProvenance{
                    MediaDemuxPacketOrigin::PostFindStreamInfoPreparedRead,
                    currentOrdinal}});
        }
    }

    ::media::ffmpeg::InputFormatContextPtr context;
    std::deque<MediaDemuxPreparedPacket> replay;
    MediaPreparedGenericInputPlan plan;
    std::uint64_t nextOrdinal = 0;
    std::size_t videoPackets = 0;
    std::size_t audioPackets = 0;
    std::uint64_t videoBytes = 0;
    std::uint64_t audioBytes = 0;
    InterruptState interrupt;
    std::mutex mutex;
    std::optional<::media::ErrorInfo> error;
    std::jthread worker;
};

MediaPreparedGenericInput::MediaPreparedGenericInput(
    std::unique_ptr<CaptureState> capture,
    std::vector<FFmpegInputStreamSnapshot> snapshots,
    MediaPreparedGenericInputPlan plan,
    MediaPreparedGenericInputEvidence evidence,
    MediaAvSyncStartupPolicy startup) noexcept
    : m_capture(std::move(capture)),
      m_snapshots(std::move(snapshots)),
      m_plan(std::move(plan)),
      m_evidence(std::move(evidence)),
      m_startup(std::move(startup))
{
}

MediaPreparedGenericInput::MediaPreparedGenericInput(
    MediaPreparedGenericInput&&) noexcept = default;

MediaPreparedGenericInput& MediaPreparedGenericInput::operator=(
    MediaPreparedGenericInput&&) noexcept = default;

MediaPreparedGenericInput::~MediaPreparedGenericInput() = default;

::media::Result<MediaPreparedGenericInput>
MediaPreparedGenericInput::prepare(
    ::media::ffmpeg::InputFormatContextPtr context,
    MediaPreparedGenericInputPlan plan,
    MediaAvSyncStartupPolicy startup)
{
    if (!context) {
        return ::media::Result<MediaPreparedGenericInput>::failure(
            ::media::ErrorInfo::invalidArgument(
                "prepared generic input requires an owned format context"));
    }
    if (auto valid = plan.validate(); !valid) {
        return ::media::Result<MediaPreparedGenericInput>::failure(valid.error());
    }
    auto snapshots = FFmpegInputStreamSnapshotFactory::fromFormatContext(*context);
    if (!snapshots) {
        return ::media::Result<MediaPreparedGenericInput>::failure(
            snapshots.error());
    }

    const auto duration = std::chrono::nanoseconds(
        plan.maximumPreparedReadDuration.nanoseconds());
    PreparedReadDeadline deadline{std::chrono::steady_clock::now() + duration};
    const AVIOInterruptCB previous = context->interrupt_callback;
    context->interrupt_callback = AVIOInterruptCB{interruptPreparedRead, &deadline};

    std::deque<MediaDemuxPreparedPacket> replay;
    std::optional<MediaPreparedDemuxFirstPacketEvidence> firstVideo;
    std::optional<MediaPreparedDemuxFirstPacketEvidence> firstAudio;
    bool observedTimedVideo = false;
    std::deque<MediaPreparedTimedPacketCandidate> videoCandidates;
    std::deque<MediaPreparedTimedPacketCandidate> audioCandidates;
    std::size_t videoPackets = 0;
    std::size_t audioPackets = 0;
    std::uint64_t videoBytes = 0;
    std::uint64_t audioBytes = 0;
    std::size_t discardedVideoPackets = 0;
    std::uint64_t discardedVideoBytes = 0;
    std::size_t discardedTimedVideoPackets = 0;
    std::uint64_t discardedTimedVideoBytes = 0;
    std::size_t discardedTimedAudioPackets = 0;
    std::uint64_t discardedTimedAudioBytes = 0;
    std::uint64_t ordinal = 0;

    while (!firstVideo || !firstAudio) {
        if (std::chrono::steady_clock::now() >= deadline.value) {
            context->interrupt_callback = previous;
            return ::media::Result<MediaPreparedGenericInput>::failure(
                ::media::ErrorInfo::wouldBlock(
                    "prepared generic input read deadline expired before a common A/V window"));
        }
        auto packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            context->interrupt_callback = previous;
            return ::media::Result<MediaPreparedGenericInput>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "prepared generic input packet allocation failed"));
        }
        const int read = av_read_frame(context.get(), packet.get());
        if (read < 0) {
            context->interrupt_callback = previous;
            const bool expired = std::chrono::steady_clock::now() >= deadline.value;
            return ::media::Result<MediaPreparedGenericInput>::failure(
                expired
                    ? ::media::ErrorInfo::ffmpegFailure(
                          "prepared generic input read deadline expired",
                          AVERROR_EXIT)
                    : ::media::ErrorInfo::ffmpegFailure(
                          "prepared generic input av_read_frame: " +
                              ffmpegError(read),
                          read));
        }
        const std::uint64_t currentOrdinal = ordinal++;
        const bool video = packet->stream_index == plan.videoStreamIndex;
        const bool audio = packet->stream_index == plan.audioStreamIndex;
        if (!video && !audio) continue;

        const auto footprint = ffmpegPacketPayloadFootprintBytes(*packet);
        if (!footprint) {
            context->interrupt_callback = previous;
            return ::media::Result<MediaPreparedGenericInput>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "prepared generic input packet footprint is malformed"));
        }

        std::size_t& packetCount = video ? videoPackets : audioPackets;
        std::uint64_t& byteCount = video ? videoBytes : audioBytes;
        auto bounded = addBounded(
            packetCount, byteCount, *footprint,
            video ? plan.videoPacketCapacity : plan.audioPacketCapacity,
            video ? plan.videoByteCapacity : plan.audioByteCapacity,
            video ? plan.maximumVideoPacketBytes : plan.maximumAudioPacketBytes,
            video ? "video" : "audio");
        if (!bounded) {
            context->interrupt_callback = previous;
            return ::media::Result<MediaPreparedGenericInput>::failure(
                bounded.error());
        }

        if (!completeTiming(*packet)) {
            const bool discardable = video && !observedTimedVideo &&
                whollyUntimed(*packet) &&
                !(packet->flags & AV_PKT_FLAG_KEY) &&
                plan.leadingVideoDisposition ==
                    MediaPreparedLeadingVideoDisposition::
                        DiscardUntimedNonKeyBeforeFirstTimedVideo;
            if (discardable) {
                ++discardedVideoPackets;
                discardedVideoBytes += *footprint;
                continue;
            }
            context->interrupt_callback = previous;
            return ::media::Result<MediaPreparedGenericInput>::failure(
                ::media::ErrorInfo::invalidArgument(
                    audio
                        ? "prepared generic input rejects untimed or partial audio"
                        : (packet->flags & AV_PKT_FLAG_KEY)
                            ? "prepared generic input rejects an untimed video key packet"
                            : "prepared generic input rejects partial or post-timed video timing"));
        }

        const MediaRational timeBase = video
            ? plan.videoTimeBase : plan.audioTimeBase;
        if (video) observedTimedVideo = true;
        MediaPreparedDemuxFirstPacketEvidence evidence{
            packet->stream_index,
            timeBase,
            packet->pts,
            packet->dts,
            packet->duration,
            currentOrdinal};
        auto& candidates = video ? videoCandidates : audioCandidates;
        candidates.push_back(MediaPreparedTimedPacketCandidate{
            evidence, *footprint});
        replay.push_back(MediaDemuxPreparedPacket{
            std::move(packet),
            MediaDemuxPacketProvenance{
                MediaDemuxPacketOrigin::PostFindStreamInfoPreparedRead,
                currentOrdinal}});

        while (!videoCandidates.empty() && !audioCandidates.empty()) {
            auto videoPresentation = presentation(
                videoCandidates.front().packet);
            auto audioPresentation = presentation(
                audioCandidates.front().packet);
            if (!videoPresentation || !audioPresentation) {
                context->interrupt_callback = previous;
                return ::media::Result<MediaPreparedGenericInput>::failure(
                    videoPresentation
                        ? audioPresentation.error()
                        : videoPresentation.error());
            }
            auto skew = videoPresentation.value() >= audioPresentation.value()
                ? videoPresentation.value().checkedSubtract(
                      audioPresentation.value())
                : audioPresentation.value().checkedSubtract(
                      videoPresentation.value());
            if (!skew) {
                context->interrupt_callback = previous;
                return ::media::Result<MediaPreparedGenericInput>::failure(
                    skew.error());
            }
            if (skew.value() <= plan.firstWindowMaximumSkew) {
                firstVideo = videoCandidates.front().packet;
                firstAudio = audioCandidates.front().packet;
                break;
            }
            if (plan.timedStartupPrefixDisposition !=
                MediaPreparedTimedStartupPrefixDisposition::
                    DiscardEarlierCompleteTimedUntilCommonWindow) {
                context->interrupt_callback = previous;
                return ::media::Result<MediaPreparedGenericInput>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "prepared generic input rejects an unpaired first timed A/V window"));
            }
            if (videoPresentation.value() < audioPresentation.value()) {
                ++discardedTimedVideoPackets;
                discardedTimedVideoBytes +=
                    videoCandidates.front().payloadBytes;
                videoCandidates.pop_front();
            } else {
                ++discardedTimedAudioPackets;
                discardedTimedAudioBytes +=
                    audioCandidates.front().payloadBytes;
                audioCandidates.pop_front();
            }
        }
    }
    context->interrupt_callback = previous;

    auto videoPresentation = presentation(*firstVideo);
    auto audioPresentation = presentation(*firstAudio);
    if (!videoPresentation || !audioPresentation) {
        return ::media::Result<MediaPreparedGenericInput>::failure(
            videoPresentation ? audioPresentation.error() : videoPresentation.error());
    }
    auto skew = videoPresentation.value() >= audioPresentation.value()
        ? videoPresentation.value().checkedSubtract(audioPresentation.value())
        : audioPresentation.value().checkedSubtract(videoPresentation.value());
    if (!skew || skew.value() > plan.firstWindowMaximumSkew) {
        return ::media::Result<MediaPreparedGenericInput>::failure(
            ::media::ErrorInfo::invalidArgument(
                "prepared generic input rejects excessive first timed A/V skew"));
    }

    std::deque<MediaDemuxPreparedPacket> selectedReplay;
    for (auto& prepared : replay) {
        const bool discardVideo =
            prepared.packet->stream_index == plan.videoStreamIndex &&
            prepared.provenance.ordinal < firstVideo->preparedReadOrdinal;
        const bool discardAudio =
            prepared.packet->stream_index == plan.audioStreamIndex &&
            prepared.provenance.ordinal < firstAudio->preparedReadOrdinal;
        if (!discardVideo && !discardAudio) {
            selectedReplay.push_back(std::move(prepared));
        }
    }
    replay = std::move(selectedReplay);
    const auto firstRetained = [&](int streamIndex)
        -> const MediaDemuxPreparedPacket* {
        for (const auto& prepared : replay) {
            if (prepared.packet->stream_index == streamIndex) {
                return &prepared;
            }
        }
        return nullptr;
    };
    const auto matches = [](const MediaDemuxPreparedPacket* retained,
                            const MediaPreparedDemuxFirstPacketEvidence& chosen) {
        return retained && retained->provenance.origin ==
                MediaDemuxPacketOrigin::PostFindStreamInfoPreparedRead &&
            retained->provenance.ordinal == chosen.preparedReadOrdinal &&
            retained->packet->stream_index == chosen.streamIndex &&
            retained->packet->pts == chosen.pts &&
            retained->packet->dts == chosen.dts &&
            retained->packet->duration == chosen.duration;
    };
    if (!matches(firstRetained(plan.videoStreamIndex), *firstVideo) ||
        !matches(firstRetained(plan.audioStreamIndex), *firstAudio)) {
        return ::media::Result<MediaPreparedGenericInput>::failure(
            ::media::ErrorInfo::internalError(
                "prepared generic input retained prefix disagrees with its chosen evidence"));
    }
    MediaPreparedGenericInputEvidence evidence{
        *firstVideo, *firstAudio,
        discardedVideoPackets, discardedVideoBytes,
        discardedTimedVideoPackets, discardedTimedVideoBytes,
        discardedTimedAudioPackets, discardedTimedAudioBytes};
    auto capture = std::make_unique<CaptureState>(
        std::move(context), std::move(replay), plan, ordinal);
    capture->start();
    return ::media::Result<MediaPreparedGenericInput>::success(
        MediaPreparedGenericInput(
            std::move(capture), std::move(snapshots).value(),
            std::move(plan), std::move(evidence), std::move(startup)));
}

::media::Result<MediaDemuxInputSession>
MediaPreparedGenericInput::takeSession()
{
    if (!m_capture) {
        return ::media::Result<MediaDemuxInputSession>::failure(
            ::media::ErrorInfo::invalidArgument(
                "prepared generic handoff session was already transferred"));
    }
    auto capture = std::move(m_capture);
    return capture->takeSession();
}

} // namespace media::ffmpeg::graph
