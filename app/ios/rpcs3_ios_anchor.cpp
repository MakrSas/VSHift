// iOS link boundary for the full RPCS3 emulator archive.
//
// This file deliberately contains no Qt or Win32 code.  It forces the
// platform-independent RPCS3 archive to be linked into the iOS executable;
// the runtime/render callbacks are installed by the native iOS bridge in the
// next stage.  Keeping this boundary separate prevents the existing iOS UI
// target from accidentally pulling the desktop frontend.

extern "C" void vshift_rpcs3_headless_anchor();

extern "C" void vshift_rpcs3_ios_link_core()
{
    vshift_rpcs3_headless_anchor();
}
