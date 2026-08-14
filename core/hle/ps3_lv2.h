#pragma once

#include "core/cpu/ppu_runtime.h"
#include "core/loader/ps3_self.h"
#include "core/memory/guest_memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace vshift::hle {

// Public LV2 error values from the PS3 Cell ABI.
constexpr std::uint64_t kCellOk = 0;
constexpr std::uint64_t kCellEagain = 0x80010001u;
constexpr std::uint64_t kCellEinval = 0x80010002u;
constexpr std::uint64_t kCellEnosys = 0x80010003u;
constexpr std::uint64_t kCellEnomem = 0x80010004u;
constexpr std::uint64_t kCellEsrch = 0x80010005u;
constexpr std::uint64_t kCellEnoent = 0x80010006u;
constexpr std::uint64_t kCellEbusy = 0x8001000au;
constexpr std::uint64_t kCellEfault = 0x8001000du;

struct Ps3Lv2Trace final {
    std::uint64_t syscall = 0;
    std::uint64_t result = 0;
    std::array<std::uint64_t, 4> arguments{};
    std::size_t ordinal = 0;
    std::uint64_t pc = 0;
};

// First real LV2 boundary for the PS3 guest. This is intentionally a small,
// deterministic kernel model rather than a fake "boot successful" switch:
// known calls mutate guest memory/object state and unknown calls stop PPU.
class Ps3Lv2 final {
public:
    using FirmwareFiles =
        std::unordered_map<std::string, std::vector<std::uint8_t>>;

    explicit Ps3Lv2(memory::GuestMemory& memory,
                    const FirmwareFiles* firmware_files = nullptr) noexcept
        : memory_(memory), firmware_files_(firmware_files) {}

    void PrepareThread(cpu::PpuRegisters& registers) noexcept;
    bool Dispatch(cpu::PpuRegisters& registers, std::string& error);

    const std::vector<Ps3Lv2Trace>& trace() const noexcept { return trace_; }

    static const char* Name(std::uint64_t syscall) noexcept;

private:
    enum class ObjectType : std::uint8_t {
        Mutex,
        Condition,
        Semaphore,
        EventFlag,
        EventQueue,
        EventPort,
        RwLock,
        Thread,
        Memory,
        Rsx,
    };

    struct Object final {
        ObjectType type = ObjectType::Mutex;
        bool locked = false;
        std::int64_t value = 0;
        std::uint64_t entry = 0;
    };

    struct RegisteredPrx final {
        std::uint64_t name = 0;
        std::uint64_t type = 0;
        std::uint64_t library_entries = 0;
        std::uint32_t library_entries_size = 0;
        std::uint64_t library_stubs = 0;
        std::uint32_t library_stubs_size = 0;
    };

    struct LoadedPrx final {
        std::uint32_t id = 0;
        std::string path;
        std::uint64_t flags = 0;
        std::uint32_t base = 0;
        std::uint32_t start_entry = 0;
        std::uint32_t prologue_entry = 0;
        bool started = false;
    };

    bool ReadU32(std::uint64_t address, std::uint32_t& value,
                 std::string& error) const;
    bool ReadU64(std::uint64_t address, std::uint64_t& value,
                 std::string& error) const;
    bool ReadCString(std::uint64_t address, std::string& value,
                     std::string& error) const;
    bool WriteU32(std::uint64_t address, std::uint32_t value,
                  std::string& error);
    bool WriteU64(std::uint64_t address, std::uint64_t value,
                  std::string& error);
    std::uint32_t CreateObject(Object object);
    Object* FindObject(std::uint32_t id, ObjectType type = ObjectType::Mutex);
    const Object* FindObject(std::uint32_t id,
                             ObjectType type = ObjectType::Mutex) const;
    bool HandleObjectCreate(cpu::PpuRegisters& registers,
                            ObjectType type,
                            std::string& error);
    bool HandleObjectDestroy(cpu::PpuRegisters& registers,
                             ObjectType type,
                             std::string& error);
    bool HandleObjectLock(cpu::PpuRegisters& registers,
                          ObjectType type,
                          bool try_lock,
                          std::string& error);
    bool HandleObjectUnlock(cpu::PpuRegisters& registers,
                            ObjectType type,
                            std::string& error);
    bool LoadPrxImage(LoadedPrx& module, std::string& error);
    memory::GuestMemory& memory_;
    const FirmwareFiles* firmware_files_ = nullptr;
    std::unordered_map<std::uint32_t, Object> objects_;
    std::vector<RegisteredPrx> registered_prx_modules_;
    std::vector<LoadedPrx> loaded_prx_modules_;
    std::uint32_t next_object_id_ = 0x1000;
    std::uint32_t next_prx_id_ = 0x23000000;
    std::uint64_t next_memory_address_ = 0x0d000000;
    std::uint32_t next_prx_address_ = 0x01000000;
    std::vector<Ps3Lv2Trace> trace_;
};

} // namespace vshift::hle
