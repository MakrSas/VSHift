#pragma once

#include "core/memory/guest_memory.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace vshift::video {

enum class PixelFormat : std::uint8_t {
    Rgba8 = 0,
    Bgra8,
};

struct FrameDescription final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t bytes_per_row = 0;
    PixelFormat format = PixelFormat::Rgba8;
};

struct GuestFrame final {
    FrameDescription description;
    std::vector<std::uint8_t> pixels;
};

struct FrameResult final {
    std::string error;

    bool ok() const noexcept { return error.empty(); }
};

using FramePresenter = std::function<bool(const GuestFrame&)>;

// Owns a copy of the last frame submitted by the guest. The presenter is an
// adapter boundary: Metal/UIKit may consume it, but the core never fabricates
// a frame and never reads host pointers as guest addresses.
class FrameBuffer final {
public:
    explicit FrameBuffer(FramePresenter presenter = {});

    FrameResult Present(const FrameDescription& description,
                        std::span<const std::uint8_t> pixels);

    FrameResult CopyFromGuest(memory::GuestMemory& memory,
                              std::uint64_t guest_address,
                              const FrameDescription& description);

    const GuestFrame* last_frame() const noexcept;

private:
    static FrameResult Validate(const FrameDescription& description,
                                std::size_t pixel_count);

    FramePresenter presenter_;
    GuestFrame last_frame_;
    bool has_frame_ = false;
};

} // namespace vshift::video
