#include "internal/graph/builder/realtime/MediaCompositionSourceInputValidator.h"
#include <new>
namespace media::ffmpeg::graph {
::media::Status MediaCompositionSourceInputValidator::validate(const MediaRealtimeInputGraph& input)
try {
    const auto invalid = [](const char* text) { return ::media::Status::failure(::media::ErrorInfo::invalidArgument(text)); };
    if (!input.synchronized || !input.synchronized->registration.rtpClock ||
        !input.synchronized->bindings.protocol.rtp)
        return invalid("Composition source topology requires actual RTP input bindings");
    const auto& registered=input.synchronized->registration;
    const auto& bindings=input.synchronized->bindings;
    const auto& rtp=*bindings.protocol.rtp;
    const auto& actual=*registered.rtpClock;
    if (registered.epochBinder!=bindings.shared.epochBinder ||
        registered.activationSequencer!=bindings.shared.activationSequencer ||
        registered.releaseExtractor!=bindings.shared.releaseExtractor ||
        actual.clockGroup!=rtp.registered.clockGroup || actual.videoRaw!=rtp.registered.videoRaw ||
        actual.videoBinder!=rtp.registered.videoBinder || actual.audioRaw!=rtp.registered.audioRaw ||
        actual.audioBinder!=rtp.registered.audioBinder)
        return invalid("Composition input bindings differ from their runtime registration");
    return ::media::Status::success();
} catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("source topology actual binding validation"));
}

}
