#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/protocol/paid_subscription_operations.hpp>

using namespace golos::protocol;

namespace golos { namespace plugins { namespace cryptor {

struct encrypt_query {
    account_name_type author; // Author or post, or sender (`from`) of private message
    std::string body; // Body of post or message
    std::string group; // For private messages. Or empty
};

struct sign_data {
    uint32_t head_block_number = 0;
    account_name_type witness;
};

struct login_data {
    account_name_type account;
    sign_data signed_data;
    signature_type signature;
};

struct decrypt_query : login_data {
    std::vector<fc::variant_object> entries;
    paid_subscription_id oid;
};

struct message_to_decrypt {
    std::string group;

    // Fields are need to find the message...
    account_name_type from;
    account_name_type to;
    uint64_t nonce = 0;
     //...or just pass the encrypted_message if you have
    std::vector<char> encrypted_message;

    message_to_decrypt() {}

    message_to_decrypt(const std::string& g, const std::vector<char> em)
        : group(g), encrypted_message(em) {}
};

struct decrypt_messages_query : login_data {
    std::vector<message_to_decrypt> entries;

    decrypt_messages_query() {}

    decrypt_messages_query(const login_data& ld) : login_data{ld} {}
};

}}} // golos::plugins::cryptor

FC_REFLECT(
    (golos::plugins::cryptor::encrypt_query),
    (author)(body)(group)
)

FC_REFLECT(
    (golos::plugins::cryptor::sign_data),
    (head_block_number)(witness)
)

FC_REFLECT(
    (golos::plugins::cryptor::login_data),
    (account)(signed_data)(signature)
)

FC_REFLECT_DERIVED(
    (golos::plugins::cryptor::decrypt_query), ((golos::plugins::cryptor::login_data)),
    (entries)(oid)
)

FC_REFLECT(
    (golos::plugins::cryptor::message_to_decrypt),
    (group)(from)(to)(nonce)(encrypted_message)
)

FC_REFLECT_DERIVED(
    (golos::plugins::cryptor::decrypt_messages_query), ((golos::plugins::cryptor::login_data)),
    (entries)
)
