#pragma once

#include <golos/chain/account_object.hpp>

namespace golos {
    namespace plugins {
        namespace account_relations {
            struct account_relation_api_object {
                account_name_type who;
                account_name_type whom;
                bool blocking = false;

                account_relation_api_object(const account_blocking_object& abo) {
                    who = abo.account;
                    whom = abo.blocking;
                    blocking = true;
                }

                account_relation_api_object() {
                }
            };

            struct list_relation_query {
                std::set<account_name_type> my_accounts;

                relation_direction direction = relation_direction::me_to_them;
                relation_type type = relation_type::blocking;
                account_name_type from;
                size_t limit = 100;
            };

            struct get_relation_query {
                account_name_type my_account;
                std::set<account_name_type> with_accounts;

                relation_direction direction = relation_direction::me_to_them;
                relation_type type = relation_type::blocking;
            };

            using rel_list = std::vector<account_relation_api_object>;
}}}

FC_REFLECT(
    (golos::plugins::account_relations::account_relation_api_object),
    (who)(whom)(blocking)
)

FC_REFLECT(
    (golos::plugins::account_relations::list_relation_query),
    (my_accounts)(direction)(type)(from)(limit)
)

FC_REFLECT(
    (golos::plugins::account_relations::get_relation_query),
    (my_account)(with_accounts)(direction)(type)
)
