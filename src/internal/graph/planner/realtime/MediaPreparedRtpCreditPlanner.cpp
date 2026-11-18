#include "internal/graph/planner/realtime/MediaPreparedRtpCreditPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceSessionPlan.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaPreparedRtpCreditPlan> MediaPreparedRtpCreditPlanner::plan(
    const MediaPreparedRtpSourceSessionStreamPlan& stream,
    std::size_t heldPayloadBytes, std::size_t packetObjects,
    std::size_t controlObjects, std::size_t maximumCnameBytes)
{
    using R = ::media::Result<MediaPreparedRtpCreditPlan>;
    using A = MediaCheckedArithmetic;
    auto transaction = MediaPreparedRtpAccessUnitEnvelopePlanner::transactionStorageBytes(
        stream.envelope, stream.workingSet.maximumDatagramBytes);
    if (!transaction) return R::failure(transaction.error());
    const auto fixed = stream.unmigratedProtocolStorage.compressedBytes;
    if (heldPayloadBytes < fixed || !packetObjects || !controlObjects ||
        stream.envelope.maximumAccessUnitsPerPush > packetObjects)
        return R::failure(::media::ErrorInfo::invalidArgument(
            "RTP credit requires complete held assembly and per-transaction output capacity"));
    // Replacement is admitted before the old partial AU is retired. These are
    // two concurrent transactions, not an empirically chosen queue multiplier.
    auto overlap = A::multiply(transaction.value(), 2, "RTP old and new payload transactions");
    if (!overlap) return R::failure(overlap.error());
    auto minimum = A::add(fixed, overlap.value(), "RTP fixed assembly and transaction guarantee");
    if (!minimum) return R::failure(minimum.error());
    const auto guaranteed = (std::max)(minimum.value(), static_cast<std::uint64_t>(heldPayloadBytes));

    // Match SourceSession::queue's logical byte admission exactly. The packet
    // pool already includes payload storage; queue entries add only their
    // output descriptor and packet wrapper. Physical metadata is charged by
    // the container/wrapper planners, independently of this logical limit.
    auto packetUnit = A::add(sizeof(MediaPreparedRtpOutput),
        MediaPreparedRtpSourceSession::storageFacts().packetBytes, "RTP packet queue wrapper");
    if (!packetUnit) return R::failure(packetUnit.error());
    auto packetMetadata = A::multiply(packetObjects, packetUnit.value(), "RTP packet queue objects");
    if (!packetMetadata) return R::failure(packetMetadata.error());
    auto packetBytes = A::add(guaranteed - fixed, packetMetadata.value(), "RTP packet queue byte limit");
    if (!packetBytes) return R::failure(packetBytes.error());
    auto controlUnit = A::add(sizeof(MediaPreparedRtpOutput), sizeof(MediaRtpIngressEventBuffer),
        "RTP control queue wrapper");
    if (!controlUnit) return R::failure(controlUnit.error());
    controlUnit = A::add(controlUnit.value(), maximumCnameBytes, "RTP control queue CNAME");
    if (!controlUnit) return R::failure(controlUnit.error());
    auto controlBytes = A::multiply(controlObjects, controlUnit.value(), "RTP control queue byte limit");
    if (!controlBytes) return R::failure(controlBytes.error());
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    if (guaranteed > maximum || transaction.value() > maximum ||
        packetBytes.value() > maximum || controlBytes.value() > maximum ||
        stream.envelope.maximumAccessUnitsPerPush > maximum)
        return R::failure(::media::ErrorInfo::allocationFailed("RTP credit plan exceeds addressable storage"));
    return R::success({heldPayloadBytes, fixed, transaction.value(), guaranteed,
        static_cast<std::size_t>(stream.envelope.maximumAccessUnitsPerPush), packetObjects, controlObjects,
        static_cast<std::size_t>(packetBytes.value()), static_cast<std::size_t>(controlBytes.value())});
}
}
