#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshotFactory.h"

#include <limits>
#include <optional>

extern "C" {
#include <libavutil/error.h>
}

namespace media::ffmpeg::graph {

class MediaTsInputSession::EvidenceObserver final
    : public FFmpegObservedByteSink,
      public MediaTsPacketSink,
      public MediaTsProgramInventorySink {
public:
    static ::media::Result<std::unique_ptr<EvidenceObserver>> create(
        std::size_t packetStride,
        std::size_t evidenceCapacity,
        std::uint64_t maximumPositionRegressionBytes)
    {
        auto timeline = MediaTsEvidenceTimeline::create(
            evidenceCapacity, maximumPositionRegressionBytes);
        if (!timeline) {
            return ::media::Result<std::unique_ptr<EvidenceObserver>>::failure(timeline.error());
        }
        auto result = std::unique_ptr<EvidenceObserver>(
            new EvidenceObserver(std::move(timeline.value())));
        result->m_assembler = std::make_unique<MediaTsPsiSectionAssembler>(*result);
        auto parser = MediaTsPacketParser::create(packetStride, *result);
        if (!parser) {
            return ::media::Result<std::unique_ptr<EvidenceObserver>>::failure(parser.error());
        }
        result->m_parser = std::move(parser.value());
        return ::media::Result<std::unique_ptr<EvidenceObserver>>::success(std::move(result));
    }

    ::media::Status onBytes(std::uint64_t absoluteOffset,
                            std::span<const std::uint8_t> bytes) override
    {
        std::lock_guard lock(m_mutex);
        if (absoluteOffset != m_nextOffset) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("non-contiguous MPEG-TS observed offset"));
        }
        if (bytes.size() > std::numeric_limits<std::uint64_t>::max() - m_nextOffset) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS observed offset overflow"));
        }
        m_nextOffset += bytes.size();
        return m_parser->push(bytes);
    }

    ::media::Status onPacket(const MediaTsPacketView& packet) override
    {
        std::lock_guard lock(m_mutex);
        if (packet.discontinuity) ++m_generation;
        auto assembled = m_assembler->onPacket(packet);
        if (!assembled) return assembled;
        if (!m_inventoryDirty && !packet.pcr27Mhz && !packet.discontinuity) {
            return ::media::Status::success();
        }
        MediaTsEvidenceCheckpoint checkpoint;
        checkpoint.byteOffset = packet.byteOffset;
        if (m_inventory) checkpoint.inventory = *m_inventory;
        if (packet.pcr27Mhz) {
            checkpoint.pcrObservation = MediaTsRawPcrEvidence{
                .byteOffset = packet.byteOffset,
                .pid = packet.pid,
                .pcr27Mhz = *packet.pcr27Mhz,
                .discontinuity = packet.discontinuity};
        }
        checkpoint.discontinuity = packet.discontinuity;
        checkpoint.generation = m_generation;
        auto appended = m_timeline.append(std::move(checkpoint));
        if (appended) m_inventoryDirty = false;
        return appended;
    }

    ::media::Status onContinuityLoss(std::uint16_t pid) override
    {
        std::lock_guard lock(m_mutex);
        ++m_generation;
        return m_assembler->onContinuityLoss(pid);
    }

    ::media::Status onProgramInventory(MediaTsProgramInventorySnapshot snapshot) override
    {
        std::lock_guard lock(m_mutex);
        m_inventory = std::move(snapshot);
        m_inventoryDirty = true;
        return ::media::Status::success();
    }

    std::optional<MediaTsProgramInventorySnapshot> inventory() const
    {
        std::lock_guard lock(m_mutex);
        return m_inventory;
    }
    ::media::Result<MediaTsEvidenceCheckpoint> evidenceAtOrBefore(
        std::uint64_t packetPosition) const
    {
        std::lock_guard lock(m_mutex);
        return m_timeline.atOrBefore(packetPosition);
    }

    ::media::Result<std::vector<MediaTsEvidenceCheckpoint>> evidenceSnapshotAfter(
        std::optional<std::uint64_t> exclusiveOffset) const
    {
        std::lock_guard lock(m_mutex);
        return m_timeline.snapshotAfter(exclusiveOffset);
    }

private:
    explicit EvidenceObserver(MediaTsEvidenceTimeline timeline)
        : m_timeline(std::move(timeline)) {}

    std::unique_ptr<MediaTsPacketParser> m_parser;
    std::unique_ptr<MediaTsPsiSectionAssembler> m_assembler;
    MediaTsEvidenceTimeline m_timeline;
    std::optional<MediaTsProgramInventorySnapshot> m_inventory;
    bool m_inventoryDirty = false;
    mutable std::recursive_mutex m_mutex;
    std::uint64_t m_nextOffset = 0;
    std::uint64_t m_generation = 0;
};

class MediaTsInputSession::ReadLease final {
public:
    explicit ReadLease(MediaTsInputSession& session) noexcept : m_session(session) {}
    ~ReadLease()
    {
        std::lock_guard lock(m_session.m_sessionMutex);
        --m_session.m_activeReads;
        if (m_session.m_activeReads == 0) m_session.m_readsDone.notify_all();
    }
private:
    MediaTsInputSession& m_session;
};

MediaTsInputSession::~MediaTsInputSession()
{
    close();
}

::media::Status MediaTsInputSession::close() noexcept
{
    std::unique_ptr<FFmpegObservedReadAvio> observedToDestroy;
    {
        std::unique_lock lock(m_sessionMutex);
        if (m_closed) return ::media::Status::success();
        if (m_closing) {
            m_readsDone.wait(lock, [this] { return m_closed; });
            return ::media::Status::success();
        }
        m_closing = true;
    }
    if (m_observedAvio) {
        auto quiesced = m_observedAvio->close();
        if (!quiesced) return quiesced;
    }
    {
        std::unique_lock lock(m_sessionMutex);
        m_readsDone.wait(lock, [this] { return m_activeReads == 0; });
    }
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }
    {
        std::lock_guard lock(m_sessionMutex);
        if (m_observedAvio) {
            const auto observedStatus = m_observedAvio->status();
            m_finalError = observedStatus
                ? std::optional<::media::ErrorInfo>(::media::ErrorInfo::cancelled(
                      "MPEG-TS input session is closed"))
                : std::optional<::media::ErrorInfo>(observedStatus.error());
        } else if (!m_finalError) {
            m_finalError = ::media::ErrorInfo::cancelled(
                "MPEG-TS input session is closed");
        }
        observedToDestroy = std::move(m_observedAvio);
        m_closed = true;
    }
    observedToDestroy.reset();
    m_readsDone.notify_all();
    return ::media::Status::success();
}

::media::Result<MediaTsReadFrameState> MediaTsInputSession::readFrame(AVPacket& packet)
{
    {
        std::lock_guard lock(m_sessionMutex);
        if (m_closing || m_closed || !m_formatContext) {
            return ::media::Result<MediaTsReadFrameState>::failure(
                ::media::ErrorInfo::cancelled("MPEG-TS input session is closed"));
        }
        if (m_activeReads != 0) {
            return ::media::Result<MediaTsReadFrameState>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS input session permits one active reader"));
        }
        ++m_activeReads;
    }
    ReadLease guard(*this);

    const int result = av_read_frame(m_formatContext, &packet);
    const auto observerStatus = m_observedAvio->status();
    if (!observerStatus) {
        return ::media::Result<MediaTsReadFrameState>::failure(observerStatus.error());
    }
    {
        std::lock_guard lock(m_sessionMutex);
        if (m_closing || m_closed || m_interruptState.cancelled()) {
            return ::media::Result<MediaTsReadFrameState>::failure(
                ::media::ErrorInfo::cancelled("MPEG-TS input read was cancelled"));
        }
    }
    if (result >= 0) {
        return ::media::Result<MediaTsReadFrameState>::success(MediaTsReadFrameState::Frame);
    }
    if (result == AVERROR(EAGAIN)) {
        return ::media::Result<MediaTsReadFrameState>::success(MediaTsReadFrameState::Waiting);
    }
    if (result == AVERROR_EOF) {
        return ::media::Result<MediaTsReadFrameState>::success(MediaTsReadFrameState::EndOfStream);
    }
    if (result == AVERROR_EXIT) {
        return ::media::Result<MediaTsReadFrameState>::failure(
            ::media::ErrorInfo::cancelled("MPEG-TS input read was cancelled"));
    }
    return ::media::Result<MediaTsReadFrameState>::failure(
        ::media::ErrorInfo::ffmpegFailure("failed to read MPEG-TS frame", result));
}

::media::Result<std::unique_ptr<MediaTsInputSession>> MediaTsInputSession::open(
    const MediaTsInputSessionOptions& options)
{
    return openWithOpener(options, nullptr);
}

::media::Result<std::unique_ptr<MediaTsInputSession>> MediaTsInputSession::open(
    const MediaTsInputSessionOptions& options,
    FFmpegProtocolAvioOpener& opener)
{
    return openWithOpener(options, &opener);
}

::media::Result<std::unique_ptr<MediaTsInputSession>> MediaTsInputSession::openWithOpener(
    const MediaTsInputSessionOptions& options,
    FFmpegProtocolAvioOpener* opener)
{
    if (options.packetStride != 188) {
        return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(
            ::media::ErrorInfo::unsupported("only 188-byte MPEG-TS framing is supported"));
    }
    auto session = std::unique_ptr<MediaTsInputSession>(new MediaTsInputSession());
    auto observer = EvidenceObserver::create(options.packetStride, options.evidenceCapacity,
                                             options.maximumPositionRegressionBytes);
    if (!observer) return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(observer.error());
    session->m_evidenceObserver = std::move(observer.value());
    auto avio = opener
        ? FFmpegObservedReadAvio::open(
              options.protocolUrl, options.protocolOptions, options.avioBufferBytes,
              *session->m_evidenceObserver, session->m_interruptState, *opener)
        : FFmpegObservedReadAvio::open(
              options.protocolUrl, options.protocolOptions, options.avioBufferBytes,
              *session->m_evidenceObserver, session->m_interruptState);
    if (!avio) return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(avio.error());
    session->m_observedAvio = std::move(avio.value());
    session->m_formatContext = avformat_alloc_context();
    if (!session->m_formatContext) return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(
        ::media::ErrorInfo::allocationFailed("failed to allocate MPEG-TS format context"));
    session->m_formatContext->pb = session->m_observedAvio->outer();
    session->m_formatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
    const AVInputFormat* inputFormat = av_find_input_format("mpegts");
    if (!inputFormat) {
        return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(
            ::media::ErrorInfo::notInitialized("FFmpeg MPEG-TS demuxer is unavailable"));
    }
    AVDictionary* demuxOptions = nullptr;
    const int dictionaryResult = av_dict_copy(&demuxOptions, options.demuxOptions, 0);
    if (dictionaryResult < 0) {
        return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(
            ::media::ErrorInfo::allocationFailed("failed to copy MPEG-TS demux options"));
    }
    int result = avformat_open_input(&session->m_formatContext, nullptr, inputFormat, &demuxOptions);
    av_dict_free(&demuxOptions);
    if (result >= 0) result = avformat_find_stream_info(session->m_formatContext, nullptr);
    if (result < 0) return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(
        ::media::ErrorInfo::ffmpegFailure("failed to probe MPEG-TS input", result));
    auto observerStatus = session->m_observedAvio->status();
    if (!observerStatus) return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(observerStatus.error());
    const auto inventory = session->m_evidenceObserver->inventory();
    if (!inventory || inventory->programs.empty()) {
        return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS PAT/PMT inventory is incomplete"));
    }
    auto snapshots = session->buildStreamSnapshots();
    if (!snapshots) return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(snapshots.error());
    return ::media::Result<std::unique_ptr<MediaTsInputSession>>::success(std::move(session));
}

::media::Status MediaTsInputSession::buildStreamSnapshots()
{
    auto snapshots = FFmpegInputStreamSnapshotFactory::fromFormatContext(*m_formatContext);
    if (!snapshots) return ::media::Status::failure(snapshots.error());
    auto programs = MediaTsPublicProgramSnapshotFactory::fromFormatContext(*m_formatContext);
    if (!programs) return ::media::Status::failure(programs.error());
    m_streamSnapshots = std::move(snapshots.value());
    m_programSnapshots = std::move(programs.value());
    return ::media::Status::success();
}

const std::vector<FFmpegInputStreamSnapshot>& MediaTsInputSession::streamSnapshots() const noexcept
{
    return m_streamSnapshots;
}

const std::vector<FFmpegInputProgramSnapshot>& MediaTsInputSession::programSnapshots() const noexcept
{
    return m_programSnapshots;
}

::media::Result<std::vector<FFmpegInputStreamSnapshot>>
MediaTsInputSession::cloneStreamSnapshots() const
{
    std::vector<FFmpegInputStreamSnapshot> result;
    result.reserve(m_streamSnapshots.size());
    for (const auto& snapshot : m_streamSnapshots) {
        auto codec = snapshot.cloneCodecParameters();
        if (!codec) {
            return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::failure(codec.error());
        }
        auto ownedCodec = FFmpegCodecParametersSnapshot::takeOwnership(std::move(codec.value()));
        if (!ownedCodec) {
            return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::failure(ownedCodec.error());
        }
        FFmpegInputStreamSnapshot copy;
        copy.index = snapshot.index;
        copy.streamKind = snapshot.streamKind;
        copy.codec = std::move(ownedCodec.value());
        copy.format = snapshot.format;
        copy.time = snapshot.time;
        result.push_back(std::move(copy));
    }
    return ::media::Result<std::vector<FFmpegInputStreamSnapshot>>::success(std::move(result));
}

MediaTsProgramInventorySnapshot MediaTsInputSession::programInventory() const
{
    return *m_evidenceObserver->inventory();
}

::media::Result<MediaTsEvidenceCheckpoint> MediaTsInputSession::evidenceAtOrBefore(
    std::uint64_t packetPosition) const
{
    return m_evidenceObserver->evidenceAtOrBefore(packetPosition);
}

::media::Result<std::vector<MediaTsEvidenceCheckpoint>>
MediaTsInputSession::evidenceSnapshotAfter(
    std::optional<std::uint64_t> exclusiveOffset) const
{
    return m_evidenceObserver->evidenceSnapshotAfter(exclusiveOffset);
}

::media::Status MediaTsInputSession::status() const
{
    std::lock_guard lock(m_sessionMutex);
    if (m_finalError) return ::media::Status::failure(*m_finalError);
    if (!m_observedAvio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS session AVIO is unavailable"));
    }
    const auto observedStatus = m_observedAvio->status();
    if (!observedStatus) return observedStatus;
    if (m_closing || m_closed) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled("MPEG-TS input session is closing"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
