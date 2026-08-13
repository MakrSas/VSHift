#include "core/firmware/catalog.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

int main() {
    vshift::firmware::Slb2Package package;
    package.entries.push_back({0x1000, 0x200, "system_b"});
    package.entries.push_back({0x5000, 0x80, "system_ex_b"});

    const vshift::firmware::ReadOnlyFirmwareCatalog catalog(std::move(package));
    const auto *entry = catalog.Find("system_b");
    assert(entry != nullptr);
    assert(entry->size == 0x200);

    const auto range = catalog.Resolve("system_b", 0x20, 0x40);
    assert(range.ok());
    assert(range.range.absolute_offset == 0x1020);
    assert(range.range.size == 0x40);

    assert(!catalog.Resolve("system_b", 0x1f0, 0x20).ok());
    assert(!catalog.Resolve("missing", 0, 1).ok());

    const auto empty = catalog.Resolve("system_ex_b", 0x80, 0);
    assert(empty.ok());
    assert(empty.range.absolute_offset == 0x5080);
    return 0;
}
