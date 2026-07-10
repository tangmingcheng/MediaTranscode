#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

#include <filesystem>

namespace media::ffmpeg::graph {

class RawRtpInputNode final : public FFmpegNodeRuntime {
public:
    explicit RawRtpInputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

private:
    ::media::Status openInput(MediaGraphExecutionContext& context);
    ::media::Result<std::filesystem::path> writeSdpFile(const std::string& sdpText) const;
    void cleanupSdpFile() noexcept;

private:
    ::media::ffmpeg::InputFormatContextPtr m_context;
    std::filesystem::path m_sdpPath;
    bool m_formatEmitted = false;
};

} // namespace media::ffmpeg::graph
