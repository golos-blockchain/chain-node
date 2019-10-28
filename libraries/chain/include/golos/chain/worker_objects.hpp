#pragma once

#include <golos/chain/steem_object_types.hpp>
#include <golos/protocol/worker_operations.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace golos { namespace chain {

    using namespace golos::protocol;

    enum class worker_techspec_state {
        created,
        payment,
        payment_complete,
        closed_by_author,
        closed_by_expiration,
        closed_by_witnesses
    };

    class worker_techspec_object : public object<worker_techspec_object_type, worker_techspec_object> {
    public:
        worker_techspec_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_techspec_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        comment_id_type post;
        worker_techspec_state state;
        asset specification_cost;
        asset development_cost;
        account_name_type worker;
        uint16_t payments_count;
        uint32_t payments_interval;
        time_point_sec next_cashout_time = time_point_sec::maximum();
        uint16_t finished_payments_count = 0;
    };

    class worker_techspec_approve_object : public object<worker_techspec_approve_object_type, worker_techspec_approve_object> {
    public:
        worker_techspec_approve_object() = delete;

        template <typename Constructor, typename Allocator>
        worker_techspec_approve_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        account_name_type approver;
        comment_id_type post;
        worker_techspec_approve_state state;
    };

    struct by_post;

    struct by_next_cashout_time;

    using worker_techspec_index = multi_index_container<
        worker_techspec_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_techspec_object, worker_techspec_object_id_type, &worker_techspec_object::id>>,
            ordered_unique<
                tag<by_post>,
                member<worker_techspec_object, comment_id_type, &worker_techspec_object::post>>,
            ordered_unique<
                tag<by_next_cashout_time>,
                composite_key<
                    worker_techspec_object,
                    member<worker_techspec_object, time_point_sec, &worker_techspec_object::next_cashout_time>,
                    member<worker_techspec_object, worker_techspec_object_id_type, &worker_techspec_object::id>>>>,
        allocator<worker_techspec_object>>;

    struct by_techspec_approver;

    using worker_techspec_approve_index = multi_index_container<
        worker_techspec_approve_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<worker_techspec_approve_object, worker_techspec_approve_object_id_type, &worker_techspec_approve_object::id>>,
            ordered_unique<
                tag<by_techspec_approver>,
                composite_key<
                    worker_techspec_approve_object,
                    member<worker_techspec_approve_object, comment_id_type, &worker_techspec_approve_object::post>,
                    member<worker_techspec_approve_object, account_name_type, &worker_techspec_approve_object::approver>>,
                composite_key_compare<
                    std::less<comment_id_type>,
                    std::less<account_name_type>>>>,
        allocator<worker_techspec_approve_object>>;

} } // golos::chain

FC_REFLECT_ENUM(golos::chain::worker_techspec_state,
    (created)(payment)(payment_complete)(closed_by_author)(closed_by_expiration)(closed_by_witnesses))

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::worker_techspec_object,
    golos::chain::worker_techspec_index);

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::worker_techspec_approve_object,
    golos::chain::worker_techspec_approve_index);
