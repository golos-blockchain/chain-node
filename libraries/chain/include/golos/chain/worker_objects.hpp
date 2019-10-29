#pragma once

#include <golos/chain/steem_object_types.hpp>
#include <golos/protocol/worker_operations.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace golos { namespace chain {

    using namespace golos::protocol;

    enum class worker_request_state {
        created,
        payment,
        payment_complete,
        closed_by_author,
        closed_by_expiration,
        closed_by_witnesses
    };

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
        uint32_t duration;
        time_point_sec next_cashout_time = time_point_sec::maximum();
    };

    class worker_request_approve_object : public object<worker_request_approve_object_type, worker_request_approve_object> {
    public:
        worker_request_approve_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_request_approve_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        account_name_type approver;
        comment_id_type post;
        worker_request_approve_state state;
    };

    struct by_post;

    struct by_next_cashout_time;

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
                tag<by_next_cashout_time>,
                composite_key<
                    worker_request_object,
                    member<worker_request_object, time_point_sec, &worker_request_object::next_cashout_time>,
                    member<worker_request_object, worker_request_object_id_type, &worker_request_object::id>>>>,
        allocator<worker_request_object>>;

    struct by_request_approver;

    using worker_request_approve_index = multi_index_container<
        worker_request_approve_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_request_approve_object, worker_request_approve_object_id_type, &worker_request_approve_object::id>>,
            ordered_unique<
                tag<by_request_approver>,
                composite_key<
                    worker_request_approve_object,
                    member<worker_request_approve_object, comment_id_type, &worker_request_approve_object::post>,
                    member<worker_request_approve_object, account_name_type, &worker_request_approve_object::approver>>,
                composite_key_compare<
                    std::less<comment_id_type>,
                    std::less<account_name_type>>>>,
        allocator<worker_request_approve_object>>;

} } // golos::chain

FC_REFLECT_ENUM(golos::chain::worker_request_state,
    (created)(payment)(payment_complete)(closed_by_author)(closed_by_expiration)(closed_by_witnesses))

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::worker_request_object,
    golos::chain::worker_request_index);

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::worker_request_approve_object,
    golos::chain::worker_request_approve_index);
