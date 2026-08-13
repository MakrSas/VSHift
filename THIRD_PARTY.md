# Third-party research and attribution

No third-party source code is copied into VSHift at this stage. The following
projects were inspected as research inputs. If code is later adapted, the
original file, exact revision, license, modifications, and purpose must be
added before merging it.

| Project | Repository | License observed | Current use |
| --- | --- | --- | --- |
| RPCSX | https://github.com/RPCSX/rpcsx | GPL-2.0 with per-directory/file exceptions | Research only; boot/HLE/GPU behavior |
| KytyPS5 | https://github.com/KytyPS5/KytyPS5 | GPL-2.0 with separately licensed files | Research only; loader, memory, shader pipeline |
| SharpEmu | https://github.com/sharpemu/sharpemu | GPL-2.0-or-later with separately licensed files | Research only; subsystem layout and Metal reference |
| FEX | https://github.com/FEX-Emu/FEX | MIT | Research only; IR and register-allocation concepts |
| Box64 | https://github.com/ptitSeb/box64 | MIT | Research only; ARM64 dynarec/block-cache concepts |
| MoltenVK | https://github.com/KhronosGroup/MoltenVK | Apache-2.0 | Planned optional renderer dependency; not vendored |

The repository's existing top-level `LICENSE` is GPL-2.0. No Sony or
PlayStation proprietary material is included.
