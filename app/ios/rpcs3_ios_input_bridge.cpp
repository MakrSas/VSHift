#include "stdafx.h"

#include "Emu/Cell/SPURecompiler.h"
#include "Emu/Cell/Modules/cellAudioIn.h"
#include "Emu/Cell/Modules/cellGem.h"
#include "Input/pad_thread.h"

#include <cstdlib>

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

// RPCS3 normally supplies these from its Qt application. VSHift deliberately
// links the emulator core without Qt on iOS, so keep the core's wait/fatal
// hooks platform-neutral until the native frontend bridge replaces them.
void qt_events_aware_op(int /*repeat_duration_ms*/, std::function<bool()> wrapped_op)
{
	if (wrapped_op)
	{
		wrapped_op();
	}
}

[[noreturn]] void report_fatal_error(std::string_view /*text*/, bool /*is_html*/, bool /*include_help_text*/)
{
	std::abort();
}

void spu_llvm_set_compile_context(spu_llvm_compile_context* /*context*/) noexcept
{
	// LLVM is intentionally not part of the first iOS device profile. The
	// portable SPU path still references this ARM64 hook through its common
	// recompiler interface, so provide the no-LLVM context boundary here.
}

template <>
void fmt_class_string<CellAudioInError>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto) { return "CELL_AUDIO_IN_UNAVAILABLE"; });
}

template <>
void fmt_class_string<CellGemError>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto) { return "CELL_GEM_UNAVAILABLE"; });
}

void pad_thread::SetRumble(u32 /*pad*/, u8 /*large_motor*/, u8 /*small_motor*/)
{
	// The UIKit haptic/rumble adapter will be connected here in the input
	// milestone. A missing physical pad must not block VSH boot.
}

void pad_thread::SetIntercepted(bool intercepted)
{
	if (intercepted)
	{
		m_info.system_info |= CELL_PAD_INFO_INTERCEPTED;
		m_info.ignore_input = true;
	}
	else
	{
		m_info.system_info &= ~CELL_PAD_INFO_INTERCEPTED;
		m_info.ignore_input = false;
	}
}

s32 pad_thread::AddLddPad()
{
	return -1;
}

void pad_thread::UnregisterLddPad(u32 /*handle*/)
{
}
