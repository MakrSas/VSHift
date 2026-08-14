#pragma once

#include <filesystem>
#include <string>

namespace vshift::frontend {

enum class MediaKind {
    Unknown,
    Music,
    Photo,
    Video,
};

struct MediaImportReport {
    MediaKind kind = MediaKind::Unknown;
    std::filesystem::path destination;
    std::string error;

    bool ok() const noexcept { return error.empty() && !destination.empty(); }
};

MediaKind DetectMediaKind(const std::filesystem::path& source);

// Copies a user-selected file into the PS3-visible storage layout. The caller
// owns the root and is responsible for keeping it inside the app's sandbox.
MediaImportReport ImportMediaFile(const std::filesystem::path& source,
                                  const std::filesystem::path& emulator_root);

} // namespace vshift::frontend
