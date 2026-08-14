#include "third_party/rpcs3/rpcs3/util/media_utils.h"

#include <utility>

// The first iOS boot profile uses RPCS3's Null RSX/audio backends.  Its
// desktop FFmpeg media pipeline is deliberately kept out of the device link
// until an iOS-native media/RSX adapter is available.
namespace utils {

template <>
std::string media_info::get_metadata(const std::string& key,
                                     const std::string& def) const {
    const auto iterator = metadata.find(key);
    return iterator == metadata.end() ? def : iterator->second;
}

template <>
s64 media_info::get_metadata(const std::string& key, const s64& def) const {
    const auto iterator = metadata.find(key);
    if (iterator == metadata.end()) {
        return def;
    }
    try {
        return std::stoll(iterator->second);
    } catch (...) {
        return def;
    }
}

std::string av_error_to_string(int error) {
    return "FFmpeg media support is unavailable in the iOS boot profile (" +
           std::to_string(error) + ")";
}

std::vector<ffmpeg_codec> list_ffmpeg_decoders() {
    return {};
}

std::vector<ffmpeg_codec> list_ffmpeg_encoders() {
    return {};
}

std::pair<bool, media_info> get_media_info(const std::string& path,
                                           s32 /*av_media_type*/) {
    media_info info;
    info.path = path;
    return {false, std::move(info)};
}

audio_decoder::audio_decoder() = default;
audio_decoder::~audio_decoder() = default;

void audio_decoder::set_context(music_selection_context&& context) {
    m_context = std::move(context);
}

void audio_decoder::set_swap_endianness(bool swapped) {
    m_swap_endianness = swapped;
}

void audio_decoder::clear() {
    data.clear();
    timestamps_ms.clear();
    m_size = 0;
    track_fully_decoded = 0;
    track_fully_consumed = 0;
    has_error = false;
}

void audio_decoder::stop() {}
void audio_decoder::decode() {}
u32 audio_decoder::set_next_index(bool /*next*/) { return 0; }

video_encoder::video_encoder() = default;
video_encoder::~video_encoder() = default;

std::string video_encoder::path() const { return m_path; }
s64 video_encoder::last_video_pts() const { return m_last_video_pts; }
void video_encoder::set_path(const std::string& path) { m_path = path; }
void video_encoder::set_framerate(u32 /*framerate*/) {}
void video_encoder::set_video_bitrate(u32 /*bitrate*/) {}
void video_encoder::set_output_format(frame_format /*format*/) {}
void video_encoder::set_video_codec(s32 /*codec_id*/, std::string_view /*codec_name*/) {}
void video_encoder::set_max_b_frames(s32 /*max_b_frames*/) {}
void video_encoder::set_gop_size(s32 /*gop_size*/) {}
void video_encoder::set_sample_rate(u32 /*sample_rate*/) {}
void video_encoder::set_audio_channels(u32 /*channels*/) {}
void video_encoder::set_audio_bitrate(u32 /*bitrate*/) {}
void video_encoder::set_audio_codec(s32 /*codec_id*/, std::string_view /*codec_name*/) {}
void video_encoder::pause(bool /*flush*/) {}
void video_encoder::stop(bool /*flush*/) {}
void video_encoder::resume() {}
void video_encoder::encode() {}

} // namespace utils
