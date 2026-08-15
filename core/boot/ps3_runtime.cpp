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

bool LooksLikeGuestString(const memory::GuestMemory& memory,
                          std::uint32_t address) {
    if (address == 0) return false;
    std::array<std::uint8_t, 64> bytes{};
    if (!memory.Read(address, bytes).ok()) return false;
    for (const auto byte : bytes) {
        if (byte == 0) return true;
        if (byte < 0x20 || byte > 0x7e) return false;
    }
    return false;
}

std::vector<std::uint32_t> DiscoverVshImportRecords(
    const loader::Ps3SelfImage& image,
    const memory::GuestMemory& memory) {
    std::vector<std::uint32_t> records;
    for (std::size_t program_index = 0;
         program_index < image.program_headers.size(); ++program_index) {
        const auto& program = image.program_headers[program_index];
        if (program.type != 1 || program.file_size < 0x2c) continue;
        const auto section = std::find_if(
            image.sections.begin(), image.sections.end(),
            [program_index](const auto& candidate) {
                return candidate.type == 2 &&
                       candidate.program_index == program_index;
            });
        if (section == image.sections.end() || section->bytes.size() < 0x2c) {
            continue;
        }
        const auto limit = std::min<std::uint64_t>(
            program.file_size, section->bytes.size());
        for (std::uint64_t offset = 0; offset + 0x2c <= limit; offset += 4) {
            const auto record_bytes = std::span<const std::uint8_t>(section->bytes)
                .subspan(static_cast<std::size_t>(offset), 4);
            if (ReadU32BE(record_bytes) != 0x2c000001u) continue;
            const auto guest_address = program.virtual_address + offset;
            if (guest_address > std::numeric_limits<std::uint32_t>::max()) continue;
            std::array<std::uint8_t, 0x24> record{};
            if (!memory.Read(guest_address, record).ok()) continue;
            const auto attributes_and_functions = ReadU32BE(
                std::span<const std::uint8_t>(record).subspan(0x04, 4));
            const auto variables_and_tls = ReadU32BE(
                std::span<const std::uint8_t>(record).subspan(0x08, 4));
            const auto name = ReadU32BE(std::span<const std::uint8_t>(record).subspan(0x10, 4));
            const auto nids = ReadU32BE(std::span<const std::uint8_t>(record).subspan(0x14, 4));
            const auto addrs = ReadU32BE(std::span<const std::uint8_t>(record).subspan(0x18, 4));
            const auto variable_nids = ReadU32BE(
                std::span<const std::uint8_t>(record).subspan(0x1c, 4));
            const auto variable_stubs = ReadU32BE(
                std::span<const std::uint8_t>(record).subspan(0x20, 4));
            const auto function_count = static_cast<std::uint16_t>(
                attributes_and_functions);
            const auto variable_count = static_cast<std::uint16_t>(
                variables_and_tls >> 16);
            const auto valid_functions = function_count != 0 &&
                                         nids != 0 && addrs != 0;
            const auto valid_variables = variable_count != 0 &&
                                         variable_nids != 0 &&
                                         variable_stubs != 0;
            if (!LooksLikeGuestString(memory, name) ||
                (!valid_functions && !valid_variables)) {
                continue;
            }
            records.push_back(static_cast<std::uint32_t>(guest_address));
        }
    }
    return records;
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
    lv2_ = std::make_unique<hle::Ps3Lv2>(
        memory_, &firmware_files_, static_cast<std::uint32_t>(toc_address));
    const auto vsh_import_records = DiscoverVshImportRecords(self.image, memory_);
    std::string linker_error;
    if (!lv2_->RegisterMainVshImports(vsh_import_records, linker_error)) {
        return Fail("PS3 VSH import table setup failed: " + linker_error);
    }
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
