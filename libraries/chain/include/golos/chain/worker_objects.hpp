#pragma once

#include <golos/chain/steem_object_types.hpp>
#include <golos/protocol/worker_operations.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace golos { namespace chain {

    using namespace golos::protocol;

    class worker_request_object : public object<worker_request_object_type, worker_request_object> {
    public:
        worker_request_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_request_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        comment_id_type post;
        account_name_type worker;
        worker_request_state state;
        asset required_amount_min;
        asset required_amount_max;
        bool vest_reward;
        uint32_t duration;
        time_point_sec created;
        time_point_sec vote_end_time = time_point_sec::maximum();
        share_type stake_rshares;
        share_type stake_total;
        asset remaining_payment;
    };

    class worker_request_vote_object : public object<worker_request_vote_object_type, worker_request_vote_object> {
    public:
        worker_request_vote_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_request_vote_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        account_name_type voter;
        comment_id_type post;
        int16_t vote_percent;
        share_type rshares; // can be removed from consensus and re-calculated in place, but used also by API
        share_type stake;
    };

    struct by_post;
    struct by_vote_end_time;
    struct by_state;

    using worker_request_index = multi_index_container<
        worker_request_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_request_object, worker_request_object_id_type, &worker_request_object::id>>,
            ordered_unique<
                tag<by_post>,
                member<worker_request_object, comment_id_type, &worker_request_object::post>>,
            ordered_unique<
                tag<by_vote_end_time>,
                composite_key<
                    worker_request_object,
                    member<worker_request_object, time_point_sec, &worker_request_object::vote_end_time>,
                    member<worker_request_object, worker_request_object_id_type, &worker_request_object::id>>>,
            ordered_unique<
                tag<by_state>,
                composite_key<
                    worker_request_object,
                    member<worker_request_object, worker_request_state, &worker_request_object::state>,
                    member<worker_request_object, worker_request_object_id_type, &worker_request_object::id>>>>,
        allocator<worker_request_object>>;

    struct by_request_voter;
    struct by_request_rshares;

    using worker_request_vote_index = multi_index_container<
        worker_request_vote_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_request_vote_object, worker_request_vote_object_id_type, &worker_request_vote_object::id>>,
            ordered_unique<
                tag<by_request_voter>,
                composite_key<
                    worker_request_vote_object,
                    member<worker_request_vote_object, comment_id_type, &worker_request_vote_object::post>,
                    member<worker_request_vote_object, account_name_type, &worker_request_vote_object::voter>>,
                composite_key_compare<
                    std::less<comment_id_type>,
                    std::less<account_name_type>>>,
            ordered_non_unique<
                tag<by_request_rshares>,
                composite_key<
                    worker_request_vote_object,
                    member<worker_request_vote_object, comment_id_type, &worker_request_vote_object::post>,
                    member<worker_request_vote_object, share_type, &worker_request_vote_object::rshares>>,
                composite_key_compare<
                    std::less<comment_id_type>,
                    std::greater<share_type>>>>,
        allocator<worker_request_vote_object>>;

} } // golos::chain

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::worker_request_object,
    golos::chain::worker_request_index);

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::worker_request_vote_object,
    golos::chain::worker_request_vote_index);

FC_REFLECT((golos::chain::worker_request_vote_object),
    (voter)(vote_percent)(rshares)(stake)
)
