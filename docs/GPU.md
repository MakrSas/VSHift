# GPU plan

The first GPU target is a synthetic triangle, not PS5 VSH. The renderer API
will expose guest resources, command submission, shader cache, pipeline cache,
render targets, synchronization, and presentation without exposing Vulkan or
Metal types to the CPU/HLE core.

The initial host path is Vulkan through MoltenVK on Apple platforms because it
allows reuse of the existing SPIR-V-oriented research and keeps a direct Metal
backend optional. SharpEmu's direct Metal implementation is a useful reference
for the measured optimization phase, not a reason to copy its code.

No renderer is included in Milestone 1. The first renderer commit must include
one host-side test and a visible iPhone frame before any PS5 shader work begins.
