#pragma once

#include "core/loader/self_loader.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace vshift::boot {

struct BootFile final {
    std::vector<std::uint8_t> bytes;
    std::string error;

    bool ok() const noexcept { return error.empty() && !bytes.empty(); }
};

using BootFileReader = std::function<BootFile(std::string_view relative_path)>;

enum class Ps4BootStage : std::uint8_t {
    None = 0,
    FirmwareRoot,
    SysCore,
    ShellCore,
    GuestExecution,
    FramePresentation,
};

struct Ps4ModuleReport final {
    std::string path;
    bool present = false;
    bool is_self = false;
    bool payload_protected = false;
    std::uint64_t entry = 0;
    std::size_t mapped_segments = 0;
    std::string error;

    bool mapped() const noexcept {
        return error.empty() && present && mapped_segments != 0;
    }
};

struct Ps4BootReport final {
    Ps4BootStage stage = Ps4BootStage::None;
    Ps4ModuleReport syscore;
    Ps4ModuleReport shellcore;
    std::uint64_t guest_instructions = 0;
    bool guest_returned = false;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
    bool modules_mapped() const noexcept {
        return syscore.mapped() && shellcore.mapped();
    }
};

// Loads the two real PS4 system processes into separate guest address spaces.
// The caller owns the file source and supplies only user-provided bytes. No
// keys or protected SELF transformation is attempted here.
class Ps4BootSession final {
public:
    Ps4BootReport Run(const BootFileReader& read_file);

    const memory::GuestMemory& syscore_memory() const noexcept {
        return syscore_memory_;
    }
    const memory::GuestMemory& shellcore_memory() const noexcept {
        return shellcore_memory_;
    }

private:
    memory::GuestMemory syscore_memory_;
    memory::GuestMemory shellcore_memory_;
};

} // namespace vshift::boot
