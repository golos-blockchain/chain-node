#pragma once

#include <golos/chain/steem_object_types.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <golos/protocol/paid_subscription_operations.hpp>

namespace golos { namespace chain {

    using namespace golos::protocol;

    class paid_subscription_object : public object<paid_subscription_object_type, paid_subscription_object> {
    public:
        paid_subscription_object() {
        }

        template <typename Constructor, typename Allocator>
        paid_subscription_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;
        account_name_type author;
        paid_subscription_id oid;
        asset cost;
        bool tip_cost = false;
        bool allow_prepaid = true;
        uint32_t interval = 0;
        uint32_t executions = 0;

        uint32_t subscribers = 0;
        uint32_t active_subscribers = 0;
        time_point_sec created;
    };

    class paid_subscriber_object : public object<paid_subscriber_object_type, paid_subscriber_object> {
    public:
        paid_subscriber_object() {
        }

        template <typename Constructor, typename Allocator>
        paid_subscriber_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        account_name_type subscriber;
        time_point_sec subscribed;

        // all subscription info is stored here because author can delete it in any moment
        account_name_type author;
        paid_subscription_id oid;
        asset cost;
        bool tip_cost = false;
        uint32_t interval = 0;
        uint32_t executions = 0;

        uint32_t executions_left = 0;
        asset prepaid;
        time_point_sec prepaid_until; // API only
        time_point_sec next_payment;
        bool active = true;
    };

    struct by_author_oid;

    using paid_subscription_index = multi_index_container<
        paid_subscription_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<paid_subscription_object, paid_subscription_object_id_type, &paid_subscription_object::id>
            >,
            ordered_unique<
                tag<by_author_oid>,
                composite_key<
                    paid_subscription_object,
                    member<paid_subscription_object, account_name_type, &paid_subscription_object::author>,
                    member<paid_subscription_object, paid_subscription_id, &paid_subscription_object::oid>
                >
            >
        >,
        allocator<paid_subscription_object>
    >;

    struct by_author_oid_subscriber;
    struct by_author_oid_subscribed;
    struct by_subscriber_author_oid;
    struct by_subscriber_subscribed;
    struct by_next_payment;

    using paid_subscriber_index = multi_index_container<
        paid_subscriber_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<paid_subscriber_object, paid_subscriber_object_id_type, &paid_subscriber_object::id>
            >,
            ordered_unique<
                tag<by_author_oid_subscriber>,
                composite_key<
                    paid_subscriber_object,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::author>,
                    member<paid_subscriber_object, paid_subscription_id, &paid_subscriber_object::oid>,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::subscriber>
                >
            >,
            ordered_non_unique<
                tag<by_author_oid_subscribed>,
                composite_key<
                    paid_subscriber_object,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::author>,
                    member<paid_subscriber_object, paid_subscription_id, &paid_subscriber_object::oid>,
                    member<paid_subscriber_object, time_point_sec, &paid_subscriber_object::subscribed>,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::subscriber>
                >,
                composite_key_compare<
                    std::less<account_name_type>,
                    std::less<paid_subscription_id>,
                    std::greater<time_point_sec>,
                    std::less<account_name_type>
                >
            >,
            ordered_unique<
                tag<by_subscriber_author_oid>,
                composite_key<
                    paid_subscriber_object,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::subscriber>,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::author>,
                    member<paid_subscriber_object, paid_subscription_id, &paid_subscriber_object::oid>
                >
            >,
            ordered_non_unique<
                tag<by_subscriber_subscribed>,
                composite_key<
                    paid_subscriber_object,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::subscriber>,
                    member<paid_subscriber_object, time_point_sec, &paid_subscriber_object::subscribed>,
                    member<paid_subscriber_object, account_name_type, &paid_subscriber_object::author>,
                    member<paid_subscriber_object, paid_subscription_id, &paid_subscriber_object::oid>
                >,
                composite_key_compare<
                    std::less<account_name_type>,
                    std::greater<time_point_sec>,
                    std::less<account_name_type>,
                    std::less<paid_subscription_id>
                >
            >,
            ordered_non_unique<
                tag<by_next_payment>,
                member<paid_subscriber_object, time_point_sec, &paid_subscriber_object::next_payment>,
                std::less<time_point_sec>
            >
        >,
        allocator<paid_subscriber_object>
    >;

} } // golos::chain

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::paid_subscription_object,
    golos::chain::paid_subscription_index);

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::paid_subscriber_object,
    golos::chain::paid_subscriber_index);

FC_REFLECT((golos::chain::paid_subscription_object),
    (id)(author)(oid)(cost)(tip_cost)(allow_prepaid)(interval)(executions)(subscribers)(active_subscribers)(created)
)

FC_REFLECT((golos::chain::paid_subscriber_object),
    (id)(subscriber)(subscribed)
    (author)(oid)(cost)(tip_cost)(interval)(executions)
    (executions_left)(prepaid)(prepaid_until)(next_payment)(active)
)
