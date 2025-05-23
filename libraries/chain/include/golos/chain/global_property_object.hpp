#pragma once

#include <fc/uint128_t.hpp>

#include <golos/chain/steem_object_types.hpp>

#include <golos/protocol/asset.hpp>

namespace golos {
    namespace chain {

        using golos::protocol::asset_symbol_type;
        using golos::protocol::asset;
        using golos::protocol::price;

        /**
         * @class dynamic_global_property_object
         * @brief Maintains global state information
         * @ingroup object
         * @ingroup implementation
         *
         * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
         * current values of global blockchain properties.
         */
        class dynamic_global_property_object
                : public object<dynamic_global_property_object_type, dynamic_global_property_object> {
        public:
            template<typename Constructor, typename Allocator>
            dynamic_global_property_object(Constructor &&c, allocator <Allocator> a) : recent_slots_filled(0) {
                c(*this);
            }

            id_type id;

            uint32_t head_block_number = 0;
            block_id_type head_block_id;
            time_point_sec time;
            account_name_type current_witness;


            /**
             *  The total POW accumulated, aka the sum of num_pow_witness at the time new POW is added
             */
            uint64_t total_pow = -1;

            /**
             * The current count of how many pending POW witnesses there are, determines the difficulty
             * of doing pow
             */
            uint32_t num_pow_witnesses = 0;

            asset virtual_supply = asset(0, STEEM_SYMBOL);
            asset current_supply = asset(0, STEEM_SYMBOL);
            asset confidential_supply = asset(0, STEEM_SYMBOL); ///< total asset held in confidential balances
            asset current_sbd_supply = asset(0, SBD_SYMBOL);
            asset confidential_sbd_supply = asset(0, SBD_SYMBOL); ///< total asset held in confidential balances
            asset total_vesting_fund_steem = asset(0, STEEM_SYMBOL);
            asset total_vesting_shares = asset(0, VESTS_SYMBOL);
            asset accumulative_balance = asset(0, STEEM_SYMBOL);
            asset accumulative_remainder = asset(0, STEEM_SYMBOL);
            asset total_reward_fund_steem = asset(0, STEEM_SYMBOL);
            fc::uint128_t total_reward_shares2; ///< the running total of REWARD^2

            price get_vesting_share_price() const {
                if (total_vesting_fund_steem.amount == 0 ||
                    total_vesting_shares.amount == 0) {
                        return price(asset(1000, STEEM_SYMBOL), asset(1000000, VESTS_SYMBOL));
                }

                return price(total_vesting_shares, total_vesting_fund_steem);
            }

            /**
             *  This property defines the interest rate that SBD deposits receive.
             */
            uint16_t sbd_interest_rate = 0;

            uint16_t sbd_print_rate = STEEMIT_100_PERCENT;

            uint16_t sbd_debt_percent = 0;

            /**
             *  This property switches off fee for SBD holders
             */
            bool is_forced_min_price = false;

            /**
             *  Average block size is updated every block to be:
             *
             *     average_block_size = (99 * average_block_size + new_block_size) / 100
             *
             *  This property is used to update the current_reserve_ratio to maintain approximately
             *  50% or less utilization of network capacity.
             */
            uint32_t average_block_size = 0;

            /**
             *  Maximum block size is decided by the set of active witnesses which change every round.
             *  Each witness posts what they think the maximum size should be as part of their witness
             *  properties, the median size is chosen to be the maximum block size for the round.
             *
             *  @note the minimum value for maximum_block_size is defined by the protocol to prevent the
             *  network from getting stuck by witnesses attempting to set this too low.
             */
            uint32_t maximum_block_size = 0;

            /**
             * The current absolute slot number.  Equal to the total
             * number of slots since genesis.  Also equal to the total
             * number of missed slots plus head_block_number.
             */
            uint64_t current_aslot = 0;

            /**
             * used to compute witness participation.
             */
            fc::uint128_t recent_slots_filled;
            uint8_t participation_count = 0; ///< Divide by 128 to compute participation percentage

            uint32_t last_irreversible_block_num = 0;

            /**
             * The maximum bandwidth the blockchain can support is:
             *
             *    max_bandwidth = maximum_block_size * STEEMIT_BANDWIDTH_AVERAGE_WINDOW_SECONDS / STEEMIT_BLOCK_INTERVAL
             *
             * The maximum virtual bandwidth is:
             *
             *    max_bandwidth * current_reserve_ratio
             */
            uint64_t max_virtual_bandwidth = 0;

            /**
             *   Any time average_block_size <= 50% maximum_block_size this value grows by 1 until it
             *   reaches STEEMIT_MAX_RESERVE_RATIO.  Any time average_block_size is greater than
             *   50% it falls by 1%.  Upward adjustments happen once per round, downward adjustments
             *   happen every block.
             */
            uint64_t current_reserve_ratio = 1;

            uint16_t custom_ops_bandwidth_multiplier = STEEMIT_CUSTOM_OPS_BANDWIDTH_MULTIPLIER;

            uint32_t transit_block_num = UINT32_MAX;
            fc::array<account_name_type, STEEMIT_MAX_WITNESSES> transit_witnesses;

            asset accumulative_emission_per_day = asset(0, STEEM_SYMBOL);

            uint32_t last_nft_token_id = 0;
        };

        using dynamic_global_property_index = multi_index_container<
            dynamic_global_property_object,
            indexed_by<
                ordered_unique<
                    tag<by_id>,
                    member<dynamic_global_property_object, dynamic_global_property_object::id_type, &dynamic_global_property_object::id>
                >
            >,
            allocator<dynamic_global_property_object>
        >;
    }
} // golos::chain

FC_REFLECT((golos::chain::dynamic_global_property_object),
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
                (accumulative_emission_per_day)
                (last_nft_token_id)
)
CHAINBASE_SET_INDEX_TYPE(golos::chain::dynamic_global_property_object, golos::chain::dynamic_global_property_index)
