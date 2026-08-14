#include "core/video/framebuffer.h"

#include <array>
#include <cassert>

int main() {
    bool presented = false;
    vshift::video::FrameBuffer output(
        [&](const vshift::video::GuestFrame& frame) {
            presented = true;
            return frame.description.width == 2 &&
                   frame.description.height == 1 && frame.pixels.size() == 8;
        });

    const vshift::video::FrameDescription description{
        2, 1, 8, vshift::video::PixelFormat::Rgba8};
    const std::array<std::uint8_t, 8> pixels = {255, 0, 0, 255,
                                                 0, 255, 0, 255};
    assert(output.Present(description, pixels).ok());
    assert(presented);
    assert(output.last_frame() != nullptr);

    vshift::memory::GuestMemory memory;
    assert(memory.Map({0x600000, 0x1000,
                       vshift::memory::kPermissionRead |
                           vshift::memory::kPermissionWrite})
               .ok());
    assert(memory.Initialize(0x600000, pixels).ok());
    assert(output.CopyFromGuest(memory, 0x600000, description).ok());

    const vshift::video::FrameDescription invalid{
        2, 1, 4, vshift::video::PixelFormat::Rgba8};
    assert(!output.Present(invalid, pixels).ok());
    return 0;
}
