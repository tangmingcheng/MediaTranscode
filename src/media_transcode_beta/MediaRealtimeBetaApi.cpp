#include "media_transcode_beta/realtime.h"

#include "media_transcode/Result.h"
#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"
#include "media_transcode_beta/MediaRealtimeBetaSession.h"

#include <exception>
#include <memory>
#include <new>
#include <utility>

struct mt_beta_realtime_session final {
    explicit mt_beta_realtime_session(
        media::beta::MediaRealtimeBetaOwnedConfig config) noexcept
        : implementation(std::move(config))
    {
    }

    media::beta::MediaRealtimeBetaSession implementation;
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
    case media::ErrorCode::WouldBlock:
        return MT_BETA_STATUS_INVALID_STATE;
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

        auto candidate = std::make_unique<mt_beta_realtime_session>(
            std::move(ownedConfig).value());
        *session = candidate.get();

        const auto status = candidate->implementation.start();
        if (!status) {
            *session = nullptr;
            return betaStatus(status);
        }

        candidate.release();
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
