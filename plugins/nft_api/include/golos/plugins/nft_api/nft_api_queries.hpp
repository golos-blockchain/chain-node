#pragma once

#include <golos/chain/database.hpp>
#include <appbase/plugin.hpp>
#include <golos/protocol/nft_operations.hpp>
#include <golos/plugins/nft_api/nft_api_objects.hpp>

using namespace golos::protocol;

namespace golos { namespace plugins { namespace nft_api {

enum class nft_collection_sort : uint8_t {
    by_name,
    by_created,
    by_last_price,
    by_token_count,
    by_market_depth,
    by_market_asks,
    by_market_volume
};

struct nft_collections_query {
    account_name_type creator;

    std::string start_name;
    uint32_t limit = 20;

    // select 1 or few specific collections
    std::set<std::string> select_names;

    std::set<account_name_type> filter_creators;
    std::set<std::string> filter_names;

    nft_collection_sort sort = nft_collection_sort::by_name;
    bool reverse_sort = false;
};

enum class nft_tokens_sort : uint8_t {
    by_name,
    by_issued,
    by_last_update,
    by_last_price
};

enum class nft_token_state : uint8_t {
    selling_only,
    not_selling_only,
    any_not_burnt,
    burnt_only,
    any
};

enum class nft_token_illformed : uint8_t {
    sort_down,
    ignore,
    nothing
};

enum class nft_token_selling : uint8_t {
    nothing,
    sort_up,
    sort_up_by_price,
};

enum class nft_token_sorting_priority : uint8_t {
    illformed,
    selling,
};

struct nft_tokens_query {
    account_name_type owner;
    std::set<std::string> select_collections;
    uint32_t collection_limit = UINT32_MAX;

    bool collections = true;
    bool orders = true;

    uint32_t start_token_id = 0;
    uint32_t limit = 20;

    std::set<uint32_t> select_token_ids;

    std::set<account_name_type> filter_creators;
    std::set<std::string> filter_names;
    std::set<uint32_t> filter_token_ids;
    nft_token_state state = nft_token_state::any_not_burnt;

    bool is_good(const nft_api_object& obj) const {
        if (filter_token_ids.count(obj.token_id)) return false;
        if (filter_names.count(obj.name)
            || filter_creators.count(obj.creator)) return false;

        if (illformed == nft_token_illformed::ignore && obj.illformed) return false;

        if (state == nft_token_state::selling_only && !obj.selling) return false;
        if (state == nft_token_state::not_selling_only && obj.selling) return false;

        if (state != nft_token_state::any && state != nft_token_state::burnt_only && obj.burnt) return false;
        if (state == nft_token_state::burnt_only && !obj.burnt) return false;

        if (select_collections.size() && !select_collections.count(obj.name)) return false;

        return true;
    };

    nft_tokens_sort sort = nft_tokens_sort::by_name;
    bool reverse_sort = false;

    nft_token_illformed illformed = nft_token_illformed::sort_down;

    nft_token_selling selling_sorting = nft_token_selling::nothing;

    bool sort_selling() const {
        return selling_sorting != nft_token_selling::nothing;
    }

    nft_token_sorting_priority sorting_priority = nft_token_sorting_priority::selling;
};

enum nft_order_type : uint8_t {
    buying,
    selling,
    both
};

enum class nft_orders_sort : uint8_t {
    by_created,
    by_price,
};

struct nft_orders_query {
    account_name_type owner;
    std::set<std::string> select_collections;
    uint32_t collection_limit = UINT32_MAX;
    std::set<uint32_t> select_token_ids;

    bool tokens = true;

    uint32_t start_order_id = 0;
    uint32_t limit = 20;

    std::set<account_name_type> filter_creators;
    std::set<std::string> filter_names;
    std::set<account_name_type> filter_owners;
    std::set<uint32_t> filter_token_ids;
    std::set<uint32_t> filter_order_ids;

    bool is_good(const nft_order_api_object& obj) const {
        if (filter_token_ids.count(obj.token_id)) return false;
        if (filter_order_ids.count(obj.order_id)) return false;
        if (filter_owners.count(obj.owner)) return false;
        if (filter_names.count(obj.name)
            || filter_creators.count(obj.creator)) return false;
        if (type == nft_order_type::buying && obj.selling) return false;
        if (type == nft_order_type::selling && !obj.selling) return false;

        if (select_collections.size() && !select_collections.count(obj.name)) return false;
        if (select_token_ids.size() && !select_token_ids.count(obj.token_id)) return false;

        return true;
    };

    nft_order_type type = nft_order_type::both;

    nft_orders_sort sort = nft_orders_sort::by_created;
    bool reverse_sort = false;
};

}}}

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_collection_sort,
    (by_name)(by_created)(by_last_price)(by_token_count)
    (by_market_depth)(by_market_asks)(by_market_volume)
)

FC_REFLECT(
    (golos::plugins::nft_api::nft_collections_query),
    (creator)(start_name)(limit)(select_names)
    (filter_creators)(filter_names)
    (sort)(reverse_sort)
)

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_tokens_sort,
    (by_name)(by_issued)(by_last_update)(by_last_price)
)

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_token_state,
    (selling_only)(not_selling_only)(any_not_burnt)(burnt_only)(any)
)

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_token_illformed,
    (sort_down)(ignore)(nothing)
)

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_token_selling,
    (nothing)(sort_up)(sort_up_by_price)
)

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_token_sorting_priority,
    (illformed)(selling)
)

FC_REFLECT(
    (golos::plugins::nft_api::nft_tokens_query),
    (owner)(select_collections)(collection_limit)
    (collections)(orders)(start_token_id)(limit)(select_token_ids)
    (filter_creators)(filter_names)(filter_token_ids)(state)
    (sort)(reverse_sort)(illformed)(selling_sorting)(sorting_priority)
)

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_order_type,
    (buying)(selling)(both)
)

FC_REFLECT_ENUM(
    golos::plugins::nft_api::nft_orders_sort,
    (by_created)(by_price)
)

FC_REFLECT(
    (golos::plugins::nft_api::nft_orders_query),
    (owner)(select_collections)(collection_limit)(select_token_ids)
    (tokens)(start_order_id)(limit)
    (filter_creators)(filter_names)(filter_owners)(filter_token_ids)(filter_order_ids)
    (type)(sort)(reverse_sort)
)
