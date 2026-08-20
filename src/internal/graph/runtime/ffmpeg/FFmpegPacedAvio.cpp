#include "internal/graph/runtime/ffmpeg/FFmpegPacedAvio.h"
#include "internal/graph/runtime/ffmpeg/FFmpegAvioWritePacket.h"

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <new>
#include <thread>

namespace media::ffmpeg {
namespace {

constexpr uint32_t PacedAvioMagic = 0x4d545041U; // MTPA
constexpr int AvioBufferSize = 32 * 1024;
constexpr int64_t DefaultBurstBytes = 48 * 1024;

struct PacedAvioState {
    uint32_t magic = PacedAvioMagic;
    AVIOContext* inner = nullptr;
    int64_t bytesPerSecond = 0;
    int64_t burstBytes = DefaultBurstBytes;
    int64_t bytesWritten = 0;
    std::chrono::steady_clock::time_point startedAt = std::chrono::steady_clock::now();
};

PacedAvioState* pacedState(const AVIOContext* context) noexcept
{
    if (!context || !context->opaque) {
        return nullptr;
    }
    auto* state = static_cast<PacedAvioState*>(context->opaque);
    return state->magic == PacedAvioMagic ? state : nullptr;
}

void paceWrite(PacedAvioState& state, int size) noexcept
{
    if (state.bytesPerSecond <= 0 || size <= 0) {
        return;
    }

    const int64_t nextBytes = state.bytesWritten + size;
    const int64_t pacedBytes = std::max<int64_t>(0, nextBytes - state.burstBytes);
    const int64_t targetUs = pacedBytes * 1000000 / state.bytesPerSecond;
    const auto target = state.startedAt + std::chrono::microseconds(targetUs);
    const auto now = std::chrono::steady_clock::now();
    if (target > now) {
        std::this_thread::sleep_until(target);
    }
}

int pacedWritePacket(void* opaque,
                     FFmpegAvioWritePacketByte* buffer,
                     int size) noexcept
{
    auto* state = static_cast<PacedAvioState*>(opaque);
    if (!state || state->magic != PacedAvioMagic || !state->inner || !buffer || size < 0) {
        return AVERROR(EINVAL);
    }

    paceWrite(*state, size);
    avio_write(state->inner, buffer, size);
    avio_flush(state->inner);
    state->bytesWritten += size;
    return size;
}

} // namespace

int openPacedWriteAvio(AVIOContext** context,
                       const std::string& url,
                       const FFmpegPacedAvioOptions& options) noexcept
{
    if (!context) {
        return AVERROR(EINVAL);
    }
    *context = nullptr;

    AVIOContext* inner = nullptr;
    const int openRet = avio_open(&inner, url.c_str(), AVIO_FLAG_WRITE);
    if (openRet < 0) {
        return openRet;
    }

    auto* state = new (std::nothrow) PacedAvioState();
    if (!state) {
        avio_closep(&inner);
        return AVERROR(ENOMEM);
    }
    state->inner = inner;
    state->bytesPerSecond = options.bytesPerSecond;
    state->burstBytes = options.burstBytes > 0 ? options.burstBytes : DefaultBurstBytes;
    state->startedAt = std::chrono::steady_clock::now();

    auto* avioBuffer = static_cast<unsigned char*>(av_malloc(AvioBufferSize));
    if (!avioBuffer) {
        avio_closep(&inner);
        delete state;
        return AVERROR(ENOMEM);
    }

    AVIOContext* paced = avio_alloc_context(avioBuffer,
                                            AvioBufferSize,
                                            1,
                                            state,
                                            nullptr,
                                            pacedWritePacket,
                                            nullptr);
    if (!paced) {
        av_freep(&avioBuffer);
        avio_closep(&inner);
        delete state;
        return AVERROR(ENOMEM);
    }

    paced->seekable = 0;
    paced->max_packet_size = inner ? inner->max_packet_size : 0;
    *context = paced;
    return 0;
}

bool isPacedWriteAvio(const AVIOContext* context) noexcept
{
    return pacedState(context) != nullptr;
}

void resetPacedWriteAvio(AVIOContext* context) noexcept
{
    PacedAvioState* state = pacedState(context);
    if (!state) {
        return;
    }

    state->bytesWritten = 0;
    state->startedAt = std::chrono::steady_clock::now();
}

void closePacedWriteAvio(AVIOContext** context) noexcept
{
    if (!context || !*context) {
        return;
    }

    AVIOContext* paced = *context;
    PacedAvioState* state = pacedState(paced);
    if (!state) {
        return;
    }

    avio_flush(paced);
    if (state->inner) {
        avio_flush(state->inner);
        avio_closep(&state->inner);
    }
    state->magic = 0;
    delete state;
    avio_context_free(context);
}

} // namespace media::ffmpeg
