#include "Emu/System.h"
#include "Emu/RSX/GL/GLGSRender.h"
#if defined(HAVE_VULKAN)
#include "Emu/RSX/VK/VKGSRender.h"
#endif
#include "Emu/RSX/Null/NullGSRender.h"
#include "Emu/RSX/RSXThread.h"
#include "Emu/Io/Null/null_camera_handler.h"
#include "Emu/Io/Null/null_music_handler.h"
#include "Emu/Io/Null/NullKeyboardHandler.h"
#include "Emu/Io/Null/NullMouseHandler.h"
#include "Emu/Audio/Cubeb/CubebBackend.h"
#include "Emu/Audio/Null/NullAudioBackend.h"
#include "Emu/Audio/Null/null_enumerator.h"
#include "Emu/Memory/vm.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/vfs_config.h"
#include "Emu/Cell/Modules/cellSaveData.h"
#include "Emu/Cell/Modules/sceNpTrophy.h"
#include "Emu/Cell/Modules/cellMsgDialog.h"
#include "Emu/Cell/lv2/sys_sync.h"
#include "Input/pad_thread.h"
#include "util/video_source.h"

#include <iostream>
#include <filesystem>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <fstream>
#include <atomic>
#include <stdexcept>
#include <map>
#include <set>
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#endif
#include <exception>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
LONG CALLBACK vshift_exception_probe(EXCEPTION_POINTERS* info)
{
	if (!info || !info->ExceptionRecord || info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	void* frames[16]{};
	const USHORT count = CaptureStackBackTrace(0, 16, frames, nullptr);
	const auto image_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
	const auto exception_address = reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress);
	std::fprintf(stderr, "VSHIFT_FIRST_CHANCE_AV address=%p thread=%lu frames=%u\n",
		info->ExceptionRecord->ExceptionAddress, GetCurrentThreadId(), static_cast<unsigned>(count));
	std::fprintf(stderr, "VSHIFT_AV_IMAGE_BASE=%p offset=0x%llx\n", reinterpret_cast<void*>(image_base),
		static_cast<unsigned long long>(exception_address - image_base));
	if (info->ExceptionRecord->NumberParameters >= 2)
		std::fprintf(stderr, "VSHIFT_AV_ACCESS operation=%llu target=%p\n",
			static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[0]),
			reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
#if defined(_M_X64)
	if (info->ContextRecord)
		std::fprintf(stderr, "VSHIFT_AV_CONTEXT rip=%p rsp=%p rcx=%p rdx=%p\n",
			reinterpret_cast<void*>(info->ContextRecord->Rip),
			reinterpret_cast<void*>(info->ContextRecord->Rsp),
			reinterpret_cast<void*>(info->ContextRecord->Rcx),
			reinterpret_cast<void*>(info->ContextRecord->Rdx));
#endif
	for (USHORT i = 0; i < count; ++i)
		std::fprintf(stderr, "VSHIFT_AV_FRAME %u=%p\n", static_cast<unsigned>(i), frames[i]);
	std::fflush(stderr);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

extern "C" void vshift_rpcs3_headless_anchor();
extern const std::map<std::string_view, int> g_prx_list;

namespace
{
	std::mutex g_main_tasks_mutex;
	std::vector<std::function<void()>> g_main_tasks;
	std::thread::id g_main_thread_id;
	std::atomic<u64> g_flip_count{0};
	std::atomic<u32> g_main_task_trace_count{0};
	std::atomic_bool g_frame_captured{false};
	std::filesystem::path g_frame_path;
	bool g_use_gl = false;
	bool g_use_vulkan = false;

	class VshiftAutoMsgDialog final : public MsgDialogBase
	{
	public:
		void Create(const std::string& msg, const std::string& = "") override
		{
			std::fprintf(stderr, "VSHIFT_DIALOG_AUTO msg=%s\n", msg.c_str());
			state = MsgDialogState::Open;
			if (on_close)
				on_close(CELL_MSGDIALOG_BUTTON_OK);
		}
		void Close(bool success) override
		{
			state = MsgDialogState::Close;
			if (on_close)
				on_close(success ? CELL_MSGDIALOG_BUTTON_OK : CELL_MSGDIALOG_BUTTON_NONE);
		}
		void SetMsg(const std::string&) override {}
		void ProgressBarSetMsg(u32, const std::string&) override {}
		void ProgressBarReset(u32) override {}
		void ProgressBarInc(u32, u32) override {}
		void ProgressBarSetValue(u32, u32) override {}
		void ProgressBarSetLimit(u32, u32) override {}
	};

#ifdef _WIN32
	class HeadlessGLFrame : public GSFrameBase
	{
		HWND m_window = nullptr;
		HDC m_device = nullptr;
		HGLRC m_context = nullptr;

		static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
		{
			return DefWindowProcA(window, message, wparam, lparam);
		}

		void create_context()
		{
			std::fprintf(stderr, "VSHIFT_GL_FRAME create_context_begin\n");
			static constexpr char class_name[] = "VSHiftHeadlessGL";
			static std::once_flag class_once;
			std::call_once(class_once, [] {
				WNDCLASSA klass{};
				klass.lpfnWndProc = &HeadlessGLFrame::window_proc;
				klass.hInstance = GetModuleHandleA(nullptr);
				klass.lpszClassName = class_name;
				RegisterClassA(&klass);
			});

			m_window = CreateWindowExA(0, class_name, "VSHift PS3", WS_POPUP,
				0, 0, 1280, 720, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
			if (!m_window)
				throw std::runtime_error("CreateWindowExA failed");
			std::fprintf(stderr, "VSHIFT_GL_FRAME window=%p\n", static_cast<void*>(m_window));

			m_device = GetDC(m_window);
			PIXELFORMATDESCRIPTOR format{};
			format.nSize = sizeof(format);
			format.nVersion = 1;
			format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
			format.iPixelType = PFD_TYPE_RGBA;
			format.cColorBits = 32;
			const int pixel_format = ChoosePixelFormat(m_device, &format);
			if (!pixel_format || !SetPixelFormat(m_device, pixel_format, &format))
				throw std::runtime_error("SetPixelFormat failed");

			const HGLRC bootstrap = wglCreateContext(m_device);
			if (!bootstrap || !wglMakeCurrent(m_device, bootstrap))
				throw std::runtime_error("wglCreateContext failed");

			using create_context_proc = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
			auto create_context = reinterpret_cast<create_context_proc>(
				wglGetProcAddress("wglCreateContextAttribsARB"));
			if (create_context)
			{
				const int attributes[] = {
					0x2091, 4, // WGL_CONTEXT_MAJOR_VERSION_ARB
					0x2092, 3, // WGL_CONTEXT_MINOR_VERSION_ARB
					0x9126, 1, // WGL_CONTEXT_PROFILE_MASK_ARB / core
					0
				};
				m_context = create_context(m_device, nullptr, attributes);
			}
			if (!m_context)
				m_context = bootstrap;
			else
				wglDeleteContext(bootstrap);

			wglMakeCurrent(nullptr, nullptr);
			ShowWindow(m_window, SW_SHOW);
			UpdateWindow(m_window);
			std::fprintf(stderr, "VSHIFT_GL_FRAME context=%p\n", static_cast<void*>(m_context));
		}

	public:
		HeadlessGLFrame() { create_context(); }
		~HeadlessGLFrame() override { close(); }

		void close() override
		{
			if (m_context)
			{
				wglMakeCurrent(nullptr, nullptr);
				wglDeleteContext(m_context);
				m_context = nullptr;
			}
			if (m_device && m_window)
			{
				ReleaseDC(m_window, m_device);
				m_device = nullptr;
			}
			if (m_window)
			{
				DestroyWindow(m_window);
				m_window = nullptr;
			}
		}

		void reset() override {}
		bool shown() override { return true; }
		void hide() override { ShowWindow(m_window, SW_HIDE); }
		void show() override { ShowWindow(m_window, SW_SHOW); UpdateWindow(m_window); }
		void toggle_fullscreen() override {}
		void delete_context(draw_context_t) override {}
		draw_context_t make_context() override { return m_context; }
		void set_current(draw_context_t context) override
		{
			if (!wglMakeCurrent(m_device, static_cast<HGLRC>(context)))
				throw std::runtime_error("wglMakeCurrent failed");
		}
		void flip(draw_context_t, bool = false) override
		{
			glFlush();
			SwapBuffers(m_device);
		}
		int client_width() override { return 1280; }
		int client_height() override { return 720; }
		f64 client_display_rate() override { return 60.; }
		bool has_alpha() override { return false; }
		display_handle_t handle() const override { return m_window; }
		bool can_consume_frame() const override { return true; }
		void present_frame(std::vector<u8>&&, u32, u32, u32, bool) const override {}
		void take_screenshot(std::vector<u8>&&, u32, u32, bool) override {}
		void update_title(double = 0.) override {}
	};

	class VshiftGLGSRender : public GLGSRender
	{
		std::chrono::steady_clock::time_point m_capture_after{};

		void capture_current_frame()
		{
			if (g_frame_path.empty() || g_frame_captured.load())
				return;

			constexpr u32 width = 1280;
			constexpr u32 height = 720;
			std::vector<u8> pixels(width * height * 4);
			glReadBuffer(GL_BACK);
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			std::ofstream output(g_frame_path, std::ios::binary);
			if (!output)
				return;
			output << "P6\n" << width << ' ' << height << "\n255\n";
			for (u32 y = 0; y < height; ++y)
			{
				const u32 source_y = height - 1 - y;
				for (u32 x = 0; x < width; ++x)
				{
					const auto* pixel = pixels.data() + (source_y * width + x) * 4;
					output.put(static_cast<char>(pixel[0]));
					output.put(static_cast<char>(pixel[1]));
					output.put(static_cast<char>(pixel[2]));
				}
			}
			g_frame_captured.store(true);
			std::fprintf(stderr, "VSHIFT_GL_FRAME_CAPTURED path=%s\n", g_frame_path.string().c_str());
		}

	public:
		using GLGSRender::GLGSRender;

	protected:
		void on_init_thread() override
		{
			std::fprintf(stderr, "VSHIFT_GL_RENDER init_begin\n");
			GLGSRender::on_init_thread();
			m_capture_after = std::chrono::steady_clock::now() + std::chrono::seconds(2);
			std::fprintf(stderr, "VSHIFT_GL_RENDER init_end\n");
		}

		void end() override
		{
			GLGSRender::end();
			if (std::getenv("VSHIFT_CAPTURE_ON_END") && std::chrono::steady_clock::now() >= m_capture_after)
				capture_current_frame();
		}

		void flip(const rsx::display_flip_info_t& info) override
		{
			++g_flip_count;
			GLGSRender::flip(info);
			capture_current_frame();
		}
	};
#endif

#if defined(HAVE_VULKAN)
	class VshiftVKGSRender : public VKGSRender
	{
	public:
		using VKGSRender::VKGSRender;

		void flip(const rsx::display_flip_info_t& info) override
		{
			++g_flip_count;
			VKGSRender::flip(info);
		}
	};
#endif

	class VshiftNullGSRender : public NullGSRender
	{
	public:
		using NullGSRender::NullGSRender;

		void flip(const rsx::display_flip_info_t& info) override
		{
			++g_flip_count;
			const u32 index = info.buffer % rsx::limits::color_buffers_count;
			const auto& surface = m_surface_info[index];
			std::fprintf(stderr, "VSHIFT_FLIP index=%u address=0x%08x size=%ux%u pitch=%u format=%u\n",
				index, surface.address, surface.width, surface.height, surface.pitch,
				static_cast<unsigned>(surface.color_format));

			if (g_frame_path.empty() || g_frame_captured.exchange(true) ||
				surface.address == 0 || surface.width == 0 || surface.height == 0 ||
				surface.pitch < surface.width * 4 ||
				!vm::check_addr(surface.address, vm::page_readable, surface.pitch * surface.height))
			{
				return;
			}

			std::ofstream output(g_frame_path, std::ios::binary);
			if (!output)
				return;
			output << "P6\n" << surface.width << ' ' << surface.height << "\n255\n";
			const auto* source = static_cast<const u8*>(vm::base(surface.address));
			for (u32 y = 0; y < surface.height; ++y)
			{
				for (u32 x = 0; x < surface.width; ++x)
				{
					const auto* pixel = source + y * surface.pitch + x * 4;
					output.put(static_cast<char>(pixel[2]));
					output.put(static_cast<char>(pixel[1]));
					output.put(static_cast<char>(pixel[0]));
				}
			}
		}
	};

	void pump_main_tasks()
	{
		std::vector<std::function<void()>> tasks;
		{
			std::lock_guard lock(g_main_tasks_mutex);
			tasks.swap(g_main_tasks);
		}
		for (auto& task : tasks)
		{
			if (task)
			{
				if (std::getenv("VSHIFT_TRACE_MAIN_TASK") && g_main_task_trace_count.fetch_add(1) < 32)
					std::fprintf(stderr, "VSHIFT_MAIN_TASK run queued\n");
				task();
			}
		}
	}

#ifdef _WIN32
	void pump_window_messages()
	{
		MSG message{};
		while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				continue;
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
	}
#else
	void pump_window_messages() {}
#endif
}

int main(int argc, char** argv) {
	g_main_thread_id = std::this_thread::get_id();
#ifdef _WIN32
	AddVectoredExceptionHandler(1, &vshift_exception_probe);
#endif
	std::set_terminate([] {
		std::fprintf(stderr, "VSHIFT_TERMINATE\n");
		std::fflush(stderr);
		std::abort();
	});
#ifdef _WIN32
	SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* info) -> LONG {
		const auto code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
		const auto address = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;
		std::fprintf(stderr, "VSHIFT_EXCEPTION code=0x%08lx address=%p\n", static_cast<unsigned long>(code), address);
		std::fflush(stderr);
		return EXCEPTION_EXECUTE_HANDLER;
	});
#endif
	// Force the small non-Qt host glue archive into the link.
	vshift_rpcs3_headless_anchor();
    if (argc < 2 || argc > 3) {
        std::cerr << "usage: vshift_rpcs3_boot_probe <dev_flash-or-game-path> [--gl|--vulkan]\n";
        return 2;
    }
    g_use_gl = argc == 3 && std::string_view(argv[2]) == "--gl";
    g_use_vulkan = argc == 3 && std::string_view(argv[2]) == "--vulkan";

    // RPCS3's headless fixup forcibly changes every renderer to Null. Keep
    // the standalone window host non-headless for the real GL path while
    // retaining headless mode for the Null probe.
    const bool use_window = g_use_gl || g_use_vulkan;
    Emu.SetHeadless(!use_window);
    Emu.SetHasGui(use_window);
    if (!std::getenv("VSHIFT_NO_CONTINUOUS"))
    {
        Emu.SetContinuousMode(true);
    }
    EmuCallbacks callbacks{};
	callbacks.call_from_main_thread = [](std::function<void()> task, atomic_t<u32>* wake_up) {
		if (!task)
			return;
		if (std::getenv("VSHIFT_TRACE_MAIN_TASK") && g_main_task_trace_count.fetch_add(1) < 32)
			std::fprintf(stderr, "VSHIFT_MAIN_TASK queue same=%s wake=%p\n",
				std::this_thread::get_id() == g_main_thread_id ? "true" : "false", static_cast<void*>(wake_up));
		auto run_task = [task = std::move(task), wake_up]() mutable {
			task();
			if (wake_up)
			{
				*wake_up = 1;
				wake_up->notify_one();
			}
			if (std::getenv("VSHIFT_TRACE_MAIN_TASK") && g_main_task_trace_count.fetch_add(1) < 32)
				std::fprintf(stderr, "VSHIFT_MAIN_TASK done wake=%p\n", static_cast<void*>(wake_up));
		};
		if (std::this_thread::get_id() == g_main_thread_id)
		{
			run_task();
			return;
		}
		std::lock_guard lock(g_main_tasks_mutex);
		g_main_tasks.emplace_back(std::move(run_task));
	};
    callbacks.on_run = [](bool) {};
    callbacks.on_pause = [] {};
    callbacks.on_resume = [] {};
    callbacks.on_stop = [] {};
    callbacks.on_ready = [] {};
    callbacks.on_missing_fw = [] {};
    callbacks.on_emulation_stop_no_response = [](std::shared_ptr<atomic_t<bool>>, int) {};
    callbacks.on_save_state_progress = [](std::shared_ptr<atomic_t<bool>>, stx::shared_ptr<utils::serial>, stx::atomic_ptr<std::string>*, std::shared_ptr<void>) {};
    callbacks.enable_disc_eject = [](bool) {};
    callbacks.enable_disc_insert = [](bool) {};
    callbacks.enable_display_sleep = [](bool) {};
    callbacks.enable_gamemode = [](bool) {};
    callbacks.try_to_quit = [](bool, std::function<void()> on_exit) { if (on_exit) on_exit(); return true; };
    callbacks.handle_taskbar_progress = [](s32, s32) {};
    callbacks.init_kb_handler = [] {
		ensure(g_fxo->init<KeyboardHandlerBase, NullKeyboardHandler>(Emu.DeserialManager()));
	};
    callbacks.init_mouse_handler = [] {
		ensure(g_fxo->init<MouseHandlerBase, NullMouseHandler>(Emu.DeserialManager()));
    };
	callbacks.init_pad_handler = [](std::string_view title_id) {
		// VSH expects the same pad service that the normal RPCS3 host creates.
		// Keep the host window/Qt objects out of this probe, but do not omit the
		// emulated cellPad service itself: VSH queries it during startup before
		// entering the XMB video path.
		std::fprintf(stderr, "VSHIFT_PAD_INIT title=%.*s\n",
			static_cast<int>(title_id.size()), title_id.data());
		ensure(g_fxo->init<named_thread<pad_thread>>(nullptr, nullptr, title_id));
		// The normal RPCS3 host does not return until the pad thread has
		// published its ports. VSH queries cellPad during the same startup
		// window, so returning immediately creates a subtle first-boot race.
		for (u32 i = 0; i < 5000 && !pad::g_started; ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		if (std::getenv("VSHIFT_TRACE_PAD"))
			std::fprintf(stderr, "VSHIFT_PAD_INIT_READY=%s\n", pad::g_started ? "true" : "false");
	};
    callbacks.close_gs_frame = [] {};
    callbacks.get_gs_frame = [] {
#ifdef _WIN32
        if (g_use_gl || g_use_vulkan)
            return std::unique_ptr<GSFrameBase>{std::make_unique<HeadlessGLFrame>()};
#else
#endif
        return std::unique_ptr<GSFrameBase>{};
    };
    callbacks.get_camera_handler = [] { return std::shared_ptr<camera_handler_base>{std::make_shared<null_camera_handler>()}; };
    callbacks.get_music_handler = [] { return std::shared_ptr<music_handler_base>{std::make_shared<null_music_handler>()}; };
    callbacks.init_gs_render = [](utils::serial* ar) {
#ifdef _WIN32
        if (g_use_gl) {
			g_fxo->init<rsx::thread, named_thread<VshiftGLGSRender>>(ar);
            return;
        }
#if defined(HAVE_VULKAN)
		if (g_use_vulkan) {
			g_fxo->init<rsx::thread, named_thread<VshiftVKGSRender>>(ar);
			return;
		}
#endif
#endif
        g_fxo->init<rsx::thread, named_thread<VshiftNullGSRender>>(ar);
    };
    callbacks.get_audio = [] {
        if (std::getenv("VSHIFT_REAL_AUDIO"))
            return std::shared_ptr<AudioBackend>{std::make_shared<CubebBackend>()};
        return std::shared_ptr<AudioBackend>{std::make_shared<NullAudioBackend>()};
    };
    callbacks.get_audio_enumerator = [](u64) { return std::shared_ptr<audio_device_enumerator>{std::make_shared<null_enumerator>()}; };
    callbacks.get_msg_dialog = [] {
        if (std::getenv("VSHIFT_AUTO_DIALOG"))
            return std::shared_ptr<MsgDialogBase>{std::make_shared<VshiftAutoMsgDialog>()};
        return std::shared_ptr<MsgDialogBase>{};
    };
    callbacks.get_osk_dialog = [] { return std::shared_ptr<OskDialogBase>{}; };
    callbacks.get_save_dialog = [] { return std::unique_ptr<SaveDialogBase>{}; };
	callbacks.get_trophy_notification_dialog = [] { return std::unique_ptr<TrophyNotificationBase>{}; };
	// These callbacks are normally supplied by main_application. Keep them
	// non-empty in the portable host because System::Run can invoke them while
	// applying VSH-specific settings before the first video submission.
	callbacks.update_emu_settings = [] {
		if (std::getenv("VSHIFT_TRACE_SETTINGS"))
			std::fprintf(stderr, "VSHIFT_SETTINGS_UPDATE\n");
	};
	callbacks.save_emu_settings = [] {
		if (std::getenv("VSHIFT_TRACE_SETTINGS"))
			std::fprintf(stderr, "VSHIFT_SETTINGS_SAVE\n");
	};
	// The RPCS3 progress server uses a non-empty localized string as its
	// start condition. Returning an empty string leaves g_progr_ptotal alive
	// forever and blocks PPU initialization before the first guest instruction.
	callbacks.get_localized_string = [](localized_string_id, const char*) {
		return std::string{"PS3 system loading"};
	};
	callbacks.get_localized_u32string = [](localized_string_id, const char*) {
		return std::u32string{U"PS3 system loading"};
	};
	callbacks.get_localized_setting = [](const cfg::_base*, u32) { return std::string{}; };
	callbacks.play_sound = [](const std::string&, std::optional<f32>) {};
	callbacks.add_breakpoint = [](u32) {};
	callbacks.display_sleep_control_supported = [] { return false; };
	callbacks.check_microphone_permissions = [] {};
    callbacks.make_video_source = [] { return std::unique_ptr<video_source>{}; };
    Emu.SetCallbacks(std::move(callbacks));

    Emu.Init();
    const std::filesystem::path supplied_root = std::filesystem::path(argv[1]);
    const std::filesystem::path dev_flash =
        std::filesystem::is_directory(supplied_root / "dev_flash") ? supplied_root / "dev_flash" : supplied_root;
    const auto vfs_root = dev_flash.parent_path();
    // Keep the persisted VFS portable and let RPCS3 resolve all devices from
    // one emulator directory during its internal second Init().
    g_cfg_vfs.emulator_dir.set(vfs_root.string() + "/");
    g_cfg_vfs.dev_flash.set("$(EmulatorDir)dev_flash");
    g_cfg_vfs.dev_flash2.set("$(EmulatorDir)dev_flash2");
    g_cfg_vfs.dev_flash3.set("$(EmulatorDir)dev_flash3");
    g_cfg_vfs.dev_hdd0.set("$(EmulatorDir)dev_hdd0");
    g_cfg_vfs.dev_hdd1.set("$(EmulatorDir)dev_hdd1");
    g_cfg_vfs.app_home.set("$(EmulatorDir)dev_flash/vsh/module");
    const auto dev_flash2 = vfs_root / "dev_flash2";
	const auto dev_flash3 = vfs_root / "dev_flash3";
	const auto dev_usb000 = vfs_root / "dev_usb000";
	const auto dev_bdvd = vfs_root / "dev_bdvd";
	const auto games = vfs_root / "games";
	std::filesystem::create_directories(dev_flash2);
	std::filesystem::create_directories(dev_flash3);
	std::filesystem::create_directories(dev_usb000);
	std::filesystem::create_directories(dev_bdvd);
	std::filesystem::create_directories(games);
    std::filesystem::create_directories(g_cfg_vfs.dev_hdd0.to_string());
    std::filesystem::create_directories(g_cfg_vfs.dev_hdd1.to_string());
    // BootGame performs its own Init(), so persist the VFS selection for that
    // second initialization instead of relying on in-memory cfg values.
    g_cfg_vfs.save();
    // A fresh firmware root still needs RPCS3's standard dev_hdd0/dev_usb
    // layout for VSH's first-run registry and user initialization.
    g_cfg.vfs.init_dirs.set(true);
    // Match the GUI Boot VSH path: Run() must release the guest from the
    // global debug pause. Set VSHIFT_NO_AUTOSTART only when reproducing the
    // old diagnostic behavior.
    g_cfg.misc.autostart.set(!std::getenv("VSHIFT_NO_AUTOSTART"));
    // The standalone host has no GUI shader compiler service. Keep the first
    // real frame on the renderer thread until the host-side context bridge is
    // proven; async pipe compilation can otherwise wait forever on a second
    // context in this minimal window host.
    if (use_window)
    {
        const bool official_profile = std::getenv("VSHIFT_OFFICIAL_PROFILE") != nullptr;
        g_cfg.video.renderer.set(g_use_vulkan ? video_renderer::vulkan : video_renderer::opengl);
        if (official_profile)
        {
            // Match the settings emitted by RPCS3's working Boot VSH path.
            // Keep this as an opt-in profile so the existing GL diagnostics
            // remain reproducible while the host bridge is being developed.
            // Explicitly clear state persisted by earlier diagnostic runs:
            // those runs used the static PPU decoder and forced every system
            // PRX to LLE, which lets VSH start but prevents its normal
            // video-out initialization path from completing.
            g_cfg.core.ppu_decoder.set(ppu_decoder_type::llvm);
            // Do not inherit the dynamic SPU interpreter from a previous
            // diagnostic run. The real RPCS3 VSH profile uses the LLVM SPU
            // recompiler as well; SPU services are part of VSH startup and
            // can gate the first video-out submission.
            g_cfg.core.spu_decoder.set(spu_decoder_type::llvm);
            // Precompilation is useful in the full RPCS3 GUI, but its worker
            // dialog/cache service is not present in this small host. LLVM
            // still JIT-compiles every executed block on demand.
            g_cfg.core.llvm_precompilation.set(false);
            g_cfg.core.spu_cache.set(true);
            g_cfg.core.libraries_control.set_set({});
            g_cfg.core.debug_console_mode.set(false);
            if (std::getenv("VSHIFT_GCM_LLE"))
            {
                g_cfg.core.libraries_control.set_set({"libgcm_sys.sprx:lle"});
            }
            // The Vulkan GUI profile uses the interpreter-assisted mode, but
            // the minimal WGL host cannot precompile its interpreter variants
            // without the full RPCS3 shader-dialog service. Keep compilation
            // asynchronous here so VSH can reach its first video-out frame.
            g_cfg.video.shadermode.set(shader_mode::async_recompiler);
            g_cfg.video.renderdoc_compatiblity.set(false);
            g_cfg.video.force_cpu_blit_processing.set(false);
            g_cfg.video.write_color_buffers.set(false);
            g_cfg.video.read_color_buffers.set(false);
            g_cfg.video.disable_video_output.set(false);
            g_cfg.video.strict_rendering_mode.set(false);
            g_cfg.video.vblank_rate.set(60);
            g_cfg.video.vsync.set(vsync_mode::off);
            g_cfg.video.multithreaded_rsx.set(false);
            g_cfg.core.rsx_fifo_accuracy.set(rsx_fifo_mode::atomic);
        }
        else
        {
            g_cfg.video.shadermode.set(std::getenv("VSHIFT_GL_ASYNC")
                ? shader_mode::async_recompiler
                : shader_mode::recompiler);
            // Match the known RPCS3 VSH profile. RenderDoc compatibility changes
            // the GL buffer path and is opt-in for diagnostics only.
            g_cfg.video.renderdoc_compatiblity.set(std::getenv("VSHIFT_RENDERDOC") != nullptr);
            g_cfg.video.force_cpu_blit_processing.set(true);
            // RPCS3's VSH/XMB profile requires color-buffer writeback for the
            // background wave and framebuffer-backed presentation surfaces.
            g_cfg.video.write_color_buffers.set(true);
            g_cfg.video.read_color_buffers.set(false);
            g_cfg.video.disable_video_output.set(false);
            g_cfg.video.strict_rendering_mode.set(false);
            g_cfg.video.vblank_rate.set(60);
            g_cfg.video.vsync.set(vsync_mode::full);
            g_cfg.video.multithreaded_rsx.set(true);
            // This is the FIFO mode used by the older standalone diagnostic.
            g_cfg.core.rsx_fifo_accuracy.set(rsx_fifo_mode::fast);
        }
    }
    // RPCS3's VSH profile requires two PPU workers. Keep it explicit so the
    // standalone host does not inherit a stale persisted value.
    g_cfg.core.ppu_threads.set(2);
    if (std::getenv("VSHIFT_PPU_STATIC"))
        g_cfg.core.ppu_decoder.set(ppu_decoder_type::_static);
    if (std::getenv("VSHIFT_FORCE_LLE"))
    {
        std::set<std::string> libraries;
        for (const auto& library : g_prx_list)
            libraries.emplace(std::string(library.first) + ":lle");
        g_cfg.core.libraries_control.set_set(std::move(libraries));
    }
    if (std::getenv("VSHIFT_DEBUG_CONSOLE"))
        g_cfg.core.debug_console_mode.set(true);
    Emu.SaveSettings(g_cfg.to_string(), {});
    const auto vsh_path = (dev_flash / "vsh/module/vsh.self").string();
    const auto vsh_boot_path = std::getenv("VSHIFT_VSH_PATH")
        ? std::string(std::getenv("VSHIFT_VSH_PATH"))
        : (std::getenv("VSHIFT_BOOT_VSH_DIR")
            ? (dev_flash / "vsh/module").string()
            : vsh_path);
    g_frame_path = vfs_root / "vshift_ps3_frame.ppm";
    std::cout << "vfs_dev_flash=" << g_cfg_vfs.get_dev_flash()
              << " lv2_exists=" << (std::filesystem::is_regular_file(
                  std::filesystem::path(g_cfg_vfs.get_dev_flash()) / "sys/external/liblv2.sprx") ? "true" : "false")
              << " vsh_exists=" << (std::filesystem::is_regular_file(vsh_path) ? "true" : "false")
              << std::endl;
    const bool gui_boot = std::getenv("VSHIFT_GUI_BOOT") != nullptr;
    const bool soft_boot = std::getenv("VSHIFT_SOFT_BOOT") != nullptr;
    const bool direct_boot = !gui_boot && !soft_boot;
    const auto boot_mode = gui_boot ? cfg_mode::custom : cfg_mode::continuous;
    if (gui_boot)
    {
        // Match main_window::BootVSH(): custom BootGame is preceded by the
        // normal graceful shutdown/state transition of the GUI host.
        Emu.GracefulShutdown(false);
    }
    const auto result = Emu.BootGame(vsh_boot_path, "", direct_boot, boot_mode);
    std::cout << "boot_result=" << static_cast<int>(result)
              << " state=" << static_cast<int>(Emu.GetStatus(false)) << std::endl;

    if (result != game_boot_result::no_errors) {
        Emu.CleanUp();
        return 1;
    }

    std::cout << "before_run" << std::endl;
    if (!g_cfg.misc.autostart.get())
    {
        Emu.Run(true);
    }
    pump_main_tasks();
    std::cout << "running=" << (Emu.IsRunning() ? "true" : "false") << std::endl;
    const int wait_seconds = use_window
        ? (std::getenv("VSHIFT_GL_WAIT_SECONDS") ? std::max(1, std::atoi(std::getenv("VSHIFT_GL_WAIT_SECONDS"))) : 60)
        : 10;
    const int wait_iterations = wait_seconds * 10;
    bool finalized_start = false;
    for (int i = 0; i < wait_iterations; ++i) {
		pump_window_messages();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		pump_window_messages();
		pump_main_tasks();
		if (!finalized_start && Emu.IsStarting())
		{
			if (auto* render = rsx::get_current_renderer(); render && render->is_initialized)
			{
				// The regular Qt event loop completes this transition after the
				// RSX thread publishes readiness. Keep the same invariant here.
				Emu.FinalizeRunRequest();
				finalized_start = true;
			}
		}
	}
    std::cout << "state_after_" << wait_seconds << "s"
              << "=" << static_cast<int>(Emu.GetStatus(false))
              << " flips=" << g_flip_count.load()
              << " frame=" << (std::filesystem::is_regular_file(g_frame_path) ? "captured" : "not-captured")
              << std::endl;
    if (auto* render = rsx::get_current_renderer(); render && render->ctrl)
    {
        std::fprintf(stderr, "VSHIFT_RSX_FINAL_CTRL get=0x%x put=0x%x ref=0x%x\n",
            +render->ctrl->get, +render->ctrl->put, +render->ctrl->ref);
    }
    idm::select<named_thread<ppu_thread>>([](u32 id, named_thread<ppu_thread>& thread) {
        std::fprintf(stderr, "VSHIFT_PPU_THREAD id=0x%x cia=0x%x exec_bytes=%llu stopped=%s func=%s\\n",
            id, thread.cia, static_cast<unsigned long long>(thread.exec_bytes),
            thread.is_stopped() ? "true" : "false", thread.current_function ? thread.current_function : "-");
    });
#ifdef _WIN32
    // The probe is intentionally a one-shot boot measurement. RPCS3's normal
    // GUI host owns shutdown on its main event loop; do not tear down that
    // graph from this minimal process after the measurement.
    TerminateProcess(GetCurrentProcess(), 0);
#else
    std::_Exit(0);
#endif
}
