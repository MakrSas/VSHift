#pragma once

#include "core-api/console_core.h"

#include <memory>
#include <vector>

namespace vshift::ps3 {

class PS3Core final : public coreapi::IConsoleCore {
public:
    PS3Core();
    ~PS3Core() override;

    PS3Core(const PS3Core&) = delete;
    PS3Core& operator=(const PS3Core&) = delete;

    const coreapi::CoreDescriptor& descriptor() const noexcept override;
    coreapi::CoreStatus status() const override;
    coreapi::CoreResult initialize() override;
    coreapi::CoreResult validateFirmware(std::span<const std::uint8_t> pup_bytes) override;
    coreapi::CoreResult installFirmware(std::span<const std::uint8_t> pup_bytes) override;
    coreapi::CoreResult boot() override;
    coreapi::CoreResult pause() override;
    coreapi::CoreResult resume() override;
    coreapi::CoreResult reset() override;
    coreapi::CoreResult shutdown() override;
    coreapi::CoreResult insertMedia(std::span<const std::uint8_t> media) override;
    coreapi::CoreResult ejectMedia() override;
    void setInputState(const platform::ControllerState& state) override;
    void setPowerState(const platform::PowerState& state) override;
    void setEventSink(coreapi::CoreEventSink sink) override;

private:
    class RuntimeHolder;

    void setState(coreapi::CoreState state, std::string message);
    void emit(coreapi::CoreEventType type, std::string message) const;
    coreapi::CoreResult ensureInitialized();

    coreapi::CoreDescriptor descriptor_;
    coreapi::CoreStatus status_;
    std::unique_ptr<RuntimeHolder> runtime_;
    std::vector<std::uint8_t> firmware_;
    platform::ControllerState input_;
    platform::PowerState power_;
    coreapi::CoreEventSink event_sink_;
};

} // namespace vshift::ps3
