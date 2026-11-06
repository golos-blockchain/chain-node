// Full real FC encrypted P2P integration, owned loopback only. Requires linked legacy dependencies.
#include <golos/network/discovery_delegate.hpp>
#include <golos/network/rpc_directory.hpp>
#include <golos/network/rpc_search.hpp>
#include <fc/thread/thread.hpp>
#include <fc/filesystem.hpp>
#include <cassert>
#include <iostream>
#include <csignal>
static volatile std::sig_atomic_t stop_fixture=0;
static void stop_owned_fixture(int) { stop_fixture=1; }
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
        if (discovery) peer.disable_peer_advertising();
        peer.listen_on_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0"), false);
        peer.set_hello_extension(rpc_directory::hello_key,
            [this] { return directory.hello(now_seconds()); },
            [this](const std::string& source, const std::string& wire) { directory.receive(source, wire, now_seconds()); });
        peer.listen_to_p2p_network();
        peer.connect_to_p2p_network();
    }
    ~fixture() { peer.close(); }
};
int main(int argc, char** argv) {
    try {
        fixture a("rpc-test-A", true), b("rpc-test-B", false), c("rpc-test-C", true);
        const auto expiry = now_seconds() + 600;
        FC_ASSERT(a.directory.announce("https://rpc.example.org", expiry, now_seconds()));
        // B is an ordinary peer: no directory producer/consumer at all.
        b.peer.set_hello_extension("", {}, {});
        b.peer.connect_to_endpoint(a.peer.get_actual_listening_endpoint());
        await([&] { return b.peer.get_connection_count() == 1; });
        std::cerr << "ordinary seed connected to provider\n";
        if(argc==2 && std::string(argv[1])=="--serve") {
            std::signal(SIGINT,stop_owned_fixture); std::signal(SIGTERM,stop_owned_fixture);
            std::cout << std::string(a.peer.get_actual_listening_endpoint()) << " " << std::string(b.peer.get_actual_listening_endpoint()) << std::endl;
            const auto until=fc::time_point::now()+fc::seconds(45);
            while(!stop_fixture && fc::time_point::now()<until) fc::usleep(fc::milliseconds(25));
            FC_ASSERT(a.delegate.chain_call_count()==0 && b.delegate.chain_call_count()==0);
            return 0;
        }
        rpc_search search([](const std::string& ep) { return ep.compare(0,10,"127.0.0.1:")==0; });
        search.add(std::string(b.peer.get_actual_listening_endpoint()), true);
        const auto start = fc::time_point::now();
        await([&] {
            search.referrals(c.peer.take_discovery_addresses());
            auto next = search.next((fc::time_point::now()-start).count()/1000);
            if (!next.empty()) c.peer.connect_to_endpoint(fc::ip::endpoint::from_string(next));
            return c.directory.snapshot(now_seconds()).size() == 1;
        });
        FC_ASSERT(c.directory.snapshot(now_seconds())[0].expires == expiry);
        // All chain callbacks fail closed and are counted; metadata callbacks are not chain operations.
        FC_ASSERT(a.delegate.chain_call_count() == 0 && b.delegate.chain_call_count() == 0 && c.delegate.chain_call_count() == 0);
        c.peer.close();
        FC_ASSERT(c.peer.get_connection_count() == 0);
        std::cout << "REAL encrypted ordinary-seed -> address-only provider discovery / unchanged expiry / zero chain delegate calls / close: PASS\n";
        return 0;
    } catch (const fc::exception& error) { std::cerr << error.to_detail_string() << '\n'; return 1; }
}
