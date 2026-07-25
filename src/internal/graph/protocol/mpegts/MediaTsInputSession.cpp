#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"

#include "internal/graph/protocol/mpegts/MediaTsPacketParser.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSectionAssembler.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshotFactory.h"

#include <limits>
#include <array>
#include <optional>

extern "C" {
#include <libavutil/error.h>
}

namespace media::ffmpeg::graph {

class MediaTsInputSession::EvidenceObserver final
    : public FFmpegObservedByteSink,
      public MediaTsPacketSink,
      public MediaTsIncrementalPacketSink,
      public MediaTsProgramInventorySink {
public:
    static ::media::Result<std::unique_ptr<EvidenceObserver>> create(
        std::size_t packetStride,
        std::size_t evidenceCapacity,
        std::size_t pesProvenanceCapacity,
        std::uint64_t maximumPositionRegressionBytes)
    {
        auto timeline = MediaTsEvidenceTimeline::create(
            evidenceCapacity, maximumPositionRegressionBytes);
        if (!timeline) {
            return ::media::Result<std::unique_ptr<EvidenceObserver>>::failure(timeline.error());
        }
        auto result = std::unique_ptr<EvidenceObserver>(
            new EvidenceObserver(std::move(timeline.value()), packetStride));
        auto provenance = MediaTsPesProvenanceTimeline::create(
            packetStride, pesProvenanceCapacity, maximumPositionRegressionBytes);
        if (!provenance) {
            return ::media::Result<std::unique_ptr<EvidenceObserver>>::failure(
                provenance.error());
        }
        result->m_pesProvenance =
            std::make_unique<MediaTsPesProvenanceTimeline>(std::move(provenance.value()));
        result->m_assembler = std::make_unique<MediaTsPsiSectionAssembler>(*result);
        auto parser = MediaTsPacketParser::create(packetStride, *result, result.get());
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
        if (m_pendingContinuityEvent) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS full packet arrived before its provenance prefix"));
        }
        auto assembled = m_assembler->onPacket(packet);
        if (!assembled) return assembled;
        if (!m_inventoryDirty) return ::media::Status::success();
        const auto packetEndDelta = m_packetStride - 1;
        if (packet.byteOffset >
            std::numeric_limits<std::uint64_t>::max() - packetEndDelta) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS packet-end evidence offset overflow"));
        }
        MediaTsEvidenceCheckpoint checkpoint;
        checkpoint.byteOffset = packet.byteOffset + packetEndDelta;
        if (m_inventory) checkpoint.inventory = *m_inventory;
        checkpoint.generation = m_generation;
        auto appended = m_timeline.append(std::move(checkpoint));
        if (appended) m_inventoryDirty = false;
        return appended;
    }

    ::media::Status onPacketEvidence(const MediaTsPacketEvidenceView& packet) override
    {
        std::lock_guard lock(m_mutex);
        if (m_pendingContinuityEvent &&
            m_pendingContinuityEvent->byteOffset != packet.byteOffset) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS continuity event was not merged with its packet evidence"));
        }
        if (!packet.pcr27Mhz && !m_pendingContinuityEvent) {
            return ::media::Status::success();
        }
        MediaTsEvidenceCheckpoint checkpoint;
        checkpoint.byteOffset = packet.byteOffset;
        if (m_inventory) checkpoint.inventory = *m_inventory;
        if (packet.pcr27Mhz) {
            checkpoint.pcrObservation = MediaTsRawPcrEvidence{
                packet.byteOffset, packet.pid, *packet.pcr27Mhz, packet.discontinuity};
        }
        checkpoint.continuityEvent = m_pendingContinuityEvent;
        checkpoint.discontinuity = packet.discontinuity;
        checkpoint.generation = m_generation;
        return m_timeline.append(std::move(checkpoint));
    }

    ::media::Status onPacketPrefix(const MediaTsPacketPrefixView& packet) override
    {
        std::lock_guard lock(m_mutex);
        if (m_pendingContinuityEvent &&
            m_pendingContinuityEvent->byteOffset != packet.byteOffset) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS continuity event was not merged with its packet prefix"));
        }
        if (m_pendingContinuityEvent) {
            if (!m_runtimeBinding) {
                auto continuity = m_pesProvenance->onContinuityEvent(
                    *m_pendingContinuityEvent, packet.payloadUnitStart);
                if (!continuity) return continuity;
            }
        }
        auto provenance = m_pesProvenance->onPacketPrefix(packet);
        if (provenance) m_pendingContinuityEvent.reset();
        return provenance;
    }

    ::media::Status onContinuityEvent(const MediaTsContinuityEvent& event) override
    {
        std::lock_guard lock(m_mutex);
        if (m_pendingContinuityEvent) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS packet produced duplicate continuity events"));
        }
        if (m_generation == std::numeric_limits<std::uint64_t>::max()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS transport generation exhausted"));
        }
        auto assembled = m_assembler->onContinuityEvent(event);
        if (!assembled) return assembled;
        if (m_runtimeBinding && m_runtimeBinding->isSourceClockPid(event.pid)) {
            auto boundary = m_pesProvenance->onSourceClockBoundary(event.byteOffset);
            if (!boundary) return boundary;
        }
        ++m_generation;
        m_pendingContinuityEvent = event;
        return ::media::Status::success();
    }

    ::media::Status onProgramInventory(MediaTsProgramInventorySnapshot snapshot) override
    {
        std::lock_guard lock(m_mutex);
        for (const auto& program : snapshot.programs) {
            for (const auto& stream : program.elementaryStreams) {
                if (auto tracked = m_pesProvenance->trackPid(stream.pid); !tracked) {
                    return tracked;
                }
            }
        }
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

    ::media::Status observePacketPosition(std::uint64_t packetPosition)
    {
        std::lock_guard lock(m_mutex);
        return m_timeline.observePacketPosition(packetPosition);
    }

    ::media::Status finish()
    {
        std::lock_guard lock(m_mutex);
        return m_parser->finish();
    }

    ::media::Status configureRuntimeBinding(const MediaTsRuntimeBinding& binding)
    {
        std::lock_guard lock(m_mutex);
        if (m_runtimeBinding) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS evidence observer binding is already configured"));
        }
        const std::array<std::uint16_t, 3> sourcePids{
            binding.video.pid, binding.audio.pid, binding.pcrPid};
        auto historicalBoundaries =
            m_timeline.completeContinuityOffsetsFor(sourcePids);
        if (!historicalBoundaries) {
            return ::media::Status::failure(historicalBoundaries.error());
        }
        auto candidate = *m_pesProvenance;
        const std::array<std::uint16_t, 2> selectedPids{
            binding.video.pid, binding.audio.pid};
        auto selected = candidate.configureSelectedPids(selectedPids);
        if (!selected) return selected;
        auto replayed = candidate.replaySourceClockBoundaries(
            historicalBoundaries.value());
        if (!replayed) return replayed;
        *m_pesProvenance = std::move(candidate);
        m_runtimeBinding = binding;
        return ::media::Status::success();
    }

    ::media::Result<MediaTsPesProvenanceAnchor> resolvePesAnchor(
        std::uint64_t packetPosition, std::uint16_t pid) const
    {
        std::lock_guard lock(m_mutex);
        return m_pesProvenance->resolveAnchor(packetPosition, pid);
    }

    ::media::Result<MediaTsPesProvenanceAnchor> pesStateForAnchor(
        const MediaTsPesProvenanceAnchor& anchor) const
    {
        std::lock_guard lock(m_mutex);
        return m_pesProvenance->stateForAnchor(anchor);
    }

private:
    EvidenceObserver(MediaTsEvidenceTimeline timeline, std::size_t packetStride)
        : m_timeline(std::move(timeline)), m_packetStride(packetStride) {}

    std::unique_ptr<MediaTsPacketParser> m_parser;
    std::unique_ptr<MediaTsPsiSectionAssembler> m_assembler;
    MediaTsEvidenceTimeline m_timeline;
    std::unique_ptr<MediaTsPesProvenanceTimeline> m_pesProvenance;
    std::optional<MediaTsProgramInventorySnapshot> m_inventory;
    std::optional<MediaTsContinuityEvent> m_pendingContinuityEvent;
    std::optional<MediaTsRuntimeBinding> m_runtimeBinding;
    bool m_inventoryDirty = false;
    mutable std::recursive_mutex m_mutex;
    std::uint64_t m_nextOffset = 0;
    std::uint64_t m_generation = 0;
    std::size_t m_packetStride;
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
        if (!m_finalError && m_observedAvio) {
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

::media::Result<MediaTsReadFrameEnvelope> MediaTsInputSession::readFrame()
{
    {
        std::lock_guard lock(m_sessionMutex);
        if (m_finalError) {
            return ::media::Result<MediaTsReadFrameEnvelope>::failure(
                *m_finalError);
        }
        if (m_closing || m_closed || !m_formatContext) {
            return ::media::Result<MediaTsReadFrameEnvelope>::failure(
                ::media::ErrorInfo::cancelled("MPEG-TS input session is closed"));
        }
        if (m_activeReads != 0) {
            return ::media::Result<MediaTsReadFrameEnvelope>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS input session permits one active reader"));
        }
        ++m_activeReads;
    }
    ReadLease guard(*this);

    if (m_durationProbe && !m_durationProbe->replayEmpty()) {
        auto replay = m_durationProbe->popReplay();
        if (!replay) {
            return ::media::Result<MediaTsReadFrameEnvelope>::failure(
                terminateDurationProbe(replay.error()));
        }
        return replay;
    }
    m_durationProbe.reset();
    return readFrameFromSource();
}

::media::Result<MediaTsSelectedPacketDurationEvidence>
MediaTsInputSession::probeSelectedPacketDurations(std::size_t frameLimit)
{
    {
        std::lock_guard lock(m_sessionMutex);
        if (m_finalError) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                *m_finalError);
        }
        if (m_closing || m_closed || !m_formatContext) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                ::media::ErrorInfo::cancelled("MPEG-TS input session is closed"));
        }
        if (frameLimit == 0) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS duration preflight frame limit must be positive"));
        }
        if (m_activeReads != 0 || m_durationProbe ||
            !m_runtimeContract.originBinding) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS duration preflight requires one configured unread session"));
        }
        const auto& binding = *m_runtimeContract.originBinding;
        const FFmpegInputStreamSnapshot* video = nullptr;
        const FFmpegInputStreamSnapshot* audio = nullptr;
        for (const auto& stream : m_streamSnapshots) {
            if (stream.index == binding.video.streamIndex) video = &stream;
            if (stream.index == binding.audio.streamIndex) audio = &stream;
        }
        if (!video || !audio) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS duration preflight selected stream snapshots are absent"));
        }
        auto created = MediaTsPreflightDurationProbe::create(
            binding.video, video->time.timeBase,
            binding.audio, audio->time.timeBase,
            frameLimit);
        if (!created) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                created.error());
        }
        m_durationProbe.emplace(std::move(created).value());
        ++m_activeReads;
    }
    ReadLease guard(*this);

    for (;;) {
        auto source = readFrameFromSource();
        if (!source) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                terminateDurationProbe(source.error()));
        }
        auto observed = m_durationProbe->buffer(std::move(source).value());
        if (!observed) {
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::failure(
                terminateDurationProbe(observed.error()));
        }
        if (observed.value()) {
            const auto evidence = *observed.value();
            return ::media::Result<MediaTsSelectedPacketDurationEvidence>::success(
                evidence);
        }
    }
}

::media::ErrorInfo MediaTsInputSession::terminateDurationProbe(
    ::media::ErrorInfo error)
{
    std::lock_guard lock(m_sessionMutex);
    if (!m_finalError) m_finalError = std::move(error);
    m_durationProbe.reset();
    return *m_finalError;
}

::media::Result<MediaTsReadFrameEnvelope>
MediaTsInputSession::readFrameFromSource()
{

    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Result<MediaTsReadFrameEnvelope>::failure(
            ::media::ErrorInfo::allocationFailed("failed to allocate MPEG-TS packet"));
    }
    const int result = av_read_frame(m_formatContext, packet.get());
    const auto observerStatus = m_observedAvio->status();
    if (!observerStatus) {
        return ::media::Result<MediaTsReadFrameEnvelope>::failure(observerStatus.error());
    }
    {
        std::lock_guard lock(m_sessionMutex);
        if (m_closing || m_closed || m_interruptState.cancelled()) {
            return ::media::Result<MediaTsReadFrameEnvelope>::failure(
                ::media::ErrorInfo::cancelled("MPEG-TS input read was cancelled"));
        }
    }
    if (result >= 0) {
        auto provenance = provenanceFor(*packet);
        if (!provenance) {
            return ::media::Result<MediaTsReadFrameEnvelope>::failure(provenance.error());
        }
        return ::media::Result<MediaTsReadFrameEnvelope>::success(MediaTsReadFrameEnvelope{
            MediaTsReadFrameState::Frame, std::move(packet), provenance.value()});
    }
    if (result == AVERROR(EAGAIN)) {
        return ::media::Result<MediaTsReadFrameEnvelope>::success(
            MediaTsReadFrameEnvelope{MediaTsReadFrameState::Waiting});
    }
    if (result == AVERROR_EOF) {
        auto finished = m_evidenceObserver->finish();
        if (!finished) {
            return ::media::Result<MediaTsReadFrameEnvelope>::failure(
                finished.error());
        }
        return ::media::Result<MediaTsReadFrameEnvelope>::success(
            MediaTsReadFrameEnvelope{MediaTsReadFrameState::EndOfStream});
    }
    if (result == AVERROR_EXIT) {
        return ::media::Result<MediaTsReadFrameEnvelope>::failure(
            ::media::ErrorInfo::cancelled("MPEG-TS input read was cancelled"));
    }
    return ::media::Result<MediaTsReadFrameEnvelope>::failure(
        ::media::ErrorInfo::ffmpegFailure("failed to read MPEG-TS frame", result));
}

::media::Status MediaTsInputSession::configureRuntimeBinding(
    const MediaTsRuntimeBinding& binding)
{
    if (m_runtimeBindingConfigured ||
        binding.originPolicy != MediaTsPacketOriginPolicy::PerStreamPesCarry ||
        binding.video.streamIndex < 0 || binding.audio.streamIndex < 0 ||
        binding.video.streamIndex == binding.audio.streamIndex ||
        binding.video.pid == 0 || binding.audio.pid == 0 ||
        binding.video.pid == binding.audio.pid ||
        binding.pcrPid == 0 || binding.pcrPid >= 0x1FFF ||
        binding.pesProvenanceCapacity != m_runtimeContract.pesProvenanceCapacity) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "invalid MPEG-TS runtime PES provenance binding"));
    }
    if (auto configured = m_evidenceObserver->configureRuntimeBinding(binding);
        !configured) {
        return configured;
    }
    auto cursor = MediaTsReturnedPesCursor::create(binding);
    if (!cursor) return ::media::Status::failure(cursor.error());
    m_returnedPesCursor = std::make_unique<MediaTsReturnedPesCursor>(
        std::move(cursor.value()));
    m_runtimeBindingConfigured = true;
    m_runtimeContract.originBinding = binding;
    return ::media::Status::success();
}

::media::Result<MediaTsPacketProvenance> MediaTsInputSession::provenanceFor(
    const AVPacket& packet)
{
    if (!m_runtimeBindingConfigured) {
        return ::media::Result<MediaTsPacketProvenance>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS runtime PES provenance binding is not configured"));
    }
    return m_returnedPesCursor->resolve(
        packet.stream_index, packet.pos,
        [this](std::uint64_t position, std::uint16_t pid) {
            return m_evidenceObserver->resolvePesAnchor(position, pid);
        },
        [this](const MediaTsPesProvenanceAnchor& anchor) {
            return m_evidenceObserver->pesStateForAnchor(anchor);
        });
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
    session->m_runtimeContract = MediaTsInputRuntimeContract{
        options.packetStride, options.evidenceCapacity,
        options.maximumPositionRegressionBytes, options.pesProvenanceCapacity,
        std::nullopt};
    auto observer = EvidenceObserver::create(options.packetStride, options.evidenceCapacity,
                                             options.pesProvenanceCapacity,
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

const MediaTsInputRuntimeContract& MediaTsInputSession::runtimeContract() const noexcept
{
    return m_runtimeContract;
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

::media::Status MediaTsInputSession::observePacketPosition(std::uint64_t packetPosition)
{
    return m_evidenceObserver->observePacketPosition(packetPosition);
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
