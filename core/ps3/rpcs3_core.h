#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace vshift::ps3 {

enum class BootStage : std::uint8_t {
    None = 0,
    RuntimeInitialized,
    FirmwareReady,
    VshStarted,
    Running,
};

struct CoreCallbacks final {
    std::function<void()> on_started;
    std::function<void()> on_stopped;
    std::function<void(std::string)> on_log;
};

struct BootReport final {
    BootStage stage = BootStage::None;
    std::string firmware_version;
    std::filesystem::path emulator_directory;
    std::string error;

    bool ok() const noexcept { return error.empty(); }
    bool xmb_started() const noexcept {
        return stage == BootStage::VshStarted || stage == BootStage::Running;
    }
};

// Thin ownership and lifecycle boundary around RPCS3's real rpcs3_emu target.
// VSHift's iOS frontend talks to this class instead of depending on Qt or
// desktop RPCS3 UI types.
class Rpcs3Core final {
public:
    explicit Rpcs3Core(CoreCallbacks callbacks = {});
    ~Rpcs3Core();

    Rpcs3Core(const Rpcs3Core&) = delete;
    Rpcs3Core& operator=(const Rpcs3Core&) = delete;

    BootReport StartVsh(const std::filesystem::path& emulator_directory);
    void Stop();
    bool Pause();
    void Resume();
    bool MountIso(const std::filesystem::path& iso_path);
    void EjectIso();

    bool running() const noexcept { return running_; }

private:
    CoreCallbacks callbacks_;
    bool initialized_ = false;
    bool running_ = false;
};

} // namespace vshift::ps3
