#include "core/firmware/vfs.h"

#include <cassert>
#include <cstdint>
#include <string_view>

int main() {
    vshift::firmware::ReadOnlyVfs vfs(
        [](std::string_view path) -> vshift::firmware::VfsFile {
            if (path == "system/sys/SceSysCore.elf") {
                return {{0x7f, 'E', 'L', 'F'}, {}};
            }
            return {{}, "not found"};
        });

    const auto file = vfs.ReadFile("system\\sys/SceSysCore.elf");
    assert(file.ok());
    assert(file.bytes.size() == 4);

    const auto absolute = vfs.ReadFile("C:/firmware/SceSysCore.elf");
    assert(!absolute.ok());

    const auto parent = vfs.ReadFile("system/../SceSysCore.elf");
    assert(!parent.ok());
    return 0;
}
