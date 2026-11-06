#include <golos/network/discovery_policy.hpp>
#include <cassert>
#include <iostream>
using namespace golos::network;
int main() {
    for (unsigned type = 0; type < 65536; ++type) {
        const bool control = type >= 5006 && type <= 5013;
        assert(discovery_control_message(type) == control);
    }
    hello_extension_state pending;
    assert(!pending.capture(std::string(16385, 'x')));
    assert(pending.capture("catalog"));
    assert(pending.consume(false, true, true).empty()); // wrong chain discards
    assert(pending.consume(true, true, true).empty());
    assert(pending.capture("catalog"));
    assert(pending.consume(true, false, true).empty()); // rejected by peer
    assert(pending.capture("catalog"));
    assert(pending.consume(true, true, false).empty()); // rejected locally
    assert(pending.capture("catalog"));
    assert(pending.consume(true, true, true) == "catalog");
    assert(pending.consume(true, true, true).empty()); // once only
    assert(pending.capture("catalog"));
    pending.cancel();
    assert(pending.consume(true, true, true).empty()); // cancellation/shutdown
    std::cout << "control allowlist / admission / wrong-chain / once-only / shutdown: PASS\n";
}
