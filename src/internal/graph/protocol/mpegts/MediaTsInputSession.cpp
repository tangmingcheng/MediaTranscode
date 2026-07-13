#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshotFactory.h"

#include <limits>
#include <optional>

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
        ++m_generation;
        return m_assembler->onContinuityLoss(pid);
    }

    ::media::Status onProgramInventory(MediaTsProgramInventorySnapshot snapshot) override
    {
        m_inventory = std::move(snapshot);
        m_inventoryDirty = true;
        return ::media::Status::success();
    }

    const std::optional<MediaTsProgramInventorySnapshot>& inventory() const noexcept
    {
        return m_inventory;
    }
    const MediaTsEvidenceTimeline& timeline() const noexcept { return m_timeline; }

private:
    explicit EvidenceObserver(MediaTsEvidenceTimeline timeline)
        : m_timeline(std::move(timeline)) {}

    std::unique_ptr<MediaTsPacketParser> m_parser;
    std::unique_ptr<MediaTsPsiSectionAssembler> m_assembler;
    MediaTsEvidenceTimeline m_timeline;
    std::optional<MediaTsProgramInventorySnapshot> m_inventory;
    bool m_inventoryDirty = false;
    std::uint64_t m_nextOffset = 0;
    std::uint64_t m_generation = 0;
};

MediaTsInputSession::~MediaTsInputSession()
{
    m_interruptState.cancel();
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        if (m_lifecycleSink) m_lifecycleSink->onLifecycleEvent(
            FFmpegObservedAvioLifecycleEvent::FormatClosed);
    }
    m_observedAvio.reset();
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
    session->m_lifecycleSink = options.lifecycleSink;
    auto observer = EvidenceObserver::create(options.packetStride, options.evidenceCapacity,
                                             options.maximumPositionRegressionBytes);
    if (!observer) return ::media::Result<std::unique_ptr<MediaTsInputSession>>::failure(observer.error());
    session->m_evidenceObserver = std::move(observer.value());
    auto avio = opener
        ? FFmpegObservedReadAvio::open(
              options.protocolUrl, options.protocolOptions, options.avioBufferBytes,
              *session->m_evidenceObserver, session->m_interruptState, *opener,
              options.lifecycleSink)
        : FFmpegObservedReadAvio::open(
              options.protocolUrl, options.protocolOptions, options.avioBufferBytes,
              *session->m_evidenceObserver, session->m_interruptState,
              options.lifecycleSink);
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
    if (!session->m_evidenceObserver->inventory() || session->m_evidenceObserver->inventory()->programs.empty()) {
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
    m_streamSnapshots = std::move(snapshots.value());
    return ::media::Status::success();
}

const std::vector<FFmpegInputStreamSnapshot>& MediaTsInputSession::streamSnapshots() const noexcept
{
    return m_streamSnapshots;
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

const MediaTsProgramInventorySnapshot& MediaTsInputSession::programInventory() const noexcept
{
    return *m_evidenceObserver->inventory();
}

const MediaTsEvidenceTimeline& MediaTsInputSession::evidenceTimeline() const noexcept
{
    return m_evidenceObserver->timeline();
}

::media::Status MediaTsInputSession::status() const
{
    return m_observedAvio ? m_observedAvio->status() : ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("MPEG-TS session AVIO is unavailable"));
}

} // namespace media::ffmpeg::graph
