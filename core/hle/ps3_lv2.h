#pragma once

#include "core/cpu/ppu_runtime.h"
#include "core/loader/ps3_self.h"
#include "core/memory/guest_memory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
constexpr std::uint64_t kCellEperm = 0x80010009u;
constexpr std::uint64_t kCellEbusy = 0x8001000au;
constexpr std::uint64_t kCellEfault = 0x8001000du;
constexpr std::uint64_t kCellEexist = 0x80010014u;
constexpr std::uint64_t kCellEnotdir = 0x8001002eu;
constexpr std::uint64_t kCellEnotsup = 0x80010037u;

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
                    const FirmwareFiles* firmware_files = nullptr,
                    std::uint32_t main_toc = 0) noexcept
        : memory_(memory), firmware_files_(firmware_files), main_toc_(main_toc) {}

    void PrepareThread(cpu::PpuRegisters& registers) noexcept;
    bool Dispatch(cpu::PpuRegisters& registers, std::string& error);
    bool RegisterMainVshImports(const std::vector<std::uint32_t>& records,
                                std::string& error);

    const std::vector<Ps3Lv2Trace>& trace() const noexcept { return trace_; }
    std::size_t imported_function_count() const noexcept {
        return imported_functions_.size();
    }
    bool LookupImport(std::uint32_t slot, std::string& module,
                      std::uint32_t& nid) const;
    bool LookupNearestImport(std::uint32_t address, std::uint32_t& location,
                             bool& variable, std::string& module,
                             std::uint32_t& nid) const;
    std::size_t imported_variable_count() const noexcept {
        return imported_variables_.size();
    }
    std::size_t exported_function_count() const noexcept {
        return exported_functions_.size();
    }
    const std::string& last_prx_load_error() const noexcept {
        return last_prx_load_error_;
    }

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
        std::uint64_t argument = 0;
        std::uint32_t toc = 0;
        std::uint32_t tls = 0;
        std::uint64_t stack_size = 0;
        std::uint64_t stack_address = 0;
        std::uint64_t tls_address = 0;
        std::uint64_t argument2 = 0;
        cpu::PpuRegisters context{};
        bool context_initialized = false;
        bool runnable = false;
        bool started = false;
        std::uint32_t event_queue_id = 0;
        std::deque<std::array<std::uint64_t, 4>> events;
        std::deque<std::array<std::uint64_t, 2>> event_waiters;
        std::deque<std::uint32_t> waiters;
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
        std::uint32_t stop_entry = 0;
        std::uint32_t prologue_entry = 0;
        std::uint32_t epilogue_entry = 0;
        bool started = false;
    };

    struct PrxLibraryRecord final {
        std::uint8_t size = 0;
        std::uint16_t attributes = 0;
        std::uint16_t function_count = 0;
        std::uint16_t variable_count = 0;
        std::uint16_t tls_variable_count = 0;
        std::uint32_t name = 0;
        std::uint32_t nids = 0;
        std::uint32_t addresses = 0;
        std::uint32_t variable_nids = 0;
        std::uint32_t variable_stubs = 0;
    };

    struct ImportedFunction final {
        std::string module;
        std::uint32_t nid = 0;
        std::uint32_t slot = 0;
    };

    struct ExportedFunction final {
        std::string module;
        std::uint32_t nid = 0;
        std::uint32_t address = 0;
        std::uint32_t toc = 0;
    };

    struct ImportedVariable final {
        std::string module;
        std::uint32_t nid = 0;
        std::uint32_t references = 0;
    };

    struct ExportedVariable final {
        std::string module;
        std::uint32_t nid = 0;
        std::uint32_t address = 0;
    };

    struct Directory final {
        std::vector<std::string> entries;
        std::size_t position = 0;
    };

    struct File final {
        const std::vector<std::uint8_t>* bytes = nullptr;
        std::size_t position = 0;
    };

    struct SharedMemory final {
        std::uint64_t size = 0;
        std::uint64_t flags = 0;
        std::uint32_t mapped_address = 0;
        bool mapped = false;
    };

    struct UsbEventWaiter final {
        std::uint32_t thread_id = 0;
        std::array<std::uint64_t, 3> outputs{};
    };

    struct ConfigHandle final {
        std::uint32_t queue_id = 0;
    };

    struct ConfigListener final {
        std::uint32_t config_id = 0;
        std::uint64_t service_id = 0;
        std::uint64_t min_verbosity = 0;
        std::uint32_t type = 0;
        std::vector<std::uint8_t> data;
    };

    struct ConfigService final {
        std::uint32_t config_id = 0;
        std::uint64_t service_id = 0;
        std::uint64_t user_id = 0;
        std::uint64_t verbosity = 0;
        std::vector<std::uint8_t> data;
        bool registered = true;
    };

    struct ConfigEvent final {
        std::uint32_t config_id = 0;
        std::uint32_t listener_id = 0;
        ConfigService service;
    };

    // A storage medium can notify exactly one guest event queue. Retaining
    // this association is necessary for later medium-insert/eject events;
    // it deliberately is not treated as a successful no-op.
    struct StorageMediumEvent final {
        std::uint32_t queue_id = 0;
        std::uint32_t source = 0;
    };

    struct StorageHandle final {
        std::uint64_t device = 0;
        std::uint64_t mode = 0;
        std::uint64_t flags = 0;
    };

    bool ReadU32(std::uint64_t address, std::uint32_t& value,
                 std::string& error) const;
    bool ReadU16(std::uint64_t address, std::uint16_t& value,
                 std::string& error) const;
    bool ReadU64(std::uint64_t address, std::uint64_t& value,
                 std::string& error) const;
    bool ReadCString(std::uint64_t address, std::string& value,
                     std::string& error) const;
    bool WriteU32(std::uint64_t address, std::uint32_t value,
                  std::string& error);
    bool WriteU16(std::uint64_t address, std::uint16_t value,
                  std::string& error);
    bool WriteU64(std::uint64_t address, std::uint64_t value,
                  std::string& error);
    bool ReadLibraryRecord(std::uint64_t address,
                           PrxLibraryRecord& record,
                           std::string& error) const;
    bool AddImportRecord(
        std::uint64_t address,
        const std::function<std::uint32_t(std::uint32_t)>& normalize,
        std::string& error);
    bool AddExportRecord(
        std::uint64_t address,
        const std::function<std::uint32_t(std::uint32_t)>& normalize,
        std::string& error,
        std::uint32_t toc = 0);
    void LinkImports(std::string& error);
    bool PatchVariableReferences(std::uint32_t references,
                                 std::uint32_t address,
                                 std::string& error);
    void AddExport(std::string module, std::uint32_t nid,
                   std::uint32_t address, std::uint32_t toc);
    bool IsExecutableAddress(std::uint32_t address) const noexcept;
    std::uint32_t EnsureFunctionDescriptor(std::uint32_t address,
                                            std::uint32_t toc,
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
    bool InitializeThreadContext(Object& thread, std::string& error);
    bool SwitchThread(cpu::PpuRegisters& registers,
                      bool requeue_current,
                      std::string& error);
    void MakeThreadRunnable(std::uint32_t thread_id);
    bool SendQueueEvent(std::uint32_t queue_id,
                        const std::array<std::uint64_t, 4>& event,
                        std::string& error);
    bool EnsureThreadReturnTrampoline(std::string& error);
    bool LoadPrxImage(LoadedPrx& module, std::string& error);
    memory::GuestMemory& memory_;
    const FirmwareFiles* firmware_files_ = nullptr;
    std::unordered_map<std::uint32_t, Object> objects_;
    std::vector<RegisteredPrx> registered_prx_modules_;
    std::vector<LoadedPrx> loaded_prx_modules_;
    std::vector<ImportedFunction> imported_functions_;
    std::vector<ExportedFunction> exported_functions_;
    std::vector<ImportedVariable> imported_variables_;
    std::vector<ExportedVariable> exported_variables_;
    std::unordered_map<std::uint32_t, Directory> directories_;
    std::unordered_map<std::uint32_t, File> files_;
    std::unordered_set<std::string> mutable_directories_{
        "dev_flash2", "dev_flash2/etc", "dev_hdd0", "dev_hdd1"};
    std::unordered_map<std::string, std::vector<std::uint8_t>> mutable_files_;
    std::unordered_map<std::uint32_t, SharedMemory> shared_memories_;
    std::unordered_map<std::uint32_t, std::uint64_t> mmapper_reservations_;
    std::string last_prx_load_error_;
    std::uint32_t main_toc_ = 0;
    std::uint32_t next_object_id_ = 0x1000;
    std::uint32_t next_directory_id_ = 0x2000;
    std::uint32_t next_file_id_ = 0x3000;
    std::uint32_t next_config_id_ = 0x4000;
    std::uint32_t next_config_listener_id_ = 0x42000000;
    std::uint32_t next_config_service_id_ = 0x43000000;
    std::uint32_t next_config_event_id_ = 1;
    std::uint32_t next_shared_memory_id_ = 0x5000;
    std::uint32_t next_spu_group_id_ = 0x6000;
    std::uint32_t next_spu_thread_id_ = 0x7000;
    std::uint32_t next_rsx_audio_id_ = 0x8000;
    std::uint32_t next_storage_handle_ = 0x9000;
    std::uint64_t next_rsx_audio_address_ = 0x20000000;
    std::unordered_map<std::uint32_t, ConfigHandle> config_handles_;
    std::unordered_map<std::uint32_t, ConfigListener> config_listeners_;
    std::unordered_map<std::uint32_t, ConfigService> config_services_;
    std::unordered_map<std::uint32_t, ConfigEvent> config_events_;
    std::unordered_map<std::uint32_t, StorageMediumEvent> storage_medium_events_;
    std::unordered_map<std::uint32_t, StorageHandle> storage_handles_;
    std::unordered_set<std::uint32_t> spu_group_handles_;
    std::unordered_set<std::uint32_t> spu_thread_handles_;
    std::unordered_set<std::uint32_t> rsx_audio_handles_;
    std::unordered_set<std::uint32_t> rsx_audio_connections_;
    std::unordered_map<std::uint32_t, std::uint64_t> rsx_audio_imported_memory_;
    std::unordered_map<std::uint32_t, std::uint32_t> event_queue_poll_counts_;
    std::deque<std::array<std::uint64_t, 3>> usb_events_;
    std::deque<UsbEventWaiter> usb_event_waiters_;
    std::deque<std::uint32_t> runnable_threads_;
    cpu::PpuRegisters bootstrap_context_{};
    bool bootstrap_context_valid_ = false;
    bool bootstrap_runnable_ = false;
    std::uint32_t current_thread_id_ = 0x01000000;
    bool usbd_initialized_ = false;
    bool uart_initialized_ = false;
    std::uint32_t next_descriptor_address_ = 0x0b000000;
    bool descriptor_mapping_created_ = false;
    std::uint32_t next_prx_id_ = 0x23000000;
    std::uint64_t next_memory_address_ = 0x0d000000;
    std::uint64_t next_mmapper_address_ = 0x10000000;
    std::uint64_t next_thread_stack_address_ = 0x30000000;
    std::uint64_t next_thread_tls_address_ = 0x38000000;
    std::uint64_t thread_return_trampoline_ = 0x0b100000;
    bool thread_return_trampoline_mapped_ = false;
    std::uint32_t next_prx_address_ = 0x01000000;
    std::vector<Ps3Lv2Trace> trace_;
};

} // namespace vshift::hle
