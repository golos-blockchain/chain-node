#include <fc/thread/future.hpp>
#include <cstdint>
#include <iostream>
#include <set>

// Real production method bodies are generated from node.cpp at build time.
// No sockets, reconnect tasks, chain state, or simulated future are used.
struct admission_fixture {
    bool _discovery_only = true;
    bool _network_started = false;
    bool _node_is_shutting_down = false;
    fc::future<void> _p2p_network_connect_loop_done;
    std::set<int> _handshaking_connections, _active_connections;
    uint32_t _maximum_number_of_connections = 3;
    uint32_t _desired_number_of_connections = 2;
    bool is_accepting_new_connections();
    bool is_wanting_new_connections();
    uint32_t get_number_of_connections();
};
#define VERIFY_CORRECT_THREAD() ((void)0)
#include "admission_definitions.hpp"

int main() {
    admission_fixture node;
    unsigned failures = 0;
    auto check = [&](bool condition, const char* message) {
        if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
    };
    check(!node._p2p_network_connect_loop_done.valid(), "actual FC default future is absent");
    check(node._p2p_network_connect_loop_done.canceled(), "actual FC default future is canceled");
    check(!node.is_accepting_new_connections() && !node.is_wanting_new_connections(), "discovery startup rejects admission");
    node._network_started = true;
    check(node.is_accepting_new_connections(), "started discovery accepts with missing reconnect future");
    check(node.is_wanting_new_connections(), "started discovery wants peers with missing reconnect future");
    node._active_connections = {1};
    node._handshaking_connections = {2};
    check(node.get_number_of_connections() == 2, "capacity includes handshaking connections");
    check(node.is_accepting_new_connections(), "accept below maximum");
    check(!node.is_wanting_new_connections(), "wanting excludes desired boundary");
    node._handshaking_connections.insert(3);
    check(node.is_accepting_new_connections(), "accept includes maximum boundary");
    node._handshaking_connections.insert(4);
    check(!node.is_accepting_new_connections(), "accept rejects above maximum");
    node._active_connections.clear();
    node._handshaking_connections.clear();
    node._node_is_shutting_down = true;
    check(!node.is_accepting_new_connections() && !node.is_wanting_new_connections(), "discovery shutdown rejects admission");
    node._discovery_only = false;
    check(!node.is_accepting_new_connections() && !node.is_wanting_new_connections(), "ordinary shutdown cannot reopen admission");
    node._node_is_shutting_down = false;
    check(!node.is_accepting_new_connections() && !node.is_wanting_new_connections(), "ordinary mode preserves canceled-future rejection");
    return failures ? 1 : 0;
}
