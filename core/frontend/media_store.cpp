#include "core/frontend/media_store.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace vshift::frontend {
namespace {

std::string LowerExtension(const std::filesystem::path& source) {
    std::string extension = source.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return extension;
}

std::filesystem::path DestinationDirectory(MediaKind kind,
                                            const std::filesystem::path& root) {
    switch (kind) {
    case MediaKind::Music:
        return root / "dev_hdd0" / "music" / "VSHift";
    case MediaKind::Photo:
        return root / "dev_hdd0" / "photo" / "VSHift";
    case MediaKind::Video:
        return root / "dev_hdd0" / "video" / "VSHift";
    case MediaKind::Unknown:
        return root / "dev_usb000" / "VSHift" / "Imported";
    }
    return {};
}

} // namespace

MediaKind DetectMediaKind(const std::filesystem::path& source) {
    const std::string extension = LowerExtension(source);

    if (extension == ".mp3" || extension == ".m4a" || extension == ".aac" ||
        extension == ".wav" || extension == ".flac" || extension == ".ogg") {
        return MediaKind::Music;
    }
    if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
        extension == ".gif" || extension == ".heic" || extension == ".bmp") {
        return MediaKind::Photo;
    }
    if (extension == ".mp4" || extension == ".m4v" || extension == ".mov" ||
        extension == ".avi" || extension == ".mkv") {
        return MediaKind::Video;
    }
    return MediaKind::Unknown;
}

MediaImportReport ImportMediaFile(const std::filesystem::path& source,
                                  const std::filesystem::path& emulator_root) {
    MediaImportReport report;
    if (source.empty() || emulator_root.empty()) {
        report.error = "Media source or emulator root is empty";
        return report;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(source, ec) || ec) {
        report.error = "Selected media file is not readable";
        return report;
    }

    report.kind = DetectMediaKind(source);
    const auto destination_directory =
        DestinationDirectory(report.kind, emulator_root);
    std::filesystem::create_directories(destination_directory, ec);
    if (ec) {
        report.error = "Could not create virtual media directory: " +
                       ec.message();
        return report;
    }

    report.destination = destination_directory / source.filename();
    std::filesystem::copy_file(source, report.destination,
                                std::filesystem::copy_options::overwrite_existing,
                                ec);
    if (ec) {
        report.error = "Could not import media file: " + ec.message();
        report.destination.clear();
    }
    return report;
}

} // namespace vshift::frontend
