#pragma once

#include <golos/chain/witness_objects.hpp>
#include <golos/chain/database.hpp>

namespace golos { namespace api {
    using golos::protocol::asset;
    using golos::chain::chain_properties;
    using golos::chain::database;

    struct chain_api_properties {
        chain_api_properties(const chain_properties&, const database&);
        chain_api_properties() = default;

        asset account_creation_fee;
        uint32_t maximum_block_size;
        uint16_t sbd_interest_rate;

        fc::optional<asset> create_account_min_golos_fee;
        fc::optional<asset> create_account_min_delegation;
        fc::optional<uint32_t> create_account_delegation_time;
        fc::optional<asset> min_delegation;

        fc::optional<uint16_t> max_referral_interest_rate;
        fc::optional<uint32_t> max_referral_term_sec;
        fc::optional<asset> min_referral_break_fee;
        fc::optional<asset> max_referral_break_fee;

        fc::optional<uint16_t> posts_window;
        fc::optional<uint16_t> posts_per_window;
        fc::optional<uint16_t> comments_window;
        fc::optional<uint16_t> comments_per_window;
        fc::optional<uint16_t> votes_window;
        fc::optional<uint16_t> votes_per_window;

        fc::optional<uint32_t> auction_window_size;

        fc::optional<uint16_t> max_delegated_vesting_interest_rate;

        fc::optional<uint16_t> custom_ops_bandwidth_multiplier;

        fc::optional<uint16_t> min_curation_percent;
        fc::optional<uint16_t> max_curation_percent;

        fc::optional<protocol::curation_curve> curation_reward_curve;

        fc::optional<bool> allow_distribute_auction_reward;
        fc::optional<bool> allow_return_auction_reward_to_fund;

        fc::optional<uint16_t> worker_reward_percent; // not reflected, just for cli-wallet
        fc::optional<uint16_t> witness_reward_percent; // not reflected, just for cli-wallet
        fc::optional<uint16_t> vesting_reward_percent; // not reflected, just for cli-wallet
        fc::optional<uint16_t> worker_emission_percent;
        fc::optional<uint16_t> vesting_of_remain_percent;

        fc::optional<asset> worker_request_creation_fee;
        fc::optional<uint16_t> worker_request_approve_min_percent;

        fc::optional<uint16_t> sbd_debt_convert_rate;

        fc::optional<uint32_t> vote_regeneration_per_day;

        fc::optional<uint32_t> witness_skipping_reset_time;
        fc::optional<uint32_t> witness_idleness_time;
        fc::optional<uint32_t> account_idleness_time;

        fc::optional<uint32_t> claim_idleness_time;
        fc::optional<asset> min_invite_balance;

        fc::optional<asset> asset_creation_fee;
        fc::optional<uint32_t> invite_transfer_interval_sec;

        fc::optional<uint16_t> convert_fee_percent;
        fc::optional<asset> min_golos_power_to_curate;
        fc::optional<uint16_t> negrep_posting_window;
        fc::optional<uint16_t> negrep_posting_per_window;

        fc::optional<asset> unwanted_operation_cost;
    };

} } // golos::api

FC_REFLECT(
    (golos::api::chain_api_properties),
    (account_creation_fee)(maximum_block_size)(sbd_interest_rate)
    (create_account_min_golos_fee)(create_account_min_delegation)
    (create_account_delegation_time)(min_delegation)
    (max_referral_interest_rate)(max_referral_term_sec)(min_referral_break_fee)(max_referral_break_fee)
    (posts_window)(posts_per_window)(comments_window)(comments_per_window)(votes_window)(votes_per_window)(auction_window_size)
    (max_delegated_vesting_interest_rate)(custom_ops_bandwidth_multiplier)
    (min_curation_percent)(max_curation_percent)(curation_reward_curve)
    (allow_distribute_auction_reward)(allow_return_auction_reward_to_fund)
    (worker_reward_percent)(witness_reward_percent)(vesting_reward_percent)
    (worker_emission_percent)(vesting_of_remain_percent)
    (worker_request_creation_fee)(worker_request_approve_min_percent)
    (sbd_debt_convert_rate)(vote_regeneration_per_day)
    (witness_skipping_reset_time)(witness_idleness_time)(account_idleness_time)
    (claim_idleness_time)(min_invite_balance)
    (asset_creation_fee)
    (invite_transfer_interval_sec)
    (convert_fee_percent)(min_golos_power_to_curate)
    (negrep_posting_window)(negrep_posting_per_window)
    (unwanted_operation_cost)
)
