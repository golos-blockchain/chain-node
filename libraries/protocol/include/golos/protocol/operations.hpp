#pragma once

#include <golos/protocol/operation_util.hpp>
#include <golos/protocol/proposal_operations.hpp>
#include <golos/protocol/steem_operations.hpp>
#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/paid_subscription_operations.hpp>
#include <golos/protocol/nft_operations.hpp>
#include <golos/protocol/steem_virtual_operations.hpp>
#include <golos/protocol/market_events.hpp>

namespace golos { namespace protocol {

        /** NOTE: do not change the order of any operations prior to the virtual operations
         * or it will trigger a hardfork.
         */
        typedef fc::static_variant<
                vote_operation,
                comment_operation,

                transfer_operation,
                transfer_to_vesting_operation,
                withdraw_vesting_operation,

                limit_order_create_operation,
                limit_order_cancel_operation,

                feed_publish_operation,
                convert_operation,

                account_create_operation,
                account_update_operation,

                witness_update_operation,
                account_witness_vote_operation,
                account_witness_proxy_operation,

                pow_operation,

                custom_operation,

                report_over_production_operation,

                delete_comment_operation,
                custom_json_operation,
                comment_options_operation,
                set_withdraw_vesting_route_operation,
                limit_order_create2_operation,
                challenge_authority_operation,
                prove_authority_operation,
                request_account_recovery_operation,
                recover_account_operation,
                change_recovery_account_operation,
                escrow_transfer_operation,
                escrow_dispute_operation,
                escrow_release_operation,
                pow2_operation,
                escrow_approve_operation,
                transfer_to_savings_operation,
                transfer_from_savings_operation,
                cancel_transfer_from_savings_operation,
                custom_binary_operation,
                decline_voting_rights_operation,
                reset_account_operation,
                set_reset_account_operation,
                delegate_vesting_shares_operation,
                account_create_with_delegation_operation,
                account_metadata_operation,
                proposal_create_operation,
                proposal_update_operation,
                proposal_delete_operation,
                chain_properties_update_operation,
                break_free_referral_operation,
                delegate_vesting_shares_with_interest_operation,
                reject_vesting_shares_delegation_operation,
                transit_to_cyberway_operation,
                worker_request_operation,
                worker_request_delete_operation,
                worker_request_vote_operation,
                claim_operation,
                donate_operation,
                transfer_to_tip_operation,
                transfer_from_tip_operation,
                invite_operation,
                invite_claim_operation,
                account_create_with_invite_operation,
                asset_create_operation,
                asset_update_operation,
                asset_issue_operation,
                asset_transfer_operation,
                override_transfer_operation,
                invite_donate_operation,
                invite_transfer_operation,
                limit_order_cancel_ex_operation,
                account_setup_operation,
                paid_subscription_create_operation,
                paid_subscription_update_operation,
                paid_subscription_delete_operation,
                paid_subscription_transfer_operation,
                paid_subscription_cancel_operation,
                nft_collection_operation,
                nft_collection_delete_operation,
                nft_issue_operation,
                nft_transfer_operation,
                nft_sell_operation,
                nft_buy_operation,
                nft_cancel_order_operation,
                nft_auction_operation,

                /// virtual operations below this point
                fill_convert_request_operation,
                author_reward_operation,
                curation_reward_operation,
                comment_reward_operation,
                liquidity_reward_operation,
                interest_operation,
                fill_vesting_withdraw_operation,
                fill_order_operation,
                shutdown_witness_operation,
                fill_transfer_from_savings_operation,
                hardfork_operation,
                comment_payout_update_operation,
                comment_benefactor_reward_operation,
                return_vesting_delegation_operation,
                producer_reward_operation,
                delegation_reward_operation,
                auction_window_reward_operation,
                total_comment_reward_operation,
                worker_reward_operation,
                worker_state_operation,
                convert_sbd_debt_operation,
                internal_transfer_operation,
                comment_feed_operation,
                account_reputation_operation,
                minus_reputation_operation,
                comment_reply_operation,
                comment_mention_operation,
                accumulative_remainder_operation,
                authority_updated_operation,
                account_freeze_operation,
                unwanted_cost_operation,
                unlimit_cost_operation,
                order_create_operation,
                order_delete_operation,
                subscription_payment_operation,
                subscription_inactive_operation,
                subscription_prepaid_return_operation,
                nft_token_operation,
                nft_token_sold_operation,
                referral_operation
        > operation;

        /*void operation_get_required_authorities( const operation& op,
                                                 flat_set<string>& active,
                                                 flat_set<string>& owner,
                                                 flat_set<string>& posting,
                                                 vector<authority>& other );

        void operation_validate( const operation& op );*/

        bool is_market_operation(const operation &op);

        bool is_virtual_operation(const operation &op);

        bool is_custom_json_operation(const operation& op);

        bool is_active_operation(const operation& op);

        struct operation_wrapper {
            operation_wrapper(const operation& op = operation()) : op(op) {}

            operation op;
        };

} } // golos::protocol

/*namespace fc {
    void to_variant(const golos::protocol::operation& var, fc::variant& vo);
    void from_variant(const fc::variant& var, golos::protocol::operation& vo);
}*/

DECLARE_OPERATION_TYPE(golos::protocol::operation)
FC_REFLECT_TYPENAME((golos::protocol::operation))
FC_REFLECT((golos::protocol::operation_wrapper), (op));
