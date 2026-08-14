#pragma once

#include "core/cpu/ppu_runtime.h"
#include "core/hle/ps3_lv2.h"
#include "core/loader/ps3_self.h"
#include "core/memory/guest_memory.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace vshift::boot {

struct Ps3RuntimeLoadInfo final {
    std::uint64_t package_version = 0;
    std::uint64_t image_version = 0;
    std::size_t decrypted_section_count = 0;
    std::size_t firmware_file_count = 0;
    std::uint64_t vsh_size = 0;
    std::uint64_t vsh_entry_point = 0;
    std::size_t mapped_segments = 0;
};

struct Ps3RuntimeLoadResult final {
    Ps3RuntimeLoadInfo info;
    std::string package_name;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Persistent PS3 guest boundary. It owns the guest address space and keeps
// the PPU/LV2 state alive across execution slices so a frontend can pause,
// resume, stop, and present frames without rebuilding the firmware each time.
class Ps3Runtime final {
public:
    Ps3Runtime();
    Ps3Runtime(const Ps3Runtime&) = delete;
    Ps3Runtime& operator=(const Ps3Runtime&) = delete;

    Ps3RuntimeLoadResult LoadFirmware(std::span<const std::uint8_t> pup_bytes);
    cpu::PpuRunResult Run(std::size_t max_instructions);

    void Pause(bool paused) noexcept { paused_ = paused; }
    void Stop() noexcept { stopped_ = true; }

    bool loaded() const noexcept { return loaded_; }
    bool paused() const noexcept { return paused_; }
    bool stopped() const noexcept { return stopped_; }

    const memory::GuestMemory& memory() const noexcept { return memory_; }
    memory::GuestMemory& memory() noexcept { return memory_; }
    const cpu::PpuRuntime& ppu() const noexcept { return ppu_; }
    cpu::PpuRuntime& ppu() noexcept { return ppu_; }
    const hle::Ps3Lv2* lv2() const noexcept { return lv2_.get(); }
    const Ps3RuntimeLoadInfo& load_info() const noexcept { return load_info_; }
    const std::unordered_map<std::string, std::vector<std::uint8_t>>&
    firmware_files() const noexcept { return firmware_files_; }

private:
    static bool WriteU64(memory::GuestMemory& memory,
                         std::uint64_t address,
                         std::uint64_t value);
    void ResetGuest();
    Ps3RuntimeLoadResult Fail(std::string error) const;

    memory::GuestMemory memory_;
    cpu::PpuRuntime ppu_;
    std::unique_ptr<hle::Ps3Lv2> lv2_;
    std::unordered_map<std::string, std::vector<std::uint8_t>> firmware_files_;
    Ps3RuntimeLoadInfo load_info_;
    bool loaded_ = false;
    bool paused_ = false;
    bool stopped_ = false;
};

} // namespace vshift::boot
