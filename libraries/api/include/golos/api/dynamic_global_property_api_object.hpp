#pragma once

#include <golos/chain/global_property_object.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <golos/api/chain_api_properties.hpp>

namespace golos { namespace api {
    using namespace golos::chain;

    struct dynamic_global_property_api_object {
        dynamic_global_property_api_object(const dynamic_global_property_object& dgp);

        dynamic_global_property_api_object() = default;

        dynamic_global_property_object::id_type id;
        uint32_t head_block_number = 0;
        block_id_type head_block_id;
        time_point_sec time;
        account_name_type current_witness;
        uint64_t total_pow = -1;
        uint32_t num_pow_witnesses = 0;
        asset virtual_supply = asset(0, STEEM_SYMBOL);
        asset current_supply = asset(0, STEEM_SYMBOL);
        asset confidential_supply = asset(0, STEEM_SYMBOL); ///< total asset held in confidential balances
        asset current_sbd_supply = asset(0, SBD_SYMBOL);
        asset confidential_sbd_supply = asset(0, SBD_SYMBOL); ///< total asset held in confidential balances
        asset total_vesting_fund_steem = asset(0, STEEM_SYMBOL);
        asset total_vesting_shares = asset(0, VESTS_SYMBOL);
        asset accumulative_balance = asset(0, STEEM_SYMBOL);
        asset total_reward_fund_steem = asset(0, STEEM_SYMBOL);
        fc::uint128_t total_reward_shares2; ///< the running total of REWARD^2
        uint16_t sbd_interest_rate = 0;
        uint16_t sbd_print_rate = STEEMIT_100_PERCENT;
        uint16_t sbd_debt_percent = 0;
        bool is_forced_min_price = false;
        uint32_t average_block_size = 0;
        uint32_t maximum_block_size = 0;
        uint64_t current_aslot = 0;
        fc::uint128_t recent_slots_filled;
        uint8_t participation_count = 0; ///< Divide by 128 to compute participation percentage
        uint32_t last_irreversible_block_num = 0;
        uint64_t max_virtual_bandwidth = 0;
        uint64_t current_reserve_ratio = 1;
        uint16_t custom_ops_bandwidth_multiplier = STEEMIT_CUSTOM_OPS_BANDWIDTH_MULTIPLIER;
        uint32_t transit_block_num = UINT32_MAX;
        fc::array<account_name_type, STEEMIT_MAX_WITNESSES> transit_witnesses;
        flat_map<asset_symbol_type, uint32_t> worker_requests;
        asset accumulative_emission_per_day = asset(0, STEEM_SYMBOL);
    };

} } // golos::api


FC_REFLECT(
    (golos::api::dynamic_global_property_api_object),
    (id)
    (head_block_number)
    (head_block_id)
    (time)
    (current_witness)
    (total_pow)
    (num_pow_witnesses)
    (virtual_supply)
    (current_supply)
    (confidential_supply)
    (current_sbd_supply)
    (confidential_sbd_supply)
    (total_vesting_fund_steem)
    (total_vesting_shares)
    (accumulative_balance)
    (total_reward_fund_steem)
    (total_reward_shares2)
    (sbd_interest_rate)
    (sbd_print_rate)
    (sbd_debt_percent)
    (average_block_size)
    (maximum_block_size)
    (current_aslot)
    (recent_slots_filled)
    (participation_count)
    (last_irreversible_block_num)
    (max_virtual_bandwidth)
    (current_reserve_ratio)
    (custom_ops_bandwidth_multiplier)
    (is_forced_min_price)
    (transit_block_num)
    (transit_witnesses)
    (worker_requests)
    (accumulative_emission_per_day))