#include <golos/network/discovery_delegate.hpp>
#include <golos/network/rpc_directory.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <csignal>
#include <iostream>

namespace { volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
std::uint64_t now_seconds() { return fc::time_point::now().sec_since_epoch(); }
}
int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: golos-rpc-directory-client IP:PORT P2P_CONFIG_DIR [SECONDS=5]\n"
                     "Requires an explicitly known supporting P2P peer. Results are UNVERIFIED.\n";
        return 2;
    }
    try {
        const std::string duration = argc == 4 ? argv[3] : "5";
        std::size_t consumed = 0;
        const auto seconds = std::stoul(duration, &consumed);
        if (consumed != duration.size() || !seconds || seconds > 60) return 2;
        const auto endpoint = fc::ip::endpoint::from_string(argv[1]);
        golos::network::rpc_directory::catalog catalog;
        golos::network::discovery_delegate delegate;
        golos::network::node node("golos-rpc-directory-client/1");
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        node.load_configuration(fc::path(argv[2]));
        node.set_node_delegate(&delegate);
        node.set_discovery_only(true); // MUST precede every connection/listener.
        node.accept_incoming_connections(false);
        node.disable_peer_advertising();
        node.set_hello_extension(golos::network::rpc_directory::hello_key,
            [&catalog] { return catalog.hello(now_seconds()); },
            [&catalog](const std::string& peer, const std::string& wire) { catalog.receive(peer, wire, now_seconds()); });
        node.connect_to_p2p_network(); // Discovery mode starts no fetch/advertisement/reconnect loops.
        node.connect_to_endpoint(endpoint); // One explicit attempt; never follow discovered RPC URLs.
        const auto deadline = fc::time_point::now() + fc::seconds(seconds);
        while (!stopped && fc::time_point::now() < deadline) fc::usleep(fc::milliseconds(50));
        node.close(); // Synchronously clears callbacks and closes owned sockets before catalog destruction.
        if (stopped) return 130;
        fc::variants entries;
        for (const auto& r : catalog.snapshot(now_seconds())) {
            entries.emplace_back(fc::mutable_variant_object("url", r.url)("expires", r.expires)("trust", "UNVERIFIED_PEER_ANNOUNCEMENT"));
        }
        std::cout << fc::json::to_string(fc::variant(entries)) << '\n';
        return entries.empty() ? 3 : 0;
    } catch (const fc::exception& error) {
        std::cerr << error.to_detail_string() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
