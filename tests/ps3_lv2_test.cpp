#include "core/cpu/ppu_runtime.h"
#include "core/hle/ps3_lv2.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

void WriteInstruction(vshift::memory::GuestMemory& memory,
                      std::uint64_t address,
                      std::uint32_t instruction) {
    const std::array<std::uint8_t, 4> bytes{
        static_cast<std::uint8_t>(instruction >> 24),
        static_cast<std::uint8_t>(instruction >> 16),
        static_cast<std::uint8_t>(instruction >> 8),
        static_cast<std::uint8_t>(instruction)};
    assert(memory.Initialize(address, bytes).ok());
}

void WriteU32(vshift::memory::GuestMemory& memory,
              std::uint64_t address,
              std::uint32_t value) {
    WriteInstruction(memory, address, value);
}

std::uint32_t Addi(unsigned rt, unsigned ra, std::int16_t value) {
    return (0x0eu << 26) | (rt << 21) | (ra << 16) |
           static_cast<std::uint16_t>(value);
}

std::uint32_t Sc() { return 0x44000002u; }

} // namespace

int main() {
    vshift::memory::GuestMemory memory;
    assert(memory.Map({0x1000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionExecute}).ok());
    assert(memory.Map({0x2000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionWrite}).ok());
    assert(memory.Map({0x4000, 0x1000, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(memory, 0x1000, Addi(11, 0, 100));
    WriteInstruction(memory, 0x1004, Sc());
    WriteInstruction(memory, 0x1008, Addi(11, 0, 101));
    WriteInstruction(memory, 0x100c, Addi(3, 0, 0));
    WriteInstruction(memory, 0x1010, Sc());

    vshift::hle::Ps3Lv2 linker(memory);

    // Variable imports use relocation-reference lists, not the function stub
    // table. Linking the exported variable must patch each guest reference.
    WriteU32(memory, 0x4000, 0x2c000001);
    WriteU32(memory, 0x4004, 0);
    WriteU32(memory, 0x4008, 0x00010000);
    WriteU32(memory, 0x4010, 0x4200);
    WriteU32(memory, 0x401c, 0x4210);
    WriteU32(memory, 0x4020, 0x4220);
    const std::array<std::uint8_t, 8> variable_module_name{
        'v', 'a', 'r', '_', 't', 'e', 's', 't'};
    assert(memory.Initialize(0x4200, variable_module_name).ok());
    const std::array<std::uint8_t, 1> terminator{0};
    assert(memory.Initialize(0x4208, terminator).ok());
    WriteU32(memory, 0x4210, 0xdeadbeef);
    WriteU32(memory, 0x4220, 0x4230);
    WriteU32(memory, 0x4230, 1);
    WriteU32(memory, 0x4234, 0x4240);
    WriteU32(memory, 0x4238, 4);
    WriteU32(memory, 0x423c, 0);
    WriteU32(memory, 0x4100, 0x2c000001);
    WriteU32(memory, 0x4104, 0x00010000);
    WriteU32(memory, 0x4108, 0x00010000);
    WriteU32(memory, 0x4110, 0x4200);
    WriteU32(memory, 0x4114, 0x4250);
    WriteU32(memory, 0x4118, 0x4260);
    WriteU32(memory, 0x4250, 0xdeadbeef);
    WriteU32(memory, 0x4260, 0x4300);
    std::string error;
    WriteU32(memory, 0x4400, 0);
    WriteU32(memory, 0x4404, 0x30);
    WriteU32(memory, 0x4408, 0);
    WriteU32(memory, 0x440c, 0);
    WriteU32(memory, 0x4410, 0);
    WriteU32(memory, 0x4414, 0);
    WriteU32(memory, 0x4418, 0x4100);
    WriteU32(memory, 0x441c, 0x2c);
    WriteU32(memory, 0x4420, 0x4000);
    WriteU32(memory, 0x4424, 0x2c);
    WriteU32(memory, 0x4428, 0);
    vshift::cpu::PpuRegisters link_registers{};
    link_registers.gpr[11] = 484;
    link_registers.gpr[3] = 0x4200;
    link_registers.gpr[4] = 0x4400;
    assert(linker.Dispatch(link_registers, error));
    assert(link_registers.gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 4> variable_reference{};
    assert(memory.Read(0x4240, variable_reference).ok());
    assert(variable_reference[0] == 0 && variable_reference[1] == 0 &&
           variable_reference[2] == 0x43 && variable_reference[3] == 4);

    vshift::hle::Ps3Lv2 lv2(memory);
    vshift::cpu::PpuRuntime runtime(memory);
    runtime.registers().pc = 0x1000;
    runtime.registers().gpr[3] = 0x2000;
    const auto result = runtime.Run(20, [&](auto& registers, auto& error) {
        return lv2.Dispatch(registers, error);
    });
    assert(result.reason == vshift::cpu::PpuStopReason::UnsupportedInstruction);
    assert(result.instructions == 6);
    assert(lv2.trace().size() == 2);
    std::array<std::uint8_t, 4> object{};
    assert(memory.Read(0x2000, object).ok());
    assert(object[0] == 0 && object[1] == 0 && object[2] == 0x10);
    assert(object[3] == 0);
    runtime.registers().gpr[11] = 988;
    runtime.registers().gpr[3] = 4;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 30;
    runtime.registers().gpr[3] = 0x2000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEnoent);
    std::array<std::uint8_t, 0x40> paramsfo{};
    assert(memory.Read(0x2000, paramsfo).ok());
    for (const auto byte : paramsfo) assert(byte == 0);

    // PPU thread IDs are 64-bit Cell ABI values. Writing only four bytes on
    // the big-endian guest turns a valid 0x1000 ID into 0x100000000000.
    const std::array<std::uint8_t, 8> thread_parameter{
        0x00, 0x00, 0x20, 0x60, 0x00, 0x00, 0x20, 0x80};
    const std::array<std::uint8_t, 8> thread_descriptor{
        0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x30, 0x00};
    assert(memory.Write(0x2070, thread_parameter).ok());
    assert(memory.Write(0x2060, thread_descriptor).ok());
    runtime.registers().gpr[11] = 52;
    runtime.registers().gpr[3] = 0x2050;
    runtime.registers().gpr[4] = 0x2070;
    runtime.registers().gpr[5] = 0xabcdef;
    runtime.registers().gpr[8] = 0x1800;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 8> thread_id_bytes{};
    assert(memory.Read(0x2050, thread_id_bytes).ok());
    std::uint64_t thread_id = 0;
    for (const auto byte : thread_id_bytes) thread_id = (thread_id << 8) | byte;
    assert(thread_id != 0 && thread_id <= UINT32_MAX);
    runtime.registers().gpr[11] = 53;
    runtime.registers().gpr[3] = thread_id;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);

    std::array<std::uint8_t, 0x30> prx_option{};
    prx_option[7] = 0x30;
    prx_option[15] = 1;
    assert(memory.Write(0x2040, prx_option).ok());
    runtime.registers().gpr[11] = 484;
    runtime.registers().gpr[3] = 0x2000;
    runtime.registers().gpr[4] = 0x2040;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);

    // sys_config_open requires the event queue created by the guest and
    // returns a stateful configuration handle for subsequent close.
    runtime.registers().gpr[11] = 128;
    runtime.registers().gpr[3] = 0x2028;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 4> queue_handle_bytes{};
    assert(memory.Read(0x2028, queue_handle_bytes).ok());
    const auto queue_handle = (static_cast<std::uint32_t>(queue_handle_bytes[0]) << 24) |
                              (static_cast<std::uint32_t>(queue_handle_bytes[1]) << 16) |
                              (static_cast<std::uint32_t>(queue_handle_bytes[2]) << 8) |
                              queue_handle_bytes[3];

    // Storage medium notifications retain an association with a real event
    // queue; an invalid queue must not be accepted as a successful no-op.
    runtime.registers().gpr[11] = 612;
    runtime.registers().gpr[3] = 0;
    runtime.registers().gpr[4] = queue_handle;
    runtime.registers().gpr[5] = 0x11223344;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 612;
    runtime.registers().gpr[3] = 1;
    runtime.registers().gpr[4] = 0xdead;
    runtime.registers().gpr[5] = 0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEsrch);

    runtime.registers().gpr[11] = 610;
    runtime.registers().gpr[3] = 0x2030;
    runtime.registers().gpr[4] = 0x2034;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2030, queue_handle_bytes).ok());
    assert(queue_handle_bytes[3] == 6);
    assert(memory.Read(0x2034, queue_handle_bytes).ok());
    assert(queue_handle_bytes[3] == 17);
    runtime.registers().gpr[11] = 611;
    runtime.registers().gpr[3] = 6;
    runtime.registers().gpr[4] = 0;
    runtime.registers().gpr[5] = 1;
    runtime.registers().gpr[6] = 0x20d0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 8> device_id{};
    assert(memory.Read(0x20d0, device_id).ok());
    assert(device_id[0] == 0x01 && device_id[1] == 0x03 &&
           device_id[7] == 0x0a);
    runtime.registers().gpr[11] = 609;
    runtime.registers().gpr[3] = 0x0101000000000006ull;
    runtime.registers().gpr[4] = 0x4800;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 0x40> device_info{};
    assert(memory.Read(0x4800, device_info).ok());
    assert(device_info[0] == 'u' && device_info[6] == 'd');
    assert(device_info[0x2d] == 0x04 && device_info[0x2e] == 0xd9 &&
           device_info[0x2f] == 0x55);
    assert(device_info[0x32] == 0x08 && device_info[0x3a] == 1 &&
           device_info[0x3f] == 1);
    runtime.registers().gpr[11] = 600;
    runtime.registers().gpr[3] = 0x0101000000000006ull;
    runtime.registers().gpr[4] = 0;
    runtime.registers().gpr[5] = 0x2038;
    runtime.registers().gpr[6] = 0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2038, queue_handle_bytes).ok());
    const auto storage_handle = (static_cast<std::uint32_t>(queue_handle_bytes[0]) << 24) |
                                (static_cast<std::uint32_t>(queue_handle_bytes[1]) << 16) |
                                (static_cast<std::uint32_t>(queue_handle_bytes[2]) << 8) |
                                queue_handle_bytes[3];
    assert(storage_handle != 0);
    runtime.registers().gpr[11] = 601;
    runtime.registers().gpr[3] = storage_handle;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEsrch);

    runtime.registers().gpr[11] = 516;
    runtime.registers().gpr[3] = queue_handle;
    runtime.registers().gpr[4] = 0x202c;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x202c, queue_handle_bytes).ok());
    const auto config_handle = (static_cast<std::uint32_t>(queue_handle_bytes[0]) << 24) |
                               (static_cast<std::uint32_t>(queue_handle_bytes[1]) << 16) |
                               (static_cast<std::uint32_t>(queue_handle_bytes[2]) << 8) |
                               queue_handle_bytes[3];
    runtime.registers().gpr[11] = 519;
    runtime.registers().gpr[3] = config_handle;
    runtime.registers().gpr[4] = 0x8000000000001013ull;
    runtime.registers().gpr[5] = 1;
    runtime.registers().gpr[6] = 0;
    runtime.registers().gpr[7] = 0;
    runtime.registers().gpr[8] = 1;
    runtime.registers().gpr[9] = 0x2090;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2090, queue_handle_bytes).ok());
    const auto listener_handle =
        (static_cast<std::uint32_t>(queue_handle_bytes[0]) << 24) |
        (static_cast<std::uint32_t>(queue_handle_bytes[1]) << 16) |
        (static_cast<std::uint32_t>(queue_handle_bytes[2]) << 8) |
        queue_handle_bytes[3];
    assert(listener_handle == 0x42000000);

    const std::array<std::uint8_t, 4> config_payload{1, 2, 3, 4};
    assert(memory.Write(0x2094, config_payload).ok());
    runtime.registers().gpr[11] = 521;
    runtime.registers().gpr[3] = config_handle;
    runtime.registers().gpr[4] = 0x8000000000001013ull;
    runtime.registers().gpr[5] = 0x55;
    runtime.registers().gpr[6] = 1;
    runtime.registers().gpr[7] = 0x2094;
    runtime.registers().gpr[8] = config_payload.size();
    runtime.registers().gpr[9] = 0x2098;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2098, queue_handle_bytes).ok());
    const auto service_handle =
        (static_cast<std::uint32_t>(queue_handle_bytes[0]) << 24) |
        (static_cast<std::uint32_t>(queue_handle_bytes[1]) << 16) |
        (static_cast<std::uint32_t>(queue_handle_bytes[2]) << 8) |
        queue_handle_bytes[3];
    assert(service_handle == 0x43000000);

    runtime.registers().gpr[11] = 131;
    runtime.registers().gpr[3] = queue_handle;
    runtime.registers().gpr[4] = 0x20a0;
    runtime.registers().gpr[5] = 1;
    runtime.registers().gpr[6] = 0x20c0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    const auto event_id = static_cast<std::uint32_t>(runtime.registers().gpr[6]);
    assert(event_id == 1);
    runtime.registers().gpr[11] = 518;
    runtime.registers().gpr[3] = config_handle;
    runtime.registers().gpr[4] = event_id;
    runtime.registers().gpr[5] = 0x4000;
    runtime.registers().gpr[6] = 44;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 44> config_event{};
    assert(memory.Read(0x4000, config_event).ok());
    assert(config_event[0] == 0x42 && config_event[7] == 1);
    assert(config_event[39] == 0 && config_event[40] == 1 &&
           config_event[41] == 2 && config_event[42] == 3 &&
           config_event[43] == 4);
    runtime.registers().gpr[11] = 522;
    runtime.registers().gpr[3] = config_handle;
    runtime.registers().gpr[4] = service_handle;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 873;
    runtime.registers().gpr[3] = 0x20cf;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 1> product_mode{};
    assert(memory.Read(0x20cf, product_mode).ok());
    assert(product_mode[0] == 0xff);
    const std::array<std::uint8_t, 20> stat_path{
        '/', 'd', 'e', 'v', '_', 'f', 'l', 'a', 's', 'h', '/',
        't', 'e', 's', 't', '.', 'b', 'i', 'n', 0};
    assert(memory.Write(0x20d0, stat_path).ok());
    vshift::hle::Ps3Lv2::FirmwareFiles stat_firmware_files{
        {"dev_flash/test.bin", {1, 2, 3, 4, 5}}};
    vshift::hle::Ps3Lv2 stat_lv2(memory, &stat_firmware_files);
    vshift::cpu::PpuRegisters stat_registers{};
    stat_registers.gpr[11] = 808;
    stat_registers.gpr[3] = 0x20d0;
    stat_registers.gpr[4] = 0x4040;
    assert(stat_lv2.Dispatch(stat_registers, error));
    assert(stat_registers.gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 52> stat{};
    assert(memory.Read(0x4040, stat).ok());
    assert(stat[0] == 0 && stat[1] == 0 && stat[2] == 0x81 &&
           stat[3] == 0xa4 && stat[43] == 5);
    const std::array<std::uint8_t, 23> backup_path{
        '/', 'd', 'e', 'v', '_', 'f', 'l', 'a', 's', 'h', '2',
        '/', 'e', 't', 'c', '/', 'b', 'a', 'c', 'k', 'u', 'p', 0};
    assert(memory.Write(0x20d0, backup_path).ok());
    stat_registers.gpr[11] = 811;
    stat_registers.gpr[3] = 0x20d0;
    stat_registers.gpr[4] = 0700;
    assert(stat_lv2.Dispatch(stat_registers, error));
    assert(stat_registers.gpr[3] == vshift::hle::kCellOk);
    stat_registers.gpr[11] = 808;
    stat_registers.gpr[3] = 0x20d0;
    stat_registers.gpr[4] = 0x4080;
    assert(stat_lv2.Dispatch(stat_registers, error));
    assert(stat_registers.gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x4080, stat).ok());
    assert(stat[2] == 0x41 && stat[3] == 0xc9);
    stat_registers.gpr[11] = 811;
    stat_registers.gpr[3] = 0x20d0;
    stat_registers.gpr[4] = 0700;
    assert(stat_lv2.Dispatch(stat_registers, error));
    assert(stat_registers.gpr[3] == vshift::hle::kCellEexist);
    assert(memory.Write(0x20d0, stat_path).ok());
    stat_registers.gpr[11] = 801;
    stat_registers.gpr[3] = 0x20d0;
    stat_registers.gpr[4] = 0;
    stat_registers.gpr[5] = 0x20f0;
    assert(stat_lv2.Dispatch(stat_registers, error));
    assert(stat_registers.gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x20f0, queue_handle_bytes).ok());
    const auto stat_fd =
        (static_cast<std::uint32_t>(queue_handle_bytes[0]) << 24) |
        (static_cast<std::uint32_t>(queue_handle_bytes[1]) << 16) |
        (static_cast<std::uint32_t>(queue_handle_bytes[2]) << 8) |
        queue_handle_bytes[3];
    stat_registers.gpr[11] = 818;
    stat_registers.gpr[3] = stat_fd;
    stat_registers.gpr[4] = static_cast<std::uint64_t>(-2ll);
    stat_registers.gpr[5] = 2;
    stat_registers.gpr[6] = 0x20f8;
    assert(stat_lv2.Dispatch(stat_registers, error));
    assert(stat_registers.gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 8> seek_position{};
    assert(memory.Read(0x20f8, seek_position).ok());
    assert(seek_position[7] == 3);
    stat_registers.gpr[11] = 802;
    stat_registers.gpr[3] = stat_fd;
    stat_registers.gpr[4] = 0x4100;
    stat_registers.gpr[5] = 2;
    stat_registers.gpr[6] = 0x20e8;
    assert(stat_lv2.Dispatch(stat_registers, error));
    assert(stat_registers.gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 2> tail{};
    assert(memory.Read(0x4100, tail).ok());
    assert(tail[0] == 4 && tail[1] == 5);
    runtime.registers().gpr[11] = 517;
    runtime.registers().gpr[3] = config_handle;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 530;
    runtime.registers().gpr[3] = 0x2030;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2030, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0x11 && queue_handle_bytes[3] == 0x5b);
    runtime.registers().gpr[11] = 532;
    runtime.registers().gpr[3] = 0x115b;
    runtime.registers().gpr[4] = 0x2034;
    runtime.registers().gpr[5] = 4;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == 0);
    runtime.registers().gpr[11] = 330;
    runtime.registers().gpr[3] = 0x10000;
    runtime.registers().gpr[4] = 0x20f;
    runtime.registers().gpr[5] = 0x1000;
    runtime.registers().gpr[6] = 0x2048;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2048, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0x10 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0 && queue_handle_bytes[3] == 0);
    runtime.registers().gpr[11] = 332;
    runtime.registers().gpr[3] = 0x80004d494f323211ull;
    runtime.registers().gpr[4] = 0x10000;
    runtime.registers().gpr[5] = 0xc200;
    runtime.registers().gpr[6] = 0x2050;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2050, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0x50 && queue_handle_bytes[3] == 0);
    runtime.registers().gpr[11] = 337;
    runtime.registers().gpr[3] = 0x10000000;
    runtime.registers().gpr[4] = 0x5000;
    runtime.registers().gpr[5] = 0x40000;
    runtime.registers().gpr[6] = 0x2054;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2054, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0x10 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0 && queue_handle_bytes[3] == 0);
    runtime.registers().gpr[11] = 339;
    runtime.registers().gpr[3] = 0x80004d494f323211ull;
    runtime.registers().gpr[4] = 0x30000;
    runtime.registers().gpr[5] = 0x200;
    runtime.registers().gpr[6] = 0x2058;
    runtime.registers().gpr[7] = 1;
    runtime.registers().gpr[8] = 0x205c;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x205c, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0x50 && queue_handle_bytes[3] == 1);
    runtime.registers().gpr[11] = 330;
    runtime.registers().gpr[3] = 0x30000;
    runtime.registers().gpr[4] = 0x200;
    runtime.registers().gpr[5] = 0x10000;
    runtime.registers().gpr[6] = 0x2068;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 334;
    runtime.registers().gpr[3] = 0x10010000;
    runtime.registers().gpr[4] = 0x5001;
    runtime.registers().gpr[5] = 0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 329;
    runtime.registers().gpr[3] = 0x5001;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEbusy);
    runtime.registers().gpr[11] = 329;
    runtime.registers().gpr[3] = 0x5001;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEbusy);
    runtime.registers().gpr[11] = 329;
    runtime.registers().gpr[3] = 0x5000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEbusy);
    runtime.registers().gpr[11] = 47;
    runtime.registers().gpr[3] = 0x1000000;
    runtime.registers().gpr[4] = 1000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 170;
    runtime.registers().gpr[3] = 0x2060;
    runtime.registers().gpr[4] = 1;
    runtime.registers().gpr[5] = 6;
    runtime.registers().gpr[6] = 0x2070;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2060, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0x60 && queue_handle_bytes[3] == 0);
    runtime.registers().gpr[11] = 172;
    runtime.registers().gpr[3] = 0x2064;
    runtime.registers().gpr[4] = 0x6000;
    runtime.registers().gpr[5] = 0;
    runtime.registers().gpr[6] = 0x2074;
    runtime.registers().gpr[7] = 0x2078;
    runtime.registers().gpr[8] = 0x207c;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2064, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0x70 && queue_handle_bytes[3] == 0);
    runtime.registers().gpr[11] = 173;
    runtime.registers().gpr[3] = 0x6000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 248;
    runtime.registers().gpr[3] = 0x6000;
    runtime.registers().gpr[4] = 0x10f;
    runtime.registers().gpr[5] = 0x14d5;
    runtime.registers().gpr[6] = 0x2bc;
    runtime.registers().gpr[7] = 0xa5a;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 380;
    runtime.registers().gpr[3] = 0x2010;
    runtime.registers().gpr[4] = 0x2011;
    runtime.registers().gpr[5] = 0x2012;
    runtime.registers().gpr[6] = 0x2016;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 1> parameter_byte{};
    assert(memory.Read(0x2010, parameter_byte).ok());
    assert(parameter_byte[0] == 0);
    assert(memory.Read(0x2011, parameter_byte).ok());
    assert(parameter_byte[0] == 0);
    std::array<std::uint8_t, 4> memory_parameter{};
    assert(memory.Read(0x2012, memory_parameter).ok());
    assert(memory_parameter[0] == 0 && memory_parameter[1] == 0 &&
           memory_parameter[2] == 2 && memory_parameter[3] == 0);
    std::array<std::uint8_t, 8> boot_parameter{};
    assert(memory.Read(0x2016, boot_parameter).ok());
    assert(boot_parameter[7] == 7);
    runtime.registers().gpr[11] = 367;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    runtime.registers().gpr[11] = 367;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEperm);
    runtime.registers().gpr[11] = 324;
    runtime.registers().gpr[3] = 0x2020;
    runtime.registers().gpr[4] = 0x300000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    // VSH reads appliance identity through sys_ss_appliance_info_manager.
    runtime.registers().gpr[11] = 867;
    runtime.registers().gpr[3] = 0x19003;
    runtime.registers().gpr[4] = 0x40a0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 16> idps{};
    assert(memory.Read(0x40a0, idps).ok());
    assert((idps == std::array<std::uint8_t, 16>{
        0x00, 0x00, 0x00, 0x01, 0x00, 0x89, 0x00, 0x0b,
        0x14, 0x00, 0xef, 0xdd, 0xca, 0x25, 0x52, 0x66}));
    runtime.registers().gpr[11] = 867;
    runtime.registers().gpr[3] = 0x19002;
    runtime.registers().gpr[4] = 0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellEfault);
    runtime.registers().gpr[11] = 650;
    runtime.registers().gpr[3] = 0x2090;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    assert(memory.Read(0x2090, queue_handle_bytes).ok());
    assert(queue_handle_bytes[0] == 0 && queue_handle_bytes[1] == 0 &&
           queue_handle_bytes[2] == 0x80 && queue_handle_bytes[3] == 0);
    runtime.registers().gpr[11] = 652;
    runtime.registers().gpr[3] = 0x8000;
    runtime.registers().gpr[4] = 0x2098;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 8> rsx_audio_address{};
    assert(memory.Read(0x2098, rsx_audio_address).ok());
    assert(rsx_audio_address[0] == 0 && rsx_audio_address[1] == 0 &&
           rsx_audio_address[2] == 0 && rsx_audio_address[3] == 0 &&
           rsx_audio_address[4] == 0x20 && rsx_audio_address[5] == 0 &&
           rsx_audio_address[6] == 0 && rsx_audio_address[7] == 0);
    runtime.registers().gpr[11] = 486;
    runtime.registers().gpr[3] = 0x2000;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    const std::array<std::uint8_t, 11> module_path{
        'l', 'i', 'b', 'x', '.', 's', 'p', 'r', 'x', 0, 0};
    assert(memory.Write(0x2080, module_path).ok());
    runtime.registers().gpr[11] = 497;
    runtime.registers().gpr[3] = 0x2080;
    runtime.registers().gpr[4] = 0x1000;
    runtime.registers().gpr[5] = 0;
    runtime.registers().gpr[6] = 0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == 0x23000000);
    std::array<std::uint8_t, 0x20> start_option{};
    start_option[7] = 0x20;
    start_option[15] = 1;
    assert(memory.Write(0x20a0, start_option).ok());
    runtime.registers().gpr[11] = 481;
    runtime.registers().gpr[3] = 0x23000000;
    runtime.registers().gpr[4] = 0;
    runtime.registers().gpr[5] = 0x20a0;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);

    // VSH asks LV2 for a mount id using sys_fs_fcntl command 0xc0000006.
    const std::array<std::uint8_t, 11> flash_mount{
        '/', 'd', 'e', 'v', '_', 'f', 'l', 'a', 's', 'h', 0};
    assert(memory.Write(0x20c0, flash_mount).ok());
    WriteU32(memory, 0x20e0, 0x20);
    WriteU32(memory, 0x20e4, 0x10);
    WriteU32(memory, 0x20e8, 0x18);
    WriteU32(memory, 0x20ec, static_cast<std::uint32_t>(flash_mount.size()));
    WriteU32(memory, 0x20f0, 0x20c0);
    runtime.registers().gpr[11] = 817;
    runtime.registers().gpr[3] = std::numeric_limits<std::uint64_t>::max();
    runtime.registers().gpr[4] = 0xc0000006;
    runtime.registers().gpr[5] = 0x20e0;
    runtime.registers().gpr[6] = 0x20;
    assert(lv2.Dispatch(runtime.registers(), error));
    assert(runtime.registers().gpr[3] == vshift::hle::kCellOk);
    std::array<std::uint8_t, 8> mount_result{};
    assert(memory.Read(0x20f8, mount_result).ok());
    assert(mount_result[0] == 0x80 && mount_result[1] == 0x01 &&
           mount_result[2] == 0x00 && mount_result[3] == 0x37 &&
           mount_result[4] == 0 && mount_result[5] == 0 &&
           mount_result[6] == 0 && mount_result[7] == 0);

    // A blocking event receive must yield the bootstrap context to a started
    // PPU worker without fabricating a return through the worker's LR.
    vshift::memory::GuestMemory scheduler_memory;
    assert(scheduler_memory.Map({
        0x1000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionExecute}).ok());
    assert(scheduler_memory.Map({
        0x2000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(scheduler_memory, 0x1000, Addi(3, 0, 7));
    const std::array<std::uint8_t, 8> scheduler_descriptor{
        0x00, 0x00, 0x10, 0x00, 0x55, 0x66, 0x77, 0x88};
    const std::array<std::uint8_t, 8> scheduler_parameter{
        0x00, 0x00, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00};
    assert(scheduler_memory.Write(0x2040, scheduler_descriptor).ok());
    assert(scheduler_memory.Write(0x2030, scheduler_parameter).ok());
    vshift::hle::Ps3Lv2 scheduler_lv2(scheduler_memory);
    vshift::cpu::PpuRegisters scheduler_registers{};
    scheduler_registers.gpr[11] = 52;
    scheduler_registers.gpr[3] = 0x2020;
    scheduler_registers.gpr[4] = 0x2030;
    scheduler_registers.gpr[5] = 0x11223344;
    scheduler_registers.gpr[8] = 0x2000;
    assert(scheduler_lv2.Dispatch(scheduler_registers, error));
    std::array<std::uint8_t, 8> scheduler_thread_bytes{};
    assert(scheduler_memory.Read(0x2020, scheduler_thread_bytes).ok());
    std::uint64_t scheduler_thread = 0;
    for (const auto byte : scheduler_thread_bytes) {
        scheduler_thread = (scheduler_thread << 8) | byte;
    }
    scheduler_registers.gpr[11] = 53;
    scheduler_registers.gpr[3] = scheduler_thread;
    assert(scheduler_lv2.Dispatch(scheduler_registers, error));
    scheduler_registers.gpr[11] = 128;
    scheduler_registers.gpr[3] = 0x2028;
    assert(scheduler_lv2.Dispatch(scheduler_registers, error));
    std::array<std::uint8_t, 4> scheduler_queue_bytes{};
    assert(scheduler_memory.Read(0x2028, scheduler_queue_bytes).ok());
    const auto scheduler_queue =
        (static_cast<std::uint32_t>(scheduler_queue_bytes[0]) << 24) |
        (static_cast<std::uint32_t>(scheduler_queue_bytes[1]) << 16) |
        (static_cast<std::uint32_t>(scheduler_queue_bytes[2]) << 8) |
        scheduler_queue_bytes[3];
    scheduler_registers.pc = 0x7770;
    scheduler_registers.gpr[11] = 130;
    scheduler_registers.gpr[3] = scheduler_queue;
    scheduler_registers.gpr[4] = 0x2050;
    assert(scheduler_lv2.Dispatch(scheduler_registers, error));
    assert(scheduler_registers.pc == 0x1000);
    assert(scheduler_registers.gpr[2] == 0x55667788);
    assert(scheduler_registers.gpr[3] == 0x11223344);
    assert(scheduler_registers.lr == 0x0b100000);

    // A guest sleep is a scheduling point: it must yield the bootstrap PPU
    // context to a started worker rather than becoming a tight host-side loop.
    vshift::memory::GuestMemory timer_memory;
    assert(timer_memory.Map({
        0x1000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionExecute}).ok());
    assert(timer_memory.Map({
        0x2000, 0x100, vshift::memory::kPermissionRead |
                       vshift::memory::kPermissionWrite}).ok());
    WriteInstruction(timer_memory, 0x1000, Addi(3, 0, 7));
    assert(timer_memory.Write(0x2040, scheduler_descriptor).ok());
    assert(timer_memory.Write(0x2030, scheduler_parameter).ok());
    vshift::hle::Ps3Lv2 timer_lv2(timer_memory);
    vshift::cpu::PpuRegisters timer_registers{};
    timer_registers.gpr[11] = 52;
    timer_registers.gpr[3] = 0x2020;
    timer_registers.gpr[4] = 0x2030;
    timer_registers.gpr[5] = 0x11223344;
    timer_registers.gpr[8] = 0x2000;
    assert(timer_lv2.Dispatch(timer_registers, error));
    assert(timer_memory.Read(0x2020, scheduler_thread_bytes).ok());
    scheduler_thread = 0;
    for (const auto byte : scheduler_thread_bytes) {
        scheduler_thread = (scheduler_thread << 8) | byte;
    }
    timer_registers.gpr[11] = 53;
    timer_registers.gpr[3] = scheduler_thread;
    assert(timer_lv2.Dispatch(timer_registers, error));
    timer_registers.pc = 0x7770;
    timer_registers.gpr[11] = 141;
    timer_registers.gpr[3] = 1;
    assert(timer_lv2.Dispatch(timer_registers, error));
    assert(timer_registers.pc == 0x1000);
    assert(timer_registers.gpr[2] == 0x55667788);
    assert(timer_registers.gpr[3] == 0x11223344);
    return 0;
}
