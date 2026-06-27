#pragma once

#include "internal/FFmpegAudioPipelineTypes.h"
#include "internal/graph/nodes/output/packet/PacketOutputNode.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg {

class FFmpegAudioPacketWriter {
public:
    struct Config {
        PacketOutputNode* outputNode = nullptr;
        AVStream* outputStream = nullptr;
        std::string timestampErrorPrefix = "audio packet";
        std::string writeErrorMessage = "audio packet write failed";
    };

    FFmpegAudioPacketWriter() = default;

    void reset();
    Status initialize(Config config);

    Status write(AVPacket* packet,
                 const FFmpegAudioPacketWrittenCallback& onPacketWritten);

    bool isInitialized() const;
    int64_t packetCount() const;
    int64_t lastWrittenOutTimeMs() const;

private:
    Status validateTimestamp(const AVPacket* packet);
    void updateProgressFromPacket(const AVPacket* packet);

private:
    PacketOutputNode* m_outputNode = nullptr;
    AVStream* m_outputStream = nullptr;

    std::string m_timestampErrorPrefix = "audio packet";
    std::string m_writeErrorMessage = "audio packet write failed";

    int64_t m_packetCount = 0;
    int64_t m_lastWrittenDts = -9223372036854775807LL - 1LL;
    int64_t m_lastWrittenOutTimeMs = 0;
};

} // namespace media::ffmpeg
