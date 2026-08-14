#include "core/boot/ps4_boot.h"

#include "core/cpu/x86_runtime.h"
#include "core/loader/elf_loader.h"
#include "core/loader/elf_dynamic.h"
#include "core/loader/self.h"

#include <array>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>

namespace vshift::boot {

Ps4BootSession::Ps4BootSession(video::FramePresenter frame_presenter)
    : video_output_(std::move(frame_presenter)) {}

namespace {

Ps4ModuleReport LoadModule(std::string_view path,
                           const BootFile& file,
                           memory::GuestMemory& memory) {
    Ps4ModuleReport report;
    report.path = std::string(path);
    if (!file.ok()) {
        report.error = file.error.empty() ? "module is unavailable" : file.error;
        return report;
    }

    report.present = true;
    const std::span<const std::uint8_t> bytes(file.bytes);
    if (bytes.size() >= sizeof(std::uint32_t) &&
        bytes[0] == 0x4f && bytes[1] == 0x15 && bytes[2] == 0x3d &&
        bytes[3] == 0x1d) {
        report.is_self = true;
        const auto parsed = loader::ParsePs4SelfHeaders(bytes, bytes.size());
        if (!parsed.ok()) {
            report.error = parsed.error;
            return report;
        }
        for (const auto& entry : parsed.entries) {
            report.payload_protected = report.payload_protected ||
                                       entry.is_encrypted() ||
                                       entry.is_compressed() ||
                                       entry.is_blocked();
        }
        const auto loaded = loader::MapSelfLoadSegments(parsed, bytes, memory);
        if (!loaded.ok()) {
            report.error = loaded.error;
            return report;
        }
        report.entry = loaded.entry;
        report.mapped_segments = loaded.mappings.size();
        return report;
    }

    if (bytes.size() < loader::kElf64HeaderSize) {
        report.error = "module is neither PS4 SELF nor ELF64";
        return report;
    }
    const auto parsed = loader::ParseElf64Headers(bytes, bytes.size());
    if (!parsed.ok()) {
        report.error = parsed.error;
        return report;
    }
    const auto loaded = loader::MapElfLoadSegments(parsed, bytes, memory);
    if (!loaded.ok()) {
        report.error = loaded.error;
        return report;
    }
    report.entry = loaded.entry;
    report.mapped_segments = loaded.mappings.size();
    const auto dynamic = loader::ParseElf64Dynamic(parsed, bytes);
    report.dynamic_present = dynamic.present;
    report.needed_libraries = dynamic.needed_libraries;
    report.dynamic_error = dynamic.error;
    if (!dynamic.ok()) {
        report.error = "ELF dynamic metadata is invalid: " + dynamic.error;
        return report;
    }
    return report;
}

bool LoadDependencies(firmware::ReadOnlyVfs& vfs,
                      const Ps4ModuleReport& owner,
                      memory::GuestMemory& memory,
                      std::vector<Ps4ModuleReport>& reports,
                      std::string& error) {
    const std::array<std::string, 3> prefixes = {
        "system/common/lib/",
        "system/common/lib/kernel/",
        "system/sys/",
    };
    std::unordered_set<std::string> loaded_names;
    for (const auto& name : owner.needed_libraries) {
        if (name.empty() || name.find_first_of("/\\") != std::string::npos ||
            !loaded_names.insert(name).second) {
            continue;
        }

        BootFile file;
        std::string resolved_path;
        for (const auto& prefix : prefixes) {
            const auto candidate = prefix + name;
            file = vfs.ReadFile(candidate);
            if (file.ok()) {
                resolved_path = candidate;
                break;
            }
        }
        if (!file.ok()) {
            error = "required dependency is unavailable: " + name;
            return false;
        }

        auto report = LoadModule(resolved_path, file, memory);
        if (!report.mapped()) {
            error = "dependency " + name + " could not be mapped: " +
                    (report.error.empty() ? "unknown error" : report.error);
            reports.push_back(std::move(report));
            return false;
        }
        reports.push_back(std::move(report));
    }
    return true;
}

} // namespace

Ps4BootReport Ps4BootSession::Run(const BootFileReader& read_file) {
    Ps4BootReport report;
    report.stage = Ps4BootStage::FirmwareRoot;
    if (!read_file) {
        report.error = "PS4 boot has no firmware file source";
        return report;
    }

    report.stage = Ps4BootStage::SysCore;
    firmware::ReadOnlyVfs vfs(read_file);
    report.syscore = LoadModule(
        "system/sys/SceSysCore.elf", vfs.ReadFile("system/sys/SceSysCore.elf"),
        syscore_memory_);
    if (!report.syscore.mapped()) {
        report.error = report.syscore.error.empty()
                           ? "SceSysCore.elf was not mapped"
                           : report.syscore.error;
        return report;
    }

    if (!LoadDependencies(vfs, report.syscore, syscore_memory_,
                          report.syscore_dependencies, report.error)) {
        return report;
    }

    report.stage = Ps4BootStage::GuestExecution;
    hle::SyscallContext syscore_syscall_context{syscore_memory_,
                                                 video_output_};
    const auto syscore_guest = cpu::RunGuest(
        syscore_memory_, report.syscore.entry, {},
        [&](cpu::GuestRegisters& registers) {
            return syscalls_.Dispatch(syscore_syscall_context, registers);
        });
    report.guest_instructions = syscore_guest.instructions;
    report.guest_returned = syscore_guest.returned;
    if (!syscore_guest.ok()) {
        report.error = syscore_guest.error.empty()
                           ? "SceSysCore guest execution stopped"
                           : syscore_guest.error;
        return report;
    }

    report.stage = Ps4BootStage::ShellCore;
    report.shellcore = LoadModule(
        "system/vsh/SceShellCore.elf",
        vfs.ReadFile("system/vsh/SceShellCore.elf"), shellcore_memory_);
    if (!report.shellcore.mapped()) {
        report.error = report.shellcore.error.empty()
                           ? "SceShellCore.elf was not mapped"
                           : report.shellcore.error;
        return report;
    }

    if (!LoadDependencies(vfs, report.shellcore, shellcore_memory_,
                          report.shellcore_dependencies, report.error)) {
        return report;
    }

    report.stage = Ps4BootStage::GuestExecution;
    hle::SyscallContext shellcore_syscall_context{shellcore_memory_,
                                                   video_output_};
    const auto shellcore_guest = cpu::RunGuest(
        shellcore_memory_, report.shellcore.entry, {},
        [&](cpu::GuestRegisters& registers) {
            return syscalls_.Dispatch(shellcore_syscall_context, registers);
        });
    report.guest_instructions += shellcore_guest.instructions;
    report.shell_guest_returned = shellcore_guest.returned;
    report.guest_returned = report.guest_returned && shellcore_guest.returned;
    if (!shellcore_guest.ok()) {
        report.error = shellcore_guest.error.empty()
                           ? "SceShellCore guest execution stopped"
                           : shellcore_guest.error;
        return report;
    }
    if (video_output_.last_frame() != nullptr) {
        report.stage = Ps4BootStage::FramePresentation;
    }
    return report;
}

} // namespace vshift::boot
