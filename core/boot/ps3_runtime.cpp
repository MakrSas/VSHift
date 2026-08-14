#include "core/boot/ps3_runtime.h"

#include "core/firmware/ps3_package.h"
#include "core/firmware/ps3_pup.h"
#include "core/firmware/ps3_tar.h"

#include <algorithm>
#include <array>
#include <limits>

namespace vshift::boot {

namespace {

bool RangeInside(std::uint64_t offset,
                 std::uint64_t length,
                 std::size_t size) noexcept {
    return offset <= size && length <= size - static_cast<std::size_t>(offset);
}

std::uint32_t ReadU32BE(std::span<const std::uint8_t> bytes) noexcept {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           bytes[3];
}

} // namespace

Ps3Runtime::Ps3Runtime()
    : ppu_(memory_) {}

void Ps3Runtime::ResetGuest() {
    memory_ = memory::GuestMemory{};
    ppu_.Reset();
    lv2_.reset();
    firmware_files_.clear();
    load_info_ = {};
    loaded_ = false;
    paused_ = false;
    stopped_ = false;
}

Ps3RuntimeLoadResult Ps3Runtime::Fail(std::string error) const {
    Ps3RuntimeLoadResult result;
    result.error = std::move(error);
    return result;
}

bool Ps3Runtime::WriteU64(memory::GuestMemory& memory,
                          std::uint64_t address,
                          std::uint64_t value) {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(
            value >> ((bytes.size() - index - 1) * 8));
    }
    return memory.Write(address, bytes).ok();
}

Ps3RuntimeLoadResult Ps3Runtime::LoadFirmware(
    std::span<const std::uint8_t> pup_bytes) {
    ResetGuest();
    if (pup_bytes.size() < firmware::kPs3PupHeaderSize) {
        return Fail("PS3 PUP is smaller than its fixed header");
    }
    const auto header_length = [&]() {
        std::uint64_t value = 0;
        for (std::size_t index = 0; index < 8; ++index) {
            value = (value << 8) | pup_bytes[0x20 + index];
        }
        return value;
    }();
    if (header_length > pup_bytes.size() ||
        header_length > 16ull * 1024ull * 1024ull) {
        return Fail("PS3 PUP header length is invalid");
    }
    const auto parsed = firmware::ParsePs3PupHeaders(
        pup_bytes.first(static_cast<std::size_t>(header_length)), pup_bytes.size());
    if (!parsed.ok()) return Fail(parsed.error);
    const auto update = std::find_if(
        parsed.entries.begin(), parsed.entries.end(), [](const auto& entry) {
            return entry.entry_id == 0x300;
        });
    if (update == parsed.entries.end() ||
        !RangeInside(update->data_offset, update->data_length, pup_bytes.size())) {
        return Fail("PS3 PUP update TAR is missing or outside the input");
    }
    const auto update_tar_bytes = pup_bytes.subspan(
        static_cast<std::size_t>(update->data_offset),
        static_cast<std::size_t>(update->data_length));
    const auto update_tar = firmware::ParsePs3Tar(update_tar_bytes);
    if (!update_tar.ok()) return Fail(update_tar.error);
    std::vector<const firmware::Ps3TarEntry*> package_entries;
    for (const auto& entry : update_tar.entries) {
        if (entry.regular_file && entry.name.rfind("dev_flash_", 0) == 0 &&
            RangeInside(entry.data_offset, entry.data_length,
                        update_tar_bytes.size())) {
            package_entries.push_back(&entry);
        }
    }
    if (package_entries.empty()) {
        return Fail("PS3 dev_flash packages are missing or outside the update TAR");
    }

    std::vector<std::uint8_t> vsh_bytes;
    std::size_t decrypted_section_count = 0;
    for (const auto* package_entry : package_entries) {
        const auto package_bytes = update_tar_bytes.subspan(
            static_cast<std::size_t>(package_entry->data_offset),
            static_cast<std::size_t>(package_entry->data_length));
        const auto package = firmware::DecryptPs3ScePackage(package_bytes);
        if (!package.ok()) return Fail(package.error);
        decrypted_section_count += package.sections.size();
        for (const auto& section : package.sections) {
            const auto section_tar = firmware::ParsePs3Tar(section.bytes);
            if (!section_tar.ok()) continue;
            for (const auto& entry : section_tar.entries) {
                if (!entry.regular_file ||
                    !RangeInside(entry.data_offset, entry.data_length,
                                 section.bytes.size())) {
                    continue;
                }
                const auto file_begin = static_cast<std::size_t>(entry.data_offset);
                const auto file_end = file_begin + static_cast<std::size_t>(entry.data_length);
                auto& file = firmware_files_[entry.name];
                file.assign(section.bytes.begin() + file_begin,
                            section.bytes.begin() + file_end);
                if (entry.name == "dev_flash/vsh/module/vsh.self") {
                    vsh_bytes = file;
                }
            }
        }
    }
    if (vsh_bytes.empty()) return Fail("decrypted dev_flash has no vsh.self");

    const auto self = loader::ParsePs3Self(vsh_bytes);
    if (!self.ok()) return Fail(self.error);
    const auto loaded = loader::LoadPs3SelfIntoMemory(self.image, memory_);
    if (!loaded.ok()) return Fail(loaded.error);
    const auto stack = memory_.Map({
        0x0c000000, 0x01000000,
        memory::kPermissionRead | memory::kPermissionWrite});
    if (!stack.ok()) return Fail(stack.error);

    std::array<std::uint8_t, 8> descriptor{};
    if (!memory_.Read(self.image.entry_point, descriptor).ok()) {
        return Fail("PS3 VSH entry descriptor is unreadable");
    }
    constexpr std::uint64_t argv_address = 0x0cffe000;
    constexpr std::uint64_t envp_address = 0x0cffe020;
    constexpr std::uint64_t process_name_address = 0x0cffe100;
    constexpr std::array<std::uint8_t, 12> process_name{
        'v', 's', 'h', '.', 's', 'e', 'l', 'f', 0, 0, 0, 0};
    if (!memory_.Write(process_name_address, process_name).ok() ||
        !WriteU64(memory_, argv_address, process_name_address) ||
        !WriteU64(memory_, argv_address + 8, 0) ||
        !WriteU64(memory_, envp_address, 0)) {
        return Fail("PS3 VSH process argument setup failed");
    }

    const auto code_address = ReadU32BE(std::span<const std::uint8_t>(descriptor).first(4));
    const auto toc_address = ReadU32BE(std::span<const std::uint8_t>(descriptor).subspan(4, 4));
    ppu_.registers().pc = code_address;
    ppu_.registers().gpr[1] = 0x0cfff000;
    ppu_.registers().gpr[2] = toc_address;
    ppu_.registers().gpr[12] = code_address;
    ppu_.registers().gpr[3] = 1;
    ppu_.registers().gpr[4] = argv_address;
    ppu_.registers().gpr[5] = envp_address;
    lv2_ = std::make_unique<hle::Ps3Lv2>(memory_, &firmware_files_);
    load_info_.package_version = parsed.header.package_version;
    load_info_.image_version = parsed.header.image_version;
    load_info_.decrypted_section_count = decrypted_section_count;
    load_info_.firmware_file_count = firmware_files_.size();
    load_info_.vsh_size = vsh_bytes.size();
    load_info_.vsh_entry_point = self.image.entry_point;
    load_info_.mapped_segments = loaded.loaded_segments;
    loaded_ = true;

    Ps3RuntimeLoadResult result;
    result.info = load_info_;
    result.package_name = package_entries.front()->name;
    return result;
}

cpu::PpuRunResult Ps3Runtime::Run(std::size_t max_instructions) {
    if (!loaded_ || stopped_) {
        cpu::PpuRunResult result;
        result.reason = cpu::PpuStopReason::Halted;
        result.error = loaded_ ? "PS3 runtime is stopped" : "PS3 runtime is not loaded";
        result.registers = ppu_.registers();
        return result;
    }
    if (paused_) {
        cpu::PpuRunResult result;
        result.reason = cpu::PpuStopReason::StepLimit;
        result.error = "PS3 runtime is paused";
        result.registers = ppu_.registers();
        return result;
    }
    return ppu_.Run(
        max_instructions,
        [this](auto& registers, auto& error) {
            return lv2_ != nullptr && lv2_->Dispatch(registers, error);
        },
        [this](auto& registers) {
            if (lv2_ != nullptr) lv2_->PrepareThread(registers);
        });
}

} // namespace vshift::boot
