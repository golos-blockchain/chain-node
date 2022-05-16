#pragma once

#include <golos/chain/account_object.hpp>
#include <golos/plugins/database_api/forward.hpp>
#include <golos/plugins/database_api/api_objects/account_recovery_request_api_object.hpp>

namespace golos {
    namespace plugins {
        namespace database_api {
            enum class account_recovery_fill {
                change_partner,
                recovery
            };

            struct account_recovery_query {
                std::set<account_name_type> accounts;
                std::set<account_recovery_fill> fill = { account_recovery_fill::recovery };
            };

            struct account_recovery_info {
                time_point_sec head_block_time;

                fc::optional<change_recovery_account_request_api_object> change_partner_request;
                account_name_type recovery_account;

                fc::optional<account_recovery_request_api_object> recovery_request;
                fc::optional<bool> recovered_owner;
                fc::optional<time_point_sec> last_owner_update;
                fc::optional<time_point_sec> last_account_recovery;
                fc::optional<time_point_sec> next_owner_update_possible;
                fc::optional<bool> can_update_owner_now;
                fc::optional<authority> owner_authority;
                fc::optional<std::string> json_metadata;
            };

            using account_recovery_api_object = fc::mutable_variant_object;
        }
    }
}

FC_REFLECT_ENUM(golos::plugins::database_api::account_recovery_fill,
    (change_partner)(recovery)
)

FC_REFLECT((golos::plugins::database_api::account_recovery_query),
    (accounts)(fill)
)

FC_REFLECT((golos::plugins::database_api::account_recovery_info),
    (head_block_time)
    (change_partner_request)(recovery_account)
    (recovery_request)(recovered_owner)(last_owner_update)(last_account_recovery)
    (next_owner_update_possible)(can_update_owner_now)
    (owner_authority)(json_metadata)
)
