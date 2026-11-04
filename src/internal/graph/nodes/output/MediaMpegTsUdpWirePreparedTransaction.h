#pragma once
#include "internal/graph/nodes/output/MediaMpegTsUdpWireCommitTransaction.h"
#include "internal/graph/nodes/output/MediaWireCommitPreparation.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include <utility>

namespace media::ffmpeg::graph {
// Sole unpublished UDP transaction, owned inline by its materializer.
struct MediaMpegTsUdpWirePreparedTransaction final {
    MediaMpegTsUdpWirePreparedTransaction(
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocolValue,
        std::size_t offset, MediaRunningTime preparedAt, MediaRunningTime deadlineValue,
        MediaFixedCapacityStorage<MediaWireGlobalSequenceReservationEntry> entryValues,
        MediaWireCommitPreparation storageValue,
        MediaDatagramCommitTransaction::Prepared<MediaMpegTsUdpWireCommitTransaction> controlValue) noexcept
        : protocol(std::move(protocolValue)), protocolOffset(offset), materializedAt(preparedAt),
          deadline(deadlineValue), entries(std::move(entryValues)), storage(std::move(storageValue)),
          control(std::move(controlValue)) {}
    MediaMpegTsUdpWirePreparedTransaction(MediaMpegTsUdpWirePreparedTransaction&&) noexcept = default;
    MediaMpegTsUdpWirePreparedTransaction& operator=(MediaMpegTsUdpWirePreparedTransaction&&) = delete;
    MediaMpegTsUdpWirePreparedTransaction(const MediaMpegTsUdpWirePreparedTransaction&) = delete;
    MediaMpegTsUdpWirePreparedTransaction& operator=(const MediaMpegTsUdpWirePreparedTransaction&) = delete;
    ~MediaMpegTsUdpWirePreparedTransaction() noexcept
    {
        if (protocol) protocol->abandonPreparedWire(protocolOffset);
    }
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocol;
    std::size_t protocolOffset;
    MediaRunningTime materializedAt;
    MediaRunningTime deadline;
    MediaFixedCapacityStorage<MediaWireGlobalSequenceReservationEntry> entries;
    MediaWireCommitPreparation storage;
    MediaDatagramCommitTransaction::Prepared<MediaMpegTsUdpWireCommitTransaction> control;
};
} // namespace media::ffmpeg::graph
