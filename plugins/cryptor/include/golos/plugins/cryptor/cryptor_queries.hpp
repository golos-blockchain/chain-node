#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/protocol/paid_subscription_operations.hpp>

using namespace golos::protocol;

namespace golos { namespace plugins { namespace cryptor {

struct encrypt_query {
    account_name_type author;
    std::string body;
};

struct sign_data {
    uint32_t head_block_number = 0;
    account_name_type witness;
};

struct decrypt_query {
    account_name_type account;
    sign_data signed_data;
    signature_type signature;

    std::vector<fc::variant_object> entries;
    paid_subscription_id oid;
};

}}} // golos::plugins::cryptor

FC_REFLECT(
    (golos::plugins::cryptor::encrypt_query),
    (author)(body)
)

FC_REFLECT(
    (golos::plugins::cryptor::sign_data),
    (head_block_number)(witness)
)

FC_REFLECT(
    (golos::plugins::cryptor::decrypt_query),
    (account)(signed_data)(signature)(entries)(oid)
)
