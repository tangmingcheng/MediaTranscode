#pragma once
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include <type_traits>
#include <algorithm>

namespace media::ffmpeg::graph {
// Ordered encoded packets retain their timestamps and are never display-dropped or repeated.
struct MediaEncodedAccessUnitSchedulingPlan final {
    MediaRunningTime transportLead;
    MediaRunningTime protocolPreparationLead;
    bool operator==(const MediaEncodedAccessUnitSchedulingPlan&) const = default;

    template<class Protocol>
    static ::media::Result<MediaEncodedAccessUnitSchedulingPlan> derive(
        const Protocol& protocol, MediaRunningTime targetResidence)
    {
        using Result = ::media::Result<MediaEncodedAccessUnitSchedulingPlan>;
        return std::visit([&](const auto& output) -> Result {
            using Output = std::remove_cvref_t<decltype(output)>;
            if constexpr (std::is_same_v<Output, MediaSeparateRtpOutputRuntimePlan>) {
                if (output.video.senderLead != output.audio.senderLead)
                    return Result::failure(::media::ErrorInfo::invalidArgument(
                        "Encoded output requires matching RTP sender leads"));
                return checked(output.video.senderLead, targetResidence);
            } else if constexpr (std::is_same_v<Output, MediaProjectMpegTsRuntimeOutputPlan>) {
                return checked(output.protocol.muxPlan().transportDecodeLead(), targetResidence);
            } else {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "Encoded output requires a protocol scheduling product"));
            }
        }, protocol);
    }

    template<class Protocol>
    ::media::Status validate(const Protocol& protocol, MediaRunningTime targetResidence) const
    {
        auto expected = derive(protocol, targetResidence);
        if (!expected) return ::media::Status::failure(expected.error());
        if (*this != expected.value())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Encoded scheduling differs from protocol and deployment facts"));
        return ::media::Status::success();
    }
    template<class Protocol>
    ::media::Result<MediaRunningTime> activationLead(const Protocol& protocol) const
    {
        auto valid = validate(protocol, protocolPreparationLead);
        if (!valid) return ::media::Result<MediaRunningTime>::failure(valid.error());
        return std::visit([&](const auto& output) -> ::media::Result<MediaRunningTime> {
            using Output = std::remove_cvref_t<decltype(output)>;
            if constexpr (std::is_same_v<Output, MediaProjectMpegTsRuntimeOutputPlan>)
                return transportLead.checkedAdd((std::max)(protocolPreparationLead,
                    output.protocol.muxPlan().startupEmissionPreroll()));
            return transportLead.checkedAdd(protocolPreparationLead);
        }, protocol);
    }
private:
    static ::media::Result<MediaEncodedAccessUnitSchedulingPlan> checked(
        MediaRunningTime transport, MediaRunningTime preparation)
    {
        using Result = ::media::Result<MediaEncodedAccessUnitSchedulingPlan>;
        if (transport.nanoseconds() <= 0 || preparation.nanoseconds() <= 0 || preparation > transport)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Encoded output requires positive leads within its deployment transport budget"));
        return Result::success({transport, preparation});
    }
};
} // namespace media::ffmpeg::graph
