#pragma once

#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderPort.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

namespace media_transcode::test::rtp_udp {

using namespace ::media::ffmpeg::graph;

struct FakePortState final {
    int openCalls = 0;
    int sendCalls = 0;
    int closeCalls = 0;
    bool throwOnOpen = false;
    bool throwOnSend = false;
    bool throwOnLocalEndpoint = false;
    bool throwOnRemoteEndpoint = false;
    ::media::Status openStatus = ::media::Status::success();
    std::optional<MediaUdpDatagramEndpoint> boundEndpoint;
    std::optional<MediaUdpDatagramEndpoint> scriptedBoundEndpoint;
    std::optional<MediaUdpDatagramEndpoint> remoteEndpoint;
    std::deque<MediaUdpDatagramSendOutcome> outcomes;
    std::function<void()> onSend;
    std::function<void()> onClose;
    std::mutex blockMutex;
    std::condition_variable blockCondition;
    bool sendEntered = false;
    bool releaseSend = false;
    bool blockClose = false;
    bool closeEntered = false;
    bool releaseClose = false;
};

class FakeSenderPort final : public MediaUdpDatagramSenderPort {
public:
    explicit FakeSenderPort(std::shared_ptr<FakePortState> state)
        : m_state(std::move(state))
    {
    }

    ::media::Status open(const MediaUdpDatagramSenderPortOpenRequest& request) override
    {
        ++m_state->openCalls;
        if (m_state->throwOnOpen) throw std::runtime_error("scripted open throw");
        if (!m_state->openStatus) return m_state->openStatus;
        m_state->boundEndpoint = m_state->scriptedBoundEndpoint
            ? m_state->scriptedBoundEndpoint
            : std::optional<MediaUdpDatagramEndpoint>(request.localEndpoint());
        if (m_state->boundEndpoint->port() == 0) {
            auto assigned = MediaUdpDatagramEndpoint::create(
                request.localEndpoint().addressFamily(),
                request.localEndpoint().numericAddress(),
                request.remoteEndpoint().port() == 5004 ? 41000 : 41002);
            if (!assigned) return ::media::Status::failure(assigned.error());
            m_state->boundEndpoint = std::move(assigned.value());
        }
        m_state->remoteEndpoint = request.remoteEndpoint();
        return ::media::Status::success();
    }

    MediaUdpDatagramSendOutcome send(
        std::span<const std::uint8_t> datagram) override
    {
        ++m_state->sendCalls;
        if (m_state->throwOnSend) throw std::runtime_error("scripted send throw");
        if (m_state->onSend) m_state->onSend();
        {
            std::unique_lock lock(m_state->blockMutex);
            m_state->sendEntered = true;
            m_state->blockCondition.notify_all();
            if (!m_state->releaseSend) {
                m_state->blockCondition.wait(lock, [this] {
                    return m_state->releaseSend;
                });
            }
        }
        if (m_state->outcomes.empty()) {
            return MediaUdpDatagramSendOutcome::accepted(datagram.size());
        }
        auto outcome = std::move(m_state->outcomes.front());
        m_state->outcomes.pop_front();
        return outcome;
    }

    std::optional<MediaUdpDatagramEndpoint> localEndpoint() const override
    {
        if (m_state->throwOnLocalEndpoint) {
            throw std::runtime_error("scripted local endpoint throw");
        }
        return m_state->boundEndpoint;
    }

    std::optional<MediaUdpDatagramEndpoint> remoteEndpoint() const override
    {
        if (m_state->throwOnRemoteEndpoint) {
            throw std::runtime_error("scripted remote endpoint throw");
        }
        return m_state->remoteEndpoint;
    }

    void close() noexcept override
    {
        ++m_state->closeCalls;
        if (m_state->onClose) m_state->onClose();
        if (m_state->blockClose) {
            std::unique_lock lock(m_state->blockMutex);
            m_state->closeEntered = true;
            m_state->blockCondition.notify_all();
            m_state->blockCondition.wait(lock, [this] {
                return m_state->releaseClose;
            });
        }
        m_state->boundEndpoint.reset();
        m_state->remoteEndpoint.reset();
    }

private:
    std::shared_ptr<FakePortState> m_state;
};

class FakeSenderPortFactory final : public MediaUdpDatagramSenderPortFactory {
public:
    FakeSenderPortFactory(std::shared_ptr<FakePortState> rtp,
                          std::shared_ptr<FakePortState> rtcp)
        : m_states{std::move(rtp), std::move(rtcp)}
    {
    }

    ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>> create() override
    {
        ++createCalls;
        if (throwAt == createCalls) {
            throw std::runtime_error("scripted factory throw");
        }
        if (returnNullAt == createCalls) {
            return ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>>::success(
                nullptr);
        }
        if (createCalls > 2) {
            return ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>>::failure(
                ::media::ErrorInfo::internalError("unexpected third port request"));
        }
        std::unique_ptr<MediaUdpDatagramSenderPort> port =
            std::make_unique<FakeSenderPort>(m_states[createCalls - 1]);
        return ::media::Result<std::unique_ptr<MediaUdpDatagramSenderPort>>::success(
            std::move(port));
    }

    int createCalls = 0;
    int returnNullAt = 0;
    int throwAt = 0;

private:
    std::shared_ptr<FakePortState> m_states[2];
};

inline MediaRtpUdpLocalPortPolicy osAssignedPolicy()
{
    return MediaRtpUdpLocalPortPolicy::osAssignedIndependent();
}

inline ::media::Result<MediaRtpUdpSenderConfig> makeConfig(
    MediaRtpUdpLocalPortPolicy localPolicy = osAssignedPolicy(),
    std::size_t maximumDatagramBytes = 1200)
{
    return MediaRtpUdpSenderConfig::create(
        MediaIpAddressFamily::Ipv4,
        "127.0.0.1",
        "127.0.0.1",
        5004,
        5005,
        std::move(localPolicy),
        1 << 20,
        maximumDatagramBytes,
        MediaUdpSenderIoBehavior::NonBlockingRejectOnPressure);
}

inline void releasePort(const std::shared_ptr<FakePortState>& state)
{
    std::lock_guard lock(state->blockMutex);
    state->releaseSend = true;
    state->blockCondition.notify_all();
}

} // namespace media_transcode::test::rtp_udp
