#include "Emu/Cell/lv2/sys_usbd.h"

#if defined(VSHIFT_RPCS3_HEADLESS)

error_code sys_usbd_initialize(ppu_thread&, vm::ptr<u32>) { return CELL_ENOSYS; }
error_code sys_usbd_finalize(ppu_thread&, u32) { return CELL_ENOSYS; }
error_code sys_usbd_get_device_list(ppu_thread&, u32, vm::ptr<UsbInternalDevice>, u32) { return CELL_ENOSYS; }
error_code sys_usbd_get_descriptor_size(ppu_thread&, u32, u32) { return CELL_ENOSYS; }
error_code sys_usbd_get_descriptor(ppu_thread&, u32, u32, vm::ptr<void>, u32) { return CELL_ENOSYS; }
error_code sys_usbd_register_ldd(ppu_thread&, u32, vm::cptr<char>, u16) { return CELL_ENOSYS; }
error_code sys_usbd_unregister_ldd(ppu_thread&, u32, vm::cptr<char>, u16) { return CELL_ENOSYS; }
error_code sys_usbd_open_pipe(ppu_thread&, u32, u32, u32, u64, u64, u32, u64) { return CELL_ENOSYS; }
error_code sys_usbd_open_default_pipe(ppu_thread&, u32, u32) { return CELL_ENOSYS; }
error_code sys_usbd_close_pipe(ppu_thread&, u32, u32) { return CELL_ENOSYS; }
error_code sys_usbd_receive_event(ppu_thread&, u32, vm::ptr<u64>, vm::ptr<u64>, vm::ptr<u64>) { return CELL_ENOSYS; }
error_code sys_usbd_detect_event(ppu_thread&) { return CELL_ENOSYS; }
error_code sys_usbd_attach(ppu_thread&, u32, u32, u32, u32) { return CELL_ENOSYS; }
error_code sys_usbd_transfer_data(ppu_thread&, u32, u32, vm::ptr<u8>, u32, vm::ptr<UsbDeviceRequest>, u32) { return CELL_ENOSYS; }
error_code sys_usbd_isochronous_transfer_data(ppu_thread&, u32, u32, vm::ptr<UsbDeviceIsoRequest>) { return CELL_ENOSYS; }
error_code sys_usbd_get_transfer_status(ppu_thread&, u32, u32, u32, vm::ptr<u32>, vm::ptr<u32>) { return CELL_ENOSYS; }
error_code sys_usbd_get_isochronous_transfer_status(ppu_thread&, u32, u32, u32, vm::ptr<UsbDeviceIsoRequest>, vm::ptr<u32>) { return CELL_ENOSYS; }
error_code sys_usbd_get_device_location(ppu_thread&, u32, u32, vm::ptr<u8>) { return CELL_ENOSYS; }
error_code sys_usbd_send_event(ppu_thread&) { return CELL_ENOSYS; }
error_code sys_usbd_event_port_send(ppu_thread&, u32, u64, u64, u64) { return CELL_ENOSYS; }
error_code sys_usbd_allocate_memory(ppu_thread&) { return CELL_ENOSYS; }
error_code sys_usbd_free_memory(ppu_thread&) { return CELL_ENOSYS; }
error_code sys_usbd_get_device_speed(ppu_thread&) { return CELL_ENOSYS; }
error_code sys_usbd_register_extra_ldd(ppu_thread&, u32, vm::cptr<char>, u16, u16, u16, u16) { return CELL_ENOSYS; }
error_code sys_usbd_unregister_extra_ldd(ppu_thread&, u32, vm::cptr<char>, u16) { return CELL_ENOSYS; }

void connect_usb_controller(u8, input::product_type) {}
void reconnect_usb(u32) {}
void handle_hotplug_event(bool, bool) {}

#endif
