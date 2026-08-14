#include "core/ps3/rpcs3_core.h"

#if defined(VSHIFT_HAS_RPCS3_CORE)

#include "Emu/Audio/Null/NullAudioBackend.h"
#include "Emu/Audio/Null/null_enumerator.h"
#include "Emu/Io/Null/null_camera_handler.h"
#include "Emu/Io/Null/null_music_handler.h"
#include "Emu/RSX/Null/NullGSRender.h"
#include "Emu/System.h"
#include "Emu/VFS.h"
#include "Emu/vfs_config.h"

#include <system_error>
#include <utility>

namespace {

std::string EnsureTrailingSeparator(std::filesystem::path path) {
    std::string value = path.lexically_normal().string();
    if (value.empty() || (value.back() != '/' && value.back() != '\\')) {
        value.push_back('/');
    }
    return value;
}

void InstallHeadlessCallbacks() {
    EmuCallbacks callbacks;
    callbacks.call_from_main_thread = [](std::function<void()> function,
                                         atomic_t<u32>* wake_up) {
        if (function) {
            function();
        }
        if (wake_up) {
            *wake_up = true;
            wake_up->notify_one();
        }
    };
    callbacks.on_run = [](bool) {};
    callbacks.on_pause = []() {};
    callbacks.on_resume = []() {};
    callbacks.on_stop = []() {};
    callbacks.on_ready = []() {};
    callbacks.on_missing_fw = []() {};
    callbacks.on_emulation_stop_no_response =
        [](std::shared_ptr<atomic_t<bool>>, int) {};
    callbacks.enable_disc_eject = [](bool) {};
    callbacks.enable_disc_insert = [](bool) {};
    callbacks.try_to_quit = [](bool force_quit, std::function<void()> on_exit) {
        if (force_quit && on_exit) {
            on_exit();
        }
        return force_quit;
    };
    callbacks.handle_taskbar_progress = [](s32, s32) {};
    callbacks.init_kb_handler = []() {};
    callbacks.init_mouse_handler = []() {};
    callbacks.init_pad_handler = [](std::string_view) {};
    callbacks.update_emu_settings = []() {};
    callbacks.save_emu_settings = []() {};
    callbacks.close_gs_frame = []() {};
    callbacks.get_gs_frame = []() -> std::unique_ptr<GSFrameBase> {
        return {};
    };
    callbacks.get_camera_handler =
        []() -> std::shared_ptr<camera_handler_base> {
        return std::make_shared<null_camera_handler>();
    };
    callbacks.get_music_handler = []() -> std::shared_ptr<music_handler_base> {
        return std::make_shared<null_music_handler>();
    };
    callbacks.init_gs_render = [](utils::serial* archive) {
        g_fxo->init<rsx::thread, named_thread<NullGSRender>>(archive);
    };
    callbacks.get_audio = []() -> std::shared_ptr<AudioBackend> {
        return std::make_shared<NullAudioBackend>();
    };
    callbacks.get_audio_enumerator =
        [](u64) -> std::shared_ptr<audio_device_enumerator> {
        return std::make_shared<null_enumerator>();
    };
    callbacks.get_msg_dialog = []() -> std::shared_ptr<MsgDialogBase> {
        return {};
    };
    callbacks.get_osk_dialog = []() -> std::shared_ptr<OskDialogBase> {
        return {};
    };
    callbacks.get_save_dialog = []() -> std::unique_ptr<SaveDialogBase> {
        return {};
    };
    callbacks.get_sendmessage_dialog =
        []() -> std::shared_ptr<SendMessageDialogBase> { return {}; };
    callbacks.get_recvmessage_dialog =
        []() -> std::shared_ptr<RecvMessageDialogBase> { return {}; };
    callbacks.get_trophy_notification_dialog =
        []() -> std::unique_ptr<TrophyNotificationBase> { return {}; };
    callbacks.get_localized_string =
        [](localized_string_id, const char*) { return std::string{}; };
    callbacks.get_localized_u32string =
        [](localized_string_id, const char*) { return std::u32string{}; };
    callbacks.get_localized_setting =
        [](const cfg::_base*, u32) { return std::string{}; };
    callbacks.get_photo_path = [](std::string_view) { return std::string{}; };
    callbacks.play_sound = [](const std::string&, std::optional<f32>) {};
    callbacks.get_image_info =
        [](const std::string&, std::string&, s32&, s32&, s32&) { return false; };
    callbacks.get_scaled_image = [](const std::string&, s32, s32, s32&, s32&,
                                    u8*, bool) { return false; };
    callbacks.resolve_path = [](std::string_view path) {
        return std::string{path};
    };
    callbacks.resolve_path_may_not_exist = callbacks.resolve_path;
    callbacks.get_font_dirs = []() { return std::vector<std::string>{}; };
    callbacks.on_install_pkgs = [](const std::vector<std::string>&) {
        return false;
    };
    callbacks.add_breakpoint = [](u32) {};
    callbacks.display_sleep_control_supported = []() { return false; };
    callbacks.enable_display_sleep = [](bool) {};
    callbacks.check_microphone_permissions = []() {};
    callbacks.make_video_source = []() -> std::unique_ptr<video_source> {
        return {};
    };
    callbacks.enable_gamemode = [](bool) {};
    callbacks.get_database_config = [](const std::string&) {
        return std::string{};
    };
    Emu.SetCallbacks(std::move(callbacks));
}

} // namespace

#endif

namespace vshift::ps3 {

Rpcs3Core::Rpcs3Core(CoreCallbacks callbacks)
    : callbacks_(std::move(callbacks)) {}

Rpcs3Core::~Rpcs3Core() {
    Stop();
}

BootReport Rpcs3Core::StartVsh(
    const std::filesystem::path& emulator_directory) {
    BootReport report;
    report.emulator_directory = emulator_directory;

#if defined(VSHIFT_HAS_RPCS3_CORE)
    std::error_code ec;
    std::filesystem::create_directories(emulator_directory, ec);
    if (ec) {
        report.error = "Could not create RPCS3 emulator directory: " +
                       ec.message();
        return report;
    }

    const auto emulator_root = EnsureTrailingSeparator(emulator_directory);
    InstallHeadlessCallbacks();

    // Set the user-owned root before Init(): RPCS3 uses it while creating and
    // resolving its VFS directories.
    g_cfg_vfs.emulator_dir.set(emulator_root);
    Emu.SetSupportedRenderers({video_renderer::null});
    Emu.SetDefaultRenderer(video_renderer::null);
    Emu.SetDefaultGraphicsAdapter("null");
    Emu.SetHasGui(false);
    Emu.SetHeadless(true);
    Emu.SetForceBoot(true);
    Emu.Init();

    // Init loads RPCS3's desktop config and restores the default VFS values;
    // re-apply the mobile sandbox root before remounting the guest devices.
    g_cfg_vfs.emulator_dir.set(emulator_root);

    std::filesystem::create_directories(emulator_directory / "dev_hdd0", ec);
    std::filesystem::create_directories(emulator_directory / "dev_hdd1", ec);
    std::filesystem::create_directories(emulator_directory / "dev_bdvd", ec);
    std::filesystem::create_directories(emulator_directory / "dev_usb000", ec);

    vfs::unmount("/dev_flash");
    vfs::unmount("/dev_flash2");
    vfs::unmount("/dev_flash3");
    vfs::unmount("/dev_hdd0");
    vfs::unmount("/dev_usb000");
    if (!vfs::mount("/dev_flash", g_cfg_vfs.get_dev_flash()) ||
        !vfs::mount("/dev_flash2", g_cfg_vfs.get_dev_flash2()) ||
        !vfs::mount("/dev_flash3", g_cfg_vfs.get_dev_flash3()) ||
        !vfs::mount("/dev_hdd0", g_cfg_vfs.get(g_cfg_vfs.dev_hdd0)) ||
        !vfs::mount("/dev_usb000",
                    g_cfg_vfs.get_device(g_cfg_vfs.dev_usb, "/dev_usb000").path)) {
        report.error = "RPCS3 VFS mount failed for the installed firmware";
        Emu.Kill(false);
        return report;
    }

    initialized_ = true;
    report.stage = BootStage::RuntimeInitialized;
    report.stage = BootStage::FirmwareReady;

    const auto boot_result = Emu.BootGame(
        "/dev_flash/vsh/module/vsh.self", "", true);
    if (boot_result != game_boot_result::no_errors) {
        report.error = "RPCS3 VSH boot failed (game_boot_result=";
        report.error += std::to_string(static_cast<std::uint32_t>(boot_result));
        report.error += ")";
        return report;
    }

    report.stage = BootStage::VshStarted;
    running_ = true;
    if (callbacks_.on_started) {
        callbacks_.on_started();
    }
    return report;
#else
    report.error = "RPCS3 core was not enabled in this build";
    return report;
#endif
}

bool Rpcs3Core::Pause() {
#if defined(VSHIFT_HAS_RPCS3_CORE)
    return running_ && Emu.Pause(true, false);
#else
    return false;
#endif
}

void Rpcs3Core::Resume() {
#if defined(VSHIFT_HAS_RPCS3_CORE)
    if (running_) {
        Emu.Resume();
    }
#endif
}

bool Rpcs3Core::MountIso(const std::filesystem::path& iso_path) {
#if defined(VSHIFT_HAS_RPCS3_CORE)
    if (!running_ || iso_path.empty()) {
        return false;
    }
    return Emu.InsertDisc(iso_path.string()) == game_boot_result::no_errors;
#else
    (void)iso_path;
    return false;
#endif
}

void Rpcs3Core::EjectIso() {
#if defined(VSHIFT_HAS_RPCS3_CORE)
    if (running_) {
        Emu.EjectDisc();
    }
#endif
}

void Rpcs3Core::Stop() {
#if defined(VSHIFT_HAS_RPCS3_CORE)
    if (running_ || initialized_) {
        Emu.Kill(false);
        running_ = false;
        initialized_ = false;
    }
#else
    running_ = false;
    initialized_ = false;
#endif

    if (callbacks_.on_stopped) {
        callbacks_.on_stopped();
    }
}

} // namespace vshift::ps3
