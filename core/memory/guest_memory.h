#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace vshift::memory {

constexpr std::uint32_t kPermissionRead = 1u << 0;
constexpr std::uint32_t kPermissionWrite = 1u << 1;
constexpr std::uint32_t kPermissionExecute = 1u << 2;

struct Mapping final {
    std::uint64_t guest_address = 0;
    std::uint64_t size = 0;
    std::uint32_t permissions = 0;
};

struct MemoryResult final {
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

// Sparse guest address space. Guest addresses are kept separate from host
// pointers; each mapping owns zero-initialized backing bytes.
class GuestMemory final {
public:
    MemoryResult Map(Mapping mapping);

    // Loader initialization bypasses final guest write permissions, then
    // normal guest writes still observe the mapping's permissions.
    MemoryResult Initialize(std::uint64_t guest_address,
                            std::span<const std::uint8_t> bytes);
    MemoryResult Read(std::uint64_t guest_address,
                      std::span<std::uint8_t> output) const;
    MemoryResult Write(std::uint64_t guest_address,
                       std::span<const std::uint8_t> bytes);

    std::vector<Mapping> Mappings() const;

private:
    struct Region final {
        Mapping mapping;
        std::vector<std::uint8_t> bytes;
    };

    const Region* FindRegion(std::uint64_t guest_address,
                             std::uint64_t size) const noexcept;
    Region* FindRegion(std::uint64_t guest_address,
                       std::uint64_t size) noexcept;
    static bool Contains(const Mapping& mapping,
                         std::uint64_t guest_address,
                         std::uint64_t size) noexcept;

    std::vector<Region> regions_;
};

} // namespace vshift::memory
