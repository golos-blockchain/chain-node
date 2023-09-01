#include <golos/chain/database.hpp>
#include <golos/chain/evaluator.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/nft_operations.hpp>

namespace golos { namespace chain {

const nft_collection_object& database::get_nft_collection(asset_symbol_type name) const {
    try {
        return get<nft_collection_object, by_name>(name);
    } catch(const std::out_of_range& e) {
        GOLOS_THROW_MISSING_OBJECT("nft_collection", fc::mutable_variant_object()("name", nft_name_to_string(name)));
    } FC_CAPTURE_AND_RETHROW((name))
}

const nft_collection_object* database::find_nft_collection(asset_symbol_type name) const {
    return find<nft_collection_object, by_name>(name);
}

void database::throw_if_exists_nft_collection(asset_symbol_type name) const {
    if (find_nft_collection(name)) {
        GOLOS_THROW_OBJECT_ALREADY_EXIST("nft_collection", fc::mutable_variant_object()("name", nft_name_to_string(name)));
    }
}

const nft_object& database::get_nft(asset_symbol_type name, const account_name_type& owner) const {
    try {
        return get<nft_object, by_asset_owner>(std::make_tuple(name, owner));
    } catch(const std::out_of_range& e) {
        GOLOS_THROW_MISSING_OBJECT("nft", fc::mutable_variant_object()("name", nft_name_to_string(name))("owner", owner));
    } FC_CAPTURE_AND_RETHROW((name)(owner))
}

const nft_object* database::find_nft(asset_symbol_type name, const account_name_type& owner) const {
    return find<nft_object, by_asset_owner>(std::make_tuple(name, owner));
}

const nft_object& database::get_nft(uint32_t token_id) const {
    try {
        return get<nft_object, by_token_id>(token_id);
    } catch(const std::out_of_range& e) {
        GOLOS_THROW_MISSING_OBJECT("nft", fc::mutable_variant_object()("token_id", token_id));
    } FC_CAPTURE_AND_RETHROW((token_id))
}

const nft_object* database::find_nft(uint32_t token_id) const {
    return find<nft_object, by_token_id>(token_id);
}

const nft_order_object& database::get_nft_order(uint32_t token_id) const {
    try {
        return get<nft_order_object, by_token_id>(token_id);
    } catch(const std::out_of_range& e) {
        GOLOS_THROW_MISSING_OBJECT("nft_order", fc::mutable_variant_object()("token_id", token_id));
    } FC_CAPTURE_AND_RETHROW((token_id))
}

const nft_order_object& database::get_nft_order(account_name_type owner, uint32_t order_id) const {
    try {
        return get<nft_order_object, by_owner_order_id>(std::make_tuple(owner, order_id));
    } catch(const std::out_of_range& e) {
        GOLOS_THROW_MISSING_OBJECT("nft_order", fc::mutable_variant_object()("owner", owner)("order_id", order_id));
    } FC_CAPTURE_AND_RETHROW((owner)(order_id))
}

const nft_order_object* database::find_nft_order(uint32_t token_id) const {
    return find<nft_order_object, by_token_id>(token_id);
}

const nft_order_object* database::find_nft_order(account_name_type owner, uint32_t order_id) const {
    return find<nft_order_object, by_owner_order_id>(std::make_tuple(owner, order_id));
}

void database::throw_if_exists_nft_order(uint32_t token_id) const {
    if (find_nft_order(token_id)) {
        GOLOS_THROW_OBJECT_ALREADY_EXIST("nft_order", fc::mutable_variant_object()("token_id", token_id));
    }
}

void database::throw_if_exists_nft_order(const account_name_type& owner, uint32_t order_id) const {
    if (find_nft_order(owner, order_id)) {
        GOLOS_THROW_OBJECT_ALREADY_EXIST("nft_order", fc::mutable_variant_object()("owner", owner)("order_id", order_id));
    }
}

}} // golos::chain
