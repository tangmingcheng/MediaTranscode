#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpBatchStoragePlanner.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoParameterSetValidator.h"
#include "internal/graph/protocol/rtp/MediaRtpSourceClockProjection.h"
#include "internal/graph/sync/MediaAvInitialClockAdmission.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/MediaPreparedPacketStorage.h"
#include <limits>
#include <tuple>
#include <type_traits>
#include <system_error>
#include <string>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* text) {
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(text));
}
}

MediaPreparedRtpSourceStorageFacts MediaPreparedRtpSourceSession::storageFacts() noexcept
{
    return {sizeof(MediaPreparedRtpSourceSession), sizeof(Stream), alignof(Stream), sizeof(MediaPreparedRtpOutput),
        sizeof(MediaRtpIngressEventBuffer), sizeof(FFmpegPacketBuffer) + sizeof(AVPacket) + sizeof(AVBufferRef) + AV_INPUT_BUFFER_PADDING_SIZE, sizeof(MediaBufferRef)};
}

MediaPreparedRtpSourceSession::MediaPreparedRtpSourceSession(
    ConstructionKey, MediaPreparedRtpSourceSessionPlan&& plan,
    MediaPreparedRtpSourceBudgets&& budgets,
    std::shared_ptr<MediaMasterClock>&& clock,
    std::shared_ptr<MediaPreparationMemoryResource> resource,
    MediaLineageUniquePtr<MediaRtpClockGroupSession>&& group,
    WorkingSet&& workingSet)
    : m_workingSet(std::move(workingSet)), m_plan(std::move(plan)),
      m_budgets(std::move(budgets)), m_clock(std::move(clock)),
      m_ownerResource(std::move(resource)), m_group(std::move(group)),
      m_video(nullptr, MediaLineageObjectDeleter<Stream>{MediaPreparedLineageAllocation{m_ownerResource}}),
      m_audio(nullptr, MediaLineageObjectDeleter<Stream>{MediaPreparedLineageAllocation{m_ownerResource}})
{
    static_assert(std::is_nothrow_move_constructible_v<MediaPreparedRtpSourceSessionPlan>);
}

::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>>
MediaPreparedRtpSourceSession::create(MediaPreparedRtpSourceSessionPlan plan,
    MediaPreparedRtpSourceBudgets budgets,
    std::shared_ptr<MediaMasterClock> clock)
try {
    using Result = ::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>>;
    if (!budgets.root || !budgets.metadata || !budgets.videoPayload ||
        static_cast<bool>(budgets.audioPayload) != plan.audio.has_value() || !clock || !plan.videoSignaling ||
        budgets.root->snapshot().capacity != plan.storageBytes ||
        !plan.admission.sharedCapacityBytes || plan.storageBytes != plan.admission.sharedCapacityBytes ||
        !plan.admission.video.maximumUnits || plan.video.binder.acquiringCapacity != plan.admission.video.maximumUnits ||
        plan.admission.video.maximumBytes != plan.storageBytes ||
        (plan.audio && (!plan.admission.audio || !plan.admission.audio->maximumUnits ||
            plan.audio->binder.acquiringCapacity != plan.admission.audio->maximumUnits ||
            plan.admission.audio->maximumBytes != plan.storageBytes)) ||
        std::chrono::steady_clock::now() >= plan.deadline ||
        plan.audio.has_value() != plan.admission.audio.has_value() ||
        (plan.lifecycleMode != MediaAvSourceLifecycleMode::FailSessionOnSourceLoss &&
         plan.lifecycleMode != MediaAvSourceLifecycleMode::PreserveActivatedOutput))
        return Result::failure(::media::ErrorInfo::invalidArgument("Prepared RTP session requires owned storage, clock and current plan"));
    if (auto valid = budgets.validate(plan.storageBytes, plan.audio.has_value()); !valid)
        return Result::failure(valid.error());
    if (auto audio = plan.validateAudioRetention(); !audio) return Result::failure(audio.error());
    if (plan.canonicalRuntimeRetention) {
        if (auto status = plan.canonicalRuntimeRetention->validate(); !status) return Result::failure(status.error());
    }
    auto fixed = budgets.metadata->reserve(plan.fixedStorageBytes, MediaStorageAdmission::RequiredAllocation);
    if (!fixed) return Result::failure(fixed.error());
    WorkingSet workingSet{std::move(fixed).value(), std::nullopt, std::nullopt, std::nullopt, std::nullopt};
    auto group = MediaRtpClockGroupSession::create(plan.clockGroup);
    if (!group) return Result::failure(group.error());
    auto resource = MediaPreparationMemoryResource::create(budgets.metadata);
    if (!resource) return Result::failure(resource.error());
    auto groupOwner = makeMediaLineageUnique<MediaRtpClockGroupSession>(
        MediaPreparedLineageAllocation{resource.value()}, std::move(group).value());
    auto owner = std::allocate_shared<MediaPreparedRtpSourceSession>(
        std::pmr::polymorphic_allocator<MediaPreparedRtpSourceSession>{resource.value().get()},
        ConstructionKey{}, std::move(plan), std::move(budgets), std::move(clock), resource.value(),
        std::move(groupOwner), std::move(workingSet));
    const auto make = [&](MediaPreparedRtpSourceSessionStreamPlan& p)
        -> ::media::Result<MediaLineageUniquePtr<Stream>> {
        using R = ::media::Result<MediaLineageUniquePtr<Stream>>;
        if (p.containerStorage.maximumPrefixObjects != p.binder.acquiringCapacity ||
            p.containerStorage.maximumPendingLeases != p.envelope.maximumAccessUnitsPerPush)
            return R::failure(::media::ErrorInfo::invalidArgument("Prepared stream lacks its container capacity product"));
        if (p.unmigratedProtocolStorage.metadataBytes > p.unmigratedProtocolStorage.totalBytes ||
            p.unmigratedProtocolStorage.compressedBytes != p.unmigratedProtocolStorage.totalBytes - p.unmigratedProtocolStorage.metadataBytes)
            return R::failure(::media::ErrorInfo::invalidArgument("Protocol storage classification changed its total"));
        auto storage = owner->m_budgets.metadata->reserve(p.unmigratedProtocolStorage.metadataBytes, MediaStorageAdmission::RequiredAllocation);
        if (!storage) return R::failure(storage.error());
        const auto payload = p.depacketizer.streamKind == MediaStreamKind::Video
            ? owner->m_budgets.videoPayload : owner->m_budgets.audioPayload;
        auto compressed = payload->reserve(p.unmigratedProtocolStorage.compressedBytes,
            MediaStorageAdmission::RequiredAllocation);
        if (!compressed) return R::failure(compressed.error());
        auto resource = MediaPreparationMemoryResource::create(owner->m_budgets.metadata);
        if (!resource) return R::failure(resource.error());
        auto depay = MediaRtpDepacketizerFactory::create(p.depacketizer, p.envelope.maximumAccessUnitBytes,
            MediaPreparedLineageAllocation{resource.value()});
        if (!depay) return R::failure(depay.error());
        auto protocol = MediaRtpInputProtocolSession::create(p.depacketizer, p.envelope, p.reorder,
            p.clock, std::move(depay).value(), MediaPreparedLineageAllocation{resource.value()});
        if (!protocol) return R::failure(protocol.error());
        if (p.binder.videoTiming && p.binder.videoTiming->sourceIdentity) {
            auto identity = MediaCanonicalIdentity::copy(
                p.binder.videoTiming->sourceIdentity->view(),
                MediaLineageAllocator<char>{MediaPreparedLineageAllocation{resource.value()}});
            if (!identity) return R::failure(identity.error());
            p.binder.videoTiming->sourceIdentity = std::move(identity).value();
        }
        auto binder = MediaRtpPacketClockSession::create(p.binder, MediaPreparedLineageAllocation{resource.value()});
        if (!binder) return R::failure(binder.error());
        auto s = makeMediaLineageUnique<Stream>(MediaPreparedLineageAllocation{resource.value()}, p, resource.value(), payload);
        s->protocol = makeMediaLineageUnique<MediaRtpInputProtocolSession>(
            MediaPreparedLineageAllocation{resource.value()}, std::move(protocol).value());
        s->binder = makeMediaLineageUnique<MediaRtpPacketClockSession>(
            MediaPreparedLineageAllocation{resource.value()}, std::move(binder).value());
        auto& lease = p.depacketizer.streamKind == MediaStreamKind::Video
            ? owner->m_workingSet.video : owner->m_workingSet.audio;
        lease.emplace(std::move(storage).value());
        auto& payloadLease = p.depacketizer.streamKind == MediaStreamKind::Video
            ? owner->m_workingSet.videoCompressed : owner->m_workingSet.audioCompressed;
        payloadLease.emplace(std::move(compressed).value());
        return R::success(std::move(s));
    };
    auto video = make(owner->m_plan.video);
    if (!video) return Result::failure(video.error());
    owner->m_video = std::move(video).value();
    if (owner->m_plan.audio) {
        auto audio = make(*owner->m_plan.audio);
        if (!audio) return Result::failure(audio.error());
        owner->m_audio = std::move(audio).value();
    }
    return Result::success(std::move(owner));
} catch (const std::system_error& error) {
    return ::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>>::failure(
        ::media::ErrorInfo::ioFailure(error.what()));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>>::failure(
        ::media::ErrorInfo::allocationFailed("Prepared RTP session allocation failed"));
}

::media::Result<MediaPreparedRtpSourceSession::Stream*> MediaPreparedRtpSourceSession::stream(MediaStreamKind kind)
{
    auto* result = kind == MediaStreamKind::Video ? m_video.get()
        : kind == MediaStreamKind::Audio ? m_audio.get() : nullptr;
    if (!result) return ::media::Result<Stream*>::failure(
        ::media::ErrorInfo::invalidArgument("Prepared RTP session received an unplanned stream"));
    return ::media::Result<Stream*>::success(result);
}

::media::Status MediaPreparedRtpSourceSession::consume(MediaStreamKind kind,
    MediaRtpUdpChannel channel, std::span<const std::uint8_t> bytes,
    std::int64_t observedAtNs, std::uint64_t ordinal)
try {
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    auto& s = *selected.value();
    const auto consume = [&]() -> ::media::Status {
        if (observedAtNs <= 0 || ordinal == 0 || ordinal <= s.ordinal ||
            (!m_bootstrap && ordinal - s.ordinal != 1) ||
            (m_bootstrap && observedAtNs < m_bootstrapObservedAtNs) ||
            bytes.empty() || bytes.size() > s.plan.workingSet.maximumDatagramBytes)
            return invalid("Prepared RTP ingress requires ordered bootstrap or contiguous live ordinal and admitted datagram");
        if (!m_frozen && std::chrono::steady_clock::now() >= m_plan.deadline)
            return invalid("Prepared RTP session exceeded its original open deadline");
        // Preserve capture time for reorder, activity and SR/CNAME provenance.
        // Live evidence is evaluated at processing time, independently of the
        // order in which the two channel workers acquired the session lock.
        const auto evaluationAtNs = m_bootstrap ? observedAtNs : mediaSteadyClockNowNs();
        if (channel == MediaRtpUdpChannel::Rtp) {
            auto packet = MediaRtpPacketParser::parse(bytes);
            if (!packet) return ::media::Status::failure(packet.error());
            if (s.activity && packet.value().payloadType == s.plan.depacketizer.payloadType)
                s.activity->observe(observedAtNs);
            if (auto status = s.protocol->observeRtp(std::move(packet).value(), observedAtNs); !status) return status;
            if (auto status = drain(s, evaluationAtNs); !status) return status;
            // A matching first RTP packet may remain in reorder; evidence must
            // still progress without waiting for another SR or completed AU.
            if (auto status = evidence(s, evaluationAtNs); !status) return status;
        } else {
            auto observation = s.protocol->prepareRtcp(bytes, observedAtNs, evaluationAtNs);
            if (!observation) return ::media::Status::failure(observation.error());
            auto prepared = std::move(observation).value();
            auto rejection = std::move(prepared.tracker.rejection);
            const bool invalidated = prepared.tracker.invalidated;
            const bool selectedClock = std::holds_alternative<MediaRtcpClockEvidence>(prepared.tracker.output);
            MediaBufferRef output;
            if (!std::holds_alternative<std::monostate>(prepared.tracker.output)) {
                if (s.nextSequence == 0 || s.nextSequence == std::numeric_limits<std::uint64_t>::max())
                    return invalid("Prepared RTP ingress sequence exhausted");
                if (auto* value = std::get_if<MediaRtcpClockEvidence>(&prepared.tracker.output))
                    output = std::allocate_shared<MediaRtpIngressEventBuffer>(
                        std::pmr::polymorphic_allocator<MediaRtpIngressEventBuffer>{s.resource.get()},
                        std::move(*value), s.nextSequence);
                else
                    output = std::allocate_shared<MediaRtpIngressEventBuffer>(
                        std::pmr::polymorphic_allocator<MediaRtpIngressEventBuffer>{s.resource.get()},
                        std::get<MediaRtpClockInvalidation>(prepared.tracker.output), s.nextSequence);
            }
            // The session lock keeps this reserved FIFO tail invisible until
            // the protocol delta commits. No output allocation follows commit.
            const auto previousHighWater = s.outputHighWater;
            const bool queued = m_frozen && output;
            if (queued) {
                if (auto status = event(s, output, selectedClock, evaluationAtNs); !status) return status;
            }
            auto committed = s.protocol->commitRtcp(std::move(prepared));
            if (!committed) {
                if (queued) {
                    s.outputBytes -= s.controls.back().admissionBytes;
                    s.controls.pop_back();
                    s.outputHighWater = previousHighWater;
                }
                return committed;
            }
            if (invalidated) s.pending.clear();
            if (rejection) return ::media::Status::failure(std::move(*rejection));
            if (output) {
                ++s.nextSequence;
                if (!queued) {
                    if (auto status = event(s, std::move(output), selectedClock, evaluationAtNs); !status) return status;
                }
            }
        }
        s.ordinal = ordinal;
        if (m_bootstrap) m_bootstrapObservedAtNs = observedAtNs;
        return ::media::Status::success();
    };
    auto status = consume();
    if (!status) m_error = status.error();
    for (auto* notify : {m_video.get(), m_audio.get()}) {
        if (notify && notify->wakeup) notify->wakeup->notify();
        if (notify && notify->controlWakeup) notify->controlWakeup->notify();
    }
    m_ready.notify_all();
    return status;
} catch (const std::bad_alloc&) {
    auto error = ::media::ErrorInfo::allocationFailed("Prepared RTP receive allocation failed");
    fail(error);
    return ::media::Status::failure(std::move(error));
}

::media::Status MediaPreparedRtpSourceSession::queue(Stream& s, MediaPreparedRtpOutputPort port, MediaBufferRef buffer)
{
    if (!mediaPreparedRtpOutputPortName(port))
        return invalid("Prepared RTP output has an unknown protocol port");
    // Object count and bytes are independently bounded by the typed handoff plan.
    if ((s.packets.size() + s.controls.size()) >= s.plan.maximumProtocolOutputObjects)
        return invalid("Prepared RTP output object admission exhausted");
    std::uint64_t bytes = sizeof(MediaPreparedRtpOutput);
    if (const auto* e = dynamic_cast<const MediaRtpIngressEventBuffer*>(buffer.get())) {
        bytes += sizeof(MediaRtpIngressEventBuffer);
        if (e->clockEvidence()) bytes += e->clockEvidence()->cname.size();
    } else {
        const auto payload = buffer->payloadFootprintBytes();
        const auto packetBytes = storageFacts().packetBytes;
        if (!payload || *payload > std::numeric_limits<std::size_t>::max() - bytes - packetBytes)
            return invalid("Prepared RTP output lacks representable payload footprint");
        bytes += *payload + packetBytes;
    }
    if (bytes > s.plan.maximumProtocolOutputBytes ||
        s.outputBytes > s.plan.maximumProtocolOutputBytes - bytes)
        return invalid("Prepared RTP output byte admission exhausted");
    auto& output = port == MediaPreparedRtpOutputPort::Packet ? s.packets : s.controls;
    output.push_back({port, std::move(buffer), static_cast<std::size_t>(bytes)});
    s.outputHighWater = (std::max)(s.outputHighWater, (s.packets.size() + s.controls.size()));
    s.outputBytes += static_cast<std::size_t>(bytes);
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::finishBootstrap(std::uint64_t videoFrontier,
    std::optional<std::uint64_t> audioFrontier)
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (!m_bootstrap || m_frozen || videoFrontier < m_video->ordinal ||
        audioFrontier.has_value() != static_cast<bool>(m_audio) ||
        (m_audio && *audioFrontier < m_audio->ordinal))
        return invalid("Prepared RTP bootstrap requires exact observed per-stream frontiers");
    if (std::chrono::steady_clock::now() >= m_plan.deadline)
        return invalid("Prepared RTP bootstrap exceeded its original open deadline");
    // Bootstrap may exclude accepted-but-filtered RTCP entries. Keep the actual
    // receiver frontier, rather than renumbering the retained datagram prefix.
    m_video->ordinal = videoFrontier;
    if (m_audio) m_audio->ordinal = *audioFrontier;
    m_bootstrap = false;
    auto result = publish(mediaSteadyClockNowNs());
    if (!result) m_error = result.error();
    m_ready.notify_all();
    return result;
}

::media::Status MediaPreparedRtpSourceSession::event(Stream& s, MediaBufferRef buffer,
    bool selectedClock, std::int64_t now)
{
    const auto& e = *static_cast<const MediaRtpIngressEventBuffer*>(buffer.get());
    if (e.sequence() == 0 || e.sequence() == std::numeric_limits<std::uint64_t>::max())
        return invalid("Prepared RTP ingress sequence exhausted");
    if (m_frozen) {
        // CNAME backing retains its original typed allocation resource.
        return queue(s, selectedClock ? MediaPreparedRtpOutputPort::Clock : MediaPreparedRtpOutputPort::Event, std::move(buffer));
    }
    const MediaRtpIngressEventView view{e.kind(), e.sequence(), e.clockEvidence(), e.discontinuity(),
        e.discontinuityGeneration(), e.clockObservation(), e.clockInvalidation()};
    auto consumed = m_group->consume(view, s.plan.depacketizer.streamKind, selectedClock, now);
    if (!consumed) return ::media::Status::failure(consumed.error());
    if (consumed.value().rejected)
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode,
            "prepared_rtp_clock_group_rejected error=" + consumed.value().rejected->message);
    return publish(consumed.value().observationTimeNs);
}

::media::Status MediaPreparedRtpSourceSession::evidence(Stream& s, std::int64_t now)
{
    auto value = s.protocol->takeEvidenceUpdate(now);
    if (!value) return ::media::Status::failure(value.error());
    if (!value.value()) return ::media::Status::success();
    return event(s, std::allocate_shared<MediaRtpIngressEventBuffer>(
                std::pmr::polymorphic_allocator<MediaRtpIngressEventBuffer>{s.resource.get()}, std::move(*value.value()), s.nextSequence++), true, now);
}

::media::Status MediaPreparedRtpSourceSession::publish(std::int64_t now)
{
    auto snapshot = m_group->snapshot(now);
    if (!snapshot) return ::media::Status::failure(snapshot.error());
    auto projected = projectMediaRtpSourceClockState(snapshot.value(), m_plan.video.binder.members);
    if (!projected) return ::media::Status::failure(projected.error());
    if (auto status = validateMediaInitialSourceClockAvailability(
            m_plan.lifecycleMode, false,
            projected.value().readiness, projected.value().discontinuity); !status) return status;
    if (snapshot.value().state == MediaRtpClockGroupState::Locked) {
        if (auto status = validateMediaInitialClockGeneration(
                snapshot.value().groupGeneration, m_plan.initialGeneration); !status) return status;
    }
    for (auto* s : {m_video.get(), m_audio.get()}) {
        if (!s) continue;
        if (auto status = s->binder->acceptClock(snapshot.value(), s->plan.binder.members); !status) return status;
        if (auto status = bindAvailable(*s); !status) return status;
    }
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::bindAvailable(Stream& s)
{
    auto& binder = *s.binder;
    while (!binder.m_acquiringPackets->empty()) {
        auto disposition = binder.classifyPacket(binder.m_acquiringPackets->front());
        if (!disposition) return ::media::Status::failure(disposition.error());
        if (disposition.value() == MediaRtpPacketClockSession::PacketDisposition::AwaitClock) break;
        auto packet = std::move(binder.m_acquiringPackets->front());
        binder.m_acquiringPackets->pop_front();
        if (disposition.value() == MediaRtpPacketClockSession::PacketDisposition::Retired) continue;
        auto bound = binder.bindPacket(std::move(packet));
        if (!bound) return ::media::Status::failure(bound.error());
        if (bound.value()) {
            if (s.prefix->packets.size() >= s.plan.containerStorage.maximumPrefixObjects)
                return invalid("Prepared canonical prefix exceeds planned container capacity");
            s.prefix->packets.push_back(std::move(bound).value());
            ++m_revision;
        }
    }
    if (binder.m_acquiringPackets->empty()) binder.m_acquisitionDeadline->clear();
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::drain(Stream& s, std::int64_t now)
{
    while (s.protocol->hasPending()) {
        const auto& item = s.protocol->pendingFront();
        if (const auto* packet = std::get_if<MediaRtpPacket>(&item)) {
            if (auto status = evidence(s, now); !status) return status;
            auto action = s.protocol->preparePacket(*packet, s.pending.size());
            if (!action) return ::media::Status::failure(action.error());
            if (const auto* rejected = std::get_if<MediaRtpPacketRejection>(&action.value())) {
                auto failure = s.protocol->commitPacketRejection(*rejected);
                s.pending.clear();
                return failure;
            }
            const auto& allocation = std::get<MediaRtpPacketAllocationAction>(action.value());
            if (!allocation.drop) {
                if (allocation.releaseExisting) { s.pending.clear(); s.protocol->releasePayloadState(); }
                if (s.pending.empty()) {
                    if (allocation.reserveBytes.size() > s.plan.containerStorage.maximumPendingLeases)
                        return invalid("Prepared protocol pending leases exceed planned container capacity");
                    for (auto bytes : allocation.reserveBytes) {
                        auto storage = preparedMediaPacketStorageBytes(bytes, MediaPacketStorageExtent::Payload);
                        if (!storage) return ::media::Status::failure(storage.error());
                        auto lease = s.payloadBudget->reserve(storage.value(), MediaStorageAdmission::RequiredAllocation);
                        if (!lease) return ::media::Status::failure(lease.error());
                        s.pending.push_back(std::allocate_shared<MediaPreparationStorageLease>(
                            std::pmr::polymorphic_allocator<MediaPreparationStorageLease>{s.resource.get()}, std::move(lease).value()));
                    }
                }
                auto completion = s.protocol->commitPacket(*packet, s.pending.size());
                if (!completion) return ::media::Status::failure(completion.error());
                auto& units = completion.value().media.accessUnits;
                for (std::size_t i = 0; i < units.size(); ++i) {
                    auto& unit = units[i];
                    // makeRtpAccessUnit owns an av_new_packet allocation. Settle
                    // its backing storage, including padding, before any owner
                    // is published. Protocol assembly has a separate lease.
                    if (!unit.packet || unit.packet->size <= 0 || !unit.packet->buf ||
                        unit.packet->data != unit.packet->buf->data ||
                        unit.packet->side_data_elems != 0 ||
                        unit.packet->buf->size < AV_INPUT_BUFFER_PADDING_SIZE ||
                        static_cast<std::size_t>(unit.packet->size) >
                            unit.packet->buf->size - AV_INPUT_BUFFER_PADDING_SIZE)
                        return invalid("Prepared RTP AU lacks its independent padded allocation");
                    if (i >= s.pending.size() || !s.pending[i] || s.pending[i].use_count() != 1)
                        return invalid("Prepared RTP AU lacks its exclusive reservation");
                    auto storage = preparedMediaPacketStorageBytes(unit.packet->buf->size, MediaPacketStorageExtent::PaddedAllocation);
                    if (!storage) return ::media::Status::failure(storage.error());
                    if (auto settled = s.pending[i]->shrinkTo(storage.value()); !settled)
                        return settled;
                    if (s.plan.depacketizer.streamKind == MediaStreamKind::Video) {
                        if (auto status = MediaRtpVideoParameterSetValidator::validateBinding(
                                {unit.packet->data, static_cast<std::size_t>(unit.packet->size)}, *m_plan.videoSignaling, false); !status) return status;
                    }
                    if (!s.protocol->admitAccessUnit((unit.packet->flags & AV_PKT_FLAG_KEY) != 0).accepted) continue;
                    const auto& retention = s.plan.depacketizer.streamKind == MediaStreamKind::Video
                        ? m_plan.admission.video : *m_plan.admission.audio;
                    if (!m_frozen && s.admittedUnits >= retention.maximumUnits) {
                        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                            "Prepared RTP AU prefix admission exhausted: admitted=" +
                            std::to_string(s.admittedUnits) + " maximum=" +
                            std::to_string(retention.maximumUnits) + " prefix=" +
                            std::to_string(s.prefix->packets.size()) + " acquiring=" +
                            std::to_string(s.binder->m_acquiringPackets->size()) +
                            " canonical_retention=" +
                            std::to_string(m_plan.canonicalRuntimeRetention.has_value()) +
                            " remaining_ns=" + std::to_string(
                                std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    m_plan.deadline - std::chrono::steady_clock::now()).count())));
                    }
                    if (s.nextSequence == std::numeric_limits<std::uint64_t>::max())
                        return invalid("Prepared RTP ingress sequence exhausted");
                    auto buffer = FFmpegBufferFactory::wrapPacket(*s.resource, std::move(unit.packet), s.plan.depacketizer.streamKind,
                        std::nullopt, std::nullopt, MediaRtpPacketProvenance{packet->ssrc,
                            s.protocol->generation(), s.nextSequence++});
                    if (!buffer) return ::media::Status::failure(buffer.error());
                    if (auto status = buffer.value()->attachPreparationPayloadStorage(std::move(s.pending[i])); !status) return status;
                    buffer.value()->setTimeDescriptor(MediaTimeDescriptor{unit.timeBase});
                    MediaFormatDescriptor format;
                    format.streamKind = s.plan.depacketizer.streamKind; format.streamIndex = 0;
                    format.time.timeBase = unit.timeBase; format.isInput = true; format.isRealtime = true;
                    buffer.value()->setFormatDescriptor(std::move(format));
                    if (m_frozen) {
                        if (auto status = queue(s, MediaPreparedRtpOutputPort::Packet, std::move(buffer).value()); !status) return status;
                    } else {
                        auto observed = m_clock->now();
                        if (!observed) return ::media::Status::failure(observed.error());
                        if (auto status = s.binder->bufferAcquiring(std::move(buffer).value(), observed.value()); !status) return status;
                        ++s.admittedUnits;
                        if (auto status = bindAvailable(s); !status) return status;
                    }
                }
                if (!units.empty() || completion.value().releaseReservations) {
                    // Rejected units still own their packets. Destroy them
                    // before returning the associated storage reservations.
                    units.clear();
                    s.pending.clear();
                }
            }
        } else {
            const auto& [discontinuity, generation] = std::get<std::pair<MediaRtpDiscontinuity, std::uint64_t>>(item);
            // SSRC identifies a sender generation, while PT/configuration binds
            // the media contract. Only the planner's continuous-output policy
            // permits the existing generation transition after state handoff.
            // Initial activation remains guarded by the runtime source gate.
            if (discontinuity.reason == MediaRtpDiscontinuityReason::PayloadTypeChanged)
                return ::media::Status::failure(::media::ErrorInfo::unsupported(
                    "RTP payload type changed; the prepared media contract requires replanning"));
            if (discontinuity.reason == MediaRtpDiscontinuityReason::SsrcChanged &&
                (!m_frozen || m_plan.lifecycleMode != MediaAvSourceLifecycleMode::PreserveActivatedOutput))
                return ::media::Status::failure(::media::ErrorInfo::unsupported(
                    "RTP source identity changed before an authorized runtime generation transition"));
            if (auto status = s.protocol->observeDiscontinuity(discontinuity.reason, generation); !status) return status;
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode,
                "rtp_discontinuity stream=" + std::string(s.plan.depacketizer.streamKind == MediaStreamKind::Video ? "video" : "audio") +
                " reason=" + std::to_string(static_cast<int>(discontinuity.reason)) +
                " first_missing=" + std::to_string(discontinuity.firstMissingSequence) +
                " resumed=" + std::to_string(discontinuity.resumedSequence) +
                " generation_before=" + std::to_string(generation) +
                " generation_after=" + std::to_string(s.protocol->generation()));
            s.pending.clear();
            if (auto status = event(s, std::allocate_shared<MediaRtpIngressEventBuffer>(
                std::pmr::polymorphic_allocator<MediaRtpIngressEventBuffer>{s.resource.get()}, discontinuity,
                    s.protocol->generation(), s.nextSequence++), false, now); !status) return status;
        }
        s.protocol->consumeFront();
    }
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::tickLocked(Stream& s, std::int64_t now)
{
    if (now <= 0) return invalid("Prepared RTP tick requires positive observed time");
    if (!m_frozen) {
        if (std::chrono::steady_clock::now() >= m_plan.deadline)
            return invalid("Prepared RTP tick exceeded its original open deadline");
        auto running = m_clock->now();
        if (!running) return ::media::Status::failure(running.error());
        if (auto status = s.binder->m_acquisitionDeadline->preflight(running.value()); !status) return status;
    }
    if (auto status = s.protocol->expire(std::chrono::steady_clock::time_point(std::chrono::nanoseconds(now))); !status) return status;
    if (auto status = drain(s, now); !status) return status;
    if (auto status = evidence(s, now); !status) return status;
    auto transition = s.protocol->transition(now);
    if (!transition) return ::media::Status::failure(transition.error());
    if (transition.value()) {
        const auto loss = s.protocol->clockConfig().lossPolicy;
        if (loss != MediaRtpClockLossPolicy::WaitForEvidence && loss != MediaRtpClockLossPolicy::InvalidateAndWait)
            return ::media::Status::failure(::media::ErrorInfo::ioFailure("Prepared RTP source clock evidence expired"));
        s.protocol->resetAssembly(); s.pending.clear();
        if (transition.value()->invalidation) {
            if (auto status = event(s, std::allocate_shared<MediaRtpIngressEventBuffer>(
                std::pmr::polymorphic_allocator<MediaRtpIngressEventBuffer>{s.resource.get()},
                    *transition.value()->invalidation, s.nextSequence++), false, now); !status) return status;
        }
    }
    return m_frozen ? ::media::Status::success() : publish(now);
}

::media::Status MediaPreparedRtpSourceSession::tick(MediaStreamKind kind, std::int64_t now)
try
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    if (now <= 0) return invalid("Prepared RTP timer requires positive observation");
    if (m_bootstrap) return invalid("Prepared RTP live timer precedes bootstrap handoff");
    auto status = tickLocked(*selected.value(), mediaSteadyClockNowNs());
    if (!status) m_error = status.error();
    for (auto* notify : {m_video.get(), m_audio.get()}) {
        if (notify && notify->wakeup) notify->wakeup->notify();
        if (notify && notify->controlWakeup) notify->controlWakeup->notify();
    }
    m_ready.notify_all();
    return status;
} catch (const std::bad_alloc&) {
    auto error = ::media::ErrorInfo::allocationFailed("Prepared RTP timer allocation failed");
    fail(error);
    return ::media::Status::failure(std::move(error));
}

void MediaPreparedRtpSourceSession::fail(::media::ErrorInfo error)
{
    std::scoped_lock lock(m_mutex);
    if (!m_error) m_error = std::move(error);
    for (auto* s : {m_video.get(), m_audio.get()}) {
        if (s && s->wakeup) s->wakeup->notify();
        if (s && s->controlWakeup) s->controlWakeup->notify();
    }
    m_ready.notify_all();
}

::media::Status MediaPreparedRtpSourceSession::status() const
{
    std::scoped_lock lock(m_mutex);
    return m_error ? ::media::Status::failure(*m_error) : ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::validatePreparationPhase() const
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_bootstrap || m_frozen)
        return invalid("Continuous RTP source is outside its mutable preparation phase");
    if (std::chrono::steady_clock::now() >= m_plan.deadline)
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "Continuous RTP source preparation reached its original deadline"));
    return ::media::Status::success();
}

MediaPreparedRtpStorageReservation MediaPreparedRtpSourceSession::preparationStorage() const
{
    return {m_budgets.metadata, m_plan.storageBytes, m_plan.audio ? 2u : 1u,
        "continuous-RTP-source-protocol+retained-AU+handoff-finite-admission", m_budgets};
}

::media::Status MediaPreparedRtpSourceSession::activateStream(MediaStreamKind kind,
    std::shared_ptr<MediaNodeWakeup> wakeup, std::shared_ptr<MediaInputActivity> activity)
{
    std::scoped_lock lock(m_mutex);
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    if (!m_frozen || !wakeup || !activity || selected.value()->wakeup)
        return invalid("Prepared RTP runtime activation requires one frozen stream and wakeup");
    selected.value()->wakeup = std::move(wakeup);
    selected.value()->activity = std::move(activity);
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::activateControl(
    MediaStreamKind kind, std::shared_ptr<MediaNodeWakeup> wakeup)
{
    std::scoped_lock lock(m_mutex);
    auto selected = stream(kind);
    if (!selected) return ::media::Status::failure(selected.error());
    if (!m_frozen || !wakeup || selected.value()->controlWakeup)
        return invalid("Prepared RTP control activation requires one frozen stream consumer");
    selected.value()->controlWakeup = std::move(wakeup);
    if (m_error || !selected.value()->controls.empty()) selected.value()->controlWakeup->notify();
    return ::media::Status::success();
}

void MediaPreparedRtpSourceSession::deactivateControl(
    MediaStreamKind kind, const std::shared_ptr<MediaNodeWakeup>& wakeup) noexcept
{
    std::scoped_lock lock(m_mutex);
    auto* selected = kind == MediaStreamKind::Video ? m_video.get()
        : kind == MediaStreamKind::Audio ? m_audio.get() : nullptr;
    if (selected && selected->controlWakeup == wakeup &&
        !selected->controlWakeup.owner_before(wakeup) && !wakeup.owner_before(selected->controlWakeup))
        selected->controlWakeup.reset();
}

::media::Result<std::uint64_t> MediaPreparedRtpSourceSession::purgeMedia(MediaStreamKind kind)
{
    std::scoped_lock lock(m_mutex);
    using R = ::media::Result<std::uint64_t>;
    auto selected = stream(kind);
    if (!selected) return R::failure(selected.error());
    if (!m_frozen) return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP purge precedes handoff"));
    auto& s = *selected.value();
    std::erase_if(s.packets, [&](const auto& item) {
        if (item.port != MediaPreparedRtpOutputPort::Packet) return false;
        s.outputBytes -= item.admissionBytes;
        return true;
    });
    s.protocol->resetAssembly(); s.pending.clear(); s.protocol->requireKeyframe();
    return R::success(s.nextSequence - 1);
}

::media::Result<int> MediaPreparedRtpSourceSession::receiveTimeoutMs(MediaStreamKind kind,
    std::int64_t now, int maximum)
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Result<int>::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return ::media::Result<int>::failure(selected.error());
    return selected.value()->protocol->receiveTimeoutMs(
        std::chrono::steady_clock::time_point(std::chrono::nanoseconds(now)), now, maximum);
}

::media::Result<MediaPreparedRtpAccessUnitSnapshot> MediaPreparedRtpSourceSession::probeSnapshot() const
{
    std::scoped_lock lock(m_mutex);
    using R = ::media::Result<MediaPreparedRtpAccessUnitSnapshot>;
    if (m_error) return R::failure(*m_error);
    if (m_frozen) return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP probe ended at handoff"));
    if (m_video->prefix->packets.size() > m_video->plan.containerStorage.maximumPrefixObjects)
        return R::failure(::media::ErrorInfo::invalidArgument("Prepared probe exceeds planned snapshot capacity"));
    try {
        auto resource = MediaPreparationMemoryResource::create(m_budgets.metadata);
        if (!resource) return R::failure(resource.error());
        // PMR admits the actual allocation request before allocating. Its
        // allocation owner retains fees through deallocation, including any
        // simultaneous old/new blocks if the container ever grows.
        auto references = makeMediaLineageUnique<std::pmr::vector<MediaBufferRef>>(
            MediaPreparedLineageAllocation{resource.value()},
            m_video->prefix->packets.begin(), m_video->prefix->packets.end());
        return R::success({std::move(resource).value(), std::move(references), m_revision});
    } catch (const std::bad_alloc&) {
        return R::failure(::media::ErrorInfo::allocationFailed(
            "Prepared RTP probe reference allocation failed"));
    }
}

::media::Status MediaPreparedRtpSourceSession::waitForAccessUnits(std::uint64_t revision,
    std::chrono::steady_clock::time_point deadline, std::stop_token stop)
{
    std::stop_callback cancelled(stop, [this] { std::scoped_lock lock(m_mutex); m_ready.notify_all(); });
    std::unique_lock lock(m_mutex);
    if (deadline > m_plan.deadline) return invalid("Prepared RTP probe wait exceeds original deadline");
    m_ready.wait_until(lock, deadline, [&] { return m_error || m_frozen || stop.stop_requested() || m_revision > revision; });
    if (m_error) return ::media::Status::failure(*m_error);
    if (stop.stop_requested()) return ::media::Status::failure(::media::ErrorInfo::cancelled("Prepared RTP probe cancelled"));
    if (m_frozen || std::chrono::steady_clock::now() >= deadline) return invalid("Prepared RTP probe wait ended before evidence");
    return ::media::Status::success();
}

::media::Status MediaPreparedRtpSourceSession::prepareFreezeLocked()
{
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_frozen) return invalid("Prepared RTP source can hand off only once");
    if (m_bootstrap) return invalid("Prepared RTP source cannot freeze before bootstrap handoff");
    if (std::chrono::steady_clock::now() >= m_plan.deadline)
        return invalid("Prepared RTP handoff exceeded original deadline");
    if (auto status = publish(mediaSteadyClockNowNs()); !status) {
        m_error = status.error(); return status;
    }
    return ::media::Status::success();
}

::media::Result<MediaPreparedRtpStorageSnapshot> MediaPreparedRtpSourceSession::storageSnapshotLocked() const
{
    const auto observe = [](const Stream& stream) {
        const auto& binder = *stream.binder;
        return MediaPreparedRtpStreamStorageSnapshot{
            stream.admittedUnits, stream.prefix->packets.size(), binder.m_acquiringPackets->size(),
            static_cast<std::size_t>(!!binder.m_videoLookahead) + static_cast<std::size_t>(!!binder.m_pendingTerminal),
            stream.pending.size(), stream.pending.capacity(), (stream.packets.size() + stream.controls.size()),
            {stream.admittedUnits, binder.m_acquiringHighWater, stream.outputHighWater}};
    };
    using R = ::media::Result<MediaPreparedRtpStorageSnapshot>;
    const std::array owners{m_budgets.metadata, m_budgets.videoPayload, m_budgets.audioPayload};
    std::array<MediaPreparationStorageSnapshot, 3> budgets{};
    const std::size_t count = m_audio ? 3 : 2;
    auto partition = MediaPreparationStorageBudget::snapshotPartition(
        std::span{owners}.first(count), std::span{budgets}.first(count));
    if (!partition) return R::failure(partition.error());
    MediaPreparedRtpStorageSnapshot snapshot{observe(*m_video), std::nullopt, m_revision,
        {budgets[0], budgets[1], m_audio ? std::optional{budgets[2]} : std::nullopt},
        m_budgets, {}, m_video->resource};
    if (m_audio) snapshot.audio.emplace(observe(*m_audio));
    return R::success(std::move(snapshot));
}

::media::Status MediaPreparedRtpSourceSession::validateStorageAdmissionLocked(
    const MediaPreparedRtpStorageAdmission& product, const MediaPreparedRtpStorageSnapshot& snapshot,
    std::size_t batchStorageBytes) const
{
    if (product.budgets.root != m_budgets.root || product.budgets.metadata != m_budgets.metadata ||
        product.budgets.videoPayload != m_budgets.videoPayload || product.budgets.audioPayload != m_budgets.audioPayload ||
        snapshot.budgets.sealed() || product.sharedCapacityBytes != m_plan.storageBytes || product.owner != snapshot.owner ||
        !product.totalBytes || product.totalBytes > m_plan.storageBytes ||
        batchStorageBytes > snapshot.budgets.actualBytes() ||
        product.totalBytes < snapshot.budgets.actualBytes() - batchStorageBytes ||
        product.fixedBytes > product.totalBytes ||
        product.protocolBytes > product.totalBytes - product.fixedBytes ||
        product.startupBytes != product.totalBytes - product.fixedBytes - product.protocolBytes ||
        product.audio.has_value() != static_cast<bool>(m_audio))
        return invalid("Storage admission changed source budget, owner or membership");
    const auto validateBounds = [&](const MediaPreparedRtpStreamStorageAdmission& value,
        const MediaPreparedRtpStreamStorageSnapshot& held,
        const MediaPreparedRtpSourceSessionStreamPlan& stream) -> ::media::Status {
        auto floor = MediaPreparedRtpSourceStoragePlanner::minimumBounds(stream, held);
        if (!floor) return ::media::Status::failure(floor.error());
        if (value.compressed.maximumBytes != m_plan.storageBytes ||
            value.bounds.prefixObjects != value.compressed.maximumUnits ||
            value.bounds.prefixObjects < floor.value().prefixObjects ||
            value.bounds.acquiringObjects < floor.value().acquiringObjects ||
            value.bounds.outputObjects < floor.value().outputObjects ||
            value.storage.maximumPrefixObjects != value.bounds.prefixObjects ||
            value.storage.maximumPendingLeases != stream.envelope.maximumAccessUnitsPerPush ||
            !value.storage.peakBytes)
            return invalid("Storage admission cannot discard owned input or serialized completion capacity");
        return ::media::Status::success();
    };
    if (auto status = validateBounds(product.video, snapshot.video, m_plan.video); !status) return status;
    if (m_audio) {
        if (auto status = validateBounds(*product.audio, *snapshot.audio, *m_plan.audio); !status) return status;
    }
    if (product.startup.audioUnits.has_value() != static_cast<bool>(m_audio) ||
        product.startup.audioBytes.has_value() != static_cast<bool>(m_audio) ||
        product.startup.audioUnitBytes.has_value() != static_cast<bool>(m_audio) ||
        (m_audio && (!m_plan.audioRetention || !m_plan.audioRetention->valid() ||
            product.startup.audioUnits != m_plan.audioRetention->maximumUnits ||
            product.startup.audioBytes != m_plan.audioRetention->maximumBytes ||
            product.startup.audioUnitBytes != m_plan.audioRetention->maximumUnitBytes)))
        return invalid("Storage admission changed independent audio startup retention");
    const auto preserveBacking = [](const MediaPreparedRtpSourceSessionStreamPlan& plan,
        const MediaPreparedRtpStreamStorageSnapshot& held,
        const MediaPreparedRtpContainerStoragePlan& candidate, bool probe) -> ::media::Status {
        auto history = MediaPreparedRtpSourceStoragePlanner::containers(plan, held.history, probe);
        if (!history) return ::media::Status::failure(history.error());
        const auto& prior = history.value();
        if (candidate.outputPeakBytes < prior.outputPeakBytes ||
            candidate.acquiringPeakBytes < prior.acquiringPeakBytes ||
            candidate.prefixPeakBytes < prior.prefixPeakBytes ||
            candidate.pendingPeakBytes < prior.pendingPeakBytes ||
            candidate.pendingLeaseControlBytes < prior.pendingLeaseControlBytes ||
            candidate.protocolPendingPeakBytes < prior.protocolPendingPeakBytes ||
            candidate.protocolRequestPeakBytes < prior.protocolRequestPeakBytes ||
            candidate.completionPeakBytes < prior.completionPeakBytes ||
            candidate.aacHeaderPeakBytes < prior.aacHeaderPeakBytes ||
            candidate.snapshotBytes < prior.snapshotBytes)
            return invalid("Storage admission lost retained container backing");
        return ::media::Status::success();
    };
    if (auto history = preserveBacking(m_plan.video, snapshot.video, product.video.storage, true); !history)
        return history;
    if (m_audio) {
        if (auto history = preserveBacking(*m_plan.audio, *snapshot.audio, product.audio->storage, false); !history)
            return history;
    }
    if (std::chrono::steady_clock::now() >= m_plan.deadline)
        return invalid("Prepared storage admission exceeded original deadline");
    return ::media::Status::success();
}

void MediaPreparedRtpSourceSession::applyStorageAdmissionLocked(MediaPreparedRtpStorageAdmission product) noexcept
{
    const auto apply = [](Stream& stream, MediaPreparedRtpSourceSessionStreamPlan& plan,
        MediaPreparedRtpAdmissionStream& admission, const MediaPreparedRtpStreamStorageAdmission& value) {
        admission = value.compressed;
        plan.binder.acquiringCapacity = value.bounds.acquiringObjects;
        plan.maximumProtocolOutputObjects = value.bounds.outputObjects;
        plan.containerStorage = value.storage;
        stream.binder->m_acquiringCapacity = value.bounds.acquiringObjects;
    };
    // No allocations or fallible operations after this publication point.
    static_assert(std::is_nothrow_move_constructible_v<MediaPreparedRtpStorageAdmission>);
    m_plan.sealedAdmission.emplace(std::move(product));
    apply(*m_video, m_plan.video, m_plan.admission.video, m_plan.sealedAdmission->video);
    if (m_audio) apply(*m_audio, *m_plan.audio, *m_plan.admission.audio, *m_plan.sealedAdmission->audio);
}

::media::Status MediaPreparedRtpSourceSession::freezeBatch(
    std::span<const std::shared_ptr<MediaPreparedRtpSourceSession>> sources,
    const MediaPreparedRtpBatchAdmissionSolver* solver)
try {
    if (sources.empty()) return invalid("Prepared freeze requires source owners");
    for (std::size_t i = 0; i < sources.size(); ++i) {
        if (!sources[i]) return invalid("Prepared freeze requires nonnull source owners");
        for (std::size_t j = 0; j < i; ++j)
            if (sources[i] == sources[j]) return invalid("Prepared freeze rejects duplicate source owners");
    }
    // Exact-size transient storage, charged before allocation to the same shared
    // preparation domain. Storage snapshots include these live reservations.
    std::size_t childCount = 0;
    for (const auto& source : sources) {
        const std::size_t count = source->m_audio ? 3 : 2;
        if (childCount > (std::numeric_limits<std::size_t>::max)() - count)
            return invalid("Prepared source budget membership overflows");
        childCount += count;
    }
    auto resource = sources.front()->m_ownerResource;
    std::optional<MediaPreparedRtpBatchStoragePlan> batchStorage;
    if (solver) {
        auto planned = MediaPreparedRtpBatchStoragePlanner::plan(sources.size(), childCount, resource);
        if (!planned) return ::media::Status::failure(planned.error());
        batchStorage.emplace(std::move(planned).value());
    }
    MediaFixedCapacityStorage<MediaPreparedRtpSourceSession*> ordered(sources.size(), *resource);
    MediaFixedCapacityStorage<std::unique_lock<std::mutex>> locks(sources.size(), *resource);
    MediaFixedCapacityStorage<MediaPreparedRtpStorageSourceSnapshot> snapshots(sources.size(), *resource);
    for (const auto& source : sources) {
        ordered.push_back(source.get());
        for (std::size_t index = ordered.size() - 1; index &&
            std::less<MediaPreparedRtpSourceSession*>{}(ordered[index], ordered[index - 1]); --index)
            std::swap(ordered[index], ordered[index - 1]);
    }
    for (auto* source : ordered) locks.push_back(std::unique_lock<std::mutex>{source->m_mutex});
    for (const auto& source : sources) {
        if (!solver && !source->m_plan.canonicalRuntimeRetention)
            return invalid("Source presentation batch requires complete storage admission");
        if (auto status = source->prepareFreezeLocked(); !status) return status;
    }
    for (const auto& source : sources) {
        auto snapshot = source->storageSnapshotLocked();
        if (!snapshot) return ::media::Status::failure(snapshot.error());
        snapshots.push_back({&source->m_plan, std::move(snapshot).value()});
    }
    std::optional<MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission>> admissions;
    if (solver) {
        MediaFixedCapacityStorage<std::shared_ptr<MediaPreparationStorageBudget>> budgets(childCount, *resource);
        MediaFixedCapacityStorage<MediaPreparationStorageSnapshot> budgetSnapshots(childCount, *resource);
        for (std::size_t i = 0; i < sources.size(); ++i) {
            const auto& owners = sources[i]->m_budgets;
            auto& roles = snapshots[i].snapshot.roles;
            roles.metadata = budgets.size(); budgets.push_back(owners.metadata); budgetSnapshots.push_back({});
            roles.videoPayload = budgets.size(); budgets.push_back(owners.videoPayload); budgetSnapshots.push_back({});
            if (owners.audioPayload) {
                roles.audioPayload = budgets.size(); budgets.push_back(owners.audioPayload); budgetSnapshots.push_back({});
            }
        }
        const MediaPreparedRtpBatchBudgetView budgetView{budgets.view(), budgetSnapshots.view()};
        auto partition = budgetView.refresh();
        if (!partition) return ::media::Status::failure(partition.error());
        for (std::size_t i = 0; i < sources.size(); ++i)
            snapshots[i].snapshot.budgets = budgetView.source(snapshots[i].snapshot.roles);
        auto solved = solver->solve(snapshots.view(), budgetView, *batchStorage);
        if (!solved) return ::media::Status::failure(solved.error());
        if (solved.value().size() != batchStorage->sourceCount ||
            solved.value().capacity() != batchStorage->sourceCount ||
            solved.value().resource() != batchStorage->owner.get())
            return invalid("Batch admission changed result membership, capacity or storage owner");
        admissions.emplace(std::move(solved).value());
        // Solver storage may have changed the ledger. Reobserve held credit;
        // neither observation reserves future unrelated-root allocations.
        partition = budgetView.refresh();
        if (!partition) return ::media::Status::failure(partition.error());
        std::size_t total = partition.value().otherChargedBytes;
        const auto capacity = partition.value().budget.capacity;
        if (batchStorage->totalBytes > capacity - total)
            return invalid("Batch scratch and result exceed shared deployment capacity");
        total += batchStorage->totalBytes;
        for (std::size_t i = 0; i < sources.size(); ++i) {
            if (sources[i]->m_plan.storageBytes != capacity)
                return invalid("Batch admission requires one shared deployment capacity");
            snapshots[i].snapshot.budgets = budgetView.source(snapshots[i].snapshot.roles);
            // All batch arrays are alive here and charged to source zero's
            // budget. Count them once in the batch, never in each source total.
            if (auto status = sources[i]->validateStorageAdmissionLocked((*admissions)[i], snapshots[i].snapshot,
                    i == 0 ? batchStorage->totalBytes : 0); !status)
                return status;
            if ((*admissions)[i].totalBytes > capacity - total)
                return invalid("Batch source storage exceeds shared deployment capacity");
            total += (*admissions)[i].totalBytes;
        }
    }
    // Check all deadlines before the first no-throw state publication.
    for (const auto& source : sources)
        if (std::chrono::steady_clock::now() >= source->m_plan.deadline)
            return invalid("Prepared batch admission exceeded an original source deadline");
    if (admissions)
        for (std::size_t i = 0; i < sources.size(); ++i)
            sources[i]->applyStorageAdmissionLocked(std::move((*admissions)[i]));
    for (const auto& source : sources) source->m_frozen = true;
    for (const auto& source : sources) source->m_ready.notify_all();
    return ::media::Status::success();
} catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Prepared batch storage admission failed"));
} catch (const std::system_error& error) {
    return ::media::Status::failure(::media::ErrorInfo::ioFailure(error.what()));
}

bool MediaPreparedRtpSourceSession::isFrozen() const
{
    std::scoped_lock lock(m_mutex);
    return m_frozen;
}
::media::Result<MediaRtpClockGroupSession> MediaPreparedRtpSourceSession::takeClockGroup(
    const MediaRtpClockGroupSessionConfig& expected)
{
    std::scoped_lock lock(m_mutex);
    using R = ::media::Result<MediaRtpClockGroupSession>;
    if (!m_frozen || !m_group) return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP clock group requires one frozen handoff"));
    const auto mapper = [](const auto& a) { return std::tuple(a.clockRate, a.requireCname,
        a.senderReportTimeoutNs, a.maximumExtrapolationNs, a.maximumResidualNs, a.maximumRateErrorPpm); };
    const auto validator = [](const auto& a) { return std::tuple(a.members,
        a.senderReportTimeoutNs, a.maximumExtrapolationNs, a.videoCnameTimeoutNs,
        a.requireMatchingCname, a.commonEpochPolicy, a.invalidateOnDegraded); };
    const auto& actual = m_plan.clockGroup;
    if (mapper(actual.video) != mapper(expected.video) || actual.audio.has_value() != expected.audio.has_value() ||
        (actual.audio && mapper(*actual.audio) != mapper(*expected.audio)) ||
        validator(actual.validator) != validator(expected.validator) ||
        actual.validator.audio.has_value() != expected.validator.audio.has_value() ||
        (actual.validator.audio && (actual.validator.audio->cnameTimeoutNs != expected.validator.audio->cnameTimeoutNs ||
            actual.validator.audio->maximumClockOffsetSkewNs != expected.validator.audio->maximumClockOffsetSkewNs)))
        return R::failure(::media::ErrorInfo::invalidArgument("Prepared clock group differs from runtime policy"));
    auto value = std::move(*m_group); m_group.reset();
    return R::success(std::move(value));
}

::media::Result<MediaPreparedRtpPacketClockHandoff> MediaPreparedRtpSourceSession::takePacketClock(
    const MediaRtpPacketClockSessionConfig& expected)
try {
    std::scoped_lock lock(m_mutex);
    using R = ::media::Result<MediaPreparedRtpPacketClockHandoff>;
    if (auto audio = m_plan.validateAudioRetention(); !audio) return R::failure(audio.error());
    auto selected = stream(expected.stream);
    if (!selected) return R::failure(selected.error());
    auto& s = *selected.value();
    if (!m_frozen || !s.binder) return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP binder requires one frozen handoff"));
    if (s.plan.binder.members != expected.members || s.plan.binder.acquisitionTimeout != expected.acquisitionTimeout ||
        s.plan.binder.videoClockRate != expected.videoClockRate ||
        s.plan.binder.videoTiming != expected.videoTiming)
        return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP binder differs from runtime clock policy"));
    if (!expected.acquiringCapacity ||
        s.binder->m_acquiringPackets->size() > expected.acquiringCapacity)
        return R::failure(::media::ErrorInfo::invalidArgument(
            "Prepared RTP handoff exceeds runtime acquiring capacity"));
    if (m_plan.sealedAdmission) {
        const auto& sealed = *m_plan.sealedAdmission;
        if (sealed.owner != m_video->resource ||
            (expected.stream == MediaStreamKind::Audio && !sealed.audio))
            return R::failure(::media::ErrorInfo::invalidArgument(
                "Prepared RTP handoff differs from sealed owner or stream"));
        const auto& retained = expected.stream == MediaStreamKind::Video ? sealed.video : *sealed.audio;
        const auto& admitted = expected.stream == MediaStreamKind::Video
            ? m_plan.admission.video : *m_plan.admission.audio;
        if (expected.acquiringCapacity != retained.bounds.acquiringObjects ||
            s.plan.binder.acquiringCapacity != expected.acquiringCapacity ||
            s.binder->m_acquiringCapacity != expected.acquiringCapacity ||
            admitted.maximumUnits != retained.compressed.maximumUnits ||
            admitted.maximumBytes != retained.compressed.maximumBytes ||
            s.admittedUnits > retained.compressed.maximumUnits ||
            s.prefix->packets.size() > retained.bounds.prefixObjects)
            return R::failure(::media::ErrorInfo::invalidArgument(
                "Prepared RTP handoff differs from sealed compressed capacity"));
    } else {
        if (!m_plan.canonicalRuntimeRetention)
            return R::failure(::media::ErrorInfo::invalidArgument(
                "Prepared RTP handoff requires sealed or canonical runtime admission"));
        const auto& retained = expected.stream == MediaStreamKind::Video
            ? m_plan.canonicalRuntimeRetention->video : *m_plan.audioRetention;
        if (s.admittedUnits > retained.maximumUnits ||
            s.prefix->packets.size() > retained.maximumUnits)
            return R::failure(::media::ErrorInfo::invalidArgument(
                "Prepared RTP prefix exceeds canonical runtime retention"));
        s.binder->m_acquiringCapacity = expected.acquiringCapacity;
    }
    auto binder = std::move(*s.binder); s.binder.reset();
    return R::success({std::move(binder), std::move(s.prefix)});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaPreparedRtpPacketClockHandoff>::failure(
        ::media::ErrorInfo::allocationFailed("Prepared clock handoff allocation failed"));
}

::media::Result<std::optional<MediaPreparedRtpOutput>> MediaPreparedRtpSourceSession::takeOutput(
    MediaStreamKind kind, MediaPreparedRtpOutputClass outputClass)
{
    std::scoped_lock lock(m_mutex);
    using R = ::media::Result<std::optional<MediaPreparedRtpOutput>>;
    if (m_error) return R::failure(*m_error);
    auto selected = stream(kind);
    if (!selected) return R::failure(selected.error());
    if (!m_frozen) return R::failure(::media::ErrorInfo::invalidArgument("Prepared RTP runtime output precedes handoff"));
    auto& stream = *selected.value();
    if (outputClass != MediaPreparedRtpOutputClass::Packet && outputClass != MediaPreparedRtpOutputClass::Control)
        return R::failure(::media::ErrorInfo::invalidArgument("Unknown prepared RTP output class"));
    auto& queue = outputClass == MediaPreparedRtpOutputClass::Packet ? stream.packets : stream.controls;
    if (queue.empty()) return R::success(std::nullopt);
    auto output = std::move(queue.front()); queue.pop_front();
    selected.value()->outputBytes -= output.admissionBytes;
    return R::success(std::move(output));
}

} // namespace media::ffmpeg::graph
