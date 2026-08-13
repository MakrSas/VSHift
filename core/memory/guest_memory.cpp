#include "core/memory/guest_memory.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace vshift::memory {

namespace {

bool AddWouldOverflow(std::uint64_t left,
                      std::uint64_t right) noexcept {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

} // namespace

bool GuestMemory::Contains(const Mapping& mapping,
                           std::uint64_t guest_address,
                           std::uint64_t size) noexcept {
    if (guest_address < mapping.guest_address ||
        AddWouldOverflow(guest_address, size) ||
        AddWouldOverflow(mapping.guest_address, mapping.size)) {
        return false;
    }
    const auto mapping_end = mapping.guest_address + mapping.size;
    return guest_address + size <= mapping_end;
}

MemoryResult GuestMemory::Map(Mapping mapping) {
    MemoryResult result;
    if (mapping.size == 0) {
        result.error = "guest mapping cannot be empty";
        return result;
    }
    if (AddWouldOverflow(mapping.guest_address, mapping.size)) {
        result.error = "guest mapping overflows the address space";
        return result;
    }
    if (mapping.size > std::numeric_limits<std::size_t>::max()) {
        result.error = "guest mapping is too large for this host";
        return result;
    }

    for (const auto& region : regions_) {
        const auto overlaps =
            region.mapping.guest_address < mapping.guest_address +
                                                 mapping.size &&
            mapping.guest_address < region.mapping.guest_address +
                                         region.mapping.size;
        if (overlaps) {
            result.error = "guest mapping overlaps an existing region";
            return result;
        }
    }

    Region region;
    region.mapping = mapping;
    region.bytes.resize(static_cast<std::size_t>(mapping.size), 0);
    regions_.push_back(std::move(region));
    return result;
}

const GuestMemory::Region* GuestMemory::FindRegion(
    std::uint64_t guest_address,
    std::uint64_t size) const noexcept {
    for (const auto& region : regions_) {
        if (Contains(region.mapping, guest_address, size)) {
            return &region;
        }
    }
    return nullptr;
}

GuestMemory::Region* GuestMemory::FindRegion(std::uint64_t guest_address,
                                             std::uint64_t size) noexcept {
    for (auto& region : regions_) {
        if (Contains(region.mapping, guest_address, size)) {
            return &region;
        }
    }
    return nullptr;
}

MemoryResult GuestMemory::Initialize(
    std::uint64_t guest_address,
    std::span<const std::uint8_t> bytes) {
    MemoryResult result;
    auto* region = FindRegion(guest_address, bytes.size());
    if (region == nullptr) {
        result.error = "loader range is not mapped in guest memory";
        return result;
    }

    const auto offset = static_cast<std::size_t>(
        guest_address - region->mapping.guest_address);
    std::copy(bytes.begin(), bytes.end(), region->bytes.begin() + offset);
    return result;
}

MemoryResult GuestMemory::Read(std::uint64_t guest_address,
                               std::span<std::uint8_t> output) const {
    MemoryResult result;
    const auto* region = FindRegion(guest_address, output.size());
    if (region == nullptr ||
        (region->mapping.permissions & kPermissionRead) == 0) {
        result.error = "guest read is outside a readable mapping";
        return result;
    }

    const auto offset = static_cast<std::size_t>(
        guest_address - region->mapping.guest_address);
    std::copy(region->bytes.begin() + offset,
              region->bytes.begin() + offset + output.size(),
              output.begin());
    return result;
}

MemoryResult GuestMemory::Write(
    std::uint64_t guest_address,
    std::span<const std::uint8_t> bytes) {
    MemoryResult result;
    auto* region = FindRegion(guest_address, bytes.size());
    if (region == nullptr ||
        (region->mapping.permissions & kPermissionWrite) == 0) {
        result.error = "guest write is outside a writable mapping";
        return result;
    }

    const auto offset = static_cast<std::size_t>(
        guest_address - region->mapping.guest_address);
    std::copy(bytes.begin(), bytes.end(), region->bytes.begin() + offset);
    return result;
}

std::vector<Mapping> GuestMemory::Mappings() const {
    std::vector<Mapping> mappings;
    mappings.reserve(regions_.size());
    for (const auto& region : regions_) {
        mappings.push_back(region.mapping);
    }
    return mappings;
}

} // namespace vshift::memory
