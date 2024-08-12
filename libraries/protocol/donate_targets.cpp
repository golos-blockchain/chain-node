#include <golos/protocol/donate_targets.hpp>

namespace golos { namespace protocol {

fc::optional<blog_donate> get_blog_donate(const donate_operation& op) {
    fc::optional<blog_donate> res;

    std::string author_str;
    std::string permlink;
    try {
        author_str = op.memo.target["author"].as_string();
        permlink = op.memo.target["permlink"].as_string();
    } catch (...) {
        return res;
    }

    if (!is_valid_account_name(author_str)) return res;
    auto author = account_name_type(author_str);

    res = blog_donate();
    res->author = author;
    res->permlink = std::move(permlink);
    res->wrong = op.to != author || op.from == op.to;
    return res;
}

fc::optional<message_donate> get_message_donate(const donate_operation& op) {
    fc::optional<message_donate> res;
    std::string group;
    std::string from_str;
    std::string to_str;
    uint64_t nonce = 0;
    try {
        auto group_itr = op.memo.target.find("group");
        if (group_itr != op.memo.target.end()) {
            group = group_itr->value().as_string();
        }
        from_str = op.memo.target["from"].as_string();
        to_str = op.memo.target["to"].as_string();
        nonce = op.memo.target["nonce"].as_uint64();
    } catch (...) {
    }

    if (!is_valid_account_name(from_str)) return res;
    auto from = account_name_type(from_str);

    account_name_type to;
    if (!group.size()) {
        if (!is_valid_account_name(to_str)) return res;
        to = account_name_type(to_str);
    }

    res = message_donate();
    res->group = group;
    res->from = from;
    res->to = to;
    res->nonce = nonce;
    res->wrong = op.to != from || op.from == op.to;
    return res;
}

}}
