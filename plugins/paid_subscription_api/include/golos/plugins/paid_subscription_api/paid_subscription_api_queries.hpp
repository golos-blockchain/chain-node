#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/protocol/paid_subscription_operations.hpp>

using namespace golos::protocol;

namespace golos { namespace plugins { namespace paid_subscription_api {

struct paid_subscriptions_by_author_query {
    account_name_type author;

    paid_subscription_id from = paid_subscription_id(); // start oid
    size_t limit = 20;
};

struct paid_subscription_options_query {
    account_name_type author;
    paid_subscription_id oid;
};

enum class paid_subscribers_sort : uint8_t {
    by_name,
    by_date
};

enum class paid_subscribers_state : uint8_t {
    active_only,
    inactive_only,
    active_inactive
};

struct paid_subscribers_query {
    account_name_type author;
    paid_subscription_id oid;

    account_name_type from = ""; // start subscriber
    size_t limit = 20;

    paid_subscribers_sort sort = paid_subscribers_sort::by_name;
    paid_subscribers_state state = paid_subscribers_state::active_inactive;

    std::set<account_name_type> select_subscribers;

    bool is_good_one(const account_name_type& subscriber) const {
        return !select_subscribers.size() || select_subscribers.find(subscriber) != select_subscribers.end();
    }
};

enum class paid_subscriptions_sort : uint8_t {
    by_author_oid,
    by_date
};

struct paid_subscriptions_query {
    account_name_type subscriber;

    account_name_type start_author = account_name_type();
    paid_subscription_id start_oid = paid_subscription_id();
    size_t limit = 20;

    paid_subscriptions_sort sort = paid_subscriptions_sort::by_author_oid;
    paid_subscribers_state state = paid_subscribers_state::active_inactive;
};

struct paid_subscribe_query {
    account_name_type author;
    paid_subscription_id oid;
    account_name_type subscriber;
};

}}}

FC_REFLECT(
    (golos::plugins::paid_subscription_api::paid_subscriptions_by_author_query),
    (author)(from)(limit)
)

FC_REFLECT(
    (golos::plugins::paid_subscription_api::paid_subscription_options_query),
    (author)(oid)
)

FC_REFLECT_ENUM(
    golos::plugins::paid_subscription_api::paid_subscribers_sort,
    (by_name)(by_date)
)

FC_REFLECT_ENUM(
    golos::plugins::paid_subscription_api::paid_subscribers_state,
    (active_only)(inactive_only)(active_inactive)
)

FC_REFLECT(
    (golos::plugins::paid_subscription_api::paid_subscribers_query),
    (author)(oid)(from)(limit)(sort)(state)(select_subscribers)
)

FC_REFLECT_ENUM(
    golos::plugins::paid_subscription_api::paid_subscriptions_sort,
    (by_author_oid)(by_date)
)

FC_REFLECT(
    (golos::plugins::paid_subscription_api::paid_subscriptions_query),
    (subscriber)(start_author)(start_oid)(limit)(sort)(state)
)

FC_REFLECT(
    (golos::plugins::paid_subscription_api::paid_subscribe_query),
    (subscriber)(author)(oid)
)
