#pragma once

#include <golos/chain/comment_object.hpp>

namespace golos { namespace chain {

using protocol::auction_window_reward_destination_type;

struct comment_bill {
    share_type max_accepted_payout = 1000000000; /// GBG value of the maximum payout this post will receive
    uint16_t percent_steem_dollars = STEEMIT_100_PERCENT; /// the percent of Golos Dollars to key, unkept amounts will be received as Golos Power
    bool allow_curation_rewards = true;
    share_type min_golos_power_to_curate = 0; /// GOLOS value
    uint16_t curation_rewards_percent = STEEMIT_DEF_CURATION_PERCENT;

    auction_window_reward_destination_type auction_window_reward_destination = protocol::to_author;
    uint16_t auction_window_size = STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS;
};

class comment_bill_object
        : public object<comment_bill_object_type, comment_bill_object> {
public:
    comment_bill_object() = delete;

    template<typename Constructor, typename Allocator>
    comment_bill_object(Constructor &&c, allocator <Allocator> a)
            : beneficiaries(a) {
        c(*this);
    }

    id_type id;

    comment_id_type comment;

    comment_bill bill;

    bip::vector <protocol::beneficiary_route_type, allocator<protocol::beneficiary_route_type>> beneficiaries;
};

struct by_comment;

using comment_bill_index = multi_index_container<
    comment_bill_object,
    indexed_by<
        ordered_unique <tag<by_id>,
            member<comment_bill_object, comment_bill_object::id_type, &comment_bill_object::id>
        >,
        ordered_unique <tag<by_comment>,
            member<comment_bill_object, comment_id_type, &comment_bill_object::comment>
        >
    >,
    allocator<comment_bill_object>
>;

}}

CHAINBASE_SET_INDEX_TYPE(golos::chain::comment_bill_object, golos::chain::comment_bill_index)
