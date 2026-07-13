#include "internal/graph/nodes/demux/MpegTsDemuxNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> requiredGeneration(const MediaNodeOptions* options,
                                                   const char* key)
{
    auto value = requiredNonNegativeIntNodeOption(options, "MpegTsDemuxNode", key);
    if (!value) return ::media::Result<std::uint64_t>::failure(value.error());
    return ::media::Result<std::uint64_t>::success(static_cast<std::uint64_t>(value.value()));
}

std::optional<std::uint64_t> packetTimestamp(std::int64_t value)
{
    if (value == AV_NOPTS_VALUE) return std::nullopt;
    if (value < 0) return std::numeric_limits<std::uint64_t>::max();
    return static_cast<std::uint64_t>(value);
}

} // namespace

MpegTsDemuxNode::MpegTsDemuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MpegTsDemuxNode")
{
}

MediaNodeKind MpegTsDemuxNode::staticKind() noexcept
{
    return MediaNodeKind::MpegTsDemux;
}

::media::Status MpegTsDemuxNode::bind(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Status::failure(input.error());
    if (!input.value()) return ::media::Status::success();
    if (m_session) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode rejects duplicate binding"));
    }
    auto* prepared = dynamic_cast<MediaTsPreparedInputBuffer*>(input.value()->get());
    if (!prepared) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode requires MediaTsPreparedInputBuffer"));
    }
    const auto* options = nodeOptions(context);
    auto program = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.program_number");
    auto pmt = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.pmt_pid");
    auto video = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.video_pid");
    auto audio = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.audio_pid");
    auto pcr = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.pcr_pid");
    auto interval = requiredPositiveInt64NodeOption(options, "MpegTsDemuxNode", "mpegts.pcr_interval_27mhz");
    auto jitter = requiredPositiveInt64NodeOption(options, "MpegTsDemuxNode", "mpegts.maximum_pcr_jitter_27mhz");
    auto gap = requiredPositiveInt64NodeOption(options, "MpegTsDemuxNode", "mpegts.maximum_pcr_gap_27mhz");
    auto capacity = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.projection_capacity");
    auto regression = requiredPositiveInt64NodeOption(options, "MpegTsDemuxNode", "mpegts.maximum_position_regression_bytes");
    auto numerator = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.timestamp_time_base_num");
    auto denominator = requiredPositiveIntNodeOption(options, "MpegTsDemuxNode", "mpegts.timestamp_time_base_den");
    auto sourceGeneration = requiredGeneration(options, "mpegts.initial_source_generation");
    auto rawGeneration = requiredGeneration(options, "mpegts.initial_raw_generation");
    if (!program || !pmt || !video || !audio || !pcr || !interval || !jitter || !gap ||
        !capacity || !regression || !numerator || !denominator || !sourceGeneration ||
        !rawGeneration) {
        const ::media::ErrorInfo* error = nullptr;
        if (!program) error = &program.error(); else if (!pmt) error = &pmt.error();
        else if (!video) error = &video.error(); else if (!audio) error = &audio.error();
        else if (!pcr) error = &pcr.error(); else if (!interval) error = &interval.error();
        else if (!jitter) error = &jitter.error(); else if (!gap) error = &gap.error();
        else if (!capacity) error = &capacity.error(); else if (!regression) error = &regression.error();
        else if (!numerator) error = &numerator.error(); else if (!denominator) error = &denominator.error();
        else if (!sourceGeneration) error = &sourceGeneration.error(); else error = &rawGeneration.error();
        return ::media::Status::failure(*error);
    }
    if (numerator.value() != 1 || denominator.value() != 90'000) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode requires planned 1/90000 packet timestamps"));
    }
    MediaTsProgramClockPolicy policy{
        static_cast<std::uint16_t>(program.value()), static_cast<std::uint16_t>(pmt.value()),
        static_cast<std::uint16_t>(pcr.value()), static_cast<std::uint16_t>(video.value()),
        static_cast<std::uint16_t>(audio.value()), interval.value(), jitter.value(), gap.value()};
    auto projection = MediaTsClockProjection::create(
        policy, static_cast<std::size_t>(capacity.value()),
        static_cast<std::uint64_t>(regression.value()), sourceGeneration.value(), rawGeneration.value());
    if (!projection) return ::media::Status::failure(projection.error());
    auto session = prepared->takeSession();
    if (!session) return ::media::Status::failure(session.error());
    auto preflight = session.value()->evidenceSnapshotAfter(std::nullopt);
    if (!preflight) return ::media::Status::failure(preflight.error());
    if (auto status = projection.value().replay(preflight.value()); !status) return status;
    const auto& programs = session.value()->programSnapshots();
    const auto selected = std::find_if(programs.begin(), programs.end(), [&](const auto& item) {
        return item.programNumber == program.value() && item.pmtPid == pmt.value() && item.pcrPid == pcr.value();
    });
    if (selected == programs.end()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MpegTsDemuxNode program identity mismatch"));
    }
    for (const auto& binding : selected->streamBindings) {
        if (binding.elementaryPid == video.value()) m_videoStreamIndex = binding.streamIndex;
        if (binding.elementaryPid == audio.value()) m_audioStreamIndex = binding.streamIndex;
    }
    if (m_videoStreamIndex < 0 || m_audioStreamIndex < 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MpegTsDemuxNode selected PID mismatch"));
    }
    m_policy = policy;
    m_projection = std::move(projection).value();
    m_session = std::move(session).value();
    return ::media::Status::success();
}

::media::Result<MediaPacketSourceTiming> MpegTsDemuxNode::timingFor(
    const AVPacket& packet, const MediaTsClockProjectionCheckpoint& checkpoint, StreamClock& clock)
{
    if (clock.generation != checkpoint.generation || clock.readiness != checkpoint.readiness) {
        clock.mapper.reset();
        clock.generation = checkpoint.generation;
        clock.readiness = checkpoint.readiness;
    }
    MediaPacketSourceTiming timing{std::nullopt, std::nullopt, checkpoint.readiness, checkpoint.generation};
    if (checkpoint.readiness != MediaSourceClockReadiness::Locked) {
        return ::media::Result<MediaPacketSourceTiming>::success(timing);
    }
    if (!clock.mapper) {
        auto mapper = MediaTsSourceClockMapper::create(checkpoint.calibration);
        if (!mapper) return ::media::Result<MediaPacketSourceTiming>::failure(mapper.error());
        clock.mapper = std::move(mapper).value();
    }
    auto mapped = clock.mapper->map(packetTimestamp(packet.pts), packetTimestamp(packet.dts));
    if (!mapped) return ::media::Result<MediaPacketSourceTiming>::failure(mapped.error());
    if (mapped.value().presentationTime) timing.presentationNs = mapped.value().presentationTime->nanoseconds();
    if (mapped.value().decodeTime) timing.decodeNs = mapped.value().decodeTime->nanoseconds();
    return ::media::Result<MediaPacketSourceTiming>::success(timing);
}

::media::Result<MediaNodeProcessResult> MpegTsDemuxNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_session) {
        if (auto status = bind(context); !status) return processProgress(status);
        if (!m_session) return processWaiting();
    }
    if (m_eofSent) return processFinished();
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::allocationFailed("MpegTsDemuxNode packet allocation failed"));
    auto read = m_session->readFrame(*packet);
    if (!read) return ::media::Result<MediaNodeProcessResult>::failure(read.error());
    if (read.value() == MediaTsReadFrameState::Waiting) return processWaiting();
    if (read.value() == MediaTsReadFrameState::EndOfStream) return processFinished(emitEof(context));
    if (packet->pos < 0) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode requires non-negative packet position"));
    const bool video = packet->stream_index == m_videoStreamIndex;
    const bool audio = packet->stream_index == m_audioStreamIndex;
    if (!video && !audio) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::invalidArgument("MpegTsDemuxNode packet stream/PID mismatch"));
    const auto position = static_cast<std::uint64_t>(packet->pos);
    if (auto status = m_session->observePacketPosition(position); !status) return processProgress(status);
    if (auto status = m_projection->observePacketPosition(position); !status) return processProgress(status);
    auto incremental = m_session->evidenceSnapshotAfter(m_projection->lastReplayedOffset());
    if (!incremental) return ::media::Result<MediaNodeProcessResult>::failure(incremental.error());
    if (auto status = m_projection->replay(incremental.value()); !status) return processProgress(status);
    auto checkpoint = m_projection->atOrBefore(position);
    if (!checkpoint) return ::media::Result<MediaNodeProcessResult>::failure(checkpoint.error());
    auto timing = timingFor(*packet, checkpoint.value(), video ? m_videoClock : m_audioClock);
    if (!timing) return ::media::Result<MediaNodeProcessResult>::failure(timing.error());
    const auto streamKind = video ? MediaStreamKind::Video : MediaStreamKind::Audio;
    auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), streamKind, timing.value());
    if (!buffer) return ::media::Result<MediaNodeProcessResult>::failure(buffer.error());
    return processProgress(emitOutput(context, video ? "video" : "audio", buffer.value()));
}

::media::Status MpegTsDemuxNode::emitEof(MediaGraphExecutionContext& context)
{
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    if (!eof) return ::media::Status::failure(eof.error());
    m_eofSent = true;
    return broadcastControlToAllOutputs(context, eof.value());
}

void MpegTsDemuxNode::reset() noexcept
{
    if (m_session) (void)m_session->close();
    m_session.reset(); m_projection.reset(); m_policy.reset();
    m_videoStreamIndex = -1; m_audioStreamIndex = -1;
    m_videoClock = {}; m_audioClock = {}; m_eofSent = false; m_aborted = false;
}

::media::Status MpegTsDemuxNode::stop(MediaGraphExecutionContext& context)
{
    reset();
    return FFmpegNodeRuntime::stop(context);
}

void MpegTsDemuxNode::interrupt(MediaGraphExecutionContext&) noexcept
{
    m_aborted = true;
    if (m_session) m_session->interruptState().cancel();
}

void MpegTsDemuxNode::abort(MediaGraphExecutionContext& context) noexcept
{
    interrupt(context);
    reset();
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
