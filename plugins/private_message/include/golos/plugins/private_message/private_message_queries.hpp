#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/plugins/cryptor/cryptor_queries.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/private_message/private_message_operations.hpp>

#define PRIVATE_DEFAULT_LIMIT 100

namespace golos { namespace plugins { namespace private_message {

using golos::plugins::cryptor::login_data;

using decrypt_ignore = std::set<std::string>;

struct contact_query : login_data {
    std::string owner;
    private_contact_type type = private_contact_type::unknown;
    std::set<contact_kind> kinds; // empty = any
    uint16_t limit = 20;
    uint32_t offset = 0;
    decrypt_ignore cache;
    bool accounts = false;
    bool relations = true;
};

/**
 * Query for thread messages
 */
struct message_thread_query final : login_data {
    std::string group;
    account_name_type from;
    account_name_type to;

    time_point_sec newest_date = time_point_sec::min();
    bool unread_only = false;
    uint16_t limit = PRIVATE_DEFAULT_LIMIT;
    uint32_t offset = 0;
    decrypt_ignore cache;

    bool accounts = false;
    fc::optional<contact_query> contacts;

    bool is_good(const message_object& mo) const {
        return (!unread_only || mo.read_date == time_point_sec::min());
    }
};

/**
 * Query for inbox/outbox messages
 */
struct message_box_query final {
    fc::flat_set<std::string> select_accounts;
    fc::flat_set<std::string> filter_accounts;
    time_point_sec newest_date = time_point_sec::min();
    bool unread_only = false;
    uint16_t limit = PRIVATE_DEFAULT_LIMIT;
    uint32_t offset = 0;
};

struct private_group_members {
    std::vector<account_name_type> accounts;
    account_name_type start;
    uint32_t limit = 20;
};

enum class private_group_sort : uint8_t {
    by_name,
    by_popularity,
};

struct private_group_query {
    account_name_type member;
    std::set<private_group_member_type> member_types;

    std::string start_group;
    uint32_t limit = 20;

    private_group_sort sort = private_group_sort::by_name;
    fc::optional<private_group_members> with_members;
};

struct private_contact_query : login_data {
    std::string owner;
    std::string contact;
    std::string group;
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

    bool accounts = false;
};

} } } // golos::plugins::private_message

FC_REFLECT_DERIVED(
    (golos::plugins::private_message::contact_query), ((golos::plugins::cryptor::login_data)),
    (owner)(type)(kinds)(limit)(offset)(cache)(accounts)(relations)
)

FC_REFLECT_DERIVED(
    (golos::plugins::private_message::message_thread_query), ((golos::plugins::cryptor::login_data)),
    (group)(from)(to)
    (newest_date)(unread_only)(limit)(offset)(cache)(accounts)(contacts)
)

FC_REFLECT(
    (golos::plugins::private_message::message_box_query),
    (select_accounts)(filter_accounts)(newest_date)(unread_only)(limit)(offset)
)

FC_REFLECT((golos::plugins::private_message::private_group_members),
    (accounts)(start)(limit)
)

FC_REFLECT_ENUM(
    golos::plugins::private_message::private_group_sort,
    (by_name)(by_popularity)
)

FC_REFLECT((golos::plugins::private_message::private_group_query),
    (member)(member_types)(start_group)(limit)(sort)(with_members)
)

FC_REFLECT_DERIVED(
    (golos::plugins::private_message::private_contact_query), ((golos::plugins::cryptor::login_data)),
    (owner)(contact)(group)
)

FC_REFLECT_ENUM(
    golos::plugins::private_message::private_group_member_sort_direction,
    (up)(down)
)

FC_REFLECT((golos::plugins::private_message::private_group_member_sort_condition),
    (direction)(member_type)
)

FC_REFLECT((golos::plugins::private_message::private_group_member_query),
    (group)(member_types)(sort_conditions)(start_member)(limit)(accounts)
)
