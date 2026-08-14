#include "third_party/rpcs3/rpcs3/Emu/RSX/rsx_utils.h"

#include <algorithm>
#include <cstring>

namespace rsx {

atomic_t<u64> g_rsx_shared_tag{0};

void convert_scale_image(u8* /*dst*/, AVPixelFormat /*dst_format*/,
                         int /*dst_width*/, int /*dst_height*/, int /*dst_pitch*/,
                         const u8* /*src*/, AVPixelFormat /*src_format*/,
                         int /*src_width*/, int /*src_height*/, int /*src_pitch*/,
                         int /*src_slice_h*/, bool /*bilinear*/) {}

void clip_image(u8* dst, const u8* src, int clip_x, int clip_y, int clip_w,
                int clip_h, int bpp, int src_pitch, int dst_pitch) {
    if (!dst || !src || clip_w <= 0 || clip_h <= 0 || bpp <= 0) {
        return;
    }
    const auto row_length = static_cast<std::size_t>(clip_w) * bpp;
    const auto* source = src + static_cast<std::size_t>(clip_y) * src_pitch +
                         static_cast<std::size_t>(clip_x) * bpp;
    for (int row = 0; row < clip_h; ++row) {
        std::memcpy(dst, source, row_length);
        source += src_pitch;
        dst += dst_pitch;
    }
}

void clip_image_may_overlap(u8* dst, const u8* src, int clip_x, int clip_y,
                            int clip_w, int clip_h, int bpp, int src_pitch,
                            int dst_pitch, u8* buffer) {
    if (!buffer || clip_w <= 0 || clip_h <= 0 || bpp <= 0) {
        return;
    }
    const auto row_length = static_cast<std::size_t>(clip_w) * bpp;
    const auto* source = src + static_cast<std::size_t>(clip_y) * src_pitch +
                         static_cast<std::size_t>(clip_x) * bpp;
    auto* temporary = buffer;
    for (int row = 0; row < clip_h; ++row) {
        std::memcpy(temporary, source, row_length);
        source += src_pitch;
        temporary += row_length;
    }
    temporary = buffer;
    for (int row = 0; row < clip_h; ++row) {
        std::memcpy(dst, temporary, row_length);
        temporary += row_length;
        dst += dst_pitch;
    }
}

} // namespace rsx
