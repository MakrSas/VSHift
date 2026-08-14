#pragma once

#include "core/firmware/vfs.h"
#include "core/loader/self_loader.h"
#include "core/video/framebuffer.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace vshift::boot {

using BootFile = firmware::VfsFile;
using BootFileReader = firmware::VfsReader;

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
    explicit Ps4BootSession(video::FramePresenter frame_presenter = {});

    Ps4BootReport Run(const BootFileReader& read_file);

    video::FrameBuffer& video_output() noexcept { return video_output_; }
    const video::FrameBuffer& video_output() const noexcept {
        return video_output_;
    }

    const memory::GuestMemory& syscore_memory() const noexcept {
        return syscore_memory_;
    }
    const memory::GuestMemory& shellcore_memory() const noexcept {
        return shellcore_memory_;
    }

private:
    memory::GuestMemory syscore_memory_;
    memory::GuestMemory shellcore_memory_;
    video::FrameBuffer video_output_;
};

} // namespace vshift::boot
