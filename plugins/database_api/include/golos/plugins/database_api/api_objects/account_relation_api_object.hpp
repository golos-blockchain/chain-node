#pragma once

#include <golos/chain/account_object.hpp>
#include <golos/plugins/database_api/forward.hpp>

namespace golos {
    namespace plugins {
        namespace database_api {
            using account_relation_api_object = account_relation_object;

            enum class relation_direction : uint8_t {
                me_to_them,
                they_to_me
            };

            enum class relation_type : uint8_t {
                blocking
            };

            struct list_relation_query {
                std::set<account_name_type> my_accounts;

                relation_direction direction = relation_direction::me_to_them;
                relation_type type = relation_type::blocking;
                account_name_type from;
                int limit = 100;
            };

            struct get_relation_query {
                account_name_type my_account;
                std::set<account_name_type> with_accounts;

                relation_direction direction = relation_direction::me_to_them;
                relation_type type = relation_type::blocking;
            };

            using rel_list = std::vector<account_relation_api_object>;
}}}

FC_REFLECT_ENUM(
    golos::plugins::database_api::relation_direction,
    (me_to_them)(they_to_me)
)

FC_REFLECT_ENUM(
    golos::plugins::database_api::relation_type,
    (blocking)
)

FC_REFLECT(
    (golos::plugins::database_api::list_relation_query),
    (my_accounts)(direction)(type)(from)(limit)
)

FC_REFLECT(
    (golos::plugins::database_api::get_relation_query),
    (my_account)(with_accounts)(direction)(type)
)
