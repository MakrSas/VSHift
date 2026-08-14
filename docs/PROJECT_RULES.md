# VSHift project rules

This repository is a multicore application. The application and platform
layers consume the public Core API; console-specific implementation remains
behind a core boundary.

- Work only on the branch owned by the relevant agent. The PS3 agent owns
  `ps3` and `cores/ps3/`; it does not edit PSP, frontend, PS4, or PS5 branches.
- Do not merge or rebase another agent's branch without an explicit request.
- Never force-push, remove another agent's implementation, or commit build
  artifacts, firmware, keys, or other secrets.
- Keep shared API changes small, backwards-compatible where practical, and
  document them in `MULTICORE_ARCHITECTURE.md` and the owning status file.
- The frontend must call `IConsoleCore`, not PPU/LV2/RPCS3 implementation
  classes or headers.
- A custom UI must not be presented as the guest's XMB. The PS3 milestone is
  the real firmware VSH/XMB boot path and its actual video/audio/input output.
- Every significant change must pass the configured build and tests before it
  is handed to another agent.

The repository is the communication bus. Each agent updates only its own
`docs/agents/*_STATUS.md` file and reads the other status files as needed.
