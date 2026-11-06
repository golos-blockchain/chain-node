// Full real FC encrypted P2P integration, owned loopback only. Requires linked legacy dependencies.
#include <golos/network/discovery_delegate.hpp>
#include <golos/network/rpc_directory.hpp>
#include <fc/thread/thread.hpp>
#include <fc/filesystem.hpp>
#include <cassert>
#include <iostream>
using namespace golos::network;
static uint64_t now_seconds() { return fc::time_point::now().sec_since_epoch(); }
template<class F> static void await(F ready) {
    const auto deadline = fc::time_point::now() + fc::seconds(10);
    while (!ready() && fc::time_point::now() < deadline) fc::usleep(fc::milliseconds(10));
    FC_ASSERT(ready(), "Owned loopback P2P test timed out");
}
struct fixture {
    fc::temp_directory config;
    rpc_directory::catalog directory;
    discovery_delegate delegate;
    node peer;
    fixture(const std::string& name, bool discovery) : peer(name) {
        peer.load_configuration(config.path());
        peer.set_node_delegate(&delegate);
        peer.set_discovery_only(discovery);
        peer.disable_peer_advertising();
        peer.listen_on_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0"), false);
        peer.set_hello_extension(rpc_directory::hello_key,
            [this] { return directory.hello(now_seconds()); },
            [this](const std::string& source, const std::string& wire) { directory.receive(source, wire, now_seconds()); });
        peer.listen_to_p2p_network();
        peer.connect_to_p2p_network();
    }
    ~fixture() { peer.close(); }
};
int main() {
    try {
        fixture a("rpc-test-A", true), b("rpc-test-B", false), c("rpc-test-C", true);
        const auto expiry = now_seconds() + 600;
        FC_ASSERT(a.directory.announce("https://rpc.example.org", expiry, now_seconds()));
        a.peer.connect_to_endpoint(b.peer.get_actual_listening_endpoint());
        await([&] { return b.directory.snapshot(now_seconds()).size() == 1; });
        // Only a NEW connection can receive B's updated catalog in the MVP.
        c.peer.connect_to_endpoint(b.peer.get_actual_listening_endpoint());
        await([&] { return c.directory.snapshot(now_seconds()).size() == 1; });
        FC_ASSERT(c.directory.snapshot(now_seconds())[0].expires == expiry);
        // All chain callbacks fail closed and are counted; metadata callbacks are not chain operations.
        FC_ASSERT(a.delegate.chain_call_count() == 0 && b.delegate.chain_call_count() == 0 && c.delegate.chain_call_count() == 0);
        c.peer.close();
        FC_ASSERT(c.peer.get_connection_count() == 0);
        std::cout << "REAL LOOPBACK A -> B -> C / unchanged expiry / zero chain delegate calls / close: PASS\n";
        return 0;
    } catch (const fc::exception& error) { std::cerr << error.to_detail_string() << '\n'; return 1; }
}
