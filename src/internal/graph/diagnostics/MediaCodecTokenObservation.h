#pragma once

#include "internal/graph/sync/lineage/MediaFfmpegLineageToken.h"
#include <optional>
#include <cstdint>
#include <ostream>

namespace media::ffmpeg::graph {
struct MediaCodecTokenIdentity final {
    std::uint64_t identifier;
    std::uint64_t generation;
};

inline std::optional<MediaCodecTokenIdentity> mediaCodecTokenIdentity(const AVBufferRef* opaque) noexcept
{
    try {
        auto token = mediaFfmpegLineageToken(opaque);
        if (token) return MediaCodecTokenIdentity{token.value().identifier, token.value().generation};
    } catch (...) {}
    return std::nullopt;
}

// Stored only in an existing bounded registry entry. No retained frame or lease.
struct MediaCodecSendObservation final {
    std::optional<std::int64_t> inputPts;
    int timeBaseNumerator = 0;
    int timeBaseDenominator = 0;
    std::uint64_t acceptedCount = 0;
    std::uint64_t outputCount = 0;
    bool incomplete = false;
};

// Immutable value snapshot after receive; a token is an opaque association,
// not proof of a one-input/one-output codec relation.
struct MediaCodecTokenObservation final {
    std::uint32_t encoderNode = 0;
    std::optional<MediaCodecTokenIdentity> selectedToken;
    std::optional<MediaCodecTokenIdentity> packetToken;
    MediaCodecSendObservation submission;
    std::int64_t packetPts = 0;
    std::int64_t packetDts = 0;
    int packetBytes = 0;
    std::optional<bool> copyOpaque;

    void append(std::ostream& out) const
    {
        const auto token = [&](const char* name, const std::optional<MediaCodecTokenIdentity>& value) {
            out << ' ' << name << "_id=";
            if (value) out << value->identifier; else out << "unavailable";
            out << ' ' << name << "_generation=";
            if (value) out << value->generation; else out << "unavailable";
        };
        out << " codec_encoder_node=" << encoderNode;
        token("codec_selected_token", selectedToken);
        token("codec_packet_token", packetToken);
        out << " codec_token_mapping=";
        if (copyOpaque) out << (*copyOpaque ? "copy_opaque" : "submission_order");
        else out << "unavailable";
        out << " codec_input_pts=";
        if (submission.inputPts) out << *submission.inputPts; else out << "unavailable";
        out << " codec_timebase_num=";
        if (submission.inputPts) out << submission.timeBaseNumerator; else out << "unavailable";
        out << " codec_timebase_den=";
        if (submission.inputPts) out << submission.timeBaseDenominator; else out << "unavailable";
        out << " codec_accepted_send_count=" << submission.acceptedCount
            << " codec_token_output_ordinal=" << submission.outputCount
            << " codec_packet_pts=" << packetPts << " codec_packet_dts=" << packetDts
            << " codec_packet_bytes=" << packetBytes
            << " codec_identity_incomplete=" << submission.incomplete;
    }
};
} // namespace media::ffmpeg::graph
