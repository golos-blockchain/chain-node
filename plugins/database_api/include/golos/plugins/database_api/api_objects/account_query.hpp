#pragma once

#include <golos/chain/account_object.hpp>
#include <golos/plugins/database_api/forward.hpp>
#include <golos/plugins/database_api/api_objects/account_recovery_request_api_object.hpp>

namespace golos {
    namespace plugins {
        namespace database_api {
            struct account_query {
                account_name_type current;
            };
        }
    }
}

FC_REFLECT((golos::plugins::database_api::account_query),
    (current)
)
