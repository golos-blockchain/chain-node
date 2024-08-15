#pragma once

#include <appbase/plugin.hpp>
#include <fc/optional.hpp>
#include <golos/chain/index.hpp>
#include <golos/chain/comment_object.hpp>

#ifndef CRYPTOR_SPACE_ID
#define CRYPTOR_SPACE_ID 17
#endif

namespace golos { namespace plugins { namespace cryptor {

using namespace golos::chain;

enum cryptor_object_types {
    crypto_buyer_object_type = (CRYPTOR_SPACE_ID << 8),
};

class crypto_buyer_object final: public object<crypto_buyer_object_type, crypto_buyer_object> {
public:
    crypto_buyer_object() = delete;

    template<typename Constructor, typename Allocator>
    crypto_buyer_object(Constructor&& c, allocator <Allocator> a) {
        c(*this);
    }

    id_type id;

    account_name_type donater;
    comment_extras_id_type comment;
    asset amount{0, STEEM_SYMBOL};
    bool buyed = false;
};

using crypto_buyer_id_type = object_id<crypto_buyer_object>;

struct by_comment_donater;
using crypto_buyer_index = multi_index_container<
    crypto_buyer_object,
    indexed_by<
        ordered_unique<
            tag<by_id>,
            member<crypto_buyer_object, crypto_buyer_id_type, &crypto_buyer_object::id>
        >,
        ordered_unique<
            tag<by_comment_donater>,
            composite_key<
                crypto_buyer_object,
                member<crypto_buyer_object, comment_extras_id_type, &crypto_buyer_object::comment>,
                member<crypto_buyer_object, account_name_type, &crypto_buyer_object::donater>
            >
        >
    >,
    allocator<crypto_buyer_object>>;

enum class cryptor_status : uint8_t {
    ok,
    err
};

struct encrypted_api_object {
    cryptor_status status = cryptor_status::ok;
    std::string error;

    std::string encrypted;
};

struct sub_options {
    asset cost;
    bool tip_cost = false;
};

struct decrypted_result {
    fc::optional<std::string> author;
    fc::optional<std::string> body;
    std::string permlink;   // Not reflected, internal
    hashlink_type hashlink; //
    fc::optional<std::string> err;
    fc::optional<sub_options> sub;
    fc::optional<asset> decrypt_fee;
};

struct decrypted_api_object {
    cryptor_status status = cryptor_status::ok;
    fc::optional<std::string> login_error;
    fc::optional<std::string> error;

    std::vector<decrypted_result> results;
};

} } } // golos::plugins::cryptor

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::cryptor::crypto_buyer_object,
    golos::plugins::cryptor::crypto_buyer_index
)

FC_REFLECT_ENUM(
    golos::plugins::cryptor::cryptor_status,
    (ok)(err)
)

FC_REFLECT((golos::plugins::cryptor::encrypted_api_object),
    (status)(error)(encrypted)
)

FC_REFLECT((golos::plugins::cryptor::sub_options),
    (cost)(tip_cost)
)

FC_REFLECT((golos::plugins::cryptor::decrypted_result),
    (author)(body)(err)(sub)(decrypt_fee)
)

FC_REFLECT((golos::plugins::cryptor::decrypted_api_object),
    (status)(login_error)(error)(results)
)
