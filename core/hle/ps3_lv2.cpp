#include "core/hle/ps3_lv2.h"

#include <algorithm>
#include <array>
#include <iomanip>
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
constexpr std::uint64_t kSysPpuThreadSetPriority = 47;
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
constexpr std::uint64_t kSysLwCondCreate = 111;
constexpr std::uint64_t kSysLwCondDestroy = 112;
constexpr std::uint64_t kSysLwCondQueueWait = 113;
constexpr std::uint64_t kSysLwCondSignal = 115;
constexpr std::uint64_t kSysLwCondSignalAll = 116;
constexpr std::uint64_t kSysLwMutexUnlock2 = 117;
constexpr std::uint64_t kSysTimerUsleep = 141;
constexpr std::uint64_t kSysTimerSleep = 142;
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
constexpr std::uint64_t kSysEventQueueReceive = 130;
constexpr std::uint64_t kSysEventQueueTryreceive = 131;
constexpr std::uint64_t kSysEventQueueDrain = 133;
constexpr std::uint64_t kSysSpuThreadGroupCreate = 170;
constexpr std::uint64_t kSysSpuThreadGroupInitialize = 172;
constexpr std::uint64_t kSysSpuThreadGroupStart = 173;
constexpr std::uint64_t kSysSpuThreadGroupSystemBoundary = 248;
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
constexpr std::uint64_t kSysFsOpen = 801;
constexpr std::uint64_t kSysFsRead = 802;
constexpr std::uint64_t kSysFsClose = 804;
constexpr std::uint64_t kSysFsOpendir = 805;
constexpr std::uint64_t kSysFsReaddir = 806;
constexpr std::uint64_t kSysFsClosedir = 807;
constexpr std::uint64_t kSysFsStat = 808;
constexpr std::uint64_t kSysFsMkdir = 811;
constexpr std::uint64_t kSysFsFcntl = 817;
constexpr std::uint64_t kSysFsLseek = 818;
constexpr std::uint64_t kSysFsMount = 837;
constexpr std::uint64_t kSysMmapperFreeSharedMemory = 329;
constexpr std::uint64_t kSysMmapperAllocateAddress = 330;
constexpr std::uint64_t kSysMmapperFreeAddress = 331;
constexpr std::uint64_t kSysMmapperAllocateSharedMemory = 332;
constexpr std::uint64_t kSysMmapperMapSharedMemory = 334;
constexpr std::uint64_t kSysMmapperAllocateSharedMemoryExt = 339;
constexpr std::uint64_t kSysMmapperSearchAndMap = 337;
constexpr std::uint64_t kSysMemoryContainerCreateLegacy = 324;
constexpr std::uint64_t kSysMemoryContainerCreate = 341;
constexpr std::uint64_t kSysMemoryContainerDestroy = 342;
constexpr std::uint64_t kSysMemoryContainerGetSize = 343;
constexpr std::uint64_t kSysMemoryAllocate = 348;
constexpr std::uint64_t kSysMemoryFree = 349;
constexpr std::uint64_t kSysMemoryAllocateFromContainer = 350;
constexpr std::uint64_t kSysMemoryGetUserMemorySize = 352;
constexpr std::uint64_t kSysSmInitialize = 382;
constexpr std::uint64_t kSysSmGetParams = 380;
constexpr std::uint64_t kSysUartInitialize = 367;
constexpr std::uint64_t kSysStorageOpen = 600;
constexpr std::uint64_t kSysStorageClose = 601;
constexpr std::uint64_t kSysStorageGetDeviceInfo = 609;
constexpr std::uint64_t kSysStorageGetDeviceConfig = 610;
constexpr std::uint64_t kSysStorageReportDevices = 611;
constexpr std::uint64_t kSysStorageConfigureMediumEvent = 612;
constexpr std::uint64_t kSysConfigOpen = 516;
constexpr std::uint64_t kSysConfigClose = 517;
constexpr std::uint64_t kSysConfigGetServiceEvent = 518;
constexpr std::uint64_t kSysConfigAddServiceListener = 519;
constexpr std::uint64_t kSysConfigRemoveServiceListener = 520;
constexpr std::uint64_t kSysConfigRegisterService = 521;
constexpr std::uint64_t kSysConfigUnregisterService = 522;
constexpr std::uint64_t kSysSsApplianceInfoManager = 867;
constexpr std::uint64_t kSysSsGetCacheOfProductMode = 873;
constexpr std::uint64_t kSysUsbdInitialize = 530;
constexpr std::uint64_t kSysUsbdFinalize = 531;
constexpr std::uint64_t kSysUsbdGetDeviceList = 532;
constexpr std::uint64_t kSysUsbdRegisterLdd = 535;
constexpr std::uint64_t kSysUsbdUnregisterLdd = 536;
constexpr std::uint64_t kSysUsbdReceiveEvent = 540;
constexpr std::uint64_t kSysUsbdEventPortSend = 549;
constexpr std::uint64_t kSysGl819Probe = 563;
constexpr std::uint64_t kSysPrxStartModule = 481;
constexpr std::uint64_t kSysPrxStopModule = 482;
constexpr std::uint64_t kSysPrxUnregisterModule = 483;
constexpr std::uint64_t kSysPrxRegisterModule = 484;
constexpr std::uint64_t kSysPrxRegisterLibrary = 486;
constexpr std::uint64_t kSysPrxLoadModule = 480;
constexpr std::uint64_t kSysPrxLoadModuleOnMemcontainer = 497;
constexpr std::uint64_t kSysRsxAudioInitialize = 650;
constexpr std::uint64_t kSysRsxAudioFinalize = 651;
constexpr std::uint64_t kSysRsxAudioImportSharedMemory = 652;
constexpr std::uint64_t kSysRsxAudioUnimportSharedMemory = 653;
constexpr std::uint64_t kSysRsxAudioCreateConnection = 654;
constexpr std::uint64_t kSysRsxAudioCloseConnection = 655;
constexpr std::uint64_t kSysRsxAudioPrepareProcess = 656;
constexpr std::uint64_t kSysRsxAudioStartProcess = 657;
constexpr std::uint64_t kSysRsxAudioStopProcess = 658;
constexpr std::uint64_t kSysRsxAudioGetDmaParam = 659;
constexpr std::uint64_t kSysDbgPpuExceptionHandler = 988;
constexpr std::uint64_t kSysRsxDeviceOpen = 666;
constexpr std::uint64_t kSysRsxDeviceClose = 667;
constexpr std::uint64_t kSysRsxMemoryAllocate = 668;
constexpr std::uint64_t kSysRsxMemoryFree = 669;
constexpr std::uint64_t kSysRsxContextAllocate = 670;
constexpr std::uint64_t kSysRsxContextFree = 671;

constexpr std::uint64_t kDefaultUserMemoryBase = 0x0d000000;
constexpr std::uint64_t kDefaultUserMemorySize = 0x10000000;
// The audio import is also used as a small RSX-visible staging heap by the
// early VSH path. It is not a renderer yet, but it must be writable by guest
// code after sys_rsxaudio_import_shared_memory returns its address.
constexpr std::uint64_t kRsxAudioImportedMemorySize = 0x200000;
constexpr std::uint32_t kBootstrapPpuThreadId = 0x01000000;

// This is the fixed retail PS3 storage topology exposed before any removable
// media is mounted. The device IDs are guest ABI values, not host paths.
constexpr std::array<std::uint64_t, 17> kStorageDeviceIds{
    0x010300000000000aull,
    0x0100000000000001ull, 0x0100000100000001ull,
    0x0100000200000001ull, 0x0100000300000001ull,
    0x0100000400000001ull, 0x0100000500000001ull,
    0x0100000600000001ull,
    0x0101000000000007ull, 0x0101000100000007ull,
    0x0101000200000007ull, 0x0101000000000006ull,
    0x0100000000000004ull, 0x0100000100000004ull,
    0x0100000200000004ull, 0x0100000300000004ull,
    0x0100000000000003ull,
};
constexpr std::uint32_t kStorageClassCount = 6;
constexpr std::uint64_t kStorageBaseMask = 0x0fffff00ffffffffull;
constexpr std::uint64_t kStorageAtaHdd = 0x0101000000000007ull;
constexpr std::uint64_t kStorageBdvd = 0x0101000000000006ull;
constexpr std::uint64_t kStorageUsb = 0x010300000000000aull;
constexpr std::uint64_t kStorageBuiltinFlash = 0x0100000000000001ull;
constexpr std::uint64_t kStorageNorFlash = 0x0100000000000004ull;
constexpr std::uint64_t kStorageNandUnknown = 0x0100000000000003ull;

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
    case kSysPpuThreadSetPriority: return "sys_ppu_thread_set_priority";
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
    case kSysLwCondCreate: return "_sys_lwcond_create";
    case kSysLwCondDestroy: return "_sys_lwcond_destroy";
    case kSysLwCondQueueWait: return "_sys_lwcond_queue_wait";
    case kSysLwCondSignal: return "_sys_lwcond_signal";
    case kSysLwCondSignalAll: return "_sys_lwcond_signal_all";
    case kSysLwMutexUnlock2: return "_sys_lwmutex_unlock2";
    case kSysTimerUsleep: return "sys_timer_usleep";
    case kSysTimerSleep: return "sys_timer_sleep";
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
    case kSysEventQueueReceive: return "sys_event_queue_receive";
    case kSysEventQueueTryreceive: return "sys_event_queue_tryreceive";
    case kSysEventQueueDrain: return "sys_event_queue_drain";
    case kSysSpuThreadGroupCreate: return "sys_spu_thread_group_create";
    case kSysSpuThreadGroupInitialize: return "sys_spu_thread_initialize";
    case kSysSpuThreadGroupStart: return "sys_spu_thread_group_start";
    case kSysSpuThreadGroupSystemBoundary:
        return "sys_spu_thread_group_0xf8";
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
    case kSysFsOpen: return "sys_fs_open";
    case kSysFsRead: return "sys_fs_read";
    case kSysFsClose: return "sys_fs_close";
    case kSysFsOpendir: return "sys_fs_opendir";
    case kSysFsReaddir: return "sys_fs_readdir";
    case kSysFsClosedir: return "sys_fs_closedir";
    case kSysFsStat: return "sys_fs_stat";
    case kSysFsMkdir: return "sys_fs_mkdir";
    case kSysFsFcntl: return "sys_fs_fcntl";
    case kSysFsLseek: return "sys_fs_lseek";
    case kSysFsMount: return "sys_fs_mount";
    case kSysMmapperFreeSharedMemory:
        return "sys_mmapper_free_shared_memory";
    case kSysMmapperAllocateAddress: return "sys_mmapper_allocate_address";
    case kSysMmapperAllocateSharedMemory: return "sys_mmapper_allocate_shared_memory";
    case kSysMmapperMapSharedMemory: return "sys_mmapper_map_shared_memory";
    case kSysMmapperAllocateSharedMemoryExt:
        return "sys_mmapper_allocate_shared_memory_ext";
    case kSysMmapperSearchAndMap: return "sys_mmapper_search_and_map";
    case kSysMmapperFreeAddress: return "sys_mmapper_free_address";
    case kSysMemoryContainerCreateLegacy: return "sys_memory_container_create";
    case kSysMemoryAllocate: return "sys_memory_allocate";
    case kSysMemoryFree: return "sys_memory_free";
    case kSysMemoryAllocateFromContainer: return "sys_memory_allocate_from_container";
    case kSysMemoryContainerCreate: return "sys_memory_container_create";
    case kSysMemoryContainerDestroy: return "sys_memory_container_destroy";
    case kSysMemoryContainerGetSize: return "sys_memory_container_get_size";
    case kSysMemoryGetUserMemorySize: return "sys_memory_get_user_memory_size";
    case kSysSmGetParams: return "sys_sm_get_params";
    case kSysSmInitialize: return "sys_sm_initialize";
    case kSysUartInitialize: return "sys_uart_initialize";
    case kSysStorageOpen: return "sys_storage_open";
    case kSysStorageClose: return "sys_storage_close";
    case kSysStorageGetDeviceInfo: return "sys_storage_get_device_info";
    case kSysStorageGetDeviceConfig: return "sys_storage_get_device_config";
    case kSysStorageReportDevices: return "sys_storage_report_devices";
    case kSysStorageConfigureMediumEvent: return "sys_storage_configure_medium_event";
    case kSysConfigOpen: return "sys_config_open";
    case kSysConfigClose: return "sys_config_close";
    case kSysConfigGetServiceEvent: return "sys_config_get_service_event";
    case kSysConfigAddServiceListener: return "sys_config_add_service_listener";
    case kSysConfigRemoveServiceListener: return "sys_config_remove_service_listener";
    case kSysConfigRegisterService: return "sys_config_register_service";
    case kSysConfigUnregisterService: return "sys_config_unregister_service";
    case kSysSsApplianceInfoManager:
        return "sys_ss_appliance_info_manager";
    case kSysSsGetCacheOfProductMode:
        return "sys_ss_get_cache_of_product_mode";
    case kSysUsbdInitialize: return "sys_usbd_initialize";
    case kSysUsbdFinalize: return "sys_usbd_finalize";
    case kSysUsbdGetDeviceList: return "sys_usbd_get_device_list";
    case kSysUsbdRegisterLdd: return "sys_usbd_register_ldd";
    case kSysUsbdUnregisterLdd: return "sys_usbd_unregister_ldd";
    case kSysUsbdReceiveEvent: return "sys_usbd_receive_event";
    case kSysUsbdEventPortSend: return "sys_usbd_event_port_send";
    case kSysGl819Probe: return "sys_gl819_probe";
    case kSysPrxStartModule: return "sys_prx_start_module";
    case kSysPrxStopModule: return "sys_prx_stop_module";
    case kSysPrxUnregisterModule: return "sys_prx_unregister_module";
    case kSysPrxRegisterModule: return "sys_prx_register_module";
    case kSysPrxRegisterLibrary: return "sys_prx_register_library";
    case kSysPrxLoadModule: return "sys_prx_load_module";
    case kSysPrxLoadModuleOnMemcontainer: return "sys_prx_load_module_on_memcontainer";
    case kSysRsxAudioInitialize: return "sys_rsxaudio_initialize";
    case kSysRsxAudioFinalize: return "sys_rsxaudio_finalize";
    case kSysRsxAudioImportSharedMemory:
        return "sys_rsxaudio_import_shared_memory";
    case kSysRsxAudioUnimportSharedMemory:
        return "sys_rsxaudio_unimport_shared_memory";
    case kSysRsxAudioCreateConnection:
        return "sys_rsxaudio_create_connection";
    case kSysRsxAudioCloseConnection:
        return "sys_rsxaudio_close_connection";
    case kSysRsxAudioPrepareProcess:
        return "sys_rsxaudio_prepare_process";
    case kSysRsxAudioStartProcess:
        return "sys_rsxaudio_start_process";
    case kSysRsxAudioStopProcess:
        return "sys_rsxaudio_stop_process";
    case kSysRsxAudioGetDmaParam:
        return "sys_rsxaudio_get_dma_param";
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

bool Ps3Lv2::ReadU16(std::uint64_t address, std::uint16_t& value,
                     std::string& error) const {
    std::array<std::uint8_t, 2> bytes{};
    const auto result = memory_.Read(address, bytes);
    if (!result.ok()) {
        error = result.error;
        return false;
    }
    value = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[0]) << 8) | bytes[1]);
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

bool Ps3Lv2::WriteU16(std::uint64_t address, std::uint16_t value,
                      std::string& error) {
    const std::array<std::uint8_t, 2> bytes{
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

bool Ps3Lv2::ReadLibraryRecord(std::uint64_t address,
                               PrxLibraryRecord& record,
                               std::string& error) const {
    std::uint32_t header = 0;
    std::uint32_t counts = 0;
    if (!ReadU32(address, header, error) ||
        !ReadU32(address + 0x04, counts, error)) {
        return false;
    }
    record.size = static_cast<std::uint8_t>(header >> 24);
    if (record.size == 0) record.size = 0x2c;
    if (record.size < 0x1c || record.size > 0x100) {
        error = "PS3 PRX library record has an invalid size";
        return false;
    }
    record.attributes = static_cast<std::uint16_t>(counts >> 16);
    record.function_count = static_cast<std::uint16_t>(counts);
    std::uint32_t variable_counts = 0;
    if (!ReadU32(address + 0x08, variable_counts, error) ||
        !ReadU32(address + 0x10, record.name, error) ||
        !ReadU32(address + 0x14, record.nids, error) ||
        !ReadU32(address + 0x18, record.addresses, error) ||
        !ReadU32(address + 0x1c, record.variable_nids, error) ||
        !ReadU32(address + 0x20, record.variable_stubs, error)) {
        return false;
    }
    record.variable_count = static_cast<std::uint16_t>(variable_counts >> 16);
    record.tls_variable_count = static_cast<std::uint16_t>(variable_counts);
    return true;
}

void Ps3Lv2::AddExport(std::string module, std::uint32_t nid,
                       std::uint32_t address, std::uint32_t toc) {
    const auto existing = std::find_if(
        exported_functions_.begin(), exported_functions_.end(),
        [&](const auto& entry) {
            return entry.module == module && entry.nid == nid;
        });
    if (existing != exported_functions_.end()) {
        existing->address = address;
        existing->toc = toc;
        return;
    }
    exported_functions_.push_back({std::move(module), nid, address, toc});
}

bool Ps3Lv2::AddImportRecord(
    std::uint64_t address,
    const std::function<std::uint32_t(std::uint32_t)>& normalize,
    std::string& error) {
    PrxLibraryRecord record{};
    if (!ReadLibraryRecord(address, record, error)) return false;
    if (record.name == 0) return true;
    std::string module;
    if (!ReadCString(normalize(record.name), module, error)) return false;
    if (record.function_count != 0) {
        if (record.nids == 0 || record.addresses == 0) return false;
        for (std::uint32_t index = 0; index < record.function_count; ++index) {
            std::uint32_t nid = 0;
            if (!ReadU32(normalize(record.nids) + index * 4, nid, error)) {
                return false;
            }
            imported_functions_.push_back(
                {module, nid, normalize(record.addresses) + index * 4});
        }
    }
    if (record.variable_count != 0) {
        if (record.variable_nids == 0 || record.variable_stubs == 0) {
            error = "PS3 PRX variable import table is incomplete";
            return false;
        }
        for (std::uint32_t index = 0; index < record.variable_count; ++index) {
            std::uint32_t nid = 0;
            std::uint32_t references = 0;
            if (!ReadU32(normalize(record.variable_nids) + index * 4, nid,
                         error) ||
                !ReadU32(normalize(record.variable_stubs) + index * 4,
                         references, error)) {
                return false;
            }
            imported_variables_.push_back(
                {module, nid, normalize(references)});
        }
    }
    return true;
}

bool Ps3Lv2::AddExportRecord(
    std::uint64_t address,
    const std::function<std::uint32_t(std::uint32_t)>& normalize,
    std::string& error,
    std::uint32_t toc) {
    PrxLibraryRecord record{};
    if (!ReadLibraryRecord(address, record, error)) return false;
    if ((record.attributes & 0x1u) == 0 || record.name == 0 ||
        (record.function_count == 0 && record.variable_count == 0) ||
        record.nids == 0 || record.addresses == 0) {
        return true;
    }
    std::string module;
    if (!ReadCString(normalize(record.name), module, error)) return false;
    const auto nids = normalize(record.nids);
    const auto addresses = normalize(record.addresses);
    for (std::uint32_t index = 0; index < record.function_count; ++index) {
        std::uint32_t nid = 0;
        std::uint32_t entry = 0;
        if (!ReadU32(nids + index * 4, nid, error) ||
            !ReadU32(addresses + index * 4, entry, error)) {
            return false;
        }
        AddExport(module, nid, normalize(entry), toc);
    }
    for (std::uint32_t index = 0; index < record.variable_count; ++index) {
        std::uint32_t nid = 0;
        std::uint32_t entry = 0;
        const auto table_index = record.function_count + index;
        if (!ReadU32(nids + table_index * 4, nid, error) ||
            !ReadU32(addresses + table_index * 4, entry, error)) {
            return false;
        }
        const auto variable = std::find_if(
            exported_variables_.begin(), exported_variables_.end(),
            [&](const ExportedVariable& candidate) {
                return candidate.module == module && candidate.nid == nid;
            });
        if (variable == exported_variables_.end()) {
            exported_variables_.push_back({module, nid, normalize(entry)});
        } else {
            variable->address = normalize(entry);
        }
    }
    return true;
}

bool Ps3Lv2::IsExecutableAddress(std::uint32_t address) const noexcept {
    for (const auto& mapping : memory_.Mappings()) {
        const auto start = mapping.guest_address;
        const auto end = start + mapping.size;
        const auto target = static_cast<std::uint64_t>(address);
        if ((mapping.permissions & memory::kPermissionExecute) != 0 &&
            target >= start && target <= end &&
            static_cast<std::uint64_t>(4) <= end - target) {
            return true;
        }
    }
    return false;
}

std::uint32_t Ps3Lv2::EnsureFunctionDescriptor(
    std::uint32_t address, std::uint32_t toc, std::string& error) {
    if (address == 0) return 0;

    // PS3 ELFv1 calls load [entry, toc] from a function descriptor.  Some
    // PRX export tables instead contain a direct entry address; keep genuine
    // descriptors intact and materialize the missing pair for direct code.
    if (!IsExecutableAddress(address)) {
        std::uint32_t entry = 0;
        std::string probe_error;
        if (ReadU32(address, entry, probe_error) &&
            IsExecutableAddress(entry)) {
            return address;
        }
    }

    if (!IsExecutableAddress(address)) {
        error = "PS3 export address is neither executable code nor a function descriptor";
        return 0;
    }

    if (!descriptor_mapping_created_) {
        const auto mapped = memory_.Map({
            next_descriptor_address_, 0x00100000,
            memory::kPermissionRead | memory::kPermissionWrite});
        if (!mapped.ok()) {
            error = mapped.error;
            return 0;
        }
        descriptor_mapping_created_ = true;
    }

    const auto descriptor = next_descriptor_address_;
    next_descriptor_address_ += 8;
    const auto descriptor_toc = toc != 0 ? toc : main_toc_;
    if (!WriteU32(descriptor, address, error) ||
        !WriteU32(descriptor + 4, descriptor_toc, error)) {
        return 0;
    }
    return descriptor;
}

void Ps3Lv2::LinkImports(std::string& error) {
    for (const auto& import : imported_functions_) {
        const auto exported = std::find_if(
            exported_functions_.rbegin(), exported_functions_.rend(),
            [&](const auto& entry) {
                return entry.module == import.module && entry.nid == import.nid;
            });
        if (exported == exported_functions_.rend()) continue;
        const auto descriptor = EnsureFunctionDescriptor(
            exported->address, exported->toc, error);
        if (descriptor == 0 || !WriteU32(import.slot, descriptor, error)) {
            return;
        }
    }
    for (const auto& import : imported_variables_) {
        const auto exported = std::find_if(
            exported_variables_.rbegin(), exported_variables_.rend(),
            [&](const ExportedVariable& entry) {
                return entry.module == import.module && entry.nid == import.nid;
            });
        if (exported == exported_variables_.rend()) continue;
        if (!PatchVariableReferences(import.references, exported->address,
                                     error)) {
            return;
        }
    }
}

bool Ps3Lv2::PatchVariableReferences(std::uint32_t references,
                                     std::uint32_t address,
                                     std::string& error) {
    if (references == 0) {
        error = "PS3 PRX variable import has no relocation list";
        return false;
    }
    constexpr std::uint32_t kReferenceSize = 12;
    for (std::uint32_t index = 0; index < 4096; ++index) {
        const auto record = static_cast<std::uint64_t>(references) +
                            static_cast<std::uint64_t>(index) * kReferenceSize;
        std::uint32_t type = 0;
        std::uint32_t target = 0;
        std::uint32_t addend = 0;
        if (!ReadU32(record, type, error)) return false;
        if (type == 0) return true;
        if (!ReadU32(record + 4, target, error) ||
            !ReadU32(record + 8, addend, error)) {
            return false;
        }
        const auto value = address + addend;
        switch (type) {
        case 1: // R_PPC64_ADDR32
            if (!WriteU32(target, value, error)) return false;
            break;
        case 4: // R_PPC64_ADDR16_LO
            if (!WriteU16(target, static_cast<std::uint16_t>(value), error)) {
                return false;
            }
            break;
        case 6: { // R_PPC64_ADDR16_HA
            const auto adjusted = (value >> 16) +
                                  ((value & 0x8000u) != 0 ? 1u : 0u);
            if (!WriteU16(target, static_cast<std::uint16_t>(adjusted), error)) {
                return false;
            }
            break;
        }
        case 57: { // R_PPC64_ADDR16_LO_DS
            std::uint16_t instruction = 0;
            if (!ReadU16(target, instruction, error) ||
                !WriteU16(target,
                          static_cast<std::uint16_t>(
                              (instruction & 0x3u) | (value & 0xfffcu)),
                          error)) {
                return false;
            }
            break;
        }
        default:
            error = "PS3 PRX variable relocation type " +
                    std::to_string(type) + " is not implemented";
            return false;
        }
    }
    error = "PS3 PRX variable relocation list has no terminator";
    return false;
}

bool Ps3Lv2::RegisterMainVshImports(
    const std::vector<std::uint32_t>& records, std::string& error) {
    for (const auto record : records) {
        if (!AddImportRecord(record,
                             [](std::uint32_t address) { return address; },
                             error)) return false;
    }
    LinkImports(error);
    return error.empty();
}

bool Ps3Lv2::LookupImport(std::uint32_t slot, std::string& module,
                          std::uint32_t& nid) const {
    const auto imported = std::find_if(
        imported_functions_.begin(), imported_functions_.end(),
        [slot](const ImportedFunction& entry) { return entry.slot == slot; });
    if (imported != imported_functions_.end()) {
        module = imported->module;
        nid = imported->nid;
        return true;
    }
    for (const auto& variable : imported_variables_) {
        for (std::uint32_t index = 0; index < 4096; ++index) {
            std::uint32_t type = 0;
            std::uint32_t target = 0;
            std::string ignored;
            const auto record = static_cast<std::uint64_t>(variable.references) +
                                static_cast<std::uint64_t>(index) * 12;
            if (!ReadU32(record, type, ignored) || type == 0) break;
            if (!ReadU32(record + 4, target, ignored)) break;
            if (target == slot) {
                module = variable.module;
                nid = variable.nid;
                return true;
            }
        }
    }
    return false;
}

bool Ps3Lv2::LookupNearestImport(std::uint32_t address,
                                 std::uint32_t& location,
                                 bool& variable,
                                 std::string& module,
                                 std::uint32_t& nid) const {
    std::uint64_t best_distance = std::numeric_limits<std::uint64_t>::max();
    const auto consider = [&](std::uint32_t candidate, bool is_variable,
                              const std::string& candidate_module,
                              std::uint32_t candidate_nid) {
        const auto distance = candidate > address
            ? static_cast<std::uint64_t>(candidate) - address
            : static_cast<std::uint64_t>(address) - candidate;
        if (distance >= best_distance) return;
        best_distance = distance;
        location = candidate;
        variable = is_variable;
        module = candidate_module;
        nid = candidate_nid;
    };
    for (const auto& function : imported_functions_) {
        consider(function.slot, false, function.module, function.nid);
    }
    for (const auto& imported : imported_variables_) {
        for (std::uint32_t index = 0; index < 4096; ++index) {
            std::uint32_t type = 0;
            std::uint32_t target = 0;
            std::string ignored;
            const auto record = static_cast<std::uint64_t>(imported.references) +
                                static_cast<std::uint64_t>(index) * 12;
            if (!ReadU32(record, type, ignored) || type == 0) break;
            if (!ReadU32(record + 4, target, ignored)) break;
            consider(target, true, imported.module, imported.nid);
        }
    }
    return best_distance != std::numeric_limits<std::uint64_t>::max();
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
    // SCE_PPURELA segment indexes address only PT_LOAD segments, matching
    // RPCS3's compact PRX segment vector rather than the raw ELF header list.
    std::vector<std::uint32_t> segment_bases;
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
        segment_bases.push_back(static_cast<std::uint32_t>(guest_address));
        std::uint32_t permissions = memory::kPermissionRead;
        if ((program.flags & 0x1u) != 0) {
            permissions |= memory::kPermissionExecute;
        }
        if ((program.flags & 0x2u) != 0) {
            permissions |= memory::kPermissionWrite;
        }
        // SELF relocations are applied after the PT_LOAD bytes are copied.
        // RPCS3 keeps the image writable through that relocation phase and
        // only then enforces the final segment permissions. GuestMemory does
        // not expose a protection transition yet, so retain write access for
        // this deliberately small loader slice.
        permissions |= memory::kPermissionWrite;
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

    // SCE_PPURELA records must be applied before an exported ELFv1 function
    // descriptor is called. This covers the relocation forms used by the
    // first firmware PRX reached by VSH and fails explicitly for unknown
    // forms instead of leaving a raw, unmapped guest pointer behind.
    for (std::size_t index = 0; index < parsed.image.program_headers.size(); ++index) {
        const auto& program = parsed.image.program_headers[index];
        if (program.type != 0x700000a4u || program.file_size == 0) continue;
        const loader::Ps3SelfSection* section = nullptr;
        for (const auto& candidate : parsed.image.sections) {
            if (candidate.type == 2 && candidate.program_index == index) {
                section = &candidate;
                break;
            }
        }
        constexpr std::uint64_t kRelocationRecordSize = 0x18;
        if (section == nullptr || section->bytes.size() < program.file_size ||
            (program.file_size % kRelocationRecordSize) != 0) {
            error = "PS3 PRX relocation segment is incomplete";
            return false;
        }
        for (std::uint64_t offset = 0; offset < program.file_size;
             offset += kRelocationRecordSize) {
            const auto* record = section->bytes.data() + static_cast<std::size_t>(offset);
            const auto read_u32 = [&](std::size_t field) {
                return (static_cast<std::uint32_t>(record[field]) << 24) |
                       (static_cast<std::uint32_t>(record[field + 1]) << 16) |
                       (static_cast<std::uint32_t>(record[field + 2]) << 8) |
                       record[field + 3];
            };
            const auto read_u64 = [&](std::size_t field) {
                std::uint64_t value = 0;
                for (std::size_t byte = 0; byte < 8; ++byte) {
                    value = (value << 8) | record[field + byte];
                }
                return value;
            };
            const auto target_offset = read_u64(0x00);
            const auto index_value = record[0x0a];
            const auto index_address = record[0x0b];
            const auto type = read_u32(0x0c);
            const auto pointer = read_u64(0x10);
            if (index_address >= segment_bases.size() ||
                segment_bases[index_address] == 0) {
                error = "PS3 PRX relocation target segment is invalid";
                return false;
            }
            const auto target = static_cast<std::uint64_t>(segment_bases[index_address]) +
                                target_offset;
            const auto data_base = index_value == 0xffu
                ? 0ull
                : (index_value < segment_bases.size()
                    ? segment_bases[index_value]
                    : 0ull);
            if (index_value != 0xffu && data_base == 0) {
                error = "PS3 PRX relocation value segment is invalid";
                return false;
            }
            const auto value = data_base + pointer;
            const auto relocation_write_failed = [&](const char* operation) {
                std::ostringstream detail;
                detail << "PS3 PRX relocation " << operation
                       << " failed at target 0x" << std::hex << target
                       << " (offset 0x" << target_offset
                       << ", target segment " << std::dec << static_cast<unsigned>(index_address)
                       << ", type 0x" << std::hex << type << ", value 0x" << value
                       << ", record";
                for (std::size_t byte = 0; byte < kRelocationRecordSize; ++byte) {
                    detail << (byte == 0 ? " " : "") << std::setw(2)
                           << std::setfill('0') << static_cast<unsigned>(record[byte]);
                }
                detail << "): " << error;
                error = detail.str();
                return false;
            };
            switch (type) {
            case 1: // R_PPC64_ADDR32
                if (!WriteU32(target, static_cast<std::uint32_t>(value), error)) {
                    return relocation_write_failed("ADDR32");
                }
                break;
            case 4: // R_PPC64_ADDR16_LO
                if (!WriteU16(target, static_cast<std::uint16_t>(value), error)) {
                    return relocation_write_failed("ADDR16_LO");
                }
                break;
            case 5: // R_PPC64_ADDR16_HI
                if (!WriteU16(target, static_cast<std::uint16_t>(value >> 16), error)) {
                    return relocation_write_failed("ADDR16_HI");
                }
                break;
            case 6: { // R_PPC64_ADDR16_HA
                const auto adjusted = (value >> 16) + ((value & 0x8000u) != 0 ? 1u : 0u);
                if (!WriteU16(target, static_cast<std::uint16_t>(adjusted), error)) {
                    return relocation_write_failed("ADDR16_HA");
                }
                break;
            }
            case 10: { // R_PPC64_REL24
                const auto displacement = static_cast<std::int64_t>(value) -
                                          static_cast<std::int64_t>(target);
                if ((displacement & 3) != 0 || displacement < -(1ll << 25) ||
                    displacement >= (1ll << 25)) {
                    error = "PS3 PRX REL24 relocation is out of range";
                    return false;
                }
                std::uint32_t instruction = 0;
                if (!ReadU32(target, instruction, error)) return false;
                instruction = (instruction & 0xfc000003u) |
                             ((static_cast<std::uint32_t>(displacement >> 2) & 0x00ffffffu) << 2);
                if (!WriteU32(target, instruction, error)) {
                    return relocation_write_failed("REL24");
                }
                break;
            }
            case 11: { // R_PPC64_REL14
                const auto displacement = static_cast<std::int64_t>(value) -
                                          static_cast<std::int64_t>(target);
                if ((displacement & 3) != 0 || displacement < -(1ll << 15) ||
                    displacement >= (1ll << 15)) {
                    error = "PS3 PRX REL14 relocation is out of range";
                    return false;
                }
                std::uint32_t instruction = 0;
                if (!ReadU32(target, instruction, error)) return false;
                instruction = (instruction & 0xffff0003u) |
                             ((static_cast<std::uint32_t>(displacement >> 2) & 0x3fffu) << 2);
                if (!WriteU32(target, instruction, error)) {
                    return relocation_write_failed("REL14");
                }
                break;
            }
            case 38: // R_PPC64_ADDR64
                if (!WriteU64(target, value, error)) {
                    return relocation_write_failed("ADDR64");
                }
                break;
            case 44: { // R_PPC64_REL64
                const auto displacement = static_cast<std::int64_t>(value) -
                                          static_cast<std::int64_t>(target);
                if (!WriteU64(target, static_cast<std::uint64_t>(displacement), error)) {
                    return relocation_write_failed("REL64");
                }
                break;
            }
            case 57: { // R_PPC64_ADDR16_LO_DS
                std::uint32_t instruction = 0;
                if (!ReadU32(target, instruction, error)) return false;
                instruction = (instruction & 0xffff0003u) |
                             ((static_cast<std::uint32_t>(value >> 2) & 0x3fffu) << 2);
                if (!WriteU32(target, instruction, error)) {
                    return relocation_write_failed("ADDR16_LO_DS");
                }
                break;
            }
            default:
                error = "PS3 PRX relocation type " + std::to_string(type) +
                        " is not implemented";
                return false;
            }
        }
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
        auto imports_start = normalize_address(read_info_u32_40(0x2c));
        const auto imports_end = normalize_address(read_info_u32_40(0x30));
        if (imports_start != 0 && imports_end > imports_start &&
            static_cast<std::uint64_t>(imports_end) - imports_start <= 0x100000) {
            for (std::size_t count = 0; imports_start < imports_end &&
                                      count < 256; ++count) {
                PrxLibraryRecord import_record{};
                if (!ReadLibraryRecord(imports_start, import_record, error)) {
                    return false;
                }
                const auto size = import_record.size == 0
                    ? static_cast<std::uint8_t>(0x2c)
                    : import_record.size;
                if (size < 0x1c || imports_start + size > imports_end) {
                    error = "PS3 PRX import table has an invalid module record";
                    return false;
                }
                if (!AddImportRecord(imports_start, normalize_address, error)) {
                    return false;
                }
                imports_start += size;
            }
        }
        auto exports_start = normalize_address(read_info_u32_40(0x24));
        const auto exports_end = normalize_address(read_info_u32_40(0x28));
        if (exports_start != 0 && exports_end > exports_start &&
            static_cast<std::uint64_t>(exports_end) - exports_start <= 0x100000) {
            for (std::size_t count = 0; exports_start < exports_end &&
                                      count < 256; ++count) {
                std::array<std::uint8_t, 0x2c> module_info{};
                if (!read_module_info(exports_start, module_info)) return false;
                const auto size = module_info[0] == 0 ? 0x2cu : module_info[0];
                if (size < 0x1c || exports_start + size > exports_end) {
                    std::ostringstream detail;
                    detail << "PS3 PRX export table has an invalid module record at 0x"
                           << std::hex << exports_start << " (size 0x"
                           << static_cast<unsigned>(size) << ", end 0x"
                           << exports_end << ", library info 0x" << library_info << ")";
                    error = detail.str();
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
                const auto toc = normalize_address(read_info_u32(module_info, 0x20));
                if ((attributes & 0x1u) != 0 &&
                    !AddExportRecord(exports_start, normalize_address, error,
                                     toc)) {
                    return false;
                }
                const auto nids = normalize_address(read_info_u32(module_info, 0x14));
                const auto addresses = normalize_address(
                    read_info_u32(module_info, 0x18));
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
                        case 0xab779874u: module.stop_entry = entry; break;
                        case 0x0d10fd3fu: module.prologue_entry = entry; break;
                        case 0x330f7005u: module.epilogue_entry = entry; break;
                        default: break;
                        }
                    }
                }
                exports_start += size;
            }
        }
    }

    LinkImports(error);
    if (!error.empty()) return false;

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
    WriteU32(tls_thread_id, current_thread_id_, error);
}

bool Ps3Lv2::EnsureThreadReturnTrampoline(std::string& error) {
    if (thread_return_trampoline_mapped_) return true;
    const auto mapped = memory_.Map({
        thread_return_trampoline_, 0x1000,
        memory::kPermissionRead | memory::kPermissionExecute});
    if (!mapped.ok()) {
        error = mapped.error;
        return false;
    }
    const std::array<std::uint8_t, 12> code{
        0x39, 0x60, 0x00, 0x29, // li r11, 41 (_sys_ppu_thread_exit)
        0x44, 0x00, 0x00, 0x02, // sc
        0x48, 0x00, 0x00, 0x00  // b . (must never resume an exited thread)
    };
    const auto initialized = memory_.Initialize(thread_return_trampoline_, code);
    if (!initialized.ok()) {
        error = initialized.error;
        return false;
    }
    thread_return_trampoline_mapped_ = true;
    return true;
}

bool Ps3Lv2::InitializeThreadContext(Object& thread, std::string& error) {
    if (thread.context_initialized) return true;
    if (!EnsureThreadReturnTrampoline(error)) return false;
    const auto stack_size = std::max<std::uint64_t>(thread.stack_size, 0x1000);
    const auto stack_address = next_thread_stack_address_;
    const auto stack_mapping = memory_.Map({
        stack_address, stack_size,
        memory::kPermissionRead | memory::kPermissionWrite});
    if (!stack_mapping.ok()) {
        error = stack_mapping.error;
        return false;
    }
    next_thread_stack_address_ += stack_size;

    constexpr std::uint64_t kTlsAreaSize = 0x8000;
    const auto tls_address = next_thread_tls_address_;
    const auto tls_mapping = memory_.Map({
        tls_address, kTlsAreaSize,
        memory::kPermissionRead | memory::kPermissionWrite});
    if (!tls_mapping.ok()) {
        error = tls_mapping.error;
        return false;
    }
    next_thread_tls_address_ += kTlsAreaSize;

    thread.stack_address = stack_address;
    thread.tls_address = tls_address;
    thread.context = {};
    thread.context.pc = thread.entry;
    thread.context.lr = thread_return_trampoline_;
    thread.context.gpr[1] = (stack_address + stack_size - 0x200) & ~0xfull;
    thread.context.gpr[2] = thread.toc;
    thread.context.gpr[3] = thread.argument;
    thread.context.gpr[4] = thread.argument2;
    thread.context.gpr[12] = thread.entry;
    thread.context.gpr[13] = tls_address + 0x7030;
    thread.context_initialized = true;
    return true;
}

bool Ps3Lv2::SwitchThread(cpu::PpuRegisters& registers,
                          bool requeue_current,
                          std::string& error) {
    if (current_thread_id_ == kBootstrapPpuThreadId) {
        bootstrap_context_ = registers;
        bootstrap_context_valid_ = true;
        if (requeue_current && !bootstrap_runnable_) {
            bootstrap_runnable_ = true;
            runnable_threads_.push_back(kBootstrapPpuThreadId);
        }
    } else if (auto* current = FindObject(current_thread_id_, ObjectType::Thread)) {
        current->context = registers;
        if (requeue_current && current->started && !current->runnable) {
            current->runnable = true;
            runnable_threads_.push_back(current_thread_id_);
        }
    }

    while (!runnable_threads_.empty()) {
        const auto next_id = runnable_threads_.front();
        runnable_threads_.pop_front();
        if (next_id == kBootstrapPpuThreadId) {
            bootstrap_runnable_ = false;
            if (!bootstrap_context_valid_) continue;
            registers = bootstrap_context_;
            current_thread_id_ = next_id;
            return true;
        }
        auto* next = FindObject(next_id, ObjectType::Thread);
        if (next == nullptr || !next->started || !next->context_initialized) continue;
        next->runnable = false;
        registers = next->context;
        current_thread_id_ = next_id;
        return true;
    }
    error = "no runnable PPU thread remains";
    return false;
}

void Ps3Lv2::MakeThreadRunnable(std::uint32_t thread_id) {
    if (thread_id == kBootstrapPpuThreadId) {
        if (bootstrap_context_valid_ && !bootstrap_runnable_ &&
            current_thread_id_ != kBootstrapPpuThreadId) {
            bootstrap_runnable_ = true;
            runnable_threads_.push_back(thread_id);
        }
        return;
    }
    auto* thread = FindObject(thread_id, ObjectType::Thread);
    if (thread == nullptr || !thread->started || thread->runnable ||
        current_thread_id_ == thread_id) {
        return;
    }
    thread->runnable = true;
    runnable_threads_.push_back(thread_id);
}

bool Ps3Lv2::SendQueueEvent(
    std::uint32_t queue_id,
    const std::array<std::uint64_t, 4>& event,
    std::string& error) {
    auto* queue = FindObject(queue_id, ObjectType::EventQueue);
    if (queue == nullptr) return false;
    if (queue->event_waiters.empty()) {
        queue->events.push_back(event);
        return true;
    }
    const auto waiter = queue->event_waiters.front();
    queue->event_waiters.pop_front();
    bool written = true;
    for (std::size_t index = 0; index < event.size(); ++index) {
        written = written && WriteU64(waiter[1] + index * 8, event[index], error);
    }
    const auto waiter_id = static_cast<std::uint32_t>(waiter[0]);
    cpu::PpuRegisters* context = nullptr;
    if (waiter_id == kBootstrapPpuThreadId) {
        context = bootstrap_context_valid_ ? &bootstrap_context_ : nullptr;
    } else if (auto* thread = FindObject(waiter_id, ObjectType::Thread)) {
        context = &thread->context;
    }
    if (!written || context == nullptr) return false;
    context->gpr[3] = kCellOk;
    context->gpr[4] = event[0];
    context->gpr[5] = event[1];
    context->gpr[6] = event[2];
    context->gpr[7] = event[3];
    MakeThreadRunnable(waiter_id);
    return true;
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
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysPpuThreadExit: {
        if (current_thread_id_ == kBootstrapPpuThreadId) {
            registers.gpr[3] = kCellOk;
        } else {
            if (auto* thread = FindObject(current_thread_id_, ObjectType::Thread)) {
                thread->started = false;
                thread->runnable = false;
            }
            registers.gpr[3] = kCellOk;
            if (!SwitchThread(registers, false, error)) return false;
        }
        handled = true;
        break;
    }
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
    case kSysPpuThreadJoin:
    case kSysPpuThreadDetach:
    case kSysPpuThreadOnce:
    case kSysPpuThreadSetPriority:
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysPpuThreadYield:
        registers.gpr[3] = kCellOk;
        handled = SwitchThread(registers, true, error);
        break;
    case kSysPpuThreadStart: {
        const auto id = registers.gpr[3];
        auto* thread = id <= UINT32_MAX
            ? FindObject(static_cast<std::uint32_t>(id), ObjectType::Thread)
            : nullptr;
        if (thread == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else {
            if (thread->started) {
                registers.gpr[3] = kCellEbusy;
            } else if (!InitializeThreadContext(*thread, error)) {
                registers.gpr[3] = kCellEnomem;
                error.clear();
            } else {
                thread->started = true;
                if (!thread->runnable) {
                    thread->runnable = true;
                    runnable_threads_.push_back(static_cast<std::uint32_t>(id));
                }
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
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
    case kSysPpuThreadCreate: {
        const auto output = registers.gpr[3];
        const auto parameter = registers.gpr[4];
        std::uint32_t descriptor = 0;
        std::uint32_t tls = 0;
        std::uint32_t entry = 0;
        std::uint32_t toc = 0;
        if (output == 0 || parameter == 0 ||
            !ReadU32(parameter, descriptor, error) ||
            !ReadU32(parameter + 4, tls, error) || descriptor == 0 ||
            !ReadU32(descriptor, entry, error) ||
            !ReadU32(descriptor + 4, toc, error) || entry == 0) {
            registers.gpr[3] = kCellEinval;
            error.clear();
            handled = true;
            break;
        }
        Object thread;
        thread.type = ObjectType::Thread;
        thread.entry = entry;
        thread.argument = registers.gpr[5];
        thread.argument2 = registers.gpr[6];
        thread.toc = toc;
        thread.tls = tls;
        thread.stack_size = registers.gpr[8] == 0 || registers.gpr[8] == UINT64_MAX
            ? 0x1000
            : (registers.gpr[8] + 0xfff) & ~0xfffull;
        const auto id = CreateObject(std::move(thread));
        if (!WriteU64(output, id, error)) {
            objects_.erase(id);
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
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
    case kSysSemaphoreCreate: {
        const auto output = registers.gpr[3];
        const auto initial = static_cast<std::int32_t>(registers.gpr[5]);
        const auto maximum = static_cast<std::int32_t>(registers.gpr[6]);
        if (output == 0 || initial < 0 || maximum <= 0 || initial > maximum) {
            registers.gpr[3] = kCellEinval;
        } else {
            Object semaphore;
            semaphore.type = ObjectType::Semaphore;
            semaphore.value = initial;
            semaphore.entry = static_cast<std::uint32_t>(maximum);
            const auto id = CreateObject(std::move(semaphore));
            if (!WriteU32(output, id, error)) {
                objects_.erase(id);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysSemaphoreDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Semaphore, error);
        break;
    case kSysSemaphoreWait:
    case kSysSemaphoreTrywait: {
        auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                  ObjectType::Semaphore);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else if (object->value <= 0) {
            if (syscall == kSysSemaphoreTrywait) {
                registers.gpr[3] = kCellEagain;
            } else {
                if (std::find(object->waiters.begin(), object->waiters.end(),
                              current_thread_id_) == object->waiters.end()) {
                    object->waiters.push_back(current_thread_id_);
                }
                registers.gpr[3] = kCellOk;
                handled = SwitchThread(registers, false, error);
                break;
            }
        }
        else { --object->value; registers.gpr[3] = kCellOk; }
        handled = true;
        break;
    }
    case kSysSemaphorePost: {
        auto* object = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                  ObjectType::Semaphore);
        if (object == nullptr) registers.gpr[3] = kCellEsrch;
        else {
            auto count = static_cast<std::uint32_t>(registers.gpr[4]);
            while (count != 0 && !object->waiters.empty()) {
                const auto waiter = object->waiters.front();
                object->waiters.pop_front();
                MakeThreadRunnable(waiter);
                --count;
            }
            const auto maximum = static_cast<std::int64_t>(object->entry);
            object->value = std::min(maximum,
                object->value + static_cast<std::int64_t>(count));
            registers.gpr[3] = kCellOk;
        }
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
    case kSysLwCondCreate:
        handled = HandleObjectCreate(registers, ObjectType::Condition, error);
        break;
    case kSysLwCondDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Condition, error);
        break;
    case kSysLwCondQueueWait:
    case kSysLwCondSignal:
    case kSysLwCondSignalAll: {
        const auto id = static_cast<std::uint32_t>(registers.gpr[3]);
        registers.gpr[3] = FindObject(id, ObjectType::Condition) == nullptr
            ? kCellEsrch : kCellOk;
        handled = true;
        break;
    }
    case kSysLwMutexUnlock2:
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
    case kSysCondCreate: {
        const auto output = registers.gpr[3];
        const auto mutex_id = static_cast<std::uint32_t>(registers.gpr[4]);
        if (output == 0 || FindObject(mutex_id, ObjectType::Mutex) == nullptr) {
            registers.gpr[3] = kCellEinval;
        } else {
            Object condition;
            condition.type = ObjectType::Condition;
            condition.event_queue_id = mutex_id;
            const auto id = CreateObject(std::move(condition));
            if (!WriteU32(output, id, error)) {
                objects_.erase(id);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysCondDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::Condition, error);
        break;
    case kSysCondWait: {
        auto* condition = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                     ObjectType::Condition);
        if (condition == nullptr) {
            registers.gpr[3] = kCellEsrch;
            handled = true;
            break;
        }
        if (auto* mutex = FindObject(condition->event_queue_id, ObjectType::Mutex)) {
            mutex->locked = false;
        }
        if (std::find(condition->waiters.begin(), condition->waiters.end(),
                      current_thread_id_) == condition->waiters.end()) {
            condition->waiters.push_back(current_thread_id_);
        }
        registers.gpr[3] = kCellOk;
        handled = SwitchThread(registers, false, error);
        break;
    }
    case kSysCondSignal:
    case kSysCondSignalAll:
    case kSysCondSignalTo: {
        auto* condition = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                     ObjectType::Condition);
        if (condition == nullptr) {
            registers.gpr[3] = kCellEsrch;
            handled = true;
            break;
        }
        const auto wake_all = syscall == kSysCondSignalAll;
        const auto target = syscall == kSysCondSignalTo
            ? static_cast<std::uint32_t>(registers.gpr[4]) : 0;
        for (auto it = condition->waiters.begin(); it != condition->waiters.end();) {
            if (target != 0 && *it != target) {
                ++it;
                continue;
            }
            const auto waiter = *it;
            it = condition->waiters.erase(it);
            MakeThreadRunnable(waiter);
            if (!wake_all) break;
        }
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    }
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
        if (FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                       ObjectType::EventQueue) == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else {
            const auto queue_id = static_cast<std::uint32_t>(registers.gpr[3]);
            objects_.erase(queue_id);
            std::erase_if(storage_medium_events_, [queue_id](const auto& entry) {
                return entry.second.queue_id == queue_id;
            });
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    case kSysEventQueueReceive:
    case kSysEventQueueTryreceive: {
        auto* queue = FindObject(
            static_cast<std::uint32_t>(registers.gpr[3]),
            ObjectType::EventQueue);
        if (queue == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else if (queue->events.empty()) {
            if (syscall == kSysEventQueueTryreceive) {
                if (registers.gpr[6] != 0) {
                    WriteU32(registers.gpr[6], 0, error);
                    error.clear();
                }
                registers.gpr[3] = kCellEagain;
            } else if (registers.gpr[4] == 0) {
                registers.gpr[3] = kCellEfault;
            } else {
                const std::array<std::uint64_t, 2> waiter{
                    current_thread_id_, registers.gpr[4]};
                if (std::find(queue->event_waiters.begin(),
                              queue->event_waiters.end(), waiter) ==
                    queue->event_waiters.end()) {
                    queue->event_waiters.push_back(waiter);
                }
                registers.gpr[3] = kCellOk;
                handled = SwitchThread(registers, false, error);
                break;
            }
        } else {
            const auto event = queue->events.front();
            const auto event_address = registers.gpr[4];
            const auto number_address = registers.gpr[6];
            bool written = event_address != 0;
            for (std::size_t index = 0; written && index < event.size(); ++index) {
                written = WriteU64(event_address + index * 8, event[index], error);
            }
            if (!written) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                queue->events.pop_front();
                if (syscall == kSysEventQueueTryreceive && number_address != 0) {
                    WriteU32(number_address, 1, error);
                    error.clear();
                }
                // sys_event_queue_receive also exposes the event in the
                // return registers; the guest wrapper copies them to its
                // destination structure after the syscall returns.
                registers.gpr[4] = event[0];
                registers.gpr[5] = event[1];
                registers.gpr[6] = event[2];
                registers.gpr[7] = event[3];
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysEventQueueDrain:
        registers.gpr[3] = FindObject(
            static_cast<std::uint32_t>(registers.gpr[3]),
            ObjectType::EventQueue) == nullptr ? kCellEsrch : kCellOk;
        handled = true;
        break;
    case kSysSpuThreadGroupCreate: {
        // VSH creates its first SPU group during system-service startup. Keep
        // the group identity stateful; actual SPU image/thread execution is a
        // separate boundary that will be added when the next call requires it.
        const auto output = registers.gpr[3];
        const auto count = registers.gpr[4];
        if (output == 0 || count == 0 || count > 6) {
            registers.gpr[3] = kCellEinval;
        } else {
            const auto handle = next_spu_group_id_++;
            spu_group_handles_.insert(handle);
            if (!WriteU32(output, handle, error)) {
                spu_group_handles_.erase(handle);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysSpuThreadGroupInitialize: {
        const auto output = registers.gpr[3];
        const auto group = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto spu_number = registers.gpr[5];
        if (output == 0 || spu_group_handles_.find(group) == spu_group_handles_.end() ||
            spu_number >= 6 || registers.gpr[6] == 0) {
            registers.gpr[3] = kCellEinval;
        } else {
            const auto handle = next_spu_thread_id_++;
            spu_thread_handles_.insert(handle);
            if (!WriteU32(output, handle, error)) {
                spu_thread_handles_.erase(handle);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysSpuThreadGroupStart: {
        const auto group = static_cast<std::uint32_t>(registers.gpr[3]);
        registers.gpr[3] = spu_group_handles_.find(group) == spu_group_handles_.end()
            ? kCellEsrch : kCellOk;
        handled = true;
        break;
    }
    case kSysSpuThreadGroupSystemBoundary: {
        // 0xf8 is a root-only SPU group operation whose public name and
        // exact argument semantics are still undocumented. VSH reaches this
        // boundary immediately after creating its first group. Keep the
        // handle check stateful and leave the operation itself side-effect
        // free until its ABI is identified from a later boot trace.
        const auto group = static_cast<std::uint32_t>(registers.gpr[3]);
        registers.gpr[3] = spu_group_handles_.find(group) == spu_group_handles_.end()
            ? kCellEsrch : kCellOk;
        handled = true;
        break;
    }
    case kSysEventPortCreate:
        handled = HandleObjectCreate(registers, ObjectType::EventPort, error);
        break;
    case kSysEventPortDestroy:
        handled = HandleObjectDestroy(registers, ObjectType::EventPort, error);
        break;
    case kSysEventPortConnectLocal:
    case kSysEventPortConnectIpc: {
        auto* port = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                ObjectType::EventPort);
        std::uint32_t queue_id = 0;
        if (syscall == kSysEventPortConnectLocal) {
            queue_id = static_cast<std::uint32_t>(registers.gpr[4]);
        } else {
            // The VSH IPC key is resolved by LV2. For this compact runtime,
            // bind it to the newest event queue created by the caller.
            for (const auto& [id, object] : objects_) {
                if (object.type == ObjectType::EventQueue && id > queue_id) {
                    queue_id = id;
                }
            }
        }
        if (port == nullptr || FindObject(queue_id, ObjectType::EventQueue) == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else {
            port->event_queue_id = queue_id;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysEventPortDisconnect: {
        auto* port = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                ObjectType::EventPort);
        if (port == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else {
            port->event_queue_id = 0;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysEventPortSend: {
        auto* port = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                ObjectType::EventPort);
        auto* queue = port == nullptr ? nullptr : FindObject(
            port->event_queue_id, ObjectType::EventQueue);
        if (port != nullptr && queue == nullptr) {
            // VSH connects the IPC port before creating the receiving queue.
            // Resolve that deferred connection when the first event is sent,
            // matching LV2's key-based rendezvous without dropping the event.
            for (const auto& [id, object] : objects_) {
                if (object.type == ObjectType::EventQueue && id > port->event_queue_id) {
                    port->event_queue_id = id;
                }
            }
            queue = FindObject(port->event_queue_id, ObjectType::EventQueue);
        }
        if (queue == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else {
            const std::array<std::uint64_t, 4> event{
                (UINT64_C(1) << 32) | registers.gpr[3],
                registers.gpr[4], registers.gpr[5], registers.gpr[6]};
            if (queue->event_waiters.empty()) {
                queue->events.push_back(event);
            } else {
                const auto waiter = queue->event_waiters.front();
                queue->event_waiters.pop_front();
                bool written = true;
                for (std::size_t index = 0; index < event.size(); ++index) {
                    written = written && WriteU64(
                        waiter[1] + index * 8, event[index], error);
                }
                const auto waiter_id = static_cast<std::uint32_t>(waiter[0]);
                cpu::PpuRegisters* context = nullptr;
                if (waiter_id == kBootstrapPpuThreadId) {
                    context = bootstrap_context_valid_ ? &bootstrap_context_ : nullptr;
                } else if (auto* thread = FindObject(waiter_id, ObjectType::Thread)) {
                    context = &thread->context;
                }
                if (written && context != nullptr) {
                    context->gpr[3] = kCellOk;
                    context->gpr[4] = event[0];
                    context->gpr[5] = event[1];
                    context->gpr[6] = event[2];
                    context->gpr[7] = event[3];
                    MakeThreadRunnable(waiter_id);
                }
                error.clear();
            }
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
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
    case kSysTimerUsleep:
    case kSysTimerSleep:
        // A sleep is also a scheduling point. Returning immediately without
        // yielding lets VSH's one-microsecond polling loop starve all worker
        // threads that are responsible for advancing the AV/RSX handshake.
        // The interpreter has no wall-clock scheduler yet, but round-robin
        // dispatch preserves the essential guest ordering without blocking
        // the iOS UI thread.
        registers.gpr[3] = kCellOk;
        handled = SwitchThread(registers, true, error);
        break;
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
    case kSysFsOpen: {
        std::string path;
        if (registers.gpr[5] == 0 ||
            !ReadCString(registers.gpr[3], path, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        while (!path.empty() && path.front() == '/') path.erase(path.begin());
        while (!path.empty() && path.back() == '/') path.pop_back();
        const std::vector<std::uint8_t>* bytes = nullptr;
        if (firmware_files_ != nullptr) {
            const auto it = firmware_files_->find(path);
            if (it != firmware_files_->end()) bytes = &it->second;
        }
        if (bytes == nullptr) {
            const auto mutable_file = mutable_files_.find(path);
            if (mutable_file != mutable_files_.end()) bytes = &mutable_file->second;
        }
        if (bytes == nullptr) {
            registers.gpr[3] = kCellEnoent;
            handled = true;
            break;
        }
        const auto handle = next_file_id_++;
        files_.emplace(handle, File{bytes, 0});
        if (!WriteU32(registers.gpr[5], handle, error)) {
            files_.erase(handle);
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysFsRead: {
        const auto file = files_.find(
            static_cast<std::uint32_t>(registers.gpr[3]));
        if (file == files_.end() || file->second.bytes == nullptr ||
            registers.gpr[4] == 0 || registers.gpr[6] == 0) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        const auto& bytes = *file->second.bytes;
        const auto available = file->second.position < bytes.size()
            ? bytes.size() - file->second.position : 0;
        const auto count = std::min<std::size_t>(
            available, static_cast<std::size_t>(registers.gpr[5]));
        const auto write = memory_.Write(
            registers.gpr[4], std::span<const std::uint8_t>(
                bytes.data() + file->second.position, count));
        if (!write.ok() || !WriteU64(registers.gpr[6], count, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            file->second.position += count;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysFsClose:
        registers.gpr[3] = files_.erase(
            static_cast<std::uint32_t>(registers.gpr[3])) == 0
            ? kCellEsrch : kCellOk;
        handled = true;
        break;
    case kSysFsOpendir: {
        std::string path;
        if (registers.gpr[4] == 0 ||
            !ReadCString(registers.gpr[3], path, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        while (!path.empty() && path.front() == '/') path.erase(path.begin());
        while (!path.empty() && path.back() == '/') path.pop_back();
        Directory directory;
        if (firmware_files_ != nullptr && !path.empty()) {
            const auto prefix = path + '/';
            for (const auto& [file_path, file] : *firmware_files_) {
                (void)file;
                if (file_path.rfind(prefix, 0) != 0) continue;
                const auto remainder = file_path.substr(prefix.size());
                const auto separator = remainder.find('/');
                const auto entry = remainder.substr(0, separator);
                if (entry.empty() ||
                    std::find(directory.entries.begin(), directory.entries.end(),
                              entry) != directory.entries.end()) {
                    continue;
                }
                directory.entries.push_back(entry);
            }
        }
        const auto prefix = path.empty() ? std::string{} : path + '/';
        for (const auto& directory_path : mutable_directories_) {
            if (directory_path.rfind(prefix, 0) != 0 || directory_path == path) continue;
            const auto remainder = directory_path.substr(prefix.size());
            const auto entry = remainder.substr(0, remainder.find('/'));
            if (!entry.empty() && std::find(directory.entries.begin(),
                                             directory.entries.end(), entry) ==
                                      directory.entries.end()) {
                directory.entries.push_back(entry);
            }
        }
        for (const auto& [file_path, data] : mutable_files_) {
            (void)data;
            if (file_path.rfind(prefix, 0) != 0) continue;
            const auto remainder = file_path.substr(prefix.size());
            const auto entry = remainder.substr(0, remainder.find('/'));
            if (!entry.empty() && std::find(directory.entries.begin(),
                                             directory.entries.end(), entry) ==
                                      directory.entries.end()) {
                directory.entries.push_back(entry);
            }
        }
        if (directory.entries.empty()) {
            registers.gpr[3] = kCellEnoent;
            handled = true;
            break;
        }
        const auto handle = next_directory_id_++;
        directories_.emplace(handle, std::move(directory));
        if (!WriteU32(registers.gpr[4], handle, error)) {
            directories_.erase(handle);
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysFsReaddir: {
        const auto directory = directories_.find(
            static_cast<std::uint32_t>(registers.gpr[3]));
        if (directory == directories_.end() || registers.gpr[4] == 0 ||
            registers.gpr[5] == 0) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        if (directory->second.position >= directory->second.entries.size()) {
            if (!WriteU64(registers.gpr[5], 0, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
            handled = true;
            break;
        }
        const auto& name = directory->second.entries[directory->second.position++];
        std::array<std::uint8_t, 0x104> dirent{};
        const auto name_length = std::min<std::size_t>(name.size(), 0xff);
        dirent[0] = 1; // CELL_FS_TYPE_REGULAR; sufficient for this read-only slice
        dirent[1] = static_cast<std::uint8_t>(name_length);
        const auto record_length = static_cast<std::uint16_t>(
            (4 + name_length + 1 + 3) & ~std::size_t{3});
        dirent[2] = static_cast<std::uint8_t>(record_length >> 8);
        dirent[3] = static_cast<std::uint8_t>(record_length);
        std::copy_n(name.begin(), name_length, dirent.begin() + 4);
        const auto write = memory_.Write(registers.gpr[4], dirent);
        if (!write.ok() || !WriteU64(registers.gpr[5], 1, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysFsClosedir: {
        const auto erased = directories_.erase(
            static_cast<std::uint32_t>(registers.gpr[3]));
        registers.gpr[3] = erased == 0 ? kCellEsrch : kCellOk;
        handled = true;
        break;
    }
    case kSysFsStat: {
        std::string path;
        if (registers.gpr[4] == 0 ||
            !ReadCString(registers.gpr[3], path, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        while (!path.empty() && path.front() == '/') path.erase(path.begin());
        while (!path.empty() && path.back() == '/') path.pop_back();
        const std::vector<std::uint8_t>* bytes = nullptr;
        bool directory = path.empty() || mutable_directories_.contains(path);
        if (const auto file = mutable_files_.find(path);
            file != mutable_files_.end()) {
            bytes = &file->second;
        }
        if (firmware_files_ != nullptr) {
            if (const auto file = firmware_files_->find(path);
                file != firmware_files_->end()) {
                bytes = &file->second;
            } else {
                const auto prefix = path.empty() ? std::string{} : path + '/';
                directory = directory || std::any_of(
                    firmware_files_->begin(), firmware_files_->end(),
                    [&prefix](const auto& item) {
                        return item.first.rfind(prefix, 0) == 0;
                    });
            }
        }
        if (bytes == nullptr && !directory) {
            registers.gpr[3] = kCellEnoent;
            handled = true;
            break;
        }
        // CellFsStat is a packed, big-endian 52-byte structure.
        const auto output = registers.gpr[4];
        const auto mode = directory ? (0040000u | 0711u) : (0100000u | 0644u);
        const auto size = directory ? std::uint64_t{258}
                                    : static_cast<std::uint64_t>(bytes->size());
        if (!WriteU32(output, mode, error) ||
            !WriteU32(output + 4, UINT32_MAX, error) ||
            !WriteU32(output + 8, UINT32_MAX, error) ||
            !WriteU64(output + 12, 0, error) ||
            !WriteU64(output + 20, 0, error) ||
            !WriteU64(output + 28, 0, error) ||
            !WriteU64(output + 36, size, error) ||
            !WriteU64(output + 44, 512, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysFsMkdir: {
        std::string path;
        if (!ReadCString(registers.gpr[3], path, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        while (!path.empty() && path.front() == '/') path.erase(path.begin());
        while (!path.empty() && path.back() == '/') path.pop_back();
        if (path.empty()) {
            registers.gpr[3] = kCellEinval;
        } else if (mutable_directories_.contains(path) ||
                   mutable_files_.contains(path) ||
                   (firmware_files_ != nullptr && firmware_files_->contains(path))) {
            registers.gpr[3] = kCellEexist;
        } else {
            const auto separator = path.rfind('/');
            const auto parent = separator == std::string::npos
                ? std::string{} : path.substr(0, separator);
            bool parent_exists = parent.empty() ||
                                 mutable_directories_.contains(parent);
            if (!parent_exists && firmware_files_ != nullptr) {
                const auto prefix = parent + '/';
                parent_exists = std::any_of(
                    firmware_files_->begin(), firmware_files_->end(),
                    [&prefix](const auto& item) {
                        return item.first.rfind(prefix, 0) == 0;
                    });
            }
            if (!parent_exists) {
                registers.gpr[3] = kCellEnoent;
            } else {
                mutable_directories_.insert(path);
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysFsFcntl: {
        // VSH uses this command to query a filesystem's mount identifier.
        // The public LV2 ABI reports an unsupported mount through the output
        // record while the syscall itself succeeds.
        constexpr std::uint32_t kGetMountId = 0xc0000006u;
        const auto operation = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto argument = registers.gpr[5];
        if (operation != kGetMountId) {
            registers.gpr[3] = kCellEnosys;
            handled = true;
            break;
        }
        std::uint32_t argument_size = 0;
        std::uint32_t x4 = 0;
        std::uint32_t x8 = 0;
        std::uint32_t name_size = 0;
        std::uint32_t name_address = 0;
        if (registers.gpr[6] != 0x20 || argument == 0 ||
            !ReadU32(argument, argument_size, error) ||
            !ReadU32(argument + 4, x4, error) ||
            !ReadU32(argument + 8, x8, error) ||
            !ReadU32(argument + 0x0c, name_size, error) ||
            !ReadU32(argument + 0x10, name_address, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else if (argument_size != 0x20 || x4 != 0x10 || x8 != 0x18 ||
                   name_size > 0x420 || (name_size != 0 && name_address == 0)) {
            registers.gpr[3] = kCellEinval;
        } else {
            std::vector<std::uint8_t> name(name_size);
            const auto read = name.empty()
                ? memory::MemoryResult{}
                : memory_.Read(name_address, name);
            if (!read.ok() ||
                !WriteU32(argument + 0x18,
                          static_cast<std::uint32_t>(kCellEnotsup), error) ||
                !WriteU32(argument + 0x1c, 0, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysFsLseek: {
        const auto file = files_.find(
            static_cast<std::uint32_t>(registers.gpr[3]));
        if (file == files_.end() || file->second.bytes == nullptr) {
            registers.gpr[3] = kCellEsrch;
            handled = true;
            break;
        }
        const auto offset = static_cast<std::int64_t>(registers.gpr[4]);
        const auto whence = static_cast<std::uint32_t>(registers.gpr[5]);
        std::uint64_t base = 0;
        if (whence == 1) {
            base = file->second.position;
        } else if (whence == 2) {
            base = file->second.bytes->size();
        } else if (whence != 0) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        std::uint64_t position = 0;
        if ((offset < 0 && static_cast<std::uint64_t>(-(offset + 1)) + 1 > base) ||
            (offset >= 0 && static_cast<std::uint64_t>(offset) >
                                UINT64_MAX - base)) {
            registers.gpr[3] = kCellEinval;
        } else {
            position = offset < 0
                ? base - (static_cast<std::uint64_t>(-(offset + 1)) + 1)
                : base + static_cast<std::uint64_t>(offset);
            if (registers.gpr[6] == 0 ||
                !WriteU64(registers.gpr[6], position, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                file->second.position = static_cast<std::size_t>(position);
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysFsMount:
        // The first VSH pass only establishes a firmware mount boundary. The
        // extracted PUP is already exposed through firmware_files_, so no
        // host mount is needed at this layer.
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysMmapperFreeSharedMemory: {
        const auto memory = shared_memories_.find(
            static_cast<std::uint32_t>(registers.gpr[3]));
        if (memory == shared_memories_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (memory->second.mapped) {
            registers.gpr[3] = kCellEbusy;
        } else {
            shared_memories_.erase(memory);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysMmapperAllocateAddress: {
        const auto size = registers.gpr[3];
        const auto requested_alignment = registers.gpr[5];
        const auto alignment = requested_alignment == 0 ? 0x1000ull
                                                        : requested_alignment;
        if (size == 0 || registers.gpr[6] == 0 ||
            (alignment & (alignment - 1)) != 0) {
            registers.gpr[3] = kCellEinval;
            handled = true;
            break;
        }
        auto address = (next_mmapper_address_ + alignment - 1) &
                       ~(alignment - 1);
        const auto overlaps = [&](std::uint64_t start, std::uint64_t length,
                                  std::uint64_t other_start,
                                  std::uint64_t other_length) {
            return start < other_start + other_length &&
                   other_start < start + length;
        };
        bool found = false;
        for (std::size_t attempt = 0; attempt < 4096; ++attempt) {
            if (address > UINT32_MAX || size > 0x100000000ull - address) break;
            std::uint64_t next_candidate = address;
            for (const auto& mapping : memory_.Mappings()) {
                if (overlaps(address, size, mapping.guest_address,
                             mapping.size)) {
                    next_candidate = std::max(next_candidate,
                                              mapping.guest_address + mapping.size);
                }
            }
            for (const auto& [reserved_address, reserved_size] :
                 mmapper_reservations_) {
                if (overlaps(address, size, reserved_address, reserved_size)) {
                    next_candidate = std::max(next_candidate,
                                              static_cast<std::uint64_t>(reserved_address) +
                                                  reserved_size);
                }
            }
            if (next_candidate == address) {
                found = true;
                break;
            }
            address = (next_candidate + alignment - 1) & ~(alignment - 1);
        }
        if (!found) {
            registers.gpr[3] = kCellEnomem;
        } else if (!WriteU32(registers.gpr[6], static_cast<std::uint32_t>(address), error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            mmapper_reservations_.emplace(
                static_cast<std::uint32_t>(address), size);
            next_mmapper_address_ = address + size;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysMmapperFreeAddress: {
        const auto address = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto reservation = mmapper_reservations_.find(address);
        const auto in_use = std::any_of(
            shared_memories_.begin(), shared_memories_.end(),
            [address](const auto& entry) {
                return entry.second.mapped &&
                       entry.second.mapped_address == address;
            });
        if (reservation == mmapper_reservations_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (in_use) {
            registers.gpr[3] = kCellEbusy;
        } else {
            mmapper_reservations_.erase(reservation);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysMmapperAllocateSharedMemory: {
        const auto size = registers.gpr[4];
        if (size == 0 || registers.gpr[6] == 0) {
            registers.gpr[3] = kCellEinval;
        } else {
            const auto handle = next_shared_memory_id_++;
            shared_memories_.emplace(
                handle, SharedMemory{size, registers.gpr[5], 0, false});
            if (!WriteU32(registers.gpr[6], handle, error)) {
                shared_memories_.erase(handle);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysMmapperAllocateSharedMemoryExt: {
        // The extended form carries a page-layout table in r6/r7 and returns
        // the shared-memory handle through r8.  The first VSH allocation uses
        // the table to describe the 64 KiB pages but does not require the
        // entries to be rewritten by LV2, so retain the allocation state and
        // let the existing GuestMemory mapping back it.
        const auto size = registers.gpr[4];
        const auto entry_pointer = registers.gpr[6];
        const auto entry_count = registers.gpr[7];
        const auto output_pointer = registers.gpr[8];
        if (size == 0 || output_pointer == 0 ||
            (entry_count != 0 && entry_pointer == 0) ||
            entry_count > 0x1000) {
            registers.gpr[3] = kCellEinval;
        } else {
            const auto handle = next_shared_memory_id_++;
            shared_memories_.emplace(
                handle, SharedMemory{size, registers.gpr[5], 0, false});
            if (!WriteU32(output_pointer, handle, error)) {
                shared_memories_.erase(handle);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysMmapperMapSharedMemory: {
        const auto address = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto memory = shared_memories_.find(
            static_cast<std::uint32_t>(registers.gpr[4]));
        if (memory == shared_memories_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (memory->second.mapped) {
            registers.gpr[3] = kCellEbusy;
        } else {
            const auto reservation = std::find_if(
                mmapper_reservations_.begin(), mmapper_reservations_.end(),
                [address, &memory](const auto& item) {
                    const auto start = static_cast<std::uint64_t>(item.first);
                    const auto target = static_cast<std::uint64_t>(address);
                    return target >= start && target - start <= item.second &&
                           memory->second.size <= item.second - (target - start);
                });
            const auto page_flags = memory->second.flags & 0xf00;
            const auto alignment = page_flags == 0x100 ? UINT64_C(0x1000)
                : page_flags == 0x200 ? UINT64_C(0x10000)
                : page_flags == 0x400 ? UINT64_C(0x100000) : UINT64_C(0);
            if (reservation == mmapper_reservations_.end() ||
                alignment == 0 || address % alignment != 0) {
                registers.gpr[3] = kCellEinval;
            } else {
                const auto mapped = memory_.Map({
                    address, memory->second.size,
                    memory::kPermissionRead | memory::kPermissionWrite});
                if (!mapped.ok()) {
                    registers.gpr[3] = kCellEbusy;
                } else {
                    memory->second.mapped = true;
                    memory->second.mapped_address = address;
                    registers.gpr[3] = kCellOk;
                }
            }
        }
        handled = true;
        break;
    }
    case kSysMmapperSearchAndMap: {
        const auto memory = shared_memories_.find(
            static_cast<std::uint32_t>(registers.gpr[4]));
        if (memory == shared_memories_.end() || registers.gpr[6] == 0) {
            registers.gpr[3] = memory == shared_memories_.end()
                ? kCellEsrch : kCellEfault;
        } else if (memory->second.mapped) {
            registers.gpr[3] = kCellEbusy;
        } else {
            const auto address = static_cast<std::uint32_t>(registers.gpr[3]);
            const auto reserved = mmapper_reservations_.find(address);
            if (reserved == mmapper_reservations_.end() ||
                reserved->second < memory->second.size) {
                registers.gpr[3] = kCellEinval;
            } else {
                const auto mapped = memory_.Map({
                    address, memory->second.size,
                    memory::kPermissionRead | memory::kPermissionWrite});
                if (!mapped.ok()) {
                    registers.gpr[3] = kCellEnomem;
                    handled = true;
                    break;
                }
                if (!WriteU32(registers.gpr[6], address, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
                } else {
                    memory->second.mapped = true;
                    memory->second.mapped_address = address;
                    registers.gpr[3] = kCellOk;
                }
            }
        }
        handled = true;
        break;
    }
    case kSysSmInitialize:
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    case kSysUartInitialize:
        // VSH opens the privileged virtual UART before its PS3AV display
        // handshake. This establishes state only; receive/send are separate
        // packet boundaries and must not be silently treated as successful.
        if (uart_initialized_) {
            registers.gpr[3] = kCellEperm;
        } else {
            uart_initialized_ = true;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    case kSysStorageOpen: {
        const auto device = registers.gpr[3];
        const auto output = registers.gpr[5];
        if (device == 0 || std::find(kStorageDeviceIds.begin(),
                                     kStorageDeviceIds.end(), device) ==
                               kStorageDeviceIds.end()) {
            registers.gpr[3] = kCellEnoent;
        } else if (output == 0) {
            registers.gpr[3] = kCellEfault;
        } else {
            const auto handle = next_storage_handle_++;
            if (!WriteU32(output, handle, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                storage_handles_.emplace(
                    handle, StorageHandle{device, registers.gpr[4], registers.gpr[6]});
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysStorageClose: {
        const auto handle = static_cast<std::uint32_t>(registers.gpr[3]);
        if (storage_handles_.erase(handle) == 0) {
            registers.gpr[3] = kCellEsrch;
        } else {
            storage_medium_events_.erase(handle);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysStorageGetDeviceInfo: {
        const auto device = registers.gpr[3];
        const auto output = registers.gpr[4];
        if (output == 0) {
            registers.gpr[3] = kCellEfault;
            handled = true;
            break;
        }
        std::array<std::uint8_t, 0x40> info{};
        constexpr std::array<std::uint8_t, 7> name{'u', 'n', 'n', 'a', 'm', 'e', 'd'};
        std::copy(name.begin(), name.end(), info.begin());
        const auto write_u32 = [&info](std::size_t offset, std::uint32_t value) {
            info[offset] = static_cast<std::uint8_t>(value >> 24);
            info[offset + 1] = static_cast<std::uint8_t>(value >> 16);
            info[offset + 2] = static_cast<std::uint8_t>(value >> 8);
            info[offset + 3] = static_cast<std::uint8_t>(value);
        };
        const auto write_u64 = [&info](std::size_t offset, std::uint64_t value) {
            for (std::size_t index = 0; index < 8; ++index) {
                info[offset + index] = static_cast<std::uint8_t>(
                    value >> ((7 - index) * 8));
            }
        };
        const auto storage = device & kStorageBaseMask;
        const auto number = static_cast<std::uint32_t>((device >> 32) & 0xff);
        std::uint64_t sectors = 0;
        std::uint32_t sector_size = 0;
        bool writable = false;
        bool known = true;
        if (storage == kStorageAtaHdd && number <= 2) {
            constexpr std::array<std::uint64_t, 3> sizes{
                0x2542eab0ull, 0x24faea98ull, 0x003ffff8ull};
            sectors = sizes[number];
            sector_size = 0x200;
            writable = true;
        } else if (storage == kStorageBdvd && number == 0) {
            sectors = 0x4d955;
            sector_size = 0x800;
        } else if (storage == kStorageUsb && number == 0) {
            sector_size = 0x200;
        } else if (storage == kStorageBuiltinFlash && number <= 6) {
            constexpr std::array<std::uint64_t, 7> sizes{
                0x80000, 0x75f8, 0x63e00, 0x8000, 0x400, 0x2000, 0x200};
            sectors = sizes[number];
            sector_size = 0x200;
            writable = true;
        } else if (storage == kStorageNorFlash && number <= 3) {
            constexpr std::array<std::uint64_t, 4> sizes{
                0x8000, 0x77f8, 0x100, 0x400};
            sectors = sizes[number];
            sector_size = 0x200;
        } else if (storage == kStorageNandUnknown && number == 0) {
            sectors = 0x7fffffffull;
            sector_size = 0x800;
        } else {
            known = false;
        }
        if (!known) {
            registers.gpr[3] = kCellEinval;
        } else {
            write_u64(0x28, sectors);
            write_u32(0x30, sector_size);
            write_u32(0x34, 1);
            info[0x39] = writable ? 1 : 0;
            info[0x3a] = 1;
            info[0x3f] = 1;
            const auto result = memory_.Write(output, info);
            registers.gpr[3] = result.ok() ? kCellOk : kCellEfault;
        }
        handled = true;
        break;
    }
    case kSysStorageGetDeviceConfig: {
        if (registers.gpr[3] == 0 || registers.gpr[4] == 0) {
            registers.gpr[3] = kCellEfault;
        } else if (!WriteU32(registers.gpr[3], kStorageClassCount, error) ||
                   !WriteU32(registers.gpr[4],
                             static_cast<std::uint32_t>(kStorageDeviceIds.size()),
                             error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysStorageReportDevices: {
        const auto start = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto count = static_cast<std::uint32_t>(registers.gpr[5]);
        const auto output = registers.gpr[6];
        if (count == 0 || start >= kStorageDeviceIds.size() ||
            count > kStorageDeviceIds.size() - start) {
            registers.gpr[3] = kCellEinval;
        } else if (output == 0) {
            registers.gpr[3] = kCellEfault;
        } else {
            bool written = true;
            for (std::uint32_t index = 0; index < count; ++index) {
                written = written && WriteU64(output + index * 8,
                                               kStorageDeviceIds[start + index],
                                               error);
            }
            registers.gpr[3] = written ? kCellOk : kCellEfault;
            error.clear();
        }
        handled = true;
        break;
    }
    case kSysStorageConfigureMediumEvent: {
        const auto file_descriptor = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto queue_id = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto source = static_cast<std::uint32_t>(registers.gpr[5]);
        if (FindObject(queue_id, ObjectType::EventQueue) == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else {
            storage_medium_events_[file_descriptor] = {queue_id, source};
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysConfigOpen: {
        // sys_config_open binds the system-config service to an existing LV2
        // event queue.  Keep this boundary stateful so VSH can use the handle
        // for later service operations without turning the syscall into a
        // blanket success stub.
        const auto queue = FindObject(static_cast<std::uint32_t>(registers.gpr[3]),
                                      ObjectType::EventQueue);
        if (queue == nullptr) {
            registers.gpr[3] = kCellEsrch;
        } else if (registers.gpr[4] == 0) {
            registers.gpr[3] = kCellEinval;
        } else {
            const auto handle = next_config_id_++;
            config_handles_.emplace(handle, ConfigHandle{
                static_cast<std::uint32_t>(registers.gpr[3])});
            if (!WriteU32(registers.gpr[4], handle, error)) {
                config_handles_.erase(handle);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysConfigClose: {
        const auto config_id = static_cast<std::uint32_t>(registers.gpr[3]);
        if (config_handles_.erase(config_id) == 0) {
            registers.gpr[3] = kCellEsrch;
            handled = true;
            break;
        }
        std::erase_if(config_listeners_, [config_id](const auto& item) {
            return item.second.config_id == config_id;
        });
        std::erase_if(config_services_, [config_id](const auto& item) {
            return item.second.config_id == config_id;
        });
        std::erase_if(config_events_, [config_id](const auto& item) {
            return item.second.config_id == config_id;
        });
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    }
    case kSysConfigGetServiceEvent: {
        const auto config_id = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto event_id = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto found = config_events_.find(event_id);
        if (config_handles_.find(config_id) == config_handles_.end() ||
            found == config_events_.end() || found->second.config_id != config_id) {
            registers.gpr[3] = kCellEsrch;
            handled = true;
            break;
        }
        const auto& event = found->second;
        const auto required = std::uint64_t{40} + event.service.data.size();
        if (registers.gpr[6] < required) {
            registers.gpr[3] = kCellEagain;
        } else if (registers.gpr[5] == 0 ||
                   !WriteU32(registers.gpr[5], event.listener_id, error) ||
                   !WriteU32(registers.gpr[5] + 4,
                             event.service.registered ? 1 : 0, error) ||
                   !WriteU64(registers.gpr[5] + 8, event.service.service_id, error) ||
                   !WriteU64(registers.gpr[5] + 16, event.service.user_id, error) ||
                   !WriteU64(registers.gpr[5] + 24,
                             event.service.registered ? event.service.verbosity : 0,
                             error) ||
                   !WriteU32(registers.gpr[5] + 32,
                             event.service.registered
                                 ? static_cast<std::uint32_t>(event.service.data.size())
                                 : 0, error) ||
                   !WriteU32(registers.gpr[5] + 36, 0, error) ||
                   (event.service.registered && !event.service.data.empty() &&
                    !memory_.Write(registers.gpr[5] + 40,
                                   event.service.data).ok())) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysConfigAddServiceListener: {
        const auto config_id = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto size = registers.gpr[7];
        const auto type = static_cast<std::uint32_t>(registers.gpr[8]);
        if (config_handles_.find(config_id) == config_handles_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (registers.gpr[9] == 0 || size > 0x100000 || type > 1 ||
                   (size != 0 && registers.gpr[6] == 0)) {
            registers.gpr[3] = kCellEinval;
        } else {
            ConfigListener listener;
            listener.config_id = config_id;
            listener.service_id = registers.gpr[4];
            listener.min_verbosity = registers.gpr[5];
            listener.type = type;
            listener.data.resize(static_cast<std::size_t>(size));
            if ((!listener.data.empty() &&
                 !memory_.Read(registers.gpr[6], listener.data).ok())) {
                registers.gpr[3] = kCellEfault;
            } else {
                const auto id = next_config_listener_id_;
                next_config_listener_id_ += 0x100;
                config_listeners_.emplace(id, std::move(listener));
                if (!WriteU32(registers.gpr[9], id, error)) {
                    config_listeners_.erase(id);
                    registers.gpr[3] = kCellEfault;
                    error.clear();
                } else {
                    registers.gpr[3] = kCellOk;
                }
            }
        }
        handled = true;
        break;
    }
    case kSysConfigRemoveServiceListener: {
        const auto config_id = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto listener_id = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto listener = config_listeners_.find(listener_id);
        if (config_handles_.find(config_id) == config_handles_.end() ||
            listener == config_listeners_.end() ||
            listener->second.config_id != config_id) {
            registers.gpr[3] = kCellEsrch;
        } else {
            config_listeners_.erase(listener);
            std::erase_if(config_events_, [listener_id](const auto& item) {
                return item.second.listener_id == listener_id;
            });
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysConfigRegisterService: {
        const auto config_id = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto config = config_handles_.find(config_id);
        const auto size = registers.gpr[8];
        if (config == config_handles_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (registers.gpr[9] == 0 || size > 0x100000 ||
                   (size != 0 && registers.gpr[7] == 0)) {
            registers.gpr[3] = kCellEinval;
        } else {
            ConfigService service;
            service.config_id = config_id;
            service.service_id = registers.gpr[4];
            service.user_id = registers.gpr[5];
            service.verbosity = registers.gpr[6];
            service.data.resize(static_cast<std::size_t>(size));
            if (!service.data.empty() &&
                !memory_.Read(registers.gpr[7], service.data).ok()) {
                registers.gpr[3] = kCellEfault;
            } else {
                const auto service_handle = next_config_service_id_;
                next_config_service_id_ += 0x100;
                config_services_.emplace(service_handle, service);
                bool notification_ok = true;
                for (const auto& [listener_id, listener] : config_listeners_) {
                    if (listener.service_id != service.service_id ||
                        listener.min_verbosity > service.verbosity) continue;
                    if (listener.type == 0 && std::any_of(
                            config_events_.begin(), config_events_.end(),
                            [listener_id](const auto& item) {
                                return item.second.listener_id == listener_id;
                            })) continue;
                    const auto event_id = next_config_event_id_++;
                    config_events_.emplace(event_id, ConfigEvent{
                        listener.config_id, listener_id, service});
                    const auto listener_config = config_handles_.find(listener.config_id);
                    const std::array<std::uint64_t, 4> event{
                        1, listener.config_id,
                        (UINT64_C(1) << 32) | event_id,
                        40 + service.data.size()};
                    if (listener_config == config_handles_.end() ||
                        !SendQueueEvent(listener_config->second.queue_id,
                                        event, error)) {
                        notification_ok = false;
                        break;
                    }
                }
                if (!notification_ok ||
                    !WriteU32(registers.gpr[9], service_handle, error)) {
                    config_services_.erase(service_handle);
                    registers.gpr[3] = kCellEfault;
                    error.clear();
                } else {
                    registers.gpr[3] = kCellOk;
                }
            }
        }
        handled = true;
        break;
    }
    case kSysConfigUnregisterService: {
        const auto config_id = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto service_id = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto service = config_services_.find(service_id);
        if (config_handles_.find(config_id) == config_handles_.end() ||
            service == config_services_.end() ||
            service->second.config_id != config_id) {
            registers.gpr[3] = kCellEsrch;
        } else {
            service->second.registered = false;
            config_services_.erase(service);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysSsApplianceInfoManager: {
        // VSH uses the appliance information manager before it loads its
        // system profile. These are the retail values used by RPCS3's LV2
        // implementation; retaining the exact byte layouts matters because
        // callers consume them as opaque hardware identifiers.
        const auto code = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto buffer = registers.gpr[4];
        if (buffer == 0) {
            registers.gpr[3] = kCellEfault;
            handled = true;
            break;
        }
        std::vector<std::uint8_t> payload;
        switch (code) {
        case 0x19002: // AIM_get_device_type
            payload = {0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x89};
            break;
        case 0x19003: // AIM_get_device_id / IDPS
            payload = {0x00, 0x00, 0x00, 0x01,
                       0x00, 0x89, 0x00, 0x0b,
                       0x14, 0x00, 0xef, 0xdd,
                       0xca, 0x25, 0x52, 0x66};
            break;
        case 0x19004: // AIM_get_ps_code
            payload = {0x00, 0x01, 0x00, 0x85,
                       0x00, 0x07, 0x00, 0x04};
            break;
        case 0x19005: // AIM_get_open_ps_id
            // A stable zero OpenPSID expresses an unprovisioned retail
            // profile. It is sufficient for the VSH-first boot path and is
            // intentionally not derived from a host identifier.
            payload.assign(16, 0);
            break;
        default:
            // Unknown AIM codes are intentionally accepted like RPCS3; the
            // caller owns the buffer layout and the syscall itself is not a
            // blanket memory-writing stub.
            break;
        }
        if (!payload.empty() && !memory_.Write(buffer, payload).ok()) {
            registers.gpr[3] = kCellEfault;
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysSsGetCacheOfProductMode: {
        // Retail LV2 caches one byte: 0/1 when product mode is known and
        // 0xff when the underlying hypervisor query is unavailable.
        const std::array<std::uint8_t, 1> product_mode{0xff};
        if (registers.gpr[3] == 0) {
            registers.gpr[3] = kCellEinval;
        } else if (!memory_.Write(registers.gpr[3], product_mode).ok()) {
            registers.gpr[3] = kCellEfault;
        } else {
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysUsbdInitialize:
        // RPCS3 exposes the same stable CellUsbd handle used by its
        // sys_usbd_initialize HLE.  The actual USB device model is a separate
        // boundary; for the VSH boot path we only establish the handle and
        // retain initialization state.
        if (registers.gpr[3] == 0) {
            registers.gpr[3] = kCellEfault;
        } else if (usbd_initialized_) {
            registers.gpr[3] = kCellEbusy;
        } else if (!WriteU32(registers.gpr[3], 0x115b, error)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            usbd_initialized_ = true;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    case kSysUsbdFinalize: {
        constexpr std::uint32_t kUsbdHandle = 0x115b;
        if (!usbd_initialized_ || registers.gpr[3] != kUsbdHandle) {
            registers.gpr[3] = kCellEinval;
        } else {
            usbd_initialized_ = false;
            while (!usb_event_waiters_.empty()) {
                const auto waiter = usb_event_waiters_.front();
                usb_event_waiters_.pop_front();
                WriteU64(waiter.outputs[0], 4, error);
                WriteU64(waiter.outputs[1], 0, error);
                WriteU64(waiter.outputs[2], 0, error);
                error.clear();
                MakeThreadRunnable(waiter.thread_id);
            }
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysUsbdGetDeviceList: {
        constexpr std::uint32_t kUsbdHandle = 0x115b;
        if (!usbd_initialized_ || registers.gpr[3] != kUsbdHandle) {
            registers.gpr[3] = kCellEinval;
        } else if (registers.gpr[5] != 0 && registers.gpr[4] == 0) {
            registers.gpr[3] = kCellEfault;
        } else {
            // No host USB devices are exposed to the first VSH boot slice.
            // Clear the requested internal-device array and return the real
            // number of discovered devices (zero), matching sys_usbd's
            // count-as-return-value ABI.
            const auto byte_count = registers.gpr[5] * 4;
            std::vector<std::uint8_t> devices(static_cast<std::size_t>(byte_count), 0);
            const auto result = byte_count == 0
                ? memory::MemoryResult{}
                : memory_.Write(registers.gpr[4], devices);
            registers.gpr[3] = result.ok() ? 0 : kCellEfault;
            error.clear();
        }
        handled = true;
        break;
    }
    case kSysUsbdRegisterLdd:
    case kSysUsbdUnregisterLdd: {
        constexpr std::uint32_t kUsbdHandle = 0x115b;
        // RPCS3 keeps this registration as a product-name filter. There are
        // no host USB devices in the first VSH slice, so retain the handle
        // boundary and let the empty device list remain deterministic.
        registers.gpr[3] = usbd_initialized_ &&
                           registers.gpr[3] == kUsbdHandle
            ? kCellOk : kCellEinval;
        handled = true;
        break;
    }
    case kSysUsbdReceiveEvent: {
        constexpr std::uint32_t kUsbdHandle = 0x115b;
        const std::array<std::uint64_t, 3> outputs{
            registers.gpr[4], registers.gpr[5], registers.gpr[6]};
        if (!usbd_initialized_ || registers.gpr[3] != kUsbdHandle) {
            registers.gpr[3] = kCellEinval;
        } else if (outputs[0] == 0 || outputs[1] == 0 || outputs[2] == 0) {
            registers.gpr[3] = kCellEfault;
        } else if (!usb_events_.empty()) {
            const auto event = usb_events_.front();
            usb_events_.pop_front();
            const auto written = WriteU64(outputs[0], event[0], error) &&
                                 WriteU64(outputs[1], event[1], error) &&
                                 WriteU64(outputs[2], event[2], error);
            registers.gpr[3] = written ? kCellOk : kCellEfault;
            error.clear();
        } else {
            const auto duplicate = std::find_if(
                usb_event_waiters_.begin(), usb_event_waiters_.end(),
                [this](const auto& waiter) {
                    return waiter.thread_id == current_thread_id_;
                });
            if (duplicate == usb_event_waiters_.end()) {
                usb_event_waiters_.push_back({current_thread_id_, outputs});
            }
            registers.gpr[3] = kCellOk;
            handled = SwitchThread(registers, false, error);
            break;
        }
        handled = true;
        break;
    }
    case kSysUsbdEventPortSend: {
        constexpr std::uint32_t kUsbdHandle = 0x115b;
        const std::array<std::uint64_t, 3> event{
            registers.gpr[4], registers.gpr[5], registers.gpr[6]};
        if (!usbd_initialized_ || registers.gpr[3] != kUsbdHandle) {
            registers.gpr[3] = kCellEinval;
        } else if (!usb_event_waiters_.empty()) {
            const auto waiter = usb_event_waiters_.front();
            usb_event_waiters_.pop_front();
            const auto written = WriteU64(waiter.outputs[0], event[0], error) &&
                                 WriteU64(waiter.outputs[1], event[1], error) &&
                                 WriteU64(waiter.outputs[2], event[2], error);
            if (written) MakeThreadRunnable(waiter.thread_id);
            registers.gpr[3] = written ? kCellOk : kCellEfault;
            error.clear();
        } else {
            usb_events_.push_back(event);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysGl819Probe:
        // Firmware 4.93 exposes this controller slot without a public
        // operation name. VSH only uses it as an optional hardware probe;
        // report an empty-but-successful probe so boot can continue while no
        // card-reader device is attached to the emulator.
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
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

        const auto register_range = [&](std::uint64_t start,
                                        std::uint32_t size,
                                        bool imports) {
            if (size == 0) return true;
            if (start == 0 || size > 0x100000u ||
                start > std::numeric_limits<std::uint64_t>::max() - size) {
                error = "PS3 PRX registration range is invalid";
                return false;
            }
            const auto end = start + size;
            for (std::size_t count = 0; start < end && count < 4096; ++count) {
                PrxLibraryRecord record{};
                if (!ReadLibraryRecord(start, record, error)) return false;
                const auto record_size = record.size == 0
                    ? static_cast<std::uint32_t>(0x2c)
                    : static_cast<std::uint32_t>(record.size);
                if (record_size < 0x1c || record_size > end - start) {
                    error = "PS3 PRX registration contains a truncated library record";
                    return false;
                }
                const auto identity = [](std::uint32_t address) {
                    return address;
                };
                if (imports) {
                    if (!AddImportRecord(start, identity, error)) return false;
                } else if (!AddExportRecord(start, identity, error, main_toc_)) {
                    return false;
                }
                start += record_size;
            }
            if (start != end) {
                error = "PS3 PRX registration range has too many records";
                return false;
            }
            return true;
        };
        if (!register_range(module.library_entries,
                            module.library_entries_size, false) ||
            !register_range(module.library_stubs,
                            module.library_stubs_size, true)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        LinkImports(error);
        if (!error.empty()) {
            registers.gpr[3] = kCellEfault;
            error.clear();
            handled = true;
            break;
        }
        registered_prx_modules_.push_back(module);
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
    }
    case kSysPrxRegisterLibrary: {
        if (registers.gpr[3] == 0 ||
            !AddExportRecord(registers.gpr[3],
                             [](std::uint32_t address) { return address; },
                             error, main_toc_)) {
            registers.gpr[3] = kCellEfault;
            error.clear();
        } else {
            LinkImports(error);
            if (error.empty()) {
                registers.gpr[3] = kCellOk;
            } else {
                registers.gpr[3] = kCellEfault;
                error.clear();
            }
        }
        handled = true;
        break;
    }
    case kSysPrxLoadModule:
    case kSysPrxLoadModuleOnMemcontainer: {
        last_prx_load_error_.clear();
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
            last_prx_load_error_ = error;
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
    case kSysPrxStopModule: {
        const auto id = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto loaded = std::find_if(
            loaded_prx_modules_.begin(), loaded_prx_modules_.end(),
            [id](const LoadedPrx& module) { return module.id == id; });
        if (loaded == loaded_prx_modules_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (registers.gpr[5] != 0) {
            std::uint64_t option_size = 0;
            std::uint64_t command = 0;
            if (!ReadU64(registers.gpr[5], option_size, error) ||
                !ReadU64(registers.gpr[5] + 0x08, command, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else if (option_size < 0x18) {
                registers.gpr[3] = kCellEinval;
            } else if ((command & 0xfu) == 1) {
                const auto descriptor_for = [&](std::uint32_t entry) {
                    if (entry == 0) return ~std::uint64_t{0};
                    const auto descriptor = EnsureFunctionDescriptor(
                        entry, main_toc_, error);
                    return descriptor == 0
                        ? ~std::uint64_t{0}
                        : static_cast<std::uint64_t>(descriptor);
                };
                const auto stop_entry = descriptor_for(loaded->stop_entry);
                const auto epilogue_entry = descriptor_for(loaded->epilogue_entry);
                if (!error.empty() ||
                    !WriteU64(registers.gpr[5] + 0x10, stop_entry, error) ||
                    (option_size != 0x20 &&
                     !WriteU64(registers.gpr[5] + 0x20, epilogue_entry, error))) {
                    registers.gpr[3] = kCellEfault;
                    error.clear();
                } else {
                    loaded->started = false;
                    registers.gpr[3] = kCellOk;
                }
            } else {
                loaded->started = false;
                registers.gpr[3] = kCellOk;
            }
        } else {
            loaded->started = false;
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysPrxUnregisterModule:
        // VSH unregisters the temporary registration record after the module
        // stop handshake. Keep the loaded image and its linked exports alive
        // until the matching unload path exists, but complete this ABI step.
        registers.gpr[3] = kCellOk;
        handled = true;
        break;
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
    case kSysRsxAudioInitialize: {
        // RSX audio is the first RSX-adjacent service VSH brings up. Keep a
        // real guest-visible handle so later audio calls can be validated
        // without pretending that audio DMA is already emulated.
        if (registers.gpr[3] == 0) {
            registers.gpr[3] = kCellEfault;
        } else {
            const auto handle = next_rsx_audio_id_++;
            rsx_audio_handles_.insert(handle);
            if (!WriteU32(registers.gpr[3], handle, error)) {
                rsx_audio_handles_.erase(handle);
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysRsxAudioFinalize: {
        const auto handle = static_cast<std::uint32_t>(registers.gpr[3]);
        if (rsx_audio_handles_.erase(handle) == 0) {
            registers.gpr[3] = kCellEsrch;
        } else {
            rsx_audio_connections_.erase(handle);
            rsx_audio_imported_memory_.erase(handle);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysRsxAudioImportSharedMemory: {
        const auto handle = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto output = registers.gpr[4];
        if (rsx_audio_handles_.find(handle) == rsx_audio_handles_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (output == 0) {
            registers.gpr[3] = kCellEfault;
        } else {
            auto [imported, inserted] = rsx_audio_imported_memory_.emplace(
                handle, next_rsx_audio_address_);
            if (inserted) {
                const auto mapped = memory_.Map({
                    imported->second,
                    kRsxAudioImportedMemorySize,
                    memory::kPermissionRead | memory::kPermissionWrite});
                if (!mapped.ok()) {
                    rsx_audio_imported_memory_.erase(imported);
                    registers.gpr[3] = kCellEnomem;
                    error.clear();
                    handled = true;
                    break;
                }
                next_rsx_audio_address_ += kRsxAudioImportedMemorySize;
            }
            if (!WriteU64(output, imported->second, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
    case kSysRsxAudioUnimportSharedMemory: {
        const auto handle = static_cast<std::uint32_t>(registers.gpr[3]);
        if (rsx_audio_handles_.find(handle) == rsx_audio_handles_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else {
            // Guest memory is monotonic in this first runtime slice, so the
            // mapping remains available for stale DMA pointers after the
            // logical unimport. The ownership/state transition is real.
            rsx_audio_imported_memory_.erase(handle);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysRsxAudioCreateConnection: {
        const auto handle = static_cast<std::uint32_t>(registers.gpr[3]);
        if (rsx_audio_handles_.find(handle) == rsx_audio_handles_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else {
            // The three event queues have already been created by VSH. Keep
            // the connection stateful; actual audio mixing is a later slice.
            rsx_audio_connections_.insert(handle);
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysRsxAudioCloseConnection:
    case kSysRsxAudioPrepareProcess:
    case kSysRsxAudioStartProcess:
    case kSysRsxAudioStopProcess: {
        const auto handle = static_cast<std::uint32_t>(registers.gpr[3]);
        if (rsx_audio_handles_.find(handle) == rsx_audio_handles_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else {
            if (syscall == kSysRsxAudioCloseConnection) {
                rsx_audio_connections_.erase(handle);
            } else {
                // Process state is intentionally lightweight for now: VSH can
                // drive the shared page without being stopped by an absent
                // hardware mixer, while the handle lifetime stays checked.
                rsx_audio_connections_.insert(handle);
            }
            registers.gpr[3] = kCellOk;
        }
        handled = true;
        break;
    }
    case kSysRsxAudioGetDmaParam: {
        const auto handle = static_cast<std::uint32_t>(registers.gpr[3]);
        const auto flag = static_cast<std::uint32_t>(registers.gpr[4]);
        const auto output = registers.gpr[5];
        const auto imported = rsx_audio_imported_memory_.find(handle);
        if (rsx_audio_handles_.find(handle) == rsx_audio_handles_.end()) {
            registers.gpr[3] = kCellEsrch;
        } else if (output == 0 || imported == rsx_audio_imported_memory_.end()) {
            registers.gpr[3] = kCellEfault;
        } else if (flag > 1) {
            registers.gpr[3] = kCellEinval;
        } else {
            const auto value = flag == 0 ? imported->second
                                         : static_cast<std::uint64_t>(handle);
            if (!WriteU64(output, value, error)) {
                registers.gpr[3] = kCellEfault;
                error.clear();
            } else {
                registers.gpr[3] = kCellOk;
            }
        }
        handled = true;
        break;
    }
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
        {
            std::ostringstream stream;
            stream << UnknownSyscall(syscall) << " at pc=0x" << std::hex
                   << (registers.pc >= 4 ? registers.pc - 4 : registers.pc)
                   << " args=0x" << arguments[0] << ",0x" << arguments[1]
                   << ",0x" << arguments[2] << ",0x" << arguments[3];
            error = stream.str();
        }
        return false;
    }
    if (handled) {
        trace_.push_back({syscall, registers.gpr[3], arguments, trace_.size(),
                          registers.pc >= 4 ? registers.pc - 4 : registers.pc});
    }
    return handled;
}

} // namespace vshift::hle
