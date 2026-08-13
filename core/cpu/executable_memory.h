#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace vshift::cpu {

class ExecutableMemory final {
public:
    static std::unique_ptr<ExecutableMemory> Allocate(std::size_t size);
    ~ExecutableMemory();

    ExecutableMemory(const ExecutableMemory&) = delete;
    ExecutableMemory& operator=(const ExecutableMemory&) = delete;

    std::uint8_t* writable_data() noexcept { return data_; }
    const std::uint8_t* data() const noexcept { return data_; }
    std::uint8_t* executable_data() noexcept { return executable_data_; }
    const std::uint8_t* executable_data() const noexcept { return executable_data_; }
    std::size_t size() const noexcept { return size_; }

    bool MakeExecutable() noexcept;
    void FlushInstructionCache(std::size_t offset, std::size_t size) noexcept;

private:
    ExecutableMemory(std::uint8_t* data,
                     std::uint8_t* executable_data,
                     std::size_t size) noexcept
        : data_(data), executable_data_(executable_data), size_(size) {}

    std::uint8_t* data_ = nullptr;
    std::uint8_t* executable_data_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace vshift::cpu
