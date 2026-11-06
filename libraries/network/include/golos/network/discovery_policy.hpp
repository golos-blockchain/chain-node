#pragma once
#include <cstdint>
#include <string>
#include <utility>
namespace golos { namespace network {
// Closed allowlist: unknown/application messages are never deserialized in discovery mode.
inline bool discovery_control_message(std::uint32_t type) {
    return (type >= 5006 && type <= 5013);
}
class hello_extension_state {
    std::string pending_;
public:
    bool capture(const std::string& value) {
        pending_.clear();
        if (value.size() > 16384) return false;
        pending_ = value;
        return true;
    }
    std::string consume(bool same_chain, bool accepted_by_peer, bool accepted_locally) {
        auto result = std::move(pending_);
        pending_.clear();
        return same_chain && accepted_by_peer && accepted_locally ? result : std::string();
    }
    void cancel() { pending_.clear(); }
};
}}
