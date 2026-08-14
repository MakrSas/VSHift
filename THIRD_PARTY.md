# Third-party research and attribution

RPCS3 is now connected as a pinned git submodule under `third_party/rpcs3`.
The first integration target is its `rpcs3_emu` library; the Qt application
and desktop frontend are intentionally excluded. The exact submodule revision
is recorded by git and must be updated together with this table.

| Project | Repository | License observed | Current use |
| --- | --- | --- | --- |
| RPCS3 | https://github.com/RPCS3/rpcs3 | GPL-2.0 with per-directory/file exceptions | Pinned submodule; PS3 PPU/SPU/LV2/VFS/RSX core |
| RPCSX | https://github.com/RPCSX/rpcsx | GPL-2.0 with per-directory/file exceptions | Research only; PS4/PS5 boot/HLE/GPU behavior |
| KytyPS5 | https://github.com/KytyPS5/KytyPS5 | GPL-2.0 with separately licensed files | Research only; loader, memory, shader pipeline |
| SharpEmu | https://github.com/sharpemu/sharpemu | GPL-2.0-or-later with separately licensed files | Research only; subsystem layout and Metal reference |
| FEX | https://github.com/FEX-Emu/FEX | MIT | Research only; IR and register-allocation concepts |
| Box64 | https://github.com/ptitSeb/box64 | MIT | Research only; ARM64 dynarec/block-cache concepts |
| MoltenVK | https://github.com/KhronosGroup/MoltenVK | Apache-2.0 | Planned optional renderer dependency; not vendored |

The repository's existing top-level `LICENSE` is GPL-2.0. No Sony or
PlayStation proprietary material is included.

The current RPCS3 submodule revision is
`4c63acfb40b23da7e7ceee46c8fddd1acbeee152`.
No authoritative public PS3Native iOS source tree was found. VSHift therefore
integrates the upstream RPCS3 core directly and keeps the frontend, input,
audio, and renderer adapters independent of any mobile operating system.
