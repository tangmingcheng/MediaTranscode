#pragma once

#include <optional>
#include <string>
#include <utility>

namespace media {

/**
 * @brief Stable error category returned by the public MediaTranscode API.
 *
 * The value is intentionally independent from FFmpeg's negative error codes.
 * When the failure comes from FFmpeg, nativeCode may still contain the original
 * FFmpeg return value so callers can log or map it if needed.
 */
enum class ErrorCode {
    None = 0,
    InvalidArgument,
    NotInitialized,
    AllocationFailed,
    Unsupported,
    FFmpegFailure,
    IoFailure,
    HardwareUnavailable,
    InternalError,
    Cancelled,
    WouldBlock
};

/**
 * @brief Convert an ErrorCode to a stable, non-localized string.
 */
inline const char* errorCodeName(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::None: return "None";
    case ErrorCode::InvalidArgument: return "InvalidArgument";
    case ErrorCode::NotInitialized: return "NotInitialized";
    case ErrorCode::AllocationFailed: return "AllocationFailed";
    case ErrorCode::Unsupported: return "Unsupported";
    case ErrorCode::FFmpegFailure: return "FFmpegFailure";
    case ErrorCode::IoFailure: return "IoFailure";
    case ErrorCode::HardwareUnavailable: return "HardwareUnavailable";
    case ErrorCode::WouldBlock: return "WouldBlock";
    case ErrorCode::InternalError: return "InternalError";
    case ErrorCode::Cancelled: return "Cancelled";
    default: return "Unknown";
    }
}

/**
 * @brief Error payload used by Status and Result<T>.
 *
 * message is intended for logs and diagnostics. It is not localized and should
 * not be parsed by business code. Business code should branch on code.
 */
struct ErrorInfo {
    ErrorCode code = ErrorCode::None;
    int nativeCode = 0;
    std::string message;

    bool ok() const noexcept
    {
        return code == ErrorCode::None;
    }

    explicit operator bool() const noexcept
    {
        return !ok();
    }

    std::string describe() const
    {
        if (ok()) {
            return "ok";
        }

        std::string text = std::string(errorCodeName(code)) + ": " + message;
        if (nativeCode != 0) {
            text += " (native=" + std::to_string(nativeCode) + ")";
        }
        return text;
    }

    static ErrorInfo success()
    {
        return {};
    }

    static ErrorInfo make(ErrorCode code,
                          std::string message,
                          int nativeCode = 0)
    {
        return ErrorInfo{ code, nativeCode, std::move(message) };
    }

    static ErrorInfo invalidArgument(std::string message)
    {
        return make(ErrorCode::InvalidArgument, std::move(message));
    }

    static ErrorInfo notInitialized(std::string message)
    {
        return make(ErrorCode::NotInitialized, std::move(message));
    }

    static ErrorInfo allocationFailed(std::string message)
    {
        return make(ErrorCode::AllocationFailed, std::move(message));
    }

    static ErrorInfo unsupported(std::string message)
    {
        return make(ErrorCode::Unsupported, std::move(message));
    }

    static ErrorInfo ffmpegFailure(std::string message, int nativeCode = 0)
    {
        return make(ErrorCode::FFmpegFailure, std::move(message), nativeCode);
    }

    static ErrorInfo ioFailure(std::string message, int nativeCode = 0)
    {
        return make(ErrorCode::IoFailure, std::move(message), nativeCode);
    }

    static ErrorInfo hardwareUnavailable(std::string message)
    {
        return make(ErrorCode::HardwareUnavailable, std::move(message));
    }

    static ErrorInfo wouldBlock(std::string message)
    {
        return make(ErrorCode::WouldBlock, std::move(message));
    }

    static ErrorInfo internalError(std::string message)
    {
        return make(ErrorCode::InternalError, std::move(message));
    }

    static ErrorInfo cancelled(std::string message)
    {
        return make(ErrorCode::Cancelled, std::move(message));
    }
};

/**
 * @brief Lightweight expected-like return type.
 *
 * Result<T> owns either a T value or an ErrorInfo. It does not throw exceptions.
 * Always check ok() or use the explicit bool operator before calling value().
 */
template <typename T>
class Result {
public:
    static Result success(T value)
    {
        return Result(std::move(value));
    }

    static Result failure(ErrorInfo error)
    {
        if (error.ok()) {
            error = ErrorInfo::internalError("unknown error");
        }
        return Result(std::move(error));
    }

    bool ok() const noexcept
    {
        return m_value.has_value();
    }

    explicit operator bool() const noexcept
    {
        return ok();
    }

    T& value() &
    {
        return *m_value;
    }

    const T& value() const&
    {
        return *m_value;
    }

    T&& value() &&
    {
        return std::move(*m_value);
    }

    const ErrorInfo& error() const noexcept
    {
        return m_error;
    }

    T valueOr(T fallback) const
    {
        return m_value ? *m_value : std::move(fallback);
    }

private:
    explicit Result(T value)
        : m_value(std::move(value))
    {
    }

    explicit Result(ErrorInfo error)
        : m_value(std::nullopt)
        , m_error(std::move(error))
    {
    }

private:
    std::optional<T> m_value;
    ErrorInfo m_error;
};

/**
 * @brief Result specialization for operations that only need success/failure.
 */
template <>
class Result<void> {
public:
    static Result success()
    {
        return Result();
    }

    static Result failure(ErrorInfo error)
    {
        if (error.ok()) {
            error = ErrorInfo::internalError("unknown error");
        }
        return Result(std::move(error));
    }

    bool ok() const noexcept
    {
        return m_error.ok();
    }

    explicit operator bool() const noexcept
    {
        return ok();
    }

    const ErrorInfo& error() const noexcept
    {
        return m_error;
    }

private:
    Result() = default;

    explicit Result(ErrorInfo error)
        : m_error(std::move(error))
    {
    }

private:
    ErrorInfo m_error;
};

using Status = Result<void>;

} // namespace media
