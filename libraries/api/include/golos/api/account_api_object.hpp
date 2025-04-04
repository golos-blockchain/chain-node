#ifndef GOLOS_ACCOUNT_API_OBJ_HPP
#define GOLOS_ACCOUNT_API_OBJ_HPP

#include <golos/chain/account_object.hpp>
#include <golos/chain/database.hpp>
#include <golos/protocol/types.hpp>
#include <golos/chain/steem_object_types.hpp>

namespace golos { namespace api {

using golos::chain::account_object;
using golos::chain::relation_direction;
using golos::chain::relation_type;
using golos::chain::account_freeze_object;
using protocol::asset;
using protocol::share_type;
using protocol::authority;
using protocol::account_name_type;
using protocol::public_key_type;

struct account_freeze_api_object {
    account_freeze_api_object(const account_freeze_object& afo);

    account_freeze_api_object();

    authority owner;
    authority active;
    authority posting;
    uint32_t hardfork;
    time_point_sec frozen;
};

struct account_services {
    asset post;
    asset comment;
    asset vote;
    asset vote_rep; // only downvote
};

struct account_api_object {
    account_api_object(const account_object&, const golos::chain::database&);

    account_api_object();

    account_object::id_type id;

    account_name_type name;
    authority owner;
    authority active;
    authority posting;
    public_key_type memo_key;
    std::string json_metadata;
    account_name_type proxy;

    time_point_sec last_owner_update;
    time_point_sec last_account_update;

    time_point_sec created;
    bool mined;
    bool owner_challenged;
    bool active_challenged;
    time_point_sec last_owner_proved;
    time_point_sec last_active_proved;
    account_name_type recovery_account;
    account_name_type reset_account;
    time_point_sec last_account_recovery;
    uint32_t comment_count;
    uint32_t lifetime_vote_count;
    uint32_t post_count;
    uint32_t sponsor_count;
    uint32_t referral_count;
    uint16_t posts_capacity;
    uint16_t comments_capacity;
    uint16_t voting_capacity;

    bool can_vote;
    uint16_t voting_power;
    time_point_sec last_vote_time;

    asset balance;
    asset savings_balance;
    asset accumulative_balance;
    asset tip_balance;
    asset market_balance;
    asset nft_hold_balance;

    asset sbd_balance;
    uint128_t sbd_seconds;
    time_point_sec sbd_seconds_last_update;
    time_point_sec sbd_last_interest_payment;

    asset market_sbd_balance;

    asset savings_sbd_balance;
    uint128_t savings_sbd_seconds;
    time_point_sec savings_sbd_seconds_last_update;
    time_point_sec savings_sbd_last_interest_payment;

    uint8_t savings_withdraw_requests;

    share_type benefaction_rewards;
    share_type curation_rewards;
    share_type delegation_rewards;
    share_type posting_rewards;

    asset vesting_shares;
    asset delegated_vesting_shares;
    asset received_vesting_shares;
    asset emission_delegated_vesting_shares;
    asset emission_received_vesting_shares;
    asset vesting_withdraw_rate;
    time_point_sec next_vesting_withdrawal;
    share_type withdrawn;
    share_type to_withdraw;
    uint16_t withdraw_routes;

    std::vector<share_type> proxied_vsf_votes;

    uint16_t witnesses_voted_for;

    share_type average_bandwidth;
    share_type average_market_bandwidth;
    share_type average_custom_json_bandwidth;
    share_type lifetime_bandwidth;
    share_type lifetime_market_bandwidth;
    share_type lifetime_custom_json_bandwidth;
    time_point_sec last_bandwidth_update;
    time_point_sec last_market_bandwidth_update;
    time_point_sec last_custom_json_bandwidth_update;
    time_point_sec last_comment;
    time_point_sec last_post;
    share_type post_bandwidth = STEEMIT_100_PERCENT;

    set<string> witness_votes;

    share_type reputation;

    account_name_type referrer_account;
    uint16_t referrer_interest_rate = 0;
    time_point_sec referral_end_date = time_point_sec::min();
    asset referral_break_fee = asset(0, STEEM_SYMBOL);

    time_point_sec last_active_operation = time_point_sec::min();
    time_point_sec last_claim = time_point_sec::min();
    time_point_sec claim_expiration = time_point_sec::min();

    uint32_t proved_hf = 0;
    bool frozen = false;
    fc::optional<account_freeze_api_object> freeze;
    bool do_not_bother = false;
    account_services services;
    fc::optional<fc::mutable_variant_object> relations;
};

fc::mutable_variant_object current_get_relations(
    const golos::chain::database& _db,
    account_name_type current, account_name_type account
);

} } // golos::api

FC_REFLECT((golos::api::account_freeze_api_object),
    (owner)(active)(posting)(hardfork)(frozen)
)

FC_REFLECT((golos::api::account_services),
    (post)(comment)(vote)(vote_rep)
)

FC_REFLECT((golos::api::account_api_object),
    (id)(name)(owner)(active)(posting)(memo_key)(json_metadata)(proxy)(last_owner_update)(last_account_update)
    (created)(mined)(owner_challenged)(active_challenged)(last_owner_proved)(last_active_proved)
    (recovery_account)(last_account_recovery)(reset_account)(comment_count)(lifetime_vote_count)
    (post_count)(sponsor_count)(referral_count)(can_vote)(voting_power)(last_vote_time)(balance)(savings_balance)(accumulative_balance)(tip_balance)(market_balance)
    (sbd_balance)(sbd_seconds)(sbd_seconds_last_update)(sbd_last_interest_payment)
    (market_sbd_balance)(nft_hold_balance)
    (savings_sbd_balance)(savings_sbd_seconds)(savings_sbd_seconds_last_update)(savings_sbd_last_interest_payment)
    (savings_withdraw_requests)(vesting_shares)(delegated_vesting_shares)(received_vesting_shares)
    (emission_delegated_vesting_shares)(emission_received_vesting_shares)
    (vesting_withdraw_rate)(next_vesting_withdrawal)(withdrawn)(to_withdraw)(withdraw_routes)
    (benefaction_rewards)(curation_rewards)(delegation_rewards)(posting_rewards)(proxied_vsf_votes)(witnesses_voted_for)
    (average_bandwidth)(average_market_bandwidth)(average_custom_json_bandwidth)(lifetime_bandwidth)(lifetime_market_bandwidth)(lifetime_custom_json_bandwidth)
    (last_bandwidth_update)(last_market_bandwidth_update)(last_custom_json_bandwidth_update)
    (last_comment)(last_post)(post_bandwidth)
    (witness_votes)(reputation)(posts_capacity)(comments_capacity)(voting_capacity)
    (referrer_account)(referrer_interest_rate)(referral_end_date)(referral_break_fee)
    (last_active_operation)(last_claim)(claim_expiration)
    (proved_hf)(frozen)(freeze)(do_not_bother)(services)(relations)
)

#endif //GOLOS_ACCOUNT_API_OBJ_HPP
