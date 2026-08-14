#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vshift::firmware {

struct VfsFile final {
    std::vector<std::uint8_t> bytes;
    std::string error;

    bool ok() const noexcept { return error.empty() && !bytes.empty(); }
};

using VfsReader = std::function<VfsFile(std::string_view relative_path)>;

// A small, read-only guest filesystem boundary. The host owns the actual
// storage; the guest receives only normalized relative paths and bounded file
// bytes. No host absolute path can cross this boundary.
class ReadOnlyVfs final {
public:
    explicit ReadOnlyVfs(VfsReader reader,
                         std::uint64_t maximum_file_size =
                             512ull * 1024ull * 1024ull)
        : reader_(std::move(reader)), maximum_file_size_(maximum_file_size) {}

    VfsFile ReadFile(std::string_view path) const;

    static bool NormalizePath(std::string_view path,
                              std::string& normalized,
                              std::string& error);

private:
    VfsReader reader_;
    std::uint64_t maximum_file_size_;
};

} // namespace vshift::firmware
