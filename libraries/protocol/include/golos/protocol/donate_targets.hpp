#pragma once

#include <golos/protocol/steem_operations.hpp>

namespace golos { namespace protocol {

struct blog_donate {
    account_name_type author;
    std::string permlink;
    bool wrong;
};

fc::optional<blog_donate> get_blog_donate(const donate_operation& op);

struct message_donate {
    std::string group;
    account_name_type from;
    account_name_type to;
    uint64_t nonce;
    bool wrong;
};

fc::optional<message_donate> get_message_donate(const donate_operation& op);

}}
