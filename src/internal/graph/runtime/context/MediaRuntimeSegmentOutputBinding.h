#pragma once

#include "internal/graph/core/MediaPort.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include "internal/graph/runtime/threading/MediaGraphWorkerExitToken.h"
#include <memory>

namespace media::ffmpeg::graph {
class MediaGraphPayloadCreditLedger;
class MediaGraphExecutionContext;

// Exported by an actual executing segment, never synthesized from topology.
// This retains synchronization identity without retaining the upstream context.
class MediaRuntimeSegmentOutputBinding final {
public:
    const MediaPort& port() const noexcept { return m_port; }
private:
    friend class MediaGraphExecutionContext;
    MediaRuntimeSegmentOutputBinding(MediaPort port,
        std::shared_ptr<MediaNodeWakeup> wakeup,
        std::shared_ptr<MediaGraphWorkerExitToken> exit,
        std::shared_ptr<MediaGraphPayloadCreditLedger> ledger)
        : m_port(std::move(port)), m_wakeup(std::move(wakeup)),
          m_exit(std::move(exit)), m_ledger(std::move(ledger)) {}
    MediaPort m_port;
    std::shared_ptr<MediaNodeWakeup> m_wakeup;
    std::shared_ptr<MediaGraphWorkerExitToken> m_exit;
    std::shared_ptr<MediaGraphPayloadCreditLedger> m_ledger;
};
} // namespace media::ffmpeg::graph
