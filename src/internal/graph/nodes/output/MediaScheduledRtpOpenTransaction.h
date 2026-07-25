#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpPacketizerSession.h"
#include "internal/graph/nodes/mux/ScheduledRtpSenderConfig.h"
#include "internal/graph/nodes/mux/ScheduledRtpSenderSession.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderTransport.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaOpenedScheduledRtpSender final {
public:
    MediaOpenedScheduledRtpSender(
        MediaOpenedScheduledRtpSender&&) noexcept = default;
    MediaOpenedScheduledRtpSender& operator=(
        MediaOpenedScheduledRtpSender&&) noexcept = default;
    MediaOpenedScheduledRtpSender(
        const MediaOpenedScheduledRtpSender&) = delete;
    MediaOpenedScheduledRtpSender& operator=(
        const MediaOpenedScheduledRtpSender&) = delete;

    std::unique_ptr<MediaRtpUdpSenderTransport> releaseTransport() noexcept;
    std::unique_ptr<ScheduledRtpSenderSession> releaseSender() noexcept;

private:
    friend class MediaScheduledRtpOpenTransaction;

    MediaOpenedScheduledRtpSender(
        std::unique_ptr<MediaRtpUdpSenderTransport> transport,
        std::unique_ptr<ScheduledRtpSenderSession> sender) noexcept;

    std::unique_ptr<MediaRtpUdpSenderTransport> m_transport;
    std::unique_ptr<ScheduledRtpSenderSession> m_sender;
};

class MediaScheduledRtpOpenTransaction final {
public:
    static ::media::Result<MediaOpenedScheduledRtpSender> open(
        MediaRtpUdpSenderConfig transportConfig,
        ScheduledRtpSenderConfig senderConfig,
        MediaUdpDatagramSenderPortFactory& transportFactory,
        ScheduledRtpPacketizerFactory& packetizerFactory);
};

} // namespace media::ffmpeg::graph
