#pragma once

#include <optional>
#include <string>
#include <utility>

namespace media {

enum class ErrorCode {
    None = 0,
    InvalidArgument,
    NotInitialized,
    AllocationFailed,
    Unsupported,
    FFmpegFailure,
    IoFailure,
    HardwareUnavailable,
    InternalError
};

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

    static ErrorInfo internalError(std::string message)
    {
        return make(ErrorCode::InternalError, std::move(message));
    }
};

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
