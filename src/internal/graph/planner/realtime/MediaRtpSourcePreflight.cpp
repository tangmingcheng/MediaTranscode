#include "internal/graph/planner/realtime/MediaRtpSourcePreflight.h"

#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpIngressPlanner.h"

#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"
#include <limits>
#include <mutex>
#include <new>
#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<int> remainingRawRtpStartupMilliseconds(
    std::chrono::steady_clock::time_point deadline,
    const char* phase)
{
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
        return ::media::Result<int>::failure(::media::ErrorInfo::wouldBlock(
            std::string("raw RTP preflight reached total open timeout during ") +
            phase));
    }
    const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
        deadline - now);
    if (remaining.count() <= 0 ||
        remaining.count() > (std::numeric_limits<int>::max)()) {
        return ::media::Result<int>::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP preflight deadline is outside the supported range"));
    }
    return ::media::Result<int>::success(
        static_cast<int>(remaining.count()));
}

} // namespace

MediaRtpSourcePreflight::MediaRtpSourcePreflight(MediaPreparedRawRtpProbe probe,
    std::chrono::steady_clock::time_point deadline,
    std::optional<MediaPreparedRtpIngressCandidate> videoCandidate,
    std::optional<MediaPreparedRtpIngressCandidate> audioCandidate,
    MediaPreparationStorageScope scope)
    : m_storageScope(std::move(scope)), m_probe(std::move(probe)), m_deadline(deadline),
      m_videoCandidate(std::move(videoCandidate)), m_audioCandidate(std::move(audioCandidate)) {}

MediaRtpSourcePreflight::MediaRtpSourcePreflight(MediaRtpSourcePreflight&& other) noexcept
    : m_storageScope(std::move(other.m_storageScope)),
      m_probe(std::exchange(other.m_probe, std::nullopt)), m_deadline(other.m_deadline),
      m_continuousSource(std::move(other.m_continuousSource)),
      m_videoCandidate(std::move(other.m_videoCandidate)),
      m_audioCandidate(std::move(other.m_audioCandidate)) {}

::media::Result<MediaRtpSourcePreflight> MediaRtpSourcePreflight::begin(
    const MediaRealtimeInputConfig& input, MediaTranscodeStreamSet streamSet,
    std::chrono::steady_clock::time_point deadline, std::stop_token cancellation,
    const MediaPreparationStorageScope& scope, MediaRtpVideoPreparationIntent timingIntent)
{
    using Result = ::media::Result<MediaRtpSourcePreflight>;
    if (input.type != RealtimeInputType::RtpPort || !input.openTimeoutMs ||
        *input.openTimeoutMs <= 0 ||
        (streamSet != MediaTranscodeStreamSet::VideoOnly &&
         streamSet != MediaTranscodeStreamSet::AudioVideo))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP source preflight requires explicit input type, timeout and stream set"));
    if (cancellation.stop_requested()) return Result::failure(
        ::media::ErrorInfo::cancelled("RTP source preflight cancelled before opening transports"));
    auto remaining = remainingRawRtpStartupMilliseconds(deadline, "video signaling detection");
    if (!remaining) return Result::failure(remaining.error());
    if (remaining.value() > *input.openTimeoutMs)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP source preflight deadline exceeds the input open timeout"));
    auto probeInput = input;
    probeInput.openTimeoutMs = remaining.value();
    auto probe = MediaRealtimeInputPlanner::prepareRawRtpVideo(probeInput, streamSet, cancellation, scope, timingIntent);
    if (!probe) return Result::failure(probe.error());
    auto afterProbe = remainingRawRtpStartupMilliseconds(deadline, "video signaling detection");
    if (!afterProbe) return Result::failure(afterProbe.error());
    if ((streamSet == MediaTranscodeStreamSet::AudioVideo) !=
        std::holds_alternative<MediaPreparedRawRtpAudioVideoProbe>(probe.value()))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP prepared probe stream set conflicts with request"));
    // Probe already has real signaling/cadence evidence. Freeze the per-stream
    // candidate before this source can wait for other composition sources.
    std::optional<MediaPreparedRtpIngressCandidate> videoCandidate, audioCandidate;
    auto candidates = std::visit([&](const auto& prepared) -> ::media::Status {
        if (prepared.inputWait.mode() == MediaRtpInputWaitMode::FiniteProbeObservation)
            return ::media::Status::success();
        auto video = MediaPreparedRtpIngressPlanner::candidate(prepared.video, prepared.inputWait,
            {deadline, cancellation});
        if (!video) return ::media::Status::failure(video.error());
        videoCandidate.emplace(std::move(video).value());
        if constexpr (requires { prepared.audio; }) {
            auto audio = MediaPreparedRtpIngressPlanner::candidate(prepared.audio, prepared.inputWait,
                {deadline, cancellation});
            if (!audio) return ::media::Status::failure(audio.error());
            audioCandidate.emplace(std::move(audio).value());
        }
        return ::media::Status::success();
    }, probe.value());
    if (!candidates) return Result::failure(candidates.error());
    auto afterCandidate = remainingRawRtpStartupMilliseconds(deadline, "input ingress candidate planning");
    if (!afterCandidate) return Result::failure(afterCandidate.error());
    return Result::success(MediaRtpSourcePreflight(std::move(probe).value(), deadline,
        std::move(videoCandidate), std::move(audioCandidate), scope));
}

::media::Result<MediaRtpSourceObservation> MediaRtpSourcePreflight::observation() const
{
    using Result = ::media::Result<MediaRtpSourceObservation>;
    if (!m_probe) return Result::failure(::media::ErrorInfo::notInitialized(
        "RTP source preflight was already consumed"));
    return std::visit([](const auto& probe) {
        return Result::success(MediaRtpSourceObservation{probe.signaling, probe.timing});
    }, *m_probe);
}

::media::Result<MediaCapturingRtpSourceView> MediaRtpSourcePreflight::resources() const
{
    using Result = ::media::Result<MediaCapturingRtpSourceView>;
    if (!m_probe) return Result::failure(::media::ErrorInfo::notInitialized(
        "RTP source preflight was already consumed"));
    return std::visit([](const auto& probe) {
        const MediaPreparedRealtimeInput* audio = nullptr;
        if constexpr (requires { probe.audio; }) audio = &probe.audio;
        return Result::success(MediaCapturingRtpSourceView{probe.video, audio});
    }, *m_probe);
}

::media::Status MediaRtpSourcePreflight::waitForVideoData(
    std::uint64_t previousIngressFrontier, std::stop_token cancellation)
{
    if (!m_probe) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "RTP source preflight was already consumed"));
    return std::visit([&](auto& probe) {
        return probe.video.waitForRawRtpProbeData(
            previousIngressFrontier, m_deadline, cancellation);
    }, *m_probe);
}

::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>>
MediaRtpSourcePreflight::installContinuousSession(
    const MediaRealtimeCompositionSourceFacts& facts,
    const std::shared_ptr<MediaMasterClock>& masterClock, std::stop_token cancellation) try
{
    using Result = ::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>>;
    const MediaPreparationControl control{m_deadline, cancellation};
    if (auto status = control.check("continuous RTP installation"); !status)
        return Result::failure(status.error());
    if (!m_probe || m_continuousSource || !m_videoCandidate || !masterClock)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Continuous RTP preparation requires an unconsumed candidate and shared master clock"));
    return std::visit([&](auto& probe) -> Result {
        auto video = probe.video.m_rawRtpBuffer;
        std::shared_ptr<MediaRawRtpPreparedInputBuffer> audio;
        if constexpr (requires { probe.audio; }) audio = probe.audio.m_rawRtpBuffer;
        if (!video || static_cast<bool>(audio) != m_audioCandidate.has_value() || audio == video)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Continuous RTP preparation stream owners differ from their candidates"));
        std::unique_lock videoLock(video->m_mutex, std::defer_lock);
        std::unique_lock<std::mutex> audioLock;
        if (audio) {
            audioLock = std::unique_lock(audio->m_mutex, std::defer_lock);
            std::lock(videoLock, audioLock);
        } else videoLock.lock();
        const auto validate = [&](const std::shared_ptr<MediaRawRtpPreparedInputBuffer>& input,
                                  const MediaPreparedRtpIngressCandidate& candidate) -> ::media::Status {
            const auto owner = candidate.snapshot.source.lock();
            if (!owner || owner.get() != input.get() || owner.owner_before(input) ||
                input.owner_before(owner))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Continuous RTP installation requires its exact candidate owner"));
            if (auto status = input->continuousPreparationStatusLocked({}); !status) return status;
            if (input->m_prepared->ingressFrontier < candidate.snapshot.ingressFrontier)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Continuous RTP installation requires the exact still-receiving candidate owner"));
            if (auto valid = input->m_prepared->byteBudget->validate(); !valid) return valid;
            const auto budget = input->m_prepared->byteBudget->snapshot();
            const auto socketBytes = input->m_prepared->transport.effectiveReceiveBufferBytes();
            if (socketBytes <= 0 || budget.phase != MediaRawRtpPreparationPhase::Capture || budget.probeActive ||
                budget.wireCapacity != candidate.snapshot.preparedWireBytes ||
                input->m_prepared->transport.maximumDatagramBytes() != candidate.snapshot.maximumDatagramBytes ||
                static_cast<std::size_t>(socketBytes) != candidate.snapshot.socketReceiveCapacityBytes)
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "Continuous RTP installation capacity changed or a raw probe still retains storage"));
            return candidate.plan.validateProduct();
        };
        if (auto valid = validate(video, *m_videoCandidate); !valid) return Result::failure(valid.error());
        if (audio) {
            if (auto valid = validate(audio, *m_audioCandidate); !valid) return Result::failure(valid.error());
            if (audio->m_prepared->byteBudget != video->m_prepared->byteBudget)
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Continuous RTP source requires its original shared A/V wire budget"));
        }
        // Both prefix counts and the remaining-deadline boundary are obtained
        // while the same receiver locks exclude any intervening append.
        auto videoBound = video->accessUnitBoundLocked();
        if (!videoBound) return Result::failure(videoBound.error());
        std::optional<std::uint64_t> audioBound;
        if (audio) {
            auto bound = audio->accessUnitBoundLocked();
            if (!bound) return Result::failure(bound.error());
            audioBound = bound.value();
        }
        const auto* shared = std::get_if<MediaSharedPreparationStorage>(&m_storageScope);
        if (!shared || !shared->root || !shared->root->snapshot().capacity)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Continuous RTP preparation requires explicit shared deployment capacity"));
        const auto observedNow = std::chrono::steady_clock::now();
        auto plan = MediaPreparedRtpSourceSessionPlanner::plan(facts, *m_videoCandidate,
            m_audioCandidate ? &*m_audioCandidate : nullptr, videoBound.value(), audioBound,
            m_deadline, observedNow, shared->root->snapshot().capacity);
        if (!plan) return Result::failure(plan.error());
        auto budget = MediaPreparationStorageBudget::create(plan.value().storageBytes, m_storageScope, MediaStorageReleaseUnobserved{});
        if (!budget) return Result::failure(budget.error());
        auto source = MediaPreparedRtpSourceSession::create(std::move(plan).value(), budget.value(), masterClock);
        if (!source) return Result::failure(source.error());
        // Merge the two already ordered streams without copying a datagram or
        // allocating a cross-stream reference table. Ordinals remain per stream.
        std::size_t videoIndex = 0, audioIndex = 0;
        const auto& videoPrefix = video->m_prepared->datagrams;
        while (videoIndex < videoPrefix.size() ||
               (audio && audioIndex < audio->m_prepared->datagrams.size())) {
            if (auto status = control.check("continuous RTP bootstrap replay"); !status)
                return Result::failure(status.error());
            const bool takeVideo = videoIndex < videoPrefix.size() &&
                (!audio || audioIndex == audio->m_prepared->datagrams.size() ||
                 videoPrefix[videoIndex]->observedAtNs <= audio->m_prepared->datagrams[audioIndex]->observedAtNs);
            const auto& entry = takeVideo ? videoPrefix[videoIndex++] : audio->m_prepared->datagrams[audioIndex++];
            if (!entry) return Result::failure(::media::ErrorInfo::notInitialized(
                "Continuous RTP prefix lost datagram ownership"));
            if (auto consumed = source.value()->consume(takeVideo ? MediaStreamKind::Video : MediaStreamKind::Audio,
                    entry->datagram.channel, entry->datagram.bytes, entry->observedAtNs, entry->ordinal); !consumed)
                return Result::failure(consumed.error());
        }
        if (auto completed = source.value()->finishBootstrap(video->m_prepared->ingressFrontier,
                audio ? std::optional{audio->m_prepared->ingressFrontier} : std::nullopt); !completed)
            return Result::failure(completed.error());
        if (auto status = control.check("continuous RTP bootstrap publication"); !status)
            return Result::failure(status.error());
        video->m_continuousIngressPlan = m_videoCandidate->plan;
        if (audio) audio->m_continuousIngressPlan = m_audioCandidate->plan;
        if (auto sealed = video->m_prepared->byteBudget->sealPreflight(); !sealed)
            return Result::failure(sealed.error());
        video->m_prepared->datagrams.clear();
        video->m_bufferedBytes = 0;
        if (audio) { audio->m_prepared->datagrams.clear(); audio->m_bufferedBytes = 0; }
        // Publishing both owners happens before either receiver can resume.
        // Only its original worker transfers the socket into the batch adapter.
        m_continuousSource = source.value();
        video->m_continuousSource = source.value();
        if (audio) audio->m_continuousSource = source.value();
        return source;
    }, *m_probe);
} catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<MediaPreparedRtpSourceSession>>::failure(
        ::media::ErrorInfo::allocationFailed("Continuous RTP preparation owner allocation failed"));
}

::media::Status MediaRtpSourcePreflight::validateContinuousPreparation(
    const std::shared_ptr<MediaPreparedRtpSourceSession>& source) const
{
    if (!m_probe || !source || source != m_continuousSource ||
        source.owner_before(m_continuousSource) || m_continuousSource.owner_before(source))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Composition preparation no longer owns its installed continuous source"));
    return std::visit([&](const auto& probe) -> ::media::Status {
        const auto video = probe.video.m_rawRtpBuffer;
        std::shared_ptr<MediaRawRtpPreparedInputBuffer> audio;
        if constexpr (requires { probe.audio; }) audio = probe.audio.m_rawRtpBuffer;
        if (!video || (m_audioCandidate.has_value() && !audio) || audio == video)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Composition preparation receiver ownership is absent"));
        std::unique_lock videoLock(video->m_mutex, std::defer_lock);
        std::unique_lock<std::mutex> audioLock;
        if (audio) {
            audioLock = std::unique_lock(audio->m_mutex, std::defer_lock);
            std::lock(videoLock, audioLock);
        } else videoLock.lock();
        // Match installation's receiver -> protocol lock order. This is a
        // current phase check, not a promise that asynchronous input cannot fail.
        std::optional<::media::ErrorInfo> failure;
        const auto retainFailure = [&](const ::media::Status& status) {
            if (!status && (!failure || failure->code == ::media::ErrorCode::Cancelled))
                failure = status.error();
        };
        retainFailure(source->validatePreparationPhase());
        retainFailure(video->continuousPreparationStatusLocked(source));
        if (audio) retainFailure(audio->continuousPreparationStatusLocked(source));
        return failure ? ::media::Status::failure(std::move(*failure)) : ::media::Status::success();
    }, *m_probe);
}

::media::Result<MediaPreparedRtpSource> MediaRtpSourcePreflight::seal() &&
{
    using Result = ::media::Result<MediaPreparedRtpSource>;
    auto owned = std::exchange(m_probe, std::nullopt);
    if (!owned) return Result::failure(::media::ErrorInfo::notInitialized(
        "RTP source preflight was already consumed"));
    auto beforeSeal = remainingRawRtpStartupMilliseconds(m_deadline, "input capture sealing");
    if (!beforeSeal) return Result::failure(beforeSeal.error());
    if (!m_continuousSource) {
        if (auto status = MediaRawRtpInputPreparer::sealPreflight(*owned); !status)
            return Result::failure(status.error());
    }
    return std::visit([&](auto& probe) -> Result {
        auto videoIngress = MediaPreparedRtpIngressPlanner::plan(probe.video, probe.inputWait,
            m_videoCandidate ? &*m_videoCandidate : nullptr,
            m_continuousSource ? MediaPreparedRtpIngressPhase::ContinuousSession : MediaPreparedRtpIngressPhase::SealedCapture);
        if (!videoIngress) return Result::failure(videoIngress.error());
        std::optional<MediaRtpIngressPlan> audioIngress;
        if constexpr (requires { probe.audio; }) {
            auto planned = MediaPreparedRtpIngressPlanner::plan(probe.audio, probe.inputWait,
                m_audioCandidate ? &*m_audioCandidate : nullptr,
                m_continuousSource ? MediaPreparedRtpIngressPhase::ContinuousSession : MediaPreparedRtpIngressPhase::SealedCapture);
            if (!planned) return Result::failure(planned.error());
            audioIngress.emplace(std::move(planned).value());
        }
        auto afterSeal = remainingRawRtpStartupMilliseconds(m_deadline, "input ingress planning");
        if (!afterSeal) return Result::failure(afterSeal.error());
        if (m_continuousSource && !m_continuousSource->isFrozen())
            return Result::failure(::media::ErrorInfo::notInitialized(
                "Continuous source must complete joint storage admission before sealing"));
        return Result::success(MediaPreparedRtpSource(std::move(*owned),
            std::move(videoIngress).value(), std::move(audioIngress), m_deadline,
            std::move(m_continuousSource)));
    }, *owned);
}

MediaPreparedRtpSource::MediaPreparedRtpSource(MediaPreparedRawRtpProbe probe,
    MediaRtpIngressPlan videoIngress, std::optional<MediaRtpIngressPlan> audioIngress,
    std::chrono::steady_clock::time_point deadline,
    std::shared_ptr<MediaPreparedRtpSourceSession> continuousSource)
    : m_probe(std::move(probe)), m_videoIngress(std::move(videoIngress)),
      m_audioIngress(std::move(audioIngress)), m_deadline(deadline),
      m_continuousSource(std::move(continuousSource)) {}

MediaPreparedRtpSource::MediaPreparedRtpSource(MediaPreparedRtpSource&& other) noexcept
    : m_probe(std::exchange(other.m_probe, std::nullopt)),
      m_videoIngress(std::move(other.m_videoIngress)),
      m_audioIngress(std::move(other.m_audioIngress)), m_deadline(other.m_deadline),
      m_continuousSource(std::move(other.m_continuousSource)) {}

::media::Result<MediaPreparedRtpSourceView> MediaPreparedRtpSource::resources() const
{
    using Result = ::media::Result<MediaPreparedRtpSourceView>;
    if (!m_probe) return Result::failure(::media::ErrorInfo::notInitialized(
        "Prepared RTP source was already consumed"));
    if (m_continuousSource) {
        if (auto valid = m_continuousSource->plan().validateAudioRetention(); !valid)
            return Result::failure(valid.error());
    }
    return std::visit([&](const auto& probe) {
        const MediaPreparedRealtimeInput* audio = nullptr;
        if constexpr (requires { probe.audio; }) audio = &probe.audio;
        return Result::success(MediaPreparedRtpSourceView{
            probe.video, audio, m_videoIngress, m_audioIngress ? &*m_audioIngress : nullptr,
            m_continuousSource && m_continuousSource->plan().canonicalRuntimeRetention
                ? &*m_continuousSource->plan().canonicalRuntimeRetention : nullptr,
            m_continuousSource && m_continuousSource->plan().audioRetention
                ? &*m_continuousSource->plan().audioRetention : nullptr,
            m_continuousSource ? std::optional{m_continuousSource->preparationStorage()} : std::nullopt,
            m_continuousSource ? m_continuousSource->videoReferenceResource() : nullptr,
            m_continuousSource ? m_continuousSource->sealedAdmission() : nullptr});
    }, *m_probe);
}

::media::Result<MediaPreparedRtpSourceInputs> MediaPreparedRtpSource::release() &&
{
    using Result = ::media::Result<MediaPreparedRtpSourceInputs>;
    auto owned = std::exchange(m_probe, std::nullopt);
    if (!owned) return Result::failure(::media::ErrorInfo::notInitialized(
        "Prepared RTP source was already consumed"));
    auto beforeRelease = remainingRawRtpStartupMilliseconds(m_deadline, "resolved product planning");
    if (!beforeRelease) return Result::failure(beforeRelease.error());
    return std::visit([&](auto& probe) -> Result {
        if (!m_continuousSource) {
            if (auto status = probe.video.configureRawRtpRuntimeIngress(m_videoIngress); !status)
                return Result::failure(status.error());
        } else if (auto status = probe.video.rawRtpCaptureStatus(); !status)
            return Result::failure(status.error());
        std::optional<MediaPreparedRealtimeInput> audio;
        if constexpr (requires { probe.audio; }) {
            if (!m_audioIngress) return Result::failure(::media::ErrorInfo::notInitialized(
                "Prepared audio input requires its sealed ingress plan"));
            if (!m_continuousSource) {
                if (auto status = probe.audio.configureRawRtpRuntimeIngress(*m_audioIngress); !status)
                    return Result::failure(status.error());
            } else if (auto status = probe.audio.rawRtpCaptureStatus(); !status)
                return Result::failure(status.error());
            audio.emplace(std::move(probe.audio));
        }
        auto afterRelease = remainingRawRtpStartupMilliseconds(m_deadline, "input resource handoff");
        if (!afterRelease) return Result::failure(afterRelease.error());
        return Result::success(MediaPreparedRtpSourceInputs{std::move(probe.video), std::move(audio)});
    }, *owned);
}

} // namespace media::ffmpeg::graph
