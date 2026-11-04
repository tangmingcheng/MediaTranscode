#include "internal/graph/utils/MediaCleanupError.h"
#include "internal/graph/runtime/threading/MediaWorkerStartupGate.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"
#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"
#include <exception>
#include <new>
#include <system_error>
namespace media::ffmpeg::graph {
::media::ErrorInfo MediaWorkerStartupGate::currentException() noexcept {
    try {
        throw;
    } catch (const std::bad_alloc&) {
        return {::media::ErrorCode::AllocationFailed, 0, {}};
    } catch (const std::system_error& error) {
        return mediaCleanupError(::media::ErrorCode::InternalError, error.what(), error.code().value());
    } catch (const std::exception& error) {
        return mediaCleanupError(::media::ErrorCode::InternalError, error.what(), 0);
    } catch (...) {
        return {::media::ErrorCode::InternalError, 0, {}};
    }
}

::media::Status MediaWorkerStartupGate::prepare(MediaGraphExecutionContext& context, MediaGraphScheduler& scheduler,
    MediaFixedCapacityStorage<std::unique_ptr<MediaGraphWorker>>& workers,
    MediaGraphWorkerFailureRecorder& failures, MediaGraphWorkerFailureSupervisor& supervisor) {
    try {
        if (!context.compiled() || !context.graph() || !workers.empty()) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Worker preparation requires a compiled scope and empty owners"));
        const auto& plans = context.workerExecutionPlans();
        const auto& order = context.executionOrder();
        if (plans.size() != order.size()) return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Worker contract differs from compiled execution scope"));
        for (std::size_t i = 0; i < plans.size(); ++i) {
            if (plans[i].node != order[i]) return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Worker contract identity differs from compiled execution order"));
        }
        workers = MediaFixedCapacityStorage<std::unique_ptr<MediaGraphWorker>>(plans.size());
        for (const auto& plan : plans) {
            auto* node = scheduler.findNode(plan.node);
            if (!node) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Worker plan has no runtime node"));
            workers.push_back(std::make_unique<MediaGraphWorker>(*node, context, failures, supervisor, plan, *this));
        }
        return ::media::Status::success();
    } catch (...) {
        return ::media::Status::failure(currentException());
    }
}
::media::Status MediaWorkerStartupGate::launch(std::span<const std::unique_ptr<MediaGraphWorker>> workers) {
    try {
        for (const auto& worker : workers) {
            auto status = worker->start();
            if (!status) { cancel(); return status; }
        }
        if (awaitReady()) return ::media::Status::success();
        return ::media::Status::failure(::media::ErrorInfo::cancelled("Worker preparation was cancelled before media execution"));
    } catch (...) {
        cancel();
        return ::media::Status::failure(currentException());
    }
}
}
