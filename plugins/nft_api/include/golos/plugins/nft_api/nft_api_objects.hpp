#pragma once

#include <golos/chain/database.hpp>
#include <golos/chain/nft_objects.hpp>
#include <golos/protocol/types.hpp>
#include <fc/optional.hpp>
#include <fc/io/json.hpp>

namespace golos { namespace plugins { namespace nft_api {

using golos::chain::database;
using golos::chain::nft_collection_object;
using golos::chain::nft_object;
using golos::chain::nft_order_object;
using golos::chain::nft_order_index;
using golos::chain::nft_bet_object;
using golos::chain::by_token_id;
using golos::chain::to_string;
using protocol::asset;
using protocol::asset_symbol_type;
using protocol::account_name_type;
using protocol::nft_name_from_string;

struct nft_collection_api_object {
    nft_collection_api_object(const nft_collection_object& nco) : id(nco.id),
        creator(nco.creator), name(asset(0, nco.name).symbol_name()), json_metadata(to_string(nco.json_metadata)),
        created(nco.created), token_count(nco.token_count), max_token_count(nco.max_token_count), last_token_id(nco.last_token_id),
        last_buy_price(nco.last_buy_price), buy_order_count(nco.buy_order_count), sell_order_count(nco.sell_order_count),
        market_depth(nco.market_depth), market_asks(nco.market_asks), market_volume(nco.market_volume) {
    }

    nft_collection_api_object() {}

    nft_collection_object::id_type id;

    account_name_type creator;
    std::string name;

    std::string json_metadata;
    time_point_sec created;
    uint32_t token_count = 0;
    uint32_t max_token_count = 0;
    uint32_t last_token_id = 0;
    asset last_buy_price;
    uint32_t buy_order_count = 0;
    uint32_t sell_order_count = 0;
    double market_depth = 0;
    double market_asks = 0;
    double market_volume = 0;
};

struct nft_api_object {
    nft_api_object(const nft_object& no) : id(no.id),
        creator(no.creator), name(asset(0, no.name).symbol_name()), owner(no.owner), token_id(no.token_id), burnt(no.burnt),
        issue_cost(no.issue_cost), last_buy_price(no.last_buy_price), json_metadata(to_string(no.json_metadata)), illformed(false),
        issued(no.issued), last_update(no.last_update), selling(no.selling),
        auction_min_price(no.auction_min_price), auction_expiration(no.auction_expiration) {
    }

    nft_api_object() {}

    bool check_illformed() {
        illformed = true;
        if (json_metadata.size()) {
            auto jobj = fc::json::from_string(json_metadata);
            if (jobj.is_object()) {
                auto obj = jobj.get_object();
                auto extract_string = [&](const std::string& key) {
                    auto itr = obj.find(key);
                    if (itr != obj.end() && itr->value().is_string()) {
                        return itr->value().as_string();
                    }
                    return std::string();
                };
                illformed = extract_string("title").size() == 0 ||
                    extract_string("image").size() == 0;
            }
        }
        return illformed;
    }

    nft_object::id_type id;

    account_name_type creator;
    std::string name;

    account_name_type owner;
    uint32_t token_id = 0;
    bool burnt = false;

    asset issue_cost;
    asset last_buy_price;
    std::string json_metadata;
    bool illformed = false;
    time_point_sec issued;
    time_point_sec last_update;
    bool selling = false;

    asset auction_min_price{0, STEEM_SYMBOL};
    time_point_sec auction_expiration;

    double price_real() const {
        return last_buy_price.to_real();
    }
};

struct nft_order_api_object {
    nft_order_api_object(const nft_order_object& noo) : id(noo.id),
        creator(noo.creator), name(asset(0, noo.name).symbol_name()), token_id(noo.token_id),
        owner(noo.owner), order_id(noo.order_id), price(noo.price), selling(noo.selling), holds(noo.holds),
        created(noo.created) {
    }

    nft_order_api_object() {}

    nft_order_object::id_type id;

    account_name_type creator;
    std::string name;
    uint32_t token_id = 0;

    account_name_type owner;
    uint32_t order_id = 0;
    asset price;
    bool selling = false;
    bool holds = false;

    time_point_sec created;

    fc::optional<nft_api_object> token;

    double price_real() const {
        return price.to_real();
    }
};

struct nft_extended_api_object : nft_api_object {
    nft_extended_api_object(const nft_api_object& nao) : nft_api_object(nao) {}

    nft_extended_api_object(const nft_api_object& nao, const database& _db, bool collections = true, bool orders = true)
            : nft_api_object(nao) {
        fill_extensions(_db, collections, orders);
    }

    nft_extended_api_object() {}

    void fill_collection(const database& _db) {
        collection = _db.get_nft_collection(nft_name_from_string(name));
    }

    void fill_order(const database& _db) {
        const auto& idx = _db.get_index<nft_order_index, by_token_id>();
        auto itr = idx.lower_bound(token_id);
        while (itr != idx.end() && itr->token_id == token_id) {
            if (itr->selling) {
                order = nft_order_api_object(*itr);
                return;
            }
            ++itr;
        }
    }

    void fill_extensions(const database& _db, bool collections = true, bool orders = true) {
        if (!name.size() && !token_id) {
            return;
        }
        if (collections && !collection) {
            fill_collection(_db);
        }
        if (orders && !order) {
            fill_order(_db);
        }
    }

    double current_price_real() const {
        if (!!order) {
            return order->price_real();
        }
        return 0;
    }

    fc::optional<nft_collection_api_object> collection;
    fc::optional<nft_order_api_object> order;
};

struct nft_bet_api_object {
    nft_bet_api_object(const nft_bet_object& nbo, const database& _db, bool fill_token = false) : id(nbo.id),
        creator(nbo.creator), name(asset(0, nbo.name).symbol_name()), token_id(nbo.token_id),
        owner(nbo.owner), price(nbo.price),
        created(nbo.created) {
        if (fill_token) {
            const auto& no = _db.get<nft_object, by_token_id>(token_id);
            token = no;
        }
    }

    nft_bet_api_object() {}

    nft_bet_object::id_type id;

    account_name_type creator;
    std::string name;
    uint32_t token_id = 0;

    account_name_type owner;
    asset price;

    time_point_sec created;

    fc::optional<nft_api_object> token;
};

} } } // golos::plugins::nft_api

FC_REFLECT((golos::plugins::nft_api::nft_collection_api_object),
    (id)(creator)(name)(json_metadata)(created)(token_count)(max_token_count)(last_token_id)
    (last_buy_price)(buy_order_count)(sell_order_count)
    (market_depth)(market_asks)(market_volume)
)

FC_REFLECT((golos::plugins::nft_api::nft_api_object),
    (id)(creator)(name)(owner)(token_id)(burnt)(issue_cost)(last_buy_price)(json_metadata)(illformed)
    (issued)(last_update)(selling)(auction_min_price)(auction_expiration)
)

FC_REFLECT((golos::plugins::nft_api::nft_order_api_object),
    (id)(creator)(name)(token_id)(owner)(order_id)(price)(selling)(holds)(created)(token)
)

FC_REFLECT_DERIVED((golos::plugins::nft_api::nft_extended_api_object),
    ((golos::plugins::nft_api::nft_api_object)),
    (collection)(order)
)

FC_REFLECT((golos::plugins::nft_api::nft_bet_api_object),
    (id)(creator)(name)(token_id)(owner)(price)(created)(token)
)
