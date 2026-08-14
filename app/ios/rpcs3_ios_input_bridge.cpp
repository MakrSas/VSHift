#include "stdafx.h"

#include "Input/pad_thread.h"

// The first iOS boot target does not start RPCS3's desktop pad thread.  Keep
// the core's shared input state available to HLE modules until the native
// touch/DualShock bridge is attached by the frontend.
namespace pad
{
	atomic_t<pad_thread*> g_pad_thread = nullptr;
	shared_mutex g_pad_mutex;
	std::string g_title_id;
	atomic_t<bool> g_enabled{true};
	atomic_t<bool> g_reset{false};
	atomic_t<bool> g_started{false};
	atomic_t<bool> g_home_menu_requested{false};
}
