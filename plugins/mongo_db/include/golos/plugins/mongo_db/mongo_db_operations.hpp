#pragma once

#include <golos/protocol/operations.hpp>
#include <golos/chain/database.hpp>

#include <golos/plugins/mongo_db/mongo_db_types.hpp>


namespace golos {
namespace plugins {
namespace mongo_db {

    class operation_writer {
    public:
        using result_type = document;
        
        operation_writer();

        result_type operator()(const vote_operation& op);
        result_type operator()(const comment_operation& op);
        result_type operator()(const transfer_operation& op);
        result_type operator()(const transfer_to_vesting_operation& op);
        result_type operator()(const withdraw_vesting_operation& op);
        result_type operator()(const limit_order_create_operation& op);
        result_type operator()(const limit_order_cancel_operation& op);
        result_type operator()(const feed_publish_operation& op);
        result_type operator()(const convert_operation& op);
        result_type operator()(const account_create_operation& op);
        result_type operator()(const account_update_operation& op);
        result_type operator()(const witness_update_operation& op);
        result_type operator()(const account_witness_vote_operation& op);
        result_type operator()(const account_witness_proxy_operation& op);
        result_type operator()(const pow_operation& op);
        result_type operator()(const custom_operation& op);
        result_type operator()(const report_over_production_operation& op);
        result_type operator()(const delete_comment_operation& op);
        result_type operator()(const custom_json_operation& op);
        result_type operator()(const comment_options_operation& op);
        result_type operator()(const set_withdraw_vesting_route_operation& op);
        result_type operator()(const limit_order_create2_operation& op);
        result_type operator()(const challenge_authority_operation& op);
        result_type operator()(const prove_authority_operation& op);
        result_type operator()(const request_account_recovery_operation& op);
        result_type operator()(const recover_account_operation& op);
        result_type operator()(const change_recovery_account_operation& op);
        result_type operator()(const escrow_transfer_operation& op);
        result_type operator()(const escrow_dispute_operation& op);
        result_type operator()(const escrow_release_operation&op);
        result_type operator()(const pow2_operation& op);
        result_type operator()(const escrow_approve_operation& op);
        result_type operator()(const transfer_to_savings_operation& op);
        result_type operator()(const transfer_from_savings_operation& op);
        result_type operator()(const cancel_transfer_from_savings_operation&op);
        result_type operator()(const custom_binary_operation& op);
        result_type operator()(const decline_voting_rights_operation& op);
        result_type operator()(const reset_account_operation& op);
        result_type operator()(const set_reset_account_operation& op);
//
        result_type operator()(const delegate_vesting_shares_operation& op);
        result_type operator()(const delegate_vesting_shares_with_interest_operation& op);
        result_type operator()(const reject_vesting_shares_delegation_operation& op);

        result_type operator()(const transit_to_cyberway_operation& op);
        result_type operator()(const account_create_with_delegation_operation& op);
        result_type operator()(const account_metadata_operation& op);
        result_type operator()(const break_free_referral_operation& op);
        result_type operator()(const proposal_create_operation& op);
        result_type operator()(const proposal_update_operation& op);
        result_type operator()(const proposal_delete_operation& op);
        result_type operator()(const worker_request_operation& op);
        result_type operator()(const worker_request_delete_operation& op);
        result_type operator()(const worker_request_vote_operation& op);
//
        result_type operator()(const fill_convert_request_operation& op);
        result_type operator()(const author_reward_operation& op);
        result_type operator()(const curation_reward_operation& op);
        result_type operator()(const comment_reward_operation& op);
        result_type operator()(const liquidity_reward_operation& op);
        result_type operator()(const interest_operation& op);
        result_type operator()(const fill_vesting_withdraw_operation& op);
        result_type operator()(const fill_order_operation& op);
        result_type operator()(const shutdown_witness_operation& op);
        result_type operator()(const fill_transfer_from_savings_operation& op);
        result_type operator()(const hardfork_operation& op);
        result_type operator()(const producer_reward_operation& op);
        result_type operator()(const comment_payout_update_operation& op);
        result_type operator()(const comment_benefactor_reward_operation& op);
//
        result_type operator()(const return_vesting_delegation_operation& op);
//
        result_type operator()(const chain_properties_update_operation& op);
        result_type operator()(const delegation_reward_operation& op);
        result_type operator()(const auction_window_reward_operation& op);
        result_type operator()(const total_comment_reward_operation& op);
        result_type operator()(const worker_state_operation& op);
        result_type operator()(const worker_reward_operation& op);
        result_type operator()(const convert_sbd_debt_operation& op);
        result_type operator()(const claim_operation& op);
        result_type operator()(const donate_operation& op);
        result_type operator()(const transfer_to_tip_operation& op);
        result_type operator()(const transfer_from_tip_operation& op);
        result_type operator()(const invite_operation& op);
        result_type operator()(const invite_claim_operation& op);
        result_type operator()(const account_create_with_invite_operation& op);
        result_type operator()(const asset_create_operation& op);
        result_type operator()(const asset_update_operation& op);
        result_type operator()(const asset_issue_operation& op);
        result_type operator()(const asset_transfer_operation& op);
        result_type operator()(const override_transfer_operation& op);
        result_type operator()(const invite_donate_operation& op);
        result_type operator()(const invite_transfer_operation& op);
        result_type operator()(const internal_transfer_operation& op);
        result_type operator()(const limit_order_cancel_ex_operation& op);
        result_type operator()(const account_setup_operation& op);
    };

}}} // golos::plugins::mongo_db
