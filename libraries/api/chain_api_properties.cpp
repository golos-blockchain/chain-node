#include <golos/api/chain_api_properties.hpp>

namespace golos { namespace api {

    chain_api_properties::chain_api_properties(
        const chain_properties& src,
        const database& db
    ) : account_creation_fee(src.account_creation_fee),
        maximum_block_size(src.maximum_block_size),
        sbd_interest_rate(src.sbd_interest_rate)
    {
        if (db.has_hardfork(STEEMIT_HARDFORK_0_18__673)) {
            create_account_min_golos_fee = src.create_account_min_golos_fee;
            create_account_min_delegation = src.create_account_min_delegation;
            create_account_delegation_time = src.create_account_delegation_time;
            min_delegation = src.min_delegation;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_19)) {
            max_referral_interest_rate = src.max_referral_interest_rate;
            max_referral_term_sec = src.max_referral_term_sec;
            min_referral_break_fee = src.min_referral_break_fee;
            max_referral_break_fee = src.max_referral_break_fee;
            posts_window = src.posts_window;
            posts_per_window = src.posts_per_window;
            comments_window = src.comments_window;
            comments_per_window = src.comments_per_window;
            votes_window = src.votes_window;
            votes_per_window = src.votes_per_window;
            auction_window_size = src.auction_window_size;
            max_delegated_vesting_interest_rate = src.max_delegated_vesting_interest_rate;
            custom_ops_bandwidth_multiplier = src.custom_ops_bandwidth_multiplier;
            min_curation_percent = src.min_curation_percent;
            max_curation_percent = src.max_curation_percent;
            curation_reward_curve = src.curation_reward_curve;
            allow_distribute_auction_reward = src.allow_distribute_auction_reward;
            allow_return_auction_reward_to_fund = src.allow_return_auction_reward_to_fund;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_22)) {
            worker_reward_percent = src.worker_reward_percent;
            witness_reward_percent = src.witness_reward_percent;
            vesting_reward_percent = src.vesting_reward_percent;
            worker_request_creation_fee = src.worker_request_creation_fee;
            worker_request_approve_min_percent = src.worker_request_approve_min_percent;
            sbd_debt_convert_rate = src.sbd_debt_convert_rate;
            vote_regeneration_per_day = src.vote_regeneration_per_day;
            witness_skipping_reset_time = src.witness_skipping_reset_time;
            witness_idleness_time = src.witness_idleness_time;
            account_idleness_time = src.account_idleness_time;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_23)) {
            claim_idleness_time = src.claim_idleness_time;
            min_invite_balance = src.min_invite_balance;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_24)) {
            asset_creation_fee = src.asset_creation_fee;
            invite_transfer_interval_sec = src.invite_transfer_interval_sec;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_26)) {
            worker_reward_percent = 0;
            witness_reward_percent = 0;
            vesting_reward_percent = 0;
            convert_fee_percent = src.convert_fee_percent;
            min_golos_power_to_curate = src.min_golos_power_to_curate;
            worker_emission_percent = src.worker_emission_percent;
            vesting_of_remain_percent = src.vesting_of_remain_percent;
            negrep_posting_window = src.negrep_posting_window;
            negrep_posting_per_window = src.negrep_posting_per_window;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_27)) {
            negrep_posting_window = 0;
            negrep_posting_per_window = 0;
            claim_idleness_time = 0;
            unwanted_operation_cost = src.unwanted_operation_cost;
            unlimit_operation_cost = src.unlimit_operation_cost;
        }
        if (db.has_hardfork(STEEMIT_HARDFORK_0_28)) {
            min_golos_power_to_emission = src.min_golos_power_to_emission;
        }
        chain_status = db.chain_status().first;
    }

} } // golos::api
