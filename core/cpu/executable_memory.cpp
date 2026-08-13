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
    // iOS requires MAP_JIT for dynamically generated executable memory. The
    // entitlement is supplied by the app target; macOS also accepts it when
    // the process is signed with the corresponding entitlement.
    int flags = MAP_PRIVATE | MAP_ANON;
#ifdef MAP_JIT
    flags |= MAP_JIT;
#endif
    auto* data = static_cast<std::uint8_t*>(::mmap(
        nullptr, allocation_size, PROT_READ | PROT_WRITE, flags, -1, 0));
    if (data == MAP_FAILED) {
        return nullptr;
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
        new ExecutableMemory(data, allocation_size));
}

ExecutableMemory::~ExecutableMemory() {
    if (data_ == nullptr) {
        return;
    }

#if defined(_WIN32)
    ::VirtualFree(data_, 0, MEM_RELEASE);
#elif defined(__APPLE__) || defined(__unix__)
    ::munmap(data_, size_);
#else
    std::free(data_);
#endif
}

bool ExecutableMemory::MakeExecutable() noexcept {
#if defined(_WIN32)
    DWORD old_protection = 0;
    return ::VirtualProtect(data_, size_, PAGE_EXECUTE_READ, &old_protection) != 0;
#elif defined(__APPLE__) || defined(__unix__)
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
    auto* begin = data_ + offset;
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
