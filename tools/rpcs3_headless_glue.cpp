#include "rpcs3_version.h"
#include "git-version.h"
#include "Utilities/StrUtil.h"
#include "Utilities/Thread.h"
#include "Input/pad_thread.h"
#include "Input/product_info.h"
#include "Input/ps_move_config.h"
#include "Input/ps_move_tracker.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>

namespace rpcs3
{
	std::string_view get_branch() { return RPCS3_GIT_BRANCH; }
	std::string_view get_full_branch() { return RPCS3_GIT_FULL_BRANCH; }

	std::pair<std::string, std::string> get_commit_and_hash()
	{
		auto commit_and_hash = fmt::split(RPCS3_GIT_VERSION, {"-"});
		if (commit_and_hash.size() != 2)
			return {"0", "00000000"};
		return {std::move(commit_and_hash[0]), std::move(commit_and_hash[1])};
	}

	const utils::version& get_version()
	{
		static constexpr utils::version version{0, 0, 42, utils::version_type::alpha, 1, RPCS3_GIT_VERSION};
		return version;
	}

	std::string get_version_and_branch()
	{
		if (get_branch() != "master"sv && get_branch() != "HEAD"sv)
			return get_verbose_version();
		std::string version = get_version().to_string();
		const auto last_minus = version.find_last_of('-');
		return version.substr(0, last_minus);
	}

	std::string get_verbose_version()
	{
		return fmt::format("%s | %s", get_version().to_string(), get_branch());
	}

	bool is_release_build() { return std::string_view(RPCS3_GIT_FULL_BRANCH) == "RPCS3/rpcs3/master"sv; }
	bool is_local_build() { return std::string_view(RPCS3_GIT_FULL_BRANCH) == "local_build"sv; }
}

[[noreturn]] void report_fatal_error(std::string_view text, bool, bool)
{
	std::fprintf(stderr, "RPCS3 fatal error: %.*s\n", static_cast<int>(text.size()), text.data());
	std::abort();
}

void qt_events_aware_op(int repeat_duration_ms, std::function<bool()> wrapped_op)
{
	if (!wrapped_op)
		return;
	while (!wrapped_op())
		std::this_thread::sleep_for(std::chrono::milliseconds(std::max(1, repeat_duration_ms)));
}

namespace pad
{
	atomic_t<pad_thread*> g_pad_thread = nullptr;
	shared_mutex g_pad_mutex;
	std::string g_title_id;
	atomic_t<bool> g_started{false};
	atomic_t<bool> g_reset{false};
	atomic_t<bool> g_enabled{true};
	atomic_t<bool> g_home_menu_requested{false};
}

// RPCS3 normally owns this object from its Qt pad-settings translation unit.
// The emulator core only needs the configuration node itself in headless mode.
cfg_input_configurations g_cfg_input_configs;
std::string g_input_config_override;

pad_thread::pad_thread(void*, void*, std::string_view title_id)
{
	pad::g_title_id = title_id;
	pad::g_pad_thread = this;
	pad::g_started = false;
}

pad_thread::~pad_thread()
{
	pad::g_started = false;
	pad::g_pad_thread = nullptr;
}

void pad_thread::operator()()
{
	// VSH needs a live cellPad service during boot even when no physical
	// controller has been attached yet. Keep all ports present and disconnected,
	// matching RPCS3's NullPadHandler semantics without pulling Qt/HID handlers
	// into the portable host.
	for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; ++i)
	{
		m_pads[i] = std::make_shared<Pad>(
			pad_handler::null,
			i,
			i == 0 ? CELL_PAD_STATUS_CONNECTED : CELL_PAD_STATUS_DISCONNECTED,
			CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE | CELL_PAD_CAPABILITY_ACTUATOR,
			CELL_PAD_DEV_TYPE_STANDARD);
	}

	m_info.now_connect = 1;
	pad::g_started = true;
	if (std::getenv("VSHIFT_TRACE_PAD"))
	{
		std::fprintf(stderr, "VSHIFT_PAD_THREAD_READY ports=%zu\n", m_pads.size());
	}

	while (thread_ctrl::state() != thread_state::aborting)
	{
		thread_ctrl::wait_for(30'000);
	}

	pad::g_started = false;
}

void pad_thread::SetRumble(u32, u8, u8) {}
void pad_thread::SetIntercepted(bool) {}
s32 pad_thread::AddLddPad() { return -1; }
void pad_thread::UnregisterLddPad(u32) {}

// The bundled Windows FFmpeg archive is missing this private swscale table
// initializer. It is not needed by the null renderer boot path; keep the
// symbol local to the host adapter until the Metal frame path selects a real
// video conversion implementation.
extern "C" void ff_init_half2float_tables() {}

// Keep the archive's non-Qt input/config objects reachable before the core
// archive is scanned by the linker. Volatile storage prevents LTO/IPA from
// discarding these cross-archive references as unused.
volatile void* vshift_rpcs3_cfg_input_anchor = &g_cfg_input_configs;
volatile void* vshift_rpcs3_cfg_move_anchor = &g_cfg_move;
volatile auto vshift_rpcs3_product_anchor = &input::get_products_by_class;
volatile auto vshift_rpcs3_rgb_anchor = &ps_move_tracker<false>::rgb_to_hsv;
volatile auto vshift_rpcs3_hsv_anchor = &ps_move_tracker<false>::hsv_to_rgb;
volatile auto vshift_rpcs3_tracker_anchor = &ps_move_tracker<false>::set_image_data;

extern "C" void vshift_rpcs3_headless_anchor()
{
	(void)vshift_rpcs3_cfg_input_anchor;
	(void)vshift_rpcs3_cfg_move_anchor;
	(void)vshift_rpcs3_product_anchor;
	(void)vshift_rpcs3_rgb_anchor;
	(void)vshift_rpcs3_hsv_anchor;
	(void)vshift_rpcs3_tracker_anchor;
}
