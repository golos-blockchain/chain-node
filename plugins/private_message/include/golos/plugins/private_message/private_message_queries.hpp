#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/private_message/private_message_operations.hpp>

namespace golos { namespace plugins { namespace private_message {

struct private_group_members {
    std::vector<account_name_type> accounts;
    account_name_type start;
    uint32_t limit = 20;
};

struct private_group_query {
    account_name_type member;
    std::set<private_group_member_type> member_types;

    std::string start_group;
    uint32_t limit = 20;

    fc::optional<private_group_members> with_members;
};

enum class private_group_member_sort_direction : uint8_t {
    up,
    down,
};

struct private_group_member_sort_condition {
    private_group_member_sort_direction direction = private_group_member_sort_direction::up;
    private_group_member_type member_type = private_group_member_type::pending;
};

struct private_group_member_query {
    std::string group;
    std::set<private_group_member_type> member_types;
    std::vector<private_group_member_sort_condition> sort_conditions;

    account_name_type start_member;
    uint32_t limit = 20;
};

} } } // golos::plugins::private_message

FC_REFLECT((golos::plugins::private_message::private_group_members),
    (accounts)(start)(limit)
)

FC_REFLECT((golos::plugins::private_message::private_group_query),
    (member)(member_types)(start_group)(limit)(with_members)
)

FC_REFLECT_ENUM(
    golos::plugins::private_message::private_group_member_sort_direction,
    (up)(down)
)

FC_REFLECT((golos::plugins::private_message::private_group_member_sort_condition),
    (direction)(member_type)
)

FC_REFLECT((golos::plugins::private_message::private_group_member_query),
    (group)(member_types)(sort_conditions)(start_member)(limit)
)
