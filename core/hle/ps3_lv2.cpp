#include "core/hle/ps3_lv2.h"

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <string_view>

namespace vshift::hle {

namespace {

constexpr std::uint64_t kSysProcessGetpid = 1;
constexpr std::uint64_t kSysProcessExit = 2;
constexpr std::uint64_t kSysProcessGetParamsfo = 30;
constexpr std::uint64_t kSysProcessExit2 = 22;
constexpr std::uint64_t kSysPpuThreadExit = 41;
constexpr std::uint64_t kSysPpuThreadGetId = 42;
constexpr std::uint64_t kSysPpuThreadYield = 43;
constexpr std::uint64_t kSysPpuThreadJoin = 44;
constexpr std::uint64_t kSysPpuThreadDetach = 45;
constexpr std::uint64_t kSysPpuThreadGetJoinState = 46;
constexpr std::uint64_t kSysPpuThreadGetPriority = 48;
constexpr std::uint64_t kSysPpuThreadGetStackInformation = 49;
constexpr std::uint64_t kSysPpuThreadCreate = 52;
constexpr std::uint64_t kSysPpuThreadStart = 53;
constexpr std::uint64_t kSysPpuThreadOnce = 54;
constexpr std::uint64_t kSysEventFlagCreate = 82;
constexpr std::uint64_t kSysEventFlagDestroy = 83;
constexpr std::uint64_t kSysEventFlagTrywait = 86;
constexpr std::uint64_t kSysEventFlagSet = 87;
constexpr std::uint64_t kSysEventFlagClear = 118;
constexpr std::uint64_t kSysSemaphoreCreate = 90;
constexpr std::uint64_t kSysSemaphoreDestroy = 91;
constexpr std::uint64_t kSysSemaphoreWait = 92;
constexpr std::uint64_t kSysSemaphoreTrywait = 93;
constexpr std::uint64_t kSysSemaphorePost = 94;
constexpr std::uint64_t kSysSemaphoreGetValue = 114;
constexpr std::uint64_t kSysLwMutexCreate = 95;
constexpr std::uint64_t kSysLwMutexDestroy = 96;
constexpr std::uint64_t kSysLwMutexLock = 97;
constexpr std::uint64_t kSysLwMutexUnlock = 98;
constexpr std::uint64_t kSysLwMutexTrylock = 99;
constexpr std::uint64_t kSysMutexCreate = 100;
constexpr std::uint64_t kSysMutexDestroy = 101;
constexpr std::uint64_t kSysMutexLock = 102;
constexpr std::uint64_t kSysMutexTrylock = 103;
constexpr std::uint64_t kSysMutexUnlock = 104;
constexpr std::uint64_t kSysCondCreate = 105;
constexpr std::uint64_t kSysCondDestroy = 106;
constexpr std::uint64_t kSysCondWait = 107;
constexpr std::uint64_t kSysCondSignal = 108;
constexpr std::uint64_t kSysCondSignalAll = 109;
constexpr std::uint64_t kSysCondSignalTo = 110;
constexpr std::uint64_t kSysRwlockCreate = 120;
constexpr std::uint64_t kSysRwlockDestroy = 121;
constexpr std::uint64_t kSysRwlockRlock = 122;
constexpr std::uint64_t kSysRwlockTryrlock = 123;
constexpr std::uint64_t kSysRwlockRunlock = 124;
constexpr std::uint64_t kSysRwlockWlock = 125;
constexpr std::uint64_t kSysRwlockTrywlock = 126;
constexpr std::uint64_t kSysRwlockWunlock = 127;
constexpr std::uint64_t kSysEventQueueCreate = 128;
constexpr std::uint64_t kSysEventQueueDestroy = 129;
constexpr std::uint64_t kSysEventPortCreate = 134;
constexpr std::uint64_t kSysEventPortDestroy = 135;
constexpr std::uint64_t kSysEventPortConnectLocal = 136;
constexpr std::uint64_t kSysEventPortDisconnect = 137;
constexpr std::uint64_t kSysEventPortSend = 138;
constexpr std::uint64_t kSysEventFlagGet = 139;
constexpr std::uint64_t kSysEventPortConnectIpc = 140;
constexpr std::uint64_t kSysTimeGetTimebaseFrequency = 147;
constexpr std::uint64_t kSysTtyRead = 402;
constexpr std::uint64_t kSysTtyWrite = 403;
constexpr std::uint64_t kSysMemoryContainerCreateLegacy = 324;
constexpr std::uint64_t kSysMemoryContainerCreate = 341;
constexpr std::uint64_t kSysMemoryContainerDestroy = 342;
constexpr std::uint64_t kSysMemoryContainerGetSize = 343;
constexpr std::uint64_t kSysMemoryAllocate = 348;
constexpr std::uint64_t kSysMemoryFree = 349;
constexpr std::uint64_t kSysMemoryAllocateFromContainer = 350;
constexpr std::uint64_t kSysMemoryGetUserMemorySize = 352;
constexpr std::uint64_t kSysSmGetParams = 380;
constexpr std::uint64_t kSysPrxStartModule = 481;
constexpr std::uint64_t kSysPrxRegisterModule = 484;
constexpr std::uint64_t kSysPrxRegisterLibrary = 486;
constexpr std::uint64_t kSysPrxLoadModuleOnMemcontainer = 497;
constexpr std::uint64_t kSysDbgPpuExceptionHandler = 988;
constexpr std::uint64_t kSysRsxDeviceOpen = 666;
constexpr std::uint64_t kSysRsxDeviceClose = 667;
constexpr std::uint64_t kSysRsxMemoryAllocate = 668;
constexpr std::uint64_t kSysRsxMemoryFree = 669;
constexpr std::uint64_t kSysRsxContextAllocate = 670;
constexpr std::uint64_t kSysRsxContextFree = 671;

constexpr std::uint64_t kDefaultUserMemoryBase = 0x0d000000;
constexpr std::uint64_t kDefaultUserMemorySize = 0x10000000;
constexpr std::uint32_t kBootstrapPpuThreadId = 0x01000000;

std::string UnknownSyscall(std::uint64_t syscall) {
    std::ostringstream stream;
    stream << "unimplemented PS3 LV2 syscall 0x" << std::hex << syscall;
    return stream.str();
}

} // namespace

const char* Ps3Lv2::Name(std::uint64_t syscall) noexcept {
    switch (syscall) {
    case kSysProcessGetpid: return "sys_process_getpid";
    case kSysProcessExit: return "sys_process_exit";
    case kSysProcessGetParamsfo: return "_sys_process_get_paramsfo";
    case kSysProcessExit2: return "sys_process_exit2";
    case kSysPpuThreadExit: return "sys_ppu_thread_exit";
    case kSysPpuThreadGetId: return "sys_ppu_thread_get_id";
    case kSysPpuThreadYield: return "sys_ppu_thread_yield";
    case kSysPpuThreadJoin: return "sys_ppu_thread_join";
    case kSysPpuThreadDetach: return "sys_ppu_thread_detach";
    case kSysPpuThreadGetJoinState: return "sys_ppu_thread_get_join_state";
    case kSysPpuThreadGetPriority: return "sys_ppu_thread_get_priority";
    case kSysPpuThreadGetStackInformation: return "sys_ppu_thread_get_stack_information";
    case kSysPpuThreadCreate: return "sys_ppu_thread_create";
    case kSysPpuThreadStart: return "sys_ppu_thread_start";
    case kSysPpuThreadOnce: return "sys_ppu_thread_once";
    case kSysEventFlagCreate: return "sys_event_flag_create";
    case kSysEventFlagDestroy: return "sys_event_flag_destroy";
    case kSysEventFlagTrywait: return "sys_event_flag_trywait";
    case kSysEventFlagSet: return "sys_event_flag_set";
    case kSysEventFlagClear: return "sys_event_flag_clear";
    case kSysSemaphoreCreate: return "sys_semaphore_create";
    case kSysSemaphoreDestroy: return "sys_semaphore_destroy";
    case kSysSemaphoreWait: return "sys_semaphore_wait";
    case kSysSemaphoreTrywait: return "sys_semaphore_trywait";
    case kSysSemaphorePost: return "sys_semaphore_post";
    case kSysSemaphoreGetValue: return "sys_semaphore_get_value";
    case kSysLwMutexCreate: return "_sys_lwmutex_create";
    case kSysLwMutexDestroy: return "_sys_lwmutex_destroy";
    case kSysLwMutexLock: return "_sys_lwmutex_lock";
    case kSysLwMutexUnlock: return "_sys_lwmutex_unlock";
    case kSysLwMutexTrylock: return "_sys_lwmutex_trylock";
    case kSysMutexCreate: return "sys_mutex_create";
    case kSysMutexDestroy: return "sys_mutex_destroy";
    case kSysMutexLock: return "sys_mutex_lock";
    case kSysMutexTrylock: return "sys_mutex_trylock";
    case kSysMutexUnlock: return "sys_mutex_unlock";
    case kSysCondCreate: return "sys_cond_create";
    case kSysCondDestroy: return "sys_cond_destroy";
    case kSysCondWait: return "sys_cond_wait";
    case kSysCondSignal: return "sys_cond_signal";
    case kSysCondSignalAll: return "sys_cond_signal_all";
    case kSysCondSignalTo: return "sys_cond_signal_to";
    case kSysRwlockCreate: return "sys_rwlock_create";
    case kSysRwlockDestroy: return "sys_rwlock_destroy";
    case kSysRwlockRlock: return "sys_rwlock_rlock";
    case kSysRwlockTryrlock: return "sys_rwlock_tryrlock";
    case kSysRwlockRunlock: return "sys_rwlock_runlock";
    case kSysRwlockWlock: return "sys_rwlock_wlock";
    case kSysRwlockTrywlock: return "sys_rwlock_trywlock";
    case kSysRwlockWunlock: return "sys_rwlock_wunlock";
    case kSysEventQueueCreate: return "sys_event_queue_create";
    case kSysEventQueueDestroy: return "sys_event_queue_destroy";
    case kSysEventPortCreate: return "sys_event_port_create";
    case kSysEventPortDestroy: return "sys_event_port_destroy";
    case kSysEventPortConnectLocal: return "sys_event_port_connect_local";
    case kSysEventPortDisconnect: return "sys_event_port_disconnect";
    case kSysEventPortSend: return "sys_event_port_send";
    case kSysEventFlagGet: return "sys_event_flag_get";
    case kSysEventPortConnectIpc: return "sys_event_port_connect_ipc";
    case kSysTimeGetTimebaseFrequency: return "sys_time_get_timebase_frequency";
    case kSysTtyRead: return "sys_tty_read";
    case kSysTtyWrite: return "sys_tty_write";
    case kSysMemoryContainerCreateLegacy: return "sys_memory_container_create";
    case kSysMemoryAllocate: return "sys_memory_allocate";
    case kSysMemoryFree: return "sys_memory_free";
    case kSysMemoryAllocateFromContainer: return "sys_memory_allocate_from_container";
    case kSysMemoryContainerCreate: return "sys_memory_container_create";
    case kSysMemoryContainerDestroy: return "sys_memory_container_destroy";
    case kSysMemoryContainerGetSize: return "sys_memory_container_get_size";
    case kSysMemoryGetUserMemorySize: return "sys_memory_get_user_memory_size";
    case kSysSmGetParams: return "sys_sm_get_params";
    case kSysPrxStartModule: return "sys_prx_start_module";
    case kSysPrxRegisterModule: return "sys_prx_register_module";
    case kSysPrxRegisterLibrary: return "sys_prx_register_library";
    case kSysPrxLoadModuleOnMemcontainer: return "sys_prx_load_module_on_memcontainer";
    case kSysDbgPpuExceptionHandler: return "sys_dbg_ppu_exception_handler";
    case kSysRsxDeviceOpen: return "sys_rsx_device_open";
    case kSysRsxDeviceClose: return "sys_rsx_device_close";
    case kSysRsxMemoryAllocate: return "sys_rsx_memory_allocate";
    case kSysRsxMemoryFree: return "sys_rsx_memory_free";
    case kSysRsxContextAllocate: return "sys_rsx_context_allocate";
    case kSysRsxContextFree: return "sys_rsx_context_free";
    default: return "unknown";
    }
}

bool Ps3Lv2::ReadU32(std::uint64_t address, std::uint32_t& value,
                     std::string& error) const {
    std::array<std::uint8_t, 4> bytes{};
    const auto result = memory_.Read(address, bytes);
    if (!result.ok()) {
        error = result.error;
        return false;
    }
    value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
            (static_cast<std::uint32_t>(bytes[1]) << 16) |
            (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
    return true;
}

bool Ps3Lv2::ReadU64(std::uint64_t address, std::uint64_t& value,
                     std::string& error) const {
    std::array<std::uint8_t, 8> bytes{};
    const auto result = memory_.Read(address, bytes);
    if (!result.ok()) {
        error = result.error;
        return false;
    }
    value = 0;
    for (const auto byte : bytes) value = (value << 8) | byte;
    return true;
}

bool Ps3Lv2::ReadCString(std::uint64_t address, std::string& value,
                         std::string& error) const {
    value.clear();
    if (address == 0) {
        error = "guest string pointer is null";
        return false;
    }
    for (std::size_t index = 0; index < 0x400; ++index) {
        std::array<std::uint8_t, 1> byte{};
        const auto result = memory_.Read(address + index, byte);
        if (!result.ok()) {
            error = result.error;
            return false;
        }
        if (byte[0] == 0) return true;
        value.push_back(static_cast<char>(byte[0]));
    }
    error = "guest string exceeds the maximum supported length";
    return false;
}

bool Ps3Lv2::WriteU32(std::uint64_t address, std::uint32_t value,
                      std::string& error) {
    const std::array<std::uint8_t, 4> bytes{
        static_cast<std::uint8_t>(value >> 24),
        static_cast<std::uint8_t>(value >> 16),
        static_cast<std::uint8_t>(value >> 8),
        static_cast<std::uint8_t>(value)};
    const auto result = memory_.Write(address, bytes);
    if (!result.ok()) error = result.error;
    return result.ok();
}

bool Ps3Lv2::WriteU64(std::uint64_t address, std::uint64_t value,
                      std::string& error) {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::uint8_t>(
            value >> ((bytes.size() - index - 1) * 8));
    }
    const auto result = memory_.Write(address, bytes);
    if (!result.ok()) error = result.error;
    return result.ok();
}

bool Ps3Lv2::LoadPrxImage(LoadedPrx& module, std::string& error) {
    if (firmware_files_ == nullptr) {
        // Unit callers can still exercise the syscall ABI without installing
        // a firmware image.  The runtime path always supplies the extracted
        // dev_flash file map and takes the real loader branch below.
        return true;
    }

    std::string firmware_path = module.path;
    while (!firmware_path.empty() && firmware_path.front() == '/') {
        firmware_path.erase(firmware_path.begin());
    }
    const auto file = firmware_files_->find(firmware_path);
    if (file == firmware_files_->end()) {
        error = "PS3 PRX is not present in the installed firmware: " +
                module.path;
        return false;
    }

    const auto parsed = loader::ParsePs3Self(file->second);
    if (!parsed.ok()) {
        error = "PS3 PRX SELF parse failed for " + module.path + ": " +
                parsed.error;
        return false;
    }

    constexpr std::uint32_t kPtLoad = 1;
    const auto base = static_cast<std::uint64_t>(next_prx_address_);
    std::uint64_t image_end = base;
    for (std::size_t index = 0; index < parsed.image.program_headers.size();
         ++index) {
        const auto& program = parsed.image.program_headers[index];
        if (program.type != kPtLoad || program.memory_size == 0) continue;
        const auto guest_address = base + program.virtual_address;
        if (guest_address > std::numeric_limits<std::uint32_t>::max() ||
            program.memory_size >
                std::numeric_limits<std::uint32_t>::max() - guest_address) {
            error = "PS3 PRX address space exceeds the 32-bit guest range";
            return false;
        }
        std::uint32_t permissions = memory::kPermissionRead;
        if ((program.flags & 0x1u) != 0) {
            permissions |= memory::kPermissionExecute;
        }
        if ((program.flags & 0x2u) != 0) {
            permissions |= memory::kPermissionWrite;
        }
        const auto mapped = memory_.Map({guest_address, program.memory_size,
                                         permissions});
        if (!mapped.ok()) {
            error = mapped.error;
            return false;
        }
        const loader::Ps3SelfSection* section = nullptr;
        for (const auto& candidate : parsed.image.sections) {
            if (candidate.type == 2 && candidate.program_index == index) {
                section = &candidate;
                break;
            }
        }
        if (program.file_size != 0 &&
            (section == nullptr || section->bytes.size() < program.file_size)) {
            error = "PS3 PRX PT_LOAD data is incomplete after SELF decoding";
            return false;
        }
        if (program.file_size != 0) {
            const auto initialized = memory_.Initialize(
                guest_address,
                std::span<const std::uint8_t>(section->bytes).first(
                    static_cast<std::size_t>(program.file_size)));
            if (!initialized.ok()) {
                error = initialized.error;
                return false;
            }
        }
        image_end = std::max(image_end, guest_address + program.memory_size);
    }
    if (image_end == base) {
        error = "PS3 PRX has no loadable program segment";
        return false;
    }

    const auto image_size = image_end - base;
    const auto normalize_address = [&](std::uint32_t address) {
        if (address == 0 ||
            (address >= base && address < image_end)) {
            return address;
        }
        if (static_cast<std::uint64_t>(address) < image_size) {
            return static_cast<std::uint32_t>(base + address);
        }
        return address;
    };

    const auto read_u16 = [&](std::uint64_t address, std::uint16_t& value) {
        std::array<std::uint8_t, 2> bytes{};
        const auto result = memory_.Read(address, bytes);
        if (!result.ok()) {
            error = result.error;
            return false;
        }
        value = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
        return true;
    };
    const auto read_module_info = [&](std::uint64_t address,
                                       std::array<std::uint8_t, 0x2c>& bytes) {
        const auto result = memory_.Read(address, bytes);
        if (!result.ok()) {
            error = result.error;
            return false;
        }
        return true;
    };
    const auto read_info_u32 = [](const std::array<std::uint8_t, 0x2c>& bytes,
                                  std::size_t offset) {
        return (static_cast<std::uint32_t>(bytes[offset]) << 24) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
               bytes[offset + 3];
    };

    std::uint64_t library_info = 0;
    for (const auto& program : parsed.image.program_headers) {
        if (program.type != kPtLoad || program.physical_address == 0 ||
            program.physical_address < program.offset) {
            continue;
        }
        library_info = base + program.virtual_address +
                       program.physical_address - program.offset;
        break;
    }
    if (library_info != 0) {
        std::array<std::uint8_t, 0x40> info{};
        const auto result = memory_.Read(library_info, info);
        if (!result.ok()) {
            error = result.error;
            return false;
        }
        const auto read_info_u32_40 = [&](std::size_t offset) {
            return (static_cast<std::uint32_t>(info[offset]) << 24) |
                   (static_cast<std::uint32_t>(info[offset + 1]) << 16) |
                   (static_cast<std::uint32_t>(info[offset + 2]) << 8) |
                   info[offset + 3];
        };
        auto exports_start = normalize_address(read_info_u32_40(0x24));
        const auto exports_end = normalize_address(read_info_u32_40(0x28));
        if (exports_start != 0 && exports_end > exports_start &&
            static_cast<std::uint64_t>(exports_end) - exports_start <= 0x100000) {
            for (std::size_t count = 0; exports_start < exports_end &&
                                      count < 256; ++count) {
                std::array<std::uint8_t, 0x2c> module_info{};
                if (!read_module_info(exports_start, module_info)) return false;
                const auto size = module_info[0] == 0 ? 0x2cu : module_info[0];
                if (size < 0x2c || exports_start + size > exports_end) {
                    error = "PS3 PRX export table has an invalid module record";
                    return false;
                }
                std::uint16_t attributes = 0;
                std::uint16_t function_count = 0;
                std::uint16_t variable_count = 0;
                if (!read_u16(exports_start + 0x04, attributes) ||
                    !read_u16(exports_start + 0x06, function_count) ||
                    !read_u16(exports_start + 0x08, variable_count)) {
                    return false;
                }
                const auto nids = normalize_address(read_info_u32(module_info, 0x10));
                const auto addresses = normalize_address(
                    read_info_u32(module_info, 0x14));
                if ((attributes & 0x8000u) != 0 && nids != 0 && addresses != 0) {
                    for (std::uint32_t index = 0;
                         index < static_cast<std::uint32_t>(function_count) +
                                     variable_count;
                         ++index) {
                        std::uint32_t nid = 0;
                        std::uint32_t entry = 0;
                        if (!ReadU32(nids + index * 4, nid, error) ||
                            !ReadU32(addresses + index * 4, entry, error)) {
                            return false;
                        }
                        entry = normalize_address(entry);
                        if (index >= function_count) continue;
                        switch (nid) {
                        case 0xbc9a0086u: module.start_entry = entry; break;
                        case 0x0d10fd3fu: module.prologue_entry = entry; break;
                        default: break;
                        }
                    }
                }
                exports_start += size;
            }
        }
    }

    next_prx_address_ = static_cast<std::uint32_t>(
        (image_end + 0xffffu) & ~std::uint64_t{0xffffu});
    module.base = static_cast<std::uint32_t>(base);
    return true;
}

std::uint32_t Ps3Lv2::CreateObject(Object object) {
    while (next_object_id_ == 0 || objects_.contains(next_object_id_)) {
        ++next_object_id_;
    }
    const auto id = next_object_id_++;
    objects_.emplace(id, object);
    return id;
}

Ps3Lv2::Object* Ps3Lv2::FindObject(std::uint32_t id, ObjectType type) {
    const auto it = objects_.find(id);
    if (it == objects_.end() || it->second.type != type) return nullptr;
    return &it->second;
}

void Ps3Lv2::PrepareThread(cpu::PpuRegisters& registers) noexcept {
    if (registers.gpr[13] < 0x7034) return;
    const auto tls_thread_id = registers.gpr[13] - 0x7030 + 4;
    std::string error;
    WriteU32(tls_thread_id, kBootstrapPpuThreadId, error);
}

const Ps3Lv2::Object* Ps3Lv2::FindObject(std::uint32_t id,
                                         ObjectType type) const {
    const auto it = objects_.find(id);
    if (it == objects_.end() || it->second.type != type) return nullptr;
    return &it->second;
}

bool Ps3Lv2::HandleObjectCreate(cpu::PpuRegisters& registers,
                                ObjectType type,
                                std::string& error) {
    if (registers.gpr[3] == 0) {
        registers.gpr[3] = kCellEinval;
        return true;
    }
    const auto id = CreateObject({type, false, 0, 0});
    if (!WriteU32(registers.gpr[3], id, error)) {
        registers.gpr[3] = kCellEfault;
        error.clear();
        return true;
    }
    registers.gpr[3] = kCellOk;
    return true;
}

bool Ps3Lv2::HandleObjectDestroy(cpu::PpuRegisters& registers,
                                 ObjectType type,
                                 std::string& error) {
    const auto id = static_cast<std::uint32_t>(registers.gpr[3]);
    if (FindObject(id, type) == nullptr) {
        registers.gpr[3] = kCellEsrch;
        return true;
    }
    objects_.erase(id);
    registers.gpr[3] = kCellOk;
    error.clear();
    return true;
}

bool Ps3Lv2::HandleObjectLock(cpu::PpuRegisters& registers,
                              ObjectType type,
                              bool try_lock,
                              std::string& error) {
    auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]), type);
    if (object == nullptr) {
        registers.gpr[3] = kCellEsrch;
        return true;
    }
    if (object->locked) {
        registers.gpr[3] = try_lock ? kCellEbusy : kCellEagain;
        return true;
    }
    object->locked = true;
    registers.gpr[3] = kCellOk;
    error.clear();
    return true;
}

bool Ps3Lv2::HandleObjectUnlock(cpu::PpuRegisters& registers,
                                ObjectType type,
                                std::string& error) {
    auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]), type);
    if (object == nullptr) {
        registers.gpr[3] = kCellEsrch;
        return true;
    }
    object->locked = false;
    registers.gpr[3] = kCellOk;
    error.clear();
    return true;
}

bool Ps3Lv2::Dispatch(cpu::PpuRegisters& registers, std::string& error) {
    error.clear();
    // PPU TLS keeps the current thread ID at r13 - 0x7030 + 4. The VSH
    // user-space lightweight-mutex helpers read it directly rather than
    // issuing sys_ppu_thread_get_id on every lock attempt. Keep the bootstrap
    // thread's TLS slot coherent whenever the guest crosses into LV2.
    PrepareThread(registers);
    const auto syscall = registers.gpr[11];
    const std::array<std::uint64_t, 4> arguments{
        registers.gpr[3], registers.gpr[4], registers.gpr[5], registers.gpr[6]};
    bool handled = false;
    switch (syscall) {
    case kSysProcessGetpid:
        registers.gpr[3] = 1;
        handled = true;
        break;
    case kSysProcessExit:
    case kSysProcessExit2:
    case kSysPpuThreadExit:
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysProcessGetParamsfo: {
        std::array<std::uint8_t, 0x40> paramsfo{};
        const auto result = memory_.Write(registers.gpr[3], paramsfo);
        // RPCS3 returns CELL_ENOENT for the firmware shell because it has no
        // game title ID. VSH uses this result to select its system-process
        // startup path; returning success here sends it through the game
        // process path and eventually into abort().
        registers.gpr[3] = result.ok() ? kCellEnoent : kCellEfault;
        error.clear();
        handled = true;
        break;
    }
    case kSysPpuThreadGetId:
        registers.gpr[3] = 0x1000000;
        handled = true;
        break;
    case kSysPpuThreadYield:
    case kSysPpuThreadJoin:
    case kSysPpuThreadDetach:
    case kSysPpuThreadStart:
    case kSysPpuThreadOnce:
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysPpuThreadGetJoinState:
        handled = WriteU32(registers.gpr[3], 1, error);
        registers.gpr[3] = handled ? kCellOk : kCellEfault;
        error.clear();
        break;
    case kSysPpuThreadGetPriority:
        handled = WriteU32(registers.gpr[4], 1000, error);
        registers.gpr[3] = handled ? kCellOk : kCellEfault;
        error.clear();
        break;
    case kSysPpuThreadCreate:
        handled = HandleObjectCreate(registers, ObjectType::Thread, error);
        break;
    case kSysEventFlagCreate:
        handled = HandleObjectCreate(registers, ObjectType::EventFlag, error);
        if (handled && registers.gpr[3] == kCellOk) {
            // The initial pattern is kept in the object for trywait/set/clear.
        }
        break;
    case kSysEventFlagDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::EventFlag, error);
        break;
    case kSysEventFlagTrywait: {
        auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                  ObjectType::EventFlag);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else if ((object->value & static_cast<std::int64_t>(registers.gpr[4])) == 0)
            registers.gpr[3] = kCellEagain;
        else {
            if (registers.gpr[6] != 0) WriteU64(registers.gpr[6], object->value, error);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        error.clear();
        break;
    }
    case kSysEventFlagSet: {
        auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                  ObjectType::EventFlag);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else {
            object->value |= static_cast<std::int64_t>(registers.gpr[4]);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysEventFlagClear: {
        auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                  ObjectType::EventFlag);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else {
            object->value &= ~static_cast<std::int64_t>(registers.gpr[4]);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysSemaphoreCreate:
        handled = HandleObjectCreate(registers, ObjectType::Semaphore, error);
        break;
    case kSysSemaphoreDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Semaphore, error);
        break;
    case kSysSemaphoreWait:
    case kSysSemaphoreTrywait: {
        auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                  ObjectType::Semaphore);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else if (object->value <= 0) registers.gpr[3] = kCellEagain;
        else { --object->value; registers.gpr[3] = kCellOk; }
        handled = true;
        break;
    }
    case kSysSemaphorePost: {
        auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                  ObjectType::Semaphore);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else { object->value += static_cast<std::int64_t>(registers.gpr[4]); registers.gpr[3] = kCellOk; }
        handled = true;
        break;
    }
    case kSysSemaphoreGetValue: {
        const auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                        ObjectType::Semaphore);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else {
            handled = WriteU32(registers.gpr[4], static_cast<std::uint32_t>(object->value), error);
            registers.gpr[3] = handled ? kCellOk : kCellEfault;
            error.clear();
        }
        handled = true;
        break;
    }
    case kSysLwMutexCreate:
        handled = HandleObjectCreate(registers, ObjectType::Mutex, error);
        break;
    case kSysLwMutexDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Mutex, error);
        break;
    case kSysLwMutexLock:
    case kSysLwMutexTrylock:
        handled = HandleObjectLock(registers, ObjectType::Mutex,
                                   syscall == kSysLwMutexTrylock, error);
        break;
    case kSysLwMutexUnlock:
        handled = HandleObjectUnlock(registers, ObjectType::Mutex, error);
        break;
    case kSysMutexCreate:
        handled = HandleObjectCreate(registers, ObjectType::Mutex, error);
        break;
    case kSysMutexDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Mutex, error);
        break;
    case kSysMutexLock:
    case kSysMutexTrylock:
        handled = HandleObjectLock(registers, ObjectType::Mutex,
                                   syscall == kSysMutexTrylock, error);
        break;
    case kSysMutexUnlock:
        handled = HandleObjectUnlock(registers, ObjectType::Mutex, error);
        break;
    case kSysCondCreate:
        handled = HandleObjectCreate(registers, ObjectType::Condition, error);
        break;
    case kSysCondDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Condition, error);
        break;
    case kSysCondWait:
    case kSysCondSignal:
    case kSysCondSignalAll:
    case kSysCondSignalTo:
        registers.gpr[3] = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                      ObjectType::Condition) != nullptr
                               ? kCellOk : kCellEsrch;
        handled = true;
        break;
    case kSysRwlockCreate:
        handled = HandleObjectCreate(registers, ObjectType::RwLock, error);
        break;
    case kSysRwlockDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::RwLock, error);
        break;
    case kSysRwlockRlock:
    case kSysRwlockTryrlock:
    case kSysRwlockWlock:
    case kSysRwlockTrywlock:
        handled = HandleObjectLock(registers, ObjectType::RwLock,
                                   syscall == kSysRwlockTryrlock || syscall == kSysRwlockTrywlock,
                                   error);
        break;
    case kSysRwlockRunlock:
    case kSysRwlockWunlock:
        handled = HandleObjectUnlock(registers, ObjectType::RwLock, error);
        break;
    case kSysEventQueueCreate:
        handled = HandleObjectCreate(registers, ObjectType::EventQueue, error);
        break;
    case kSysEventQueueDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::EventQueue, error);
        break;
    case kSysEventPortCreate:
        handled = HandleObjectCreate(registers, ObjectType::EventPort, error);
        break;
    case kSysEventPortDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::EventPort, error);
        break;
    case kSysEventPortConnectLocal:
    case kSysEventPortDisconnect:
    case kSysEventPortSend:
    case kSysEventPortConnectIpc:
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysEventFlagGet: {
        const auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                        ObjectType::EventFlag);
        if (object == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else {
            handled = WriteU64(registers.gpr[4], static_cast<std::uint64_t>(object->value), error);
            registers.gpr[3] = handled ? kCellOk : kCellEfault;
            error.clear();
        }
        handled = true;
        break;
    }
    case kSysTimeGetTimebaseFrequency:
        registers.gpr[3] = 79800000;
        handled = true;
        break;
    case kSysTtyRead:
        if (registers.gpr[6] != 0 && !WriteU32(registers.gpr[6], 0, error)) {
            registers.gpr[3] = kCellEfault;
        } else if (registers.gpr[4] != 0 && registers.gpr[5] != 0) {
            std::vector<std::uint8_t> zeros(static_cast<std::size_t>(
                std::min<std::uint64_t>(registers.gpr[5], 0x100000u)), 0);
            const auto result = memory_.Write(registers.gpr[4], zeros);
            registers.gpr[3] = result.ok() ? kCellOk : kCellEfault;
            if (registers.gpr[6] != 0 && result.ok()) WriteU32(registers.gpr[6], 0, error);
        } else {
            registers.gpr[3] = kCellOk;
        }
        error.clear();
        handled = true;
        break;
    case kSysTtyWrite: {
        const auto length = registers.gpr[5];
        if (length > 0x100000u || (length != 0 && registers.gpr[4] == 0)) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        std::vector<std::uint8_t> buffer(static_cast<std::size_t>(length));
        const auto result = length == 0 ? memory::MemoryResult{}
                                        : memory_.Read(registers.gpr[4], buffer);
        if (!result.ok()) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else if (registers.gpr[6] != 0 && !WriteU32(
                       registers.gpr[6], static_cast<std::uint32_t>(length), error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysMemoryContainerCreateLegacy:
    case kSysMemoryContainerCreate:
        handled = HandleObjectCreate(registers, ObjectType::Memory, error);
        break;
    case kSysMemoryContainerDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Memory, error);
        break;
    case kSysMemoryContainerGetSize:
        handled = WriteU64(registers.gpr[3], kDefaultUserMemorySize, error);
        registers.gpr[3] = handled ? kCellOk : kCellEfault;
        error.clear();
        break;
    case kSysMemoryGetUserMemorySize:
        if (registers.gpr[3] == 0) registers.gpr[3] = kCellEfault;
        else {
            handled = WriteU64(registers.gpr[3], kDefaultUserMemorySize, error);
            registers.gpr[3] = handled ? kCellOk : kCellEfault;
            error.clear();
        }
        handled = true;
        break;
    case kSysMemoryAllocate: {
        const auto size = registers.gpr[3];
        const auto alignment = registers.gpr[4] == 0 ? 0x1000ull : registers.gpr[4];
        if (registers.gpr[5] == 0 || size == 0 ||
            (alignment & (alignment - 1)) != 0) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        const auto address = (next_memory_address_ + alignment - 1) & ~(alignment - 1);
        const auto mapped = memory_.Map({address, size,
            memory::kPermissionRead | memory::kPermissionWrite});
        if (!mapped.ok()) registers.gpr[3] = kCellEnomem;
        else if (!WriteU32(registers.gpr[5], static_cast<std::uint32_t>(address), error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            if (address == kDefaultUserMemoryBase &&
                !WriteU32(address + 0x34, kBootstrapPpuThreadId, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
                handled = true;
                break;
            }
            next_memory_address_ = address + size;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysMemoryAllocateFromContainer: {
        const auto size = registers.gpr[3];
        const auto output = registers.gpr[6];
        if (output == 0 || size == 0) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        const auto alignment = 0x1000ull;
        const auto address = (next_memory_address_ + alignment - 1) & ~(alignment - 1);
        const auto mapped = memory_.Map({address, size,
            memory::kPermissionRead | memory::kPermissionWrite});
        if (!mapped.ok()) registers.gpr[3] = kCellEnomem;
        else if (!WriteU32(output, static_cast<std::uint32_t>(address), error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            next_memory_address_ = address + size;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysMemoryFree:
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysSmGetParams: {
        // RPCS3's system-manager HLE exposes the same deterministic values
        // used by VSH during its early boot path: two zero mode bytes, the
        // 0x200 memory profile and boot parameter 7.
        const auto write_byte = [&](std::uint64_t address) {
            const std::array<std::uint8_t, 1> value{0};
            const auto result = memory_.Write(address, value);
            if (!result.ok()) error = result.error;
            return result.ok();
        };
        if (registers.gpr[3] == 0 || registers.gpr[4] == 0 ||
            registers.gpr[5] == 0 || registers.gpr[6] == 0 ||
            !write_byte(registers.gpr[3]) ||
            !write_byte(registers.gpr[4]) ||
            !WriteU32(registers.gpr[5], 0x200, error) ||
            !WriteU64(registers.gpr[6], 7, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysPrxRegisterModule: {
        // RPCS3 accepts the legacy 0x1c/0x20 option and the current 0x30
        // option. The guest passes the module name in r3 and the option in
        // r4. Keep the registration metadata so a future PRX linker can use
        // the exact import/export ranges instead of treating this as a bare
        // success stub.
        const auto option = registers.gpr[4];
        if (option == 0) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        std::uint64_t option_size = 0;
        if (!ReadU64(option, option_size, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }

        RegisteredPrx module{};
        module.name = registers.gpr[3];
        if (option_size == 0x1c || option_size == 0x20) {
            std::uint32_t ignored = 0;
            std::uint32_t library_stubs = 0;
            if (!ReadU32(option + 0x08, ignored, error) ||
                !ReadU32(option + 0x0c, ignored, error) ||
                !ReadU32(option + 0x10, library_stubs, error) ||
                !ReadU32(option + 0x14, module.library_stubs_size, error) ||
                !ReadU32(option + 0x18, ignored, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
                handled = true;
                break;
            }
            module.library_stubs = library_stubs;
        } else if (option_size == 0x30) {
            std::uint32_t ignored = 0;
            std::uint32_t library_entries = 0;
            std::uint32_t library_stubs = 0;
            if (!ReadU64(option + 0x08, module.type, error) ||
                !ReadU32(option + 0x10, ignored, error) ||
                !ReadU32(option + 0x14, ignored, error) ||
                !ReadU32(option + 0x18, library_entries, error) ||
                !ReadU32(option + 0x1c, module.library_entries_size, error) ||
                !ReadU32(option + 0x20, library_stubs, error) ||
                !ReadU32(option + 0x24, module.library_stubs_size, error) ||
                !ReadU32(option + 0x28, ignored, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
                handled = true;
                break;
            }
            module.library_entries = library_entries;
            module.library_stubs = library_stubs;
        } else {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }

        registered_prx_modules_.push_back(module);
        registers.gpr[3] = kCellOk;
        error.clear();
        handled = true;
        break;
    }
    case kSysPrxRegisterLibrary:
        // RPCS3 keeps this boundary as a successful registration point. The
        // actual export table is consumed by its PRX linker; VSH can proceed
        // through this ABI while our linker metadata is still being built.
        registers.gpr[3] = kCellOk;
        error.clear();
        handled = true;
        break;
    case kSysPrxLoadModuleOnMemcontainer: {
        std::string path;
        if (!ReadCString(registers.gpr[3], path, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        if (path.empty()) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        const auto id = next_prx_id_;
        next_prx_id_ += 0x100;
        loaded_prx_modules_.push_back({id, std::move(path), registers.gpr[5]});
        if (!LoadPrxImage(loaded_prx_modules_.back(), error)) {
            loaded_prx_modules_.pop_back();
            registers.gpr[3] = kCellEnoent;
            error.clear();
            handled = true;
            break;
        }
        registers.gpr[3] = id;
        error.clear();
        handled = true;
        break;
    }
    case kSysPrxStartModule: {
        const auto id = static_cast<std::uint32_t>(registers.gpr[3]);
        auto loaded = std::find_if(loaded_prx_modules_.begin(),
                                   loaded_prx_modules_.end(),
                                   [id](const LoadedPrx& module) {
                                       return module.id == id;
                                   });
        if (id == 0 || registers.gpr[5] == 0) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        if (loaded == loaded_prx_modules_.end()) {
            registers.gpr[3] = kCellEsrch;
            handled = true;
            break;
        }
        std::uint64_t option_size = 0;
        std::uint64_t command = 0;
        if (!ReadU64(registers.gpr[5], option_size, error) ||
            !ReadU64(registers.gpr[5] + 0x08, command, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        if (option_size < 0x10) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        // sys_prx_start_module_option_t is an in/out structure.  The caller
        // consumes entry after the HLE returns, so leaving the zero-filled
        // stack slot untouched makes the guest branch to address zero.  A
        // module without a resolved start routine uses the same sentinel as
        // RPCS3; the VSH wrapper treats it as "no entry" rather than trying
        // to execute it.
        const auto entry = loaded->start_entry == 0
            ? ~std::uint64_t{0}
            : loaded->start_entry;
        const auto entry2 = loaded->prologue_entry == 0
            ? ~std::uint64_t{0}
            : loaded->prologue_entry;
        if (!WriteU64(registers.gpr[5] + 0x10, entry, error) ||
            (option_size != 0x20 &&
             !WriteU64(registers.gpr[5] + 0x20, entry2, error))) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        switch (command & 0xf) {
        case 1:
            loaded->started = true;
            registers.gpr[3] = kCellOk;
            break;
        case 2: {
            std::uint64_t result = 0;
            if (option_size < 0x20 ||
                !ReadU64(registers.gpr[5] + 0x18, result, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = (result & 0xffffffffu) == 0
                    ? kCellOk : result;
            }
            break;
        }
        default:
            registers.gpr[3] = kCellEinval;
            break;
        }
        error.clear();
        handled = true;
        break;
    }
    case kSysDbgPpuExceptionHandler:
        // Retail VSH registers a PPU exception mask during startup. The
        // interpreter has no asynchronous exception source yet, but the
        // registration itself must succeed and remain deterministic.
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysRsxDeviceOpen:
    case kSysRsxDeviceClose:
    case kSysRsxMemoryFree:
    case kSysRsxContextFree:
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysRsxMemoryAllocate:
        handled = HandleObjectCreate(registers, ObjectType::Rsx, error);
        break;
    case kSysRsxContextAllocate:
        handled = HandleObjectCreate(registers, ObjectType::Rsx, error);
        break;
    default:
        error = UnknownSyscall(syscall);
        return false;
    }
    if (handled) {
        trace_.push_back({syscall, registers.gpr[3], arguments, trace_.size(),
                          registers.pc >= 4 ? registers.pc - 4 : registers.pc});
    }
    return handled;
}

} // namespace vshift::hle
