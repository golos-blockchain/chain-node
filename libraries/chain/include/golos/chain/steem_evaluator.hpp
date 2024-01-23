#pragma once

#include <golos/protocol/steem_operations.hpp>
#include <golos/protocol/proposal_operations.hpp>
#include <golos/chain/evaluator.hpp>
#include <golos/chain/account_object.hpp>

namespace golos { namespace chain {
        using namespace golos::protocol;

        DEFINE_EVALUATOR(account_create)
        DEFINE_EVALUATOR(account_create_with_delegation)
        DEFINE_EVALUATOR(account_create_with_invite)
        DEFINE_EVALUATOR(account_update)
        DEFINE_EVALUATOR(account_metadata)
        DEFINE_EVALUATOR(transfer)
        DEFINE_EVALUATOR(transfer_to_vesting)
        DEFINE_EVALUATOR(witness_update)
        DEFINE_EVALUATOR(account_witness_vote)
        DEFINE_EVALUATOR(account_witness_proxy)
        DEFINE_EVALUATOR(withdraw_vesting)
        DEFINE_EVALUATOR(set_withdraw_vesting_route)
        DEFINE_EVALUATOR(comment)
        DEFINE_EVALUATOR(comment_options)
        DEFINE_EVALUATOR(delete_comment)
        DEFINE_EVALUATOR(vote)
        DEFINE_EVALUATOR(custom)
        DEFINE_EVALUATOR(custom_json)
        DEFINE_EVALUATOR(custom_binary)
        DEFINE_EVALUATOR(pow)
        DEFINE_EVALUATOR(pow2)
        DEFINE_EVALUATOR(feed_publish)
        DEFINE_EVALUATOR(convert)
        DEFINE_EVALUATOR(limit_order_create)
        DEFINE_EVALUATOR(limit_order_cancel)
        DEFINE_EVALUATOR(report_over_production)
        DEFINE_EVALUATOR(limit_order_create2)
        DEFINE_EVALUATOR(escrow_transfer)
        DEFINE_EVALUATOR(escrow_approve)
        DEFINE_EVALUATOR(escrow_dispute)
        DEFINE_EVALUATOR(escrow_release)
        DEFINE_EVALUATOR(challenge_authority)
        DEFINE_EVALUATOR(prove_authority)
        DEFINE_EVALUATOR(request_account_recovery)
        DEFINE_EVALUATOR(recover_account)
        DEFINE_EVALUATOR(change_recovery_account)
        DEFINE_EVALUATOR(transfer_to_savings)
        DEFINE_EVALUATOR(transfer_from_savings)
        DEFINE_EVALUATOR(cancel_transfer_from_savings)
        DEFINE_EVALUATOR(decline_voting_rights)
        DEFINE_EVALUATOR(reset_account)
        DEFINE_EVALUATOR(set_reset_account)
        DEFINE_EVALUATOR(delegate_vesting_shares)
        DEFINE_EVALUATOR(proposal_delete)
        DEFINE_EVALUATOR(chain_properties_update)
        DEFINE_EVALUATOR(break_free_referral)
        DEFINE_EVALUATOR(delegate_vesting_shares_with_interest)
        DEFINE_EVALUATOR(reject_vesting_shares_delegation)
        DEFINE_EVALUATOR(claim)
        DEFINE_EVALUATOR(donate)
        DEFINE_EVALUATOR(transfer_to_tip)
        DEFINE_EVALUATOR(transfer_from_tip)
        DEFINE_EVALUATOR(invite)
        DEFINE_EVALUATOR(invite_claim)
        DEFINE_EVALUATOR(asset_create)
        DEFINE_EVALUATOR(asset_update)
        DEFINE_EVALUATOR(asset_issue)
        DEFINE_EVALUATOR(asset_transfer)
        DEFINE_EVALUATOR(override_transfer)
        DEFINE_EVALUATOR(transit_to_cyberway)
        DEFINE_EVALUATOR(invite_donate)
        DEFINE_EVALUATOR(invite_transfer)
        DEFINE_EVALUATOR(limit_order_cancel_ex)
        DEFINE_EVALUATOR(account_setup)

        class proposal_create_evaluator: public evaluator_impl<proposal_create_evaluator> {
        public:
            using operation_type = proposal_create_operation;

            proposal_create_evaluator(database& db);

            void do_apply(const operation_type& o);

        protected:
            int depth_ = 0;
        };

        class proposal_update_evaluator: public evaluator_impl<proposal_update_evaluator> {
        public:
            using operation_type = proposal_update_operation;

            proposal_update_evaluator(database& db);

            void do_apply(const operation_type& o);

        protected:
            int depth_ = 0;
        };

        DEFINE_EVALUATOR(worker_request)
        DEFINE_EVALUATOR(worker_request_delete)
        DEFINE_EVALUATOR(worker_request_vote)

        DEFINE_EVALUATOR(paid_subscription_create)
        DEFINE_EVALUATOR(paid_subscription_update)
        DEFINE_EVALUATOR(paid_subscription_delete)
        DEFINE_EVALUATOR(paid_subscription_transfer)
        DEFINE_EVALUATOR(paid_subscription_cancel)

        DEFINE_EVALUATOR(nft_collection)
        DEFINE_EVALUATOR(nft_collection_delete)
        DEFINE_EVALUATOR(nft_issue)
        DEFINE_EVALUATOR(nft_transfer)
        DEFINE_EVALUATOR(nft_sell)
        DEFINE_EVALUATOR(nft_cancel_order)
        DEFINE_EVALUATOR(nft_buy)
        DEFINE_EVALUATOR(nft_auction)

} } // golos::chain
