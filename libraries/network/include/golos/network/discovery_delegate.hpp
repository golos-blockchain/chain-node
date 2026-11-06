#pragma once
#include <golos/network/node.hpp>
#include <atomic>

namespace golos { namespace network {
// Non-null delegate for discovery clients. Any accidental chain call fails closed.
class discovery_delegate final : public node_delegate {
    std::atomic<std::uint64_t> chain_calls_{0};
public:
    std::uint64_t chain_call_count() const { return chain_calls_.load(); }
    bool has_item(const item_id&) override { ++chain_calls_; FC_THROW("Chain access in discovery-only client"); }
    bool handle_block(const block_message&, bool, std::vector<fc::uint160_t>&) override { ++chain_calls_; FC_THROW("Block received by discovery delegate"); }
    void handle_transaction(const trx_message&) override { ++chain_calls_; FC_THROW("Transaction received by discovery delegate"); }
    void handle_message(const message&) override { ++chain_calls_; FC_THROW("Application message received by discovery delegate"); }
    std::vector<item_hash_t> get_block_ids(const std::vector<item_hash_t>&, uint32_t&, uint32_t) override { ++chain_calls_; FC_THROW("Chain access in discovery-only client"); }
    message get_item(const item_id&) override { ++chain_calls_; FC_THROW("Chain access in discovery-only client"); }
    std::vector<item_hash_t> get_blockchain_synopsis(const item_hash_t&, uint32_t) override { ++chain_calls_; FC_THROW("Chain access in discovery-only client"); }
    void sync_status(uint32_t, uint32_t) override { ++chain_calls_; FC_THROW("Synchronization in discovery-only client"); }
    void connection_count_changed(uint32_t) override {}
    uint32_t get_block_number(const item_hash_t&) override { return 0; }
    fc::time_point_sec get_block_time(const item_hash_t&) override { return fc::time_point_sec(); }
    fc::time_point_sec get_blockchain_now() override { return fc::time_point::now(); }
    item_hash_t get_head_block_id() const override { return item_hash_t(); }
    uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t) const override { return 0; }
    void error_encountered(const std::string&, const fc::oexception&) override {}
};
}}
