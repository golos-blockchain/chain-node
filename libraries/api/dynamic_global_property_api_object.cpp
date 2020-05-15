#include <golos/api/dynamic_global_property_api_object.hpp>

namespace golos { namespace api {
    dynamic_global_property_api_object::dynamic_global_property_api_object(const dynamic_global_property_object &dgp)
        : id(dgp.id), head_block_number(dgp.head_block_number), head_block_id(dgp.head_block_id),
        time(dgp.time), current_witness(dgp.current_witness), total_pow(dgp.total_pow),
        num_pow_witnesses(dgp.num_pow_witnesses), virtual_supply(dgp.virtual_supply),
        current_supply(dgp.current_supply), confidential_supply(dgp.confidential_supply),
        current_sbd_supply(dgp.current_sbd_supply), confidential_sbd_supply(dgp.confidential_sbd_supply),
        total_vesting_fund_steem(dgp.total_vesting_fund_steem), total_vesting_shares(dgp.total_vesting_shares),
        accumulative_balance(dgp.accumulative_balance), total_reward_fund_steem(dgp.total_reward_fund_steem), total_reward_shares2(dgp.total_reward_shares2),
        sbd_interest_rate(dgp.sbd_interest_rate), sbd_print_rate(dgp.sbd_print_rate), sbd_debt_percent(dgp.sbd_debt_percent),
        is_forced_min_price(dgp.is_forced_min_price), average_block_size(dgp.average_block_size),
        maximum_block_size(dgp.maximum_block_size), current_aslot(dgp.current_aslot),
        recent_slots_filled(dgp.recent_slots_filled), participation_count(dgp.participation_count),
        last_irreversible_block_num(dgp.last_irreversible_block_num), max_virtual_bandwidth(dgp.max_virtual_bandwidth),
        current_reserve_ratio(dgp.current_reserve_ratio), custom_ops_bandwidth_multiplier(dgp.custom_ops_bandwidth_multiplier),
        transit_block_num(dgp.transit_block_num), transit_witnesses(dgp.transit_witnesses) {
            worker_requests.insert(dgp.worker_requests.begin(), dgp.worker_requests.end());
    }
} } // golos::api