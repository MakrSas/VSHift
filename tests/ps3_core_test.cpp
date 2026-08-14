#include "cores/ps3/ps3_core.h"

#include <cassert>

int main() {
    vshift::ps3::PS3Core core;
    const auto& descriptor = core.descriptor();
    assert(descriptor.id == "ps3");
    assert(descriptor.display_name == "PlayStation 3");
    assert(descriptor.system_ui == "XMB");
    assert(!descriptor.input_layout.empty());
    assert((descriptor.capabilities &
            static_cast<std::uint64_t>(vshift::coreapi::CoreCapability::Boot)) != 0);

    assert(core.initialize().success);
    assert(core.status().state == vshift::coreapi::CoreState::Ready);
    assert(!core.boot().success);
    assert(core.status().state == vshift::coreapi::CoreState::Ready);
    assert(!core.insertMedia({}).success);
    return 0;
}
