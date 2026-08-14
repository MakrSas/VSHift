#include "core/video/framebuffer.h"

#include <limits>
#include <utility>

namespace vshift::video {

namespace {

constexpr std::uint32_t kBytesPerPixel = 4;
constexpr std::uint32_t kMaximumDimension = 8192;
// Keep malformed guest metadata from forcing an unbounded host allocation.
// This is well above a 4K RGBA frame while still being safe for a mobile host.
constexpr std::uint64_t kMaximumFrameBytes = 256ull * 1024ull * 1024ull;

bool MultiplyWouldOverflow(std::uint64_t left,
                           std::uint64_t right) noexcept {
    return right != 0 &&
           left > std::numeric_limits<std::uint64_t>::max() / right;
}

} // namespace

FrameBuffer::FrameBuffer(FramePresenter presenter)
    : presenter_(std::move(presenter)) {}

FrameResult FrameBuffer::Validate(const FrameDescription& description,
                                  std::size_t pixel_count) {
    FrameResult result;
    if (description.width == 0 || description.height == 0 ||
        description.width > kMaximumDimension ||
        description.height > kMaximumDimension) {
        result.error = "guest frame dimensions are invalid";
        return result;
    }
    const auto minimum_row_bytes =
        static_cast<std::uint64_t>(description.width) * kBytesPerPixel;
    if (description.bytes_per_row < minimum_row_bytes) {
        result.error = "guest frame stride is too small";
        return result;
    }
    if (MultiplyWouldOverflow(description.bytes_per_row, description.height) ||
        static_cast<std::uint64_t>(description.bytes_per_row) *
                description.height > kMaximumFrameBytes ||
        static_cast<std::uint64_t>(description.bytes_per_row) *
                description.height != pixel_count) {
        result.error = "guest frame byte count does not match its stride";
        return result;
    }
    return result;
}

FrameResult FrameBuffer::Present(
    const FrameDescription& description,
    std::span<const std::uint8_t> pixels) {
    auto result = Validate(description, pixels.size());
    if (!result.ok()) {
        return result;
    }

    GuestFrame frame;
    frame.description = description;
    frame.pixels.assign(pixels.begin(), pixels.end());
    if (presenter_ && !presenter_(frame)) {
        result.error = "guest frame presenter rejected the frame";
        return result;
    }
    last_frame_ = std::move(frame);
    has_frame_ = true;
    return result;
}

FrameResult FrameBuffer::CopyFromGuest(
    memory::GuestMemory& memory,
    std::uint64_t guest_address,
    const FrameDescription& description) {
    const auto byte_count = static_cast<std::uint64_t>(
        description.bytes_per_row) * description.height;
    if (byte_count > kMaximumFrameBytes ||
        byte_count > std::numeric_limits<std::size_t>::max()) {
        return {"guest frame is too large for the host"};
    }

    const auto validated = Validate(description, static_cast<std::size_t>(byte_count));
    if (!validated.ok()) {
        return validated;
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(byte_count));
    for (std::uint32_t row = 0; row < description.height; ++row) {
        const auto row_offset = static_cast<std::uint64_t>(
            row) * description.bytes_per_row;
        const auto read = memory.Read(
            guest_address + row_offset,
            std::span<std::uint8_t>(
                pixels.data() + static_cast<std::size_t>(row_offset),
                description.bytes_per_row));
        if (!read.ok()) {
            return {"guest framebuffer read failed: " + read.error};
        }
    }
    return Present(description, pixels);
}

const GuestFrame* FrameBuffer::last_frame() const noexcept {
    return has_frame_ ? &last_frame_ : nullptr;
}

} // namespace vshift::video
