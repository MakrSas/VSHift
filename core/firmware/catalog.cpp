#include "core/firmware/catalog.h"

#include <limits>
#include <utility>

namespace vshift::firmware {

ReadOnlyFirmwareCatalog::ReadOnlyFirmwareCatalog(Slb2Package package)
    : package_(std::move(package)) {}

const Slb2Entry* ReadOnlyFirmwareCatalog::Find(
    std::string_view name) const noexcept {
    for (const auto& entry : package_.entries) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

ComponentRangeResult ReadOnlyFirmwareCatalog::Resolve(
    std::string_view name,
    std::uint64_t relative_offset,
    std::uint64_t size) const {
    ComponentRangeResult result;
    const auto* entry = Find(name);
    if (entry == nullptr) {
        result.error = "firmware component was not found";
        return result;
    }

    if (relative_offset > entry->size ||
        size > entry->size - relative_offset) {
        result.error = "requested range is outside the firmware component";
        return result;
    }

    if (relative_offset > std::numeric_limits<std::uint64_t>::max() -
                               entry->offset) {
        result.error = "firmware component offset overflows";
        return result;
    }

    result.range.absolute_offset = entry->offset + relative_offset;
    result.range.size = size;
    return result;
}

} // namespace vshift::firmware
