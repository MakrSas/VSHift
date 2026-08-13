#include "core/cpu/executable_memory.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__) || defined(__unix__)
#include <sys/mman.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <TargetConditionals.h>
#include <mach/mach.h>
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#endif
#endif

namespace vshift::cpu {

namespace {

std::size_t PageSize() noexcept {
#if defined(_WIN32)
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#elif defined(__APPLE__) || defined(__unix__)
    const auto page_size = ::sysconf(_SC_PAGESIZE);
    return page_size > 0 ? static_cast<std::size_t>(page_size) : 4096;
#else
    return 4096;
#endif
}

std::size_t PageAlignedSize(std::size_t size) noexcept {
    const auto page_size = PageSize();
    return ((size + page_size - 1) / page_size) * page_size;
}

#if defined(__APPLE__)

extern "C" kern_return_t mach_vm_remap(
    vm_map_t target_task,
    mach_vm_address_t* target_address,
    mach_vm_size_t size,
    mach_vm_offset_t mask,
    int flags,
    vm_map_t source_task,
    mach_vm_address_t source_address,
    boolean_t copy,
    vm_prot_t* cur_protection,
    vm_prot_t* max_protection,
    vm_inherit_t inheritance);

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
bool IsDebuggerAttached() noexcept {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc info{};
    std::size_t info_size = sizeof(info);

    if (::sysctl(mib, 4, &info, &info_size, nullptr, 0) != 0) {
        return false;
    }
    return (info.kp_proc.p_flag & P_TRACED) != 0;
}

bool RequiresTxmWorkaround() noexcept {
    char version[64] = {};
    std::size_t version_size = sizeof(version);
    if (::sysctlbyname("kern.osproductversion", version, &version_size,
                       nullptr, 0) != 0) {
        // Be conservative on a future iOS version whose product version we
        // cannot read: use the debugger-assisted path.
        return true;
    }

    int major = 0;
    for (const char* current = version; *current >= '0' && *current <= '9';
         ++current) {
        major = major * 10 + (*current - '0');
    }
    return major >= 26;
}

#if defined(__aarch64__) || defined(__arm64__) || defined(__ARM64__)
void PrepareJitRegionForStikDebug(std::uint8_t* address,
                                  std::size_t size) noexcept {
    // This is the same call used by UTM. StikDebug's legacy.js catches the
    // breakpoint and invokes prepare_memory_region(address, size).
    asm volatile("mov x0, %0\n"
                 "mov x1, %1\n"
                 "brk #0x69"
                 :: "r"(reinterpret_cast<std::uintptr_t>(address)),
                    "r"(size)
                 : "x0", "x1", "memory");
}
#endif
#endif

bool AllocateSplitWx(std::size_t size,
                     std::uint8_t*& writable,
                     std::uint8_t*& executable) noexcept {
    // UTM's Darwin backend first creates an anonymous mapping, then creates a
    // second VM mapping of the same pages. Code is written through the first
    // mapping and executed through the second one.
    int original_protection = PROT_READ | PROT_WRITE;
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    if (RequiresTxmWorkaround()) {
        original_protection = PROT_READ | PROT_EXEC;
    }
#endif

    auto* rw = static_cast<std::uint8_t*>(::mmap(
        nullptr, size, original_protection, MAP_PRIVATE | MAP_ANON, -1, 0));
    if (rw == MAP_FAILED) {
        return false;
    }

    mach_vm_address_t rx_address = 0;
    vm_prot_t current_protection = VM_PROT_DEFAULT;
    vm_prot_t max_protection = VM_PROT_DEFAULT;
    const auto remap_result = mach_vm_remap(
        mach_task_self(), &rx_address, size, 0, VM_FLAGS_ANYWHERE,
        mach_task_self(), reinterpret_cast<mach_vm_address_t>(rw), false,
        &current_protection, &max_protection, VM_INHERIT_NONE);
    if (remap_result != KERN_SUCCESS) {
        ::munmap(rw, size);
        return false;
    }

    auto* rx = reinterpret_cast<std::uint8_t*>(rx_address);
    if (::mprotect(rx, size, PROT_READ | PROT_EXEC) != 0) {
        ::munmap(rx, size);
        ::munmap(rw, size);
        return false;
    }

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#if defined(__aarch64__) || defined(__arm64__) || defined(__ARM64__)
    // On newer iOS/TXM, the debugger must bless the executable mapping before
    // the writable mapping is restored. This is the same conditional path as
    // UTM; older iOS uses the ordinary split-W^X mapping without a breakpoint.
    if (RequiresTxmWorkaround() && IsDebuggerAttached()) {
        PrepareJitRegionForStikDebug(rx, size);
    }
#endif
#endif

    if (::mprotect(rw, size, PROT_READ | PROT_WRITE) != 0) {
        ::munmap(rx, size);
        ::munmap(rw, size);
        return false;
    }

    writable = rw;
    executable = rx;
    return true;
}

#endif

} // namespace

std::unique_ptr<ExecutableMemory> ExecutableMemory::Allocate(std::size_t size) {
    if (size == 0) {
        return nullptr;
    }

    const auto allocation_size = PageAlignedSize(size);

#if defined(_WIN32)
    auto* data = static_cast<std::uint8_t*>(::VirtualAlloc(
        nullptr, allocation_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (data == nullptr) {
        return nullptr;
    }
#elif defined(__APPLE__)
    // Prefer Apple's normal MAP_JIT path when the signing entitlements allow
    // it. iLoader/StikDebug deployments often cannot retain the restricted
    // dynamic-codesigning entitlement, so mirror UTM's split-W^X fallback.
    int flags = MAP_PRIVATE | MAP_ANON;
#ifdef MAP_JIT
    flags |= MAP_JIT;
#endif
    auto* data = static_cast<std::uint8_t*>(::mmap(
        nullptr, allocation_size, PROT_READ | PROT_WRITE, flags, -1, 0));
    if (data == MAP_FAILED) {
        std::uint8_t* writable = nullptr;
        std::uint8_t* executable = nullptr;
        if (!AllocateSplitWx(allocation_size, writable, executable)) {
            return nullptr;
        }
        return std::unique_ptr<ExecutableMemory>(
            new ExecutableMemory(writable, executable, allocation_size));
    }
#elif defined(__unix__)
    auto* data = static_cast<std::uint8_t*>(::mmap(
        nullptr, allocation_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (data == MAP_FAILED) {
        return nullptr;
    }
#else
    auto* data = static_cast<std::uint8_t*>(std::malloc(allocation_size));
    if (data == nullptr) {
        return nullptr;
    }
#endif

    return std::unique_ptr<ExecutableMemory>(
        new ExecutableMemory(data, data, allocation_size));
}

ExecutableMemory::~ExecutableMemory() {
    if (data_ == nullptr) {
        return;
    }

#if defined(_WIN32)
    ::VirtualFree(data_, 0, MEM_RELEASE);
#elif defined(__APPLE__) || defined(__unix__)
    ::munmap(data_, size_);
    if (executable_data_ != data_) {
        ::munmap(executable_data_, size_);
    }
#else
    std::free(data_);
#endif
}

bool ExecutableMemory::MakeExecutable() noexcept {
#if defined(_WIN32)
    DWORD old_protection = 0;
    return ::VirtualProtect(data_, size_, PAGE_EXECUTE_READ, &old_protection) != 0;
#elif defined(__APPLE__) || defined(__unix__)
    if (executable_data_ != data_) {
        return true;
    }
    return ::mprotect(data_, size_, PROT_READ | PROT_EXEC) == 0;
#else
    return false;
#endif
}

void ExecutableMemory::FlushInstructionCache(std::size_t offset,
                                              std::size_t size) noexcept {
    if (data_ == nullptr || offset >= size_) {
        return;
    }
    const auto length = std::min(size, size_ - offset);
    auto* begin = executable_data_ + offset;
    auto* end = begin + length;

#if defined(_WIN32)
    ::FlushInstructionCache(::GetCurrentProcess(), begin, length);
#elif defined(__APPLE__)
    ::sys_icache_invalidate(begin, length);
#elif defined(__GNUC__) || defined(__clang__)
    __builtin___clear_cache(reinterpret_cast<char*>(begin),
                            reinterpret_cast<char*>(end));
#else
    (void)end;
#endif
}

} // namespace vshift::cpu
