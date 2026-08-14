#include "core/firmware/vfs.h"

#include <limits>

namespace vshift::firmware {

bool ReadOnlyVfs::NormalizePath(std::string_view path,
                                std::string& normalized,
                                std::string& error) {
    normalized.clear();
    error.clear();

    if (path.empty() || path.front() == '/' || path.front() == '\\' ||
        (path.size() >= 2 && path[1] == ':')) {
        error = "guest VFS path must be a relative path";
        return false;
    }

    std::size_t component_start = 0;
    while (component_start < path.size()) {
        const auto separator = path.find_first_of("/\\", component_start);
        const auto component_end = separator == std::string_view::npos
                                       ? path.size()
                                       : separator;
        const auto component = path.substr(component_start,
                                           component_end - component_start);
        if (component.empty() || component == "." || component == "..") {
            error = "guest VFS path contains an invalid component";
            normalized.clear();
            return false;
        }
        if (!normalized.empty()) {
            normalized.push_back('/');
        }
        normalized.append(component);
        if (separator == std::string_view::npos) {
            break;
        }
        component_start = separator + 1;
    }

    if (normalized.empty()) {
        error = "guest VFS path is empty";
        return false;
    }
    return true;
}

VfsFile ReadOnlyVfs::ReadFile(std::string_view path) const {
    VfsFile result;
    std::string normalized;
    if (!NormalizePath(path, normalized, result.error)) {
        return result;
    }
    if (!reader_) {
        result.error = "guest VFS has no file source";
        return result;
    }

    result = reader_(normalized);
    if (!result.error.empty()) {
        return result;
    }
    if (result.bytes.empty()) {
        result.error = "guest VFS returned an empty file";
        return result;
    }
    if (result.bytes.size() > maximum_file_size_ ||
        result.bytes.size() > std::numeric_limits<std::uint64_t>::max()) {
        result.bytes.clear();
        result.error = "guest VFS file exceeds the configured size limit";
    }
    return result;
}

} // namespace vshift::firmware
