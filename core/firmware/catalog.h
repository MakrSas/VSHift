#pragma once

#include "core/firmware/slb2.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace vshift::firmware {

struct ComponentRange final {
    std::uint64_t absolute_offset = 0;
    std::uint64_t size = 0;
};

struct ComponentRangeResult final {
    ComponentRange range;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Metadata-only view over an SLB2 table. It owns component names and ranges,
// but never owns or reads firmware bytes.
class ReadOnlyFirmwareCatalog final {
public:
    explicit ReadOnlyFirmwareCatalog(Slb2Package package);

    const Slb2Package& package() const noexcept { return package_; }
    const Slb2Entry* Find(std::string_view name) const noexcept;

    // Resolves a relative range inside one component to an absolute container
    // range. The caller remains responsible for opening the source file and
    // reading only the returned range.
    ComponentRangeResult Resolve(std::string_view name,
                                 std::uint64_t relative_offset,
                                 std::uint64_t size) const;

private:
    Slb2Package package_;
};

} // namespace vshift::firmware
