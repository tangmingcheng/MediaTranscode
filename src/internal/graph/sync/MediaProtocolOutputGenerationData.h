#pragma once

#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/model/MediaProtocolGenerationStoragePlan.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"

#include <array>
#include <limits>
#include <type_traits>

namespace media::ffmpeg::graph {

// One worker alternates two preadmitted slots. Prepare constructs the spare
// before authority mutation; reset retires the old Data in its member order.
template <typename Data>
class MediaProtocolOutputGenerationData final : public MediaProtocolOutputGenerationSessionState {
    static_assert(std::is_default_constructible_v<Data>);
    static_assert(std::is_nothrow_destructible_v<Data>);
public:
    struct ConstructionKey final {
    private:
        friend class MediaProtocolOutputGenerationData;
        ConstructionKey() = default;
    };
    MediaProtocolOutputGenerationData(ConstructionKey,
        MediaOwnedMemoryAllocation first, MediaOwnedMemoryAllocation second)
        : m_slots{std::move(first), std::move(second)}
    { data = std::construct_at(static_cast<Data*>(m_slots[0].data())); }

    ~MediaProtocolOutputGenerationData() noexcept override
    {
        if (m_prepared) std::destroy_at(m_prepared);
        if (data) std::destroy_at(data);
    }

    static ::media::Result<MediaProtocolGenerationStorageGeometry> storageGeometry()
    {
        using Result = ::media::Result<MediaProtocolGenerationStorageGeometry>;
        auto shared = probeMediaSharedAllocationGeometry<MediaProtocolOutputGenerationData,
            ConstructionKey, MediaOwnedMemoryAllocation, MediaOwnedMemoryAllocation>();
        if (!shared) return Result::failure(shared.error());
        auto session = MediaNetworkMemoryResource::allocationFootprint(shared.value().bytes, shared.value().alignment);
        auto slot = MediaNetworkMemoryResource::allocationFootprint(sizeof(Data), alignof(Data));
        if (!session || !slot) return Result::failure(!session ? session.error() : slot.error());
        const auto maximum = (std::numeric_limits<std::size_t>::max)();
        if (slot.value() > (maximum - session.value()) / 2)
            return Result::failure(::media::ErrorInfo::invalidArgument("Generation storage footprint overflows"));
        return Result::success({shared.value(), sizeof(Data), alignof(Data),
            session.value(), slot.value(), session.value() + 2 * slot.value()});
    }

    static ::media::Result<std::shared_ptr<MediaProtocolOutputGenerationData>> create(
        const MediaProtocolGenerationStorageGeometry& geometry,
        const std::shared_ptr<MediaNetworkMemoryResource>& resource)
    {
        using Result = ::media::Result<std::shared_ptr<MediaProtocolOutputGenerationData>>;
        if (!resource || geometry.dataBytes != sizeof(Data) || geometry.dataAlignment != alignof(Data))
            return Result::failure(::media::ErrorInfo::invalidArgument("Generation storage requires its typed geometry and resource"));
        const auto sessionFootprint = MediaNetworkMemoryResource::allocationFootprint(
            geometry.session.bytes, geometry.session.alignment);
        const auto dataFootprint = MediaNetworkMemoryResource::allocationFootprint(
            geometry.dataBytes, geometry.dataAlignment);
        const auto maximum = (std::numeric_limits<std::size_t>::max)();
        // Geometry.bytes already includes count * sizeof(rebound allocation).
        // The allocator checks the exact rebound request at allocate_shared.
        if (!geometry.session.count || !geometry.session.bytes ||
            !sessionFootprint || !dataFootprint ||
            sessionFootprint.value() != geometry.sessionFootprint ||
            dataFootprint.value() != geometry.dataFootprint ||
            geometry.dataFootprint > (maximum - geometry.sessionFootprint) / 2 ||
            geometry.totalBytes != geometry.sessionFootprint + 2 * geometry.dataFootprint)
            return Result::failure(::media::ErrorInfo::invalidArgument("Generation storage footprints do not match its allocation requests"));
        const auto allocate = [&](std::size_t bytes, std::size_t alignment, std::size_t footprint)
            -> ::media::Result<MediaOwnedMemoryAllocation> {
            auto lease = resource->reserveStorage(footprint, MediaStorageAdmission::RequiredAllocation);
            if (!lease) return ::media::Result<MediaOwnedMemoryAllocation>::failure(lease.error());
            return resource->allocateFromReservation(bytes, alignment, std::move(lease).value());
        };
        auto first = allocate(geometry.dataBytes, geometry.dataAlignment, geometry.dataFootprint);
        if (!first) return Result::failure(first.error());
        auto second = allocate(geometry.dataBytes, geometry.dataAlignment, geometry.dataFootprint);
        if (!second) return Result::failure(second.error());
        auto shared = allocate(geometry.session.bytes, geometry.session.alignment, geometry.sessionFootprint);
        if (!shared) return Result::failure(shared.error());
        try {
            return Result::success(std::allocate_shared<MediaProtocolOutputGenerationData>(
                MediaSharedAllocationAllocator<MediaProtocolOutputGenerationData>{
                    MediaSharedAllocationAdmitted{resource, geometry.session, &shared.value()}},
                ConstructionKey{}, std::move(first).value(), std::move(second).value()));
        } catch (const MediaSharedAllocationMismatch&) {
            return Result::failure(::media::ErrorInfo::invalidArgument("Generation shared allocation geometry mismatch"));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed("Protocol output generation storage"));
        } catch (const std::exception& error) {
            return Result::failure(::media::ErrorInfo::invalidArgument(error.what()));
        }
    }

    Data* data = nullptr;

private:
    ::media::Status prepareForGenerationPurge() override
    {
        if (m_prepared) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Generation storage already has a prepared replacement"));
        try { m_prepared = std::construct_at(static_cast<Data*>(m_slots[1 - m_active].data())); }
        catch (const std::bad_alloc&) {
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Protocol output generation data"));
        } catch (const std::exception& error) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(error.what()));
        }
        return ::media::Status::success();
    }
    void resetForGenerationPurge() noexcept override
    {
        auto* retired = data;
        data = std::exchange(m_prepared, nullptr);
        m_active = 1 - m_active;
        std::destroy_at(retired);
    }
    std::array<MediaOwnedMemoryAllocation, 2> m_slots;
    std::size_t m_active = 0;
    Data* m_prepared = nullptr;
};

} // namespace media::ffmpeg::graph
