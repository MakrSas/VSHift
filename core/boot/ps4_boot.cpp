#include "core/boot/ps4_boot.h"

#include "core/cpu/x86_runtime.h"
#include "core/loader/elf_loader.h"
#include "core/loader/self.h"

#include <span>

namespace vshift::boot {

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
    return report;
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
    report.syscore = LoadModule(
        "system/sys/SceSysCore.elf", read_file("system/sys/SceSysCore.elf"),
        syscore_memory_);
    if (!report.syscore.mapped()) {
        report.error = report.syscore.error.empty()
                           ? "SceSysCore.elf was not mapped"
                           : report.syscore.error;
        return report;
    }

    report.stage = Ps4BootStage::GuestExecution;
    const auto guest = cpu::RunGuest(syscore_memory_, report.syscore.entry);
    report.guest_instructions = guest.instructions;
    report.guest_returned = guest.returned;
    if (!guest.ok()) {
        report.error = guest.error.empty()
                           ? "SceSysCore guest execution stopped"
                           : guest.error;
        return report;
    }

    report.stage = Ps4BootStage::ShellCore;
    report.shellcore = LoadModule(
        "system/vsh/SceShellCore.elf",
        read_file("system/vsh/SceShellCore.elf"), shellcore_memory_);
    if (!report.shellcore.mapped()) {
        report.error = report.shellcore.error.empty()
                           ? "SceShellCore.elf was not mapped"
                           : report.shellcore.error;
        return report;
    }

    report.stage = Ps4BootStage::GuestExecution;
    return report;
}

} // namespace vshift::boot
