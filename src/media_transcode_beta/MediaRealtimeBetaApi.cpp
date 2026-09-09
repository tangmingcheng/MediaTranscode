#include "media_transcode_beta/realtime.h"

#include "media_transcode/Result.h"
#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"
#include "media_transcode_beta/MediaRealtimeBetaSession.h"
#include "media_transcode_beta/MediaRealtimeBetaStartPublication.h"

#include <exception>
#include <memory>
#include <new>
#include <utility>

struct mt_beta_realtime_session final {
    explicit mt_beta_realtime_session(
        media::beta::MediaRealtimeBetaOwnedConfig config,
        std::shared_ptr<media::beta::MediaRealtimeBetaStartPublication>
            startPublication) noexcept
        : implementation(std::move(config), std::move(startPublication))
    {
    }

    media::beta::MediaRealtimeBetaSession implementation;
};

struct mt_beta_output_snapshots final {
    std::vector<media::ffmpeg::graph::MediaRealtimeOutputSnapshot> outputs;
};

namespace {

mt_beta_status betaStatus(media::ErrorCode errorCode) noexcept
{
    switch (errorCode) {
    case media::ErrorCode::None:
        return MT_BETA_STATUS_OK;
    case media::ErrorCode::InvalidArgument:
        return MT_BETA_STATUS_INVALID_ARGUMENT;
    case media::ErrorCode::AllocationFailed:
        return MT_BETA_STATUS_ALLOCATION_FAILED;
    case media::ErrorCode::NotInitialized:
    case media::ErrorCode::Cancelled:
        return MT_BETA_STATUS_INVALID_STATE;
    case media::ErrorCode::WouldBlock:
        return MT_BETA_STATUS_BUSY;
    case media::ErrorCode::Unsupported:
    case media::ErrorCode::FFmpegFailure:
    case media::ErrorCode::IoFailure:
    case media::ErrorCode::HardwareUnavailable:
    case media::ErrorCode::InternalError:
        return MT_BETA_STATUS_INTERNAL_ERROR;
    }
    return MT_BETA_STATUS_INTERNAL_ERROR;
}

mt_beta_status betaStatus(const media::Status& status) noexcept
{
    return status ? MT_BETA_STATUS_OK : betaStatus(status.error().code);
}

template <typename Operation>
mt_beta_status invokeStatus(Operation&& operation) noexcept
{
    try {
        return operation();
    } catch (const std::bad_alloc&) {
        return MT_BETA_STATUS_ALLOCATION_FAILED;
    } catch (...) {
        return MT_BETA_STATUS_INTERNAL_ERROR;
    }
}

} // namespace

extern "C" mt_beta_status mt_beta_realtime_start(
    const mt_beta_realtime_config* config,
    const mt_beta_realtime_callbacks* callbacks,
    mt_beta_realtime_session** session)
{
    if (session == nullptr) {
        return MT_BETA_STATUS_INVALID_ARGUMENT;
    }
    *session = nullptr;

    try {
        auto ownedConfig = media::beta::MediaRealtimeBetaOwnedConfig::create(
            config, callbacks, session);
        if (!ownedConfig) {
            return betaStatus(ownedConfig.error().code);
        }

        auto startPublication = std::make_shared<
            media::beta::MediaRealtimeBetaStartPublication>();
        auto candidate = std::make_unique<mt_beta_realtime_session>(
            std::move(ownedConfig).value(), startPublication);

        const auto status = candidate->implementation.start();
        if (!status) {
            return betaStatus(status);
        }

        *session = candidate.release();
        startPublication->publishOwnership();
        return MT_BETA_STATUS_OK;
    } catch (const std::bad_alloc&) {
        *session = nullptr;
        return MT_BETA_STATUS_ALLOCATION_FAILED;
    } catch (const std::exception&) {
        *session = nullptr;
        return MT_BETA_STATUS_INTERNAL_ERROR;
    } catch (...) {
        *session = nullptr;
        return MT_BETA_STATUS_INTERNAL_ERROR;
    }
}

extern "C" mt_beta_status mt_beta_realtime_request_stop(
    mt_beta_realtime_session* session)
{
    if (session == nullptr) {
        return MT_BETA_STATUS_INVALID_ARGUMENT;
    }

    try {
        session->implementation.requestStop();
        return MT_BETA_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return MT_BETA_STATUS_ALLOCATION_FAILED;
    } catch (const std::exception&) {
        return MT_BETA_STATUS_INTERNAL_ERROR;
    } catch (...) {
        return MT_BETA_STATUS_INTERNAL_ERROR;
    }
}

extern "C" mt_beta_status mt_beta_realtime_get_snapshot(
    mt_beta_realtime_session* session,
    mt_beta_realtime_snapshot* snapshot)
{
    if (session == nullptr || snapshot == nullptr) {
        return MT_BETA_STATUS_INVALID_ARGUMENT;
    }

    try {
        *snapshot = session->implementation.snapshot();
        return MT_BETA_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return MT_BETA_STATUS_ALLOCATION_FAILED;
    } catch (const std::exception&) {
        return MT_BETA_STATUS_INTERNAL_ERROR;
    } catch (...) {
        return MT_BETA_STATUS_INTERNAL_ERROR;
    }
}

extern "C" void mt_beta_realtime_release(mt_beta_realtime_session** session)
{
    if (session == nullptr || *session == nullptr) {
        return;
    }

    try {
        mt_beta_realtime_session* candidate = *session;
        if (candidate->implementation.isCurrentThreadEventThread()) {
            return;
        }
        delete candidate;
        *session = nullptr;
    } catch (const std::bad_alloc&) {
    } catch (const std::exception&) {
    } catch (...) {
    }
}

extern "C" mt_beta_status mt_beta_realtime_add_output(
    mt_beta_realtime_session* session,
    const mt_beta_video_output* output,
    uint64_t* output_id)
{
    if (output_id) *output_id = 0;
    if (!session || !output || !output_id) return MT_BETA_STATUS_INVALID_ARGUMENT;
    return invokeStatus([&] {
        auto result = session->implementation.addOutput(*output);
        if (!result) return betaStatus(result.error().code);
        *output_id = result.value();
        return MT_BETA_STATUS_OK;
    });
}

extern "C" mt_beta_status mt_beta_realtime_remove_output(
    mt_beta_realtime_session* session, uint64_t output_id)
{
    if (!session || !output_id) return MT_BETA_STATUS_INVALID_ARGUMENT;
    return invokeStatus([&] { return betaStatus(session->implementation.removeOutput(output_id)); });
}

extern "C" mt_beta_status mt_beta_realtime_get_output_snapshots(
    mt_beta_realtime_session* session, mt_beta_output_snapshots** snapshots)
{
    if (snapshots) *snapshots = nullptr;
    if (!session || !snapshots) return MT_BETA_STATUS_INVALID_ARGUMENT;
    return invokeStatus([&] {
        auto result = std::make_unique<mt_beta_output_snapshots>();
        result->outputs = session->implementation.outputSnapshots();
        *snapshots = result.release();
        return MT_BETA_STATUS_OK;
    });
}

extern "C" size_t mt_beta_output_snapshots_count(const mt_beta_output_snapshots* snapshots)
{
    return snapshots ? snapshots->outputs.size() : 0;
}

extern "C" mt_beta_status mt_beta_output_snapshots_get(
    const mt_beta_output_snapshots* snapshots, size_t index, mt_beta_output_snapshot* snapshot)
{
    if (!snapshots || !snapshot || index >= snapshots->outputs.size())
        return MT_BETA_STATUS_INVALID_ARGUMENT;
    *snapshot = media::beta::MediaRealtimeBetaSession::projectOutput(snapshots->outputs[index]);
    return MT_BETA_STATUS_OK;
}

extern "C" void mt_beta_output_snapshots_release(mt_beta_output_snapshots** snapshots)
{
    if (!snapshots) return;
    delete *snapshots;
    *snapshots = nullptr;
}
