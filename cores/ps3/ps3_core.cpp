#include "cores/ps3/ps3_core.h"

#include "core/boot/ps3_runtime.h"

#include <utility>

namespace vshift::ps3 {

class PS3Core::RuntimeHolder final {
public:
    boot::Ps3Runtime runtime;
};

namespace {

constexpr std::size_t kBootSliceInstructions = 100'000;

const char* stopReason(cpu::PpuStopReason reason) noexcept {
    switch (reason) {
    case cpu::PpuStopReason::StepLimit: return "step limit";
    case cpu::PpuStopReason::Syscall: return "LV2 syscall";
    case cpu::PpuStopReason::UnsupportedInstruction: return "unsupported PPU instruction";
    case cpu::PpuStopReason::MemoryFault: return "PPU memory fault";
    case cpu::PpuStopReason::Halted: return "PPU halted";
    }
    return "unknown";
}

} // namespace

PS3Core::PS3Core()
    : descriptor_{
          .id = "ps3",
          .display_name = "PlayStation 3",
          .manufacturer = "Sony",
          .firmware_requirement = "PS3UPDAT.PUP",
          .firmware_extensions = {"PUP"},
          .media_types = {"disc"},
          .input_layout = {"DPad", "LeftStick", "RightStick", "Cross", "Circle",
                           "Square", "Triangle", "L1", "L2", "R1", "R2", "L3",
                           "R3", "Start", "Select", "PS"},
          .storage_types = {"dev_flash", "hdd"},
          .capabilities = static_cast<std::uint64_t>(coreapi::CoreCapability::FirmwareInstall) |
                          static_cast<std::uint64_t>(coreapi::CoreCapability::Boot) |
                          static_cast<std::uint64_t>(coreapi::CoreCapability::Pause) |
                          static_cast<std::uint64_t>(coreapi::CoreCapability::Reset) |
                          static_cast<std::uint64_t>(coreapi::CoreCapability::Shutdown),
          .system_ui = "XMB"},
      runtime_(std::make_unique<RuntimeHolder>()) {}

PS3Core::~PS3Core() = default;

const coreapi::CoreDescriptor& PS3Core::descriptor() const noexcept {
    return descriptor_;
}

coreapi::CoreStatus PS3Core::status() const {
    return status_;
}

void PS3Core::emit(coreapi::CoreEventType type, std::string message) const {
    if (event_sink_) event_sink_({type, std::move(message)});
}

void PS3Core::setState(coreapi::CoreState state, std::string message) {
    status_.state = state;
    status_.message = std::move(message);
    emit(coreapi::CoreEventType::StateChanged, status_.message);
}

coreapi::CoreResult PS3Core::ensureInitialized() {
    if (status_.state == coreapi::CoreState::Created) return initialize();
    if (status_.state == coreapi::CoreState::Error) {
        return coreapi::CoreResult::Error(status_.message);
    }
    return coreapi::CoreResult::Ok();
}

coreapi::CoreResult PS3Core::initialize() {
    if (status_.state != coreapi::CoreState::Created) return coreapi::CoreResult::Ok();
    setState(coreapi::CoreState::Initializing, "Initializing PS3 core");
    setState(coreapi::CoreState::Ready, "PS3 core ready");
    return coreapi::CoreResult::Ok(status_.message);
}

coreapi::CoreResult PS3Core::validateFirmware(std::span<const std::uint8_t> pup_bytes) {
    const auto initialized = ensureInitialized();
    if (!initialized.success) return initialized;
    boot::Ps3Runtime validator;
    const auto loaded = validator.LoadFirmware(pup_bytes);
    if (!loaded.ok()) return coreapi::CoreResult::Error(loaded.error);
    return coreapi::CoreResult::Ok("PS3 firmware is structurally valid");
}

coreapi::CoreResult PS3Core::installFirmware(std::span<const std::uint8_t> pup_bytes) {
    const auto initialized = ensureInitialized();
    if (!initialized.success) return initialized;
    setState(coreapi::CoreState::InstallingFirmware, "Installing PS3 firmware");
    firmware_.assign(pup_bytes.begin(), pup_bytes.end());
    runtime_ = std::make_unique<RuntimeHolder>();
    const auto loaded = runtime_->runtime.LoadFirmware(firmware_);
    if (!loaded.ok()) {
        firmware_.clear();
        setState(coreapi::CoreState::Error, loaded.error);
        return coreapi::CoreResult::Error(loaded.error);
    }
    setState(coreapi::CoreState::Ready, "PS3 firmware installed");
    return coreapi::CoreResult::Ok(status_.message);
}

coreapi::CoreResult PS3Core::boot() {
    const auto initialized = ensureInitialized();
    if (!initialized.success) return initialized;
    if (firmware_.empty() || !runtime_->runtime.loaded()) {
        return coreapi::CoreResult::Error("PS3 firmware is not installed");
    }
    setState(coreapi::CoreState::Booting, "Booting PS3 VSH");
    const auto result = runtime_->runtime.Run(kBootSliceInstructions);
    status_.instructions = result.instructions;
    status_.pc = result.registers.pc;
    status_.register11 = result.registers.gpr[11];
    status_.stop_reason = stopReason(result.reason);
    if (result.reason == cpu::PpuStopReason::UnsupportedInstruction ||
        result.reason == cpu::PpuStopReason::MemoryFault) {
        setState(coreapi::CoreState::Error, result.error.empty() ? status_.stop_reason : result.error);
        return coreapi::CoreResult::Error(status_.message);
    }
    setState(coreapi::CoreState::Running,
             "PS3 VSH execution slice completed: " + status_.stop_reason);
    return coreapi::CoreResult::Ok(status_.message);
}

coreapi::CoreResult PS3Core::pause() {
    if (!runtime_ || !runtime_->runtime.loaded()) return coreapi::CoreResult::Error("PS3 is not running");
    runtime_->runtime.Pause(true);
    setState(coreapi::CoreState::Paused, "PS3 paused");
    return coreapi::CoreResult::Ok(status_.message);
}

coreapi::CoreResult PS3Core::resume() {
    if (!runtime_ || !runtime_->runtime.loaded()) return coreapi::CoreResult::Error("PS3 is not running");
    runtime_->runtime.Pause(false);
    setState(coreapi::CoreState::Running, "PS3 resumed");
    return coreapi::CoreResult::Ok(status_.message);
}

coreapi::CoreResult PS3Core::reset() {
    if (firmware_.empty()) return coreapi::CoreResult::Error("PS3 firmware is not installed");
    runtime_ = std::make_unique<RuntimeHolder>();
    const auto loaded = runtime_->runtime.LoadFirmware(firmware_);
    if (!loaded.ok()) {
        setState(coreapi::CoreState::Error, loaded.error);
        return coreapi::CoreResult::Error(loaded.error);
    }
    status_ = {};
    setState(coreapi::CoreState::Ready, "PS3 reset and firmware reloaded");
    return coreapi::CoreResult::Ok(status_.message);
}

coreapi::CoreResult PS3Core::shutdown() {
    if (runtime_) runtime_->runtime.Stop();
    setState(coreapi::CoreState::Stopped, "PS3 powered off");
    return coreapi::CoreResult::Ok(status_.message);
}

coreapi::CoreResult PS3Core::insertMedia(std::span<const std::uint8_t> media) {
    (void)media;
    return coreapi::CoreResult::Error("PS3 optical media is not implemented yet");
}

coreapi::CoreResult PS3Core::ejectMedia() {
    return coreapi::CoreResult::Error("PS3 optical media is not implemented yet");
}

void PS3Core::setInputState(const platform::ControllerState& state) {
    input_ = state;
}

void PS3Core::setPowerState(const platform::PowerState& state) {
    power_ = state;
}

void PS3Core::setEventSink(coreapi::CoreEventSink sink) {
    event_sink_ = std::move(sink);
}

} // namespace vshift::ps3
