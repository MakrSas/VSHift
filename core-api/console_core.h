#pragma once

#include "platform/host_services.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace vshift::coreapi {

enum class CoreState {
    Created,
    Initializing,
    Ready,
    InstallingFirmware,
    Booting,
    Running,
    Paused,
    Stopping,
    Stopped,
    Error,
};

enum class CoreCapability : std::uint64_t {
    None = 0,
    FirmwareInstall = 1ull << 0,
    Boot = 1ull << 1,
    Pause = 1ull << 2,
    Reset = 1ull << 3,
    Shutdown = 1ull << 4,
    OpticalDisc = 1ull << 5,
    AudioOutput = 1ull << 6,
    VideoOutput = 1ull << 7,
    PhysicalController = 1ull << 8,
    SuspendResume = 1ull << 9,
};

constexpr std::uint64_t operator|(CoreCapability left, CoreCapability right) noexcept {
    return static_cast<std::uint64_t>(left) | static_cast<std::uint64_t>(right);
}

struct CoreResult final {
    bool success = false;
    std::string message;

    static CoreResult Ok(std::string message = {}) {
        return {true, std::move(message)};
    }

    static CoreResult Error(std::string message) {
        return {false, std::move(message)};
    }
};

struct CoreDescriptor final {
    std::string id;
    std::string display_name;
    std::string manufacturer;
    std::string firmware_requirement;
    std::vector<std::string> firmware_extensions;
    std::vector<std::string> media_types;
    std::vector<std::string> input_layout;
    std::vector<std::string> storage_types;
    std::uint64_t capabilities = 0;
    std::string system_ui;
};

struct CoreStatus final {
    CoreState state = CoreState::Created;
    std::string message;
    std::string stop_reason;
    std::size_t instructions = 0;
    std::uint64_t pc = 0;
    std::uint64_t register11 = 0;
};

enum class CoreEventType {
    StateChanged,
    Log,
    VideoFrame,
    AudioFrame,
    Haptic,
};

struct CoreEvent final {
    CoreEventType type = CoreEventType::Log;
    std::string message;
};

using CoreEventSink = std::function<void(const CoreEvent&)>;

class IConsoleCore {
public:
    virtual ~IConsoleCore() = default;

    virtual const CoreDescriptor& descriptor() const noexcept = 0;
    virtual CoreStatus status() const = 0;
    virtual CoreResult initialize() = 0;
    virtual CoreResult validateFirmware(std::span<const std::uint8_t> pup_bytes) = 0;
    virtual CoreResult installFirmware(std::span<const std::uint8_t> pup_bytes) = 0;
    virtual CoreResult boot() = 0;
    virtual CoreResult pause() = 0;
    virtual CoreResult resume() = 0;
    virtual CoreResult reset() = 0;
    virtual CoreResult shutdown() = 0;
    virtual CoreResult insertMedia(std::span<const std::uint8_t> media) = 0;
    virtual CoreResult ejectMedia() = 0;
    virtual void setInputState(const platform::ControllerState& state) = 0;
    virtual void setPowerState(const platform::PowerState& state) = 0;
    virtual void setEventSink(CoreEventSink sink) = 0;
};

} // namespace vshift::coreapi
