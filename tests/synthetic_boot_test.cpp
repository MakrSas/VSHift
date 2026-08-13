#include "core/boot/synthetic_boot.h"

#include <cassert>

int main() {
    const auto image = vshift::boot::BuildSyntheticElfFixture();
    const auto report = vshift::boot::RunSyntheticElfBoot(
        image, vshift::boot::ExecutionMode::JitLess);
    assert(report.ok());
    assert(report.entry == 0x401000);
    assert(report.mapped_segments == 1);
    assert(report.result == 42);

    auto malformed = image;
    malformed[0] = 0;
    const auto failed = vshift::boot::RunSyntheticElfBoot(
        malformed, vshift::boot::ExecutionMode::JitLess);
    assert(!failed.ok());
    return 0;
}
