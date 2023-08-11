#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/chain/paid_subscription_objects.hpp>

using namespace golos::chain;

namespace golos { namespace plugins { namespace paid_subscription_api {

struct paid_subscribe_result {
    paid_subscriber_object subscribe;
    paid_subscription_object subscription;
};

}}}

FC_REFLECT(
    (golos::plugins::paid_subscription_api::paid_subscribe_result),
    (subscribe)(subscription)
)
