#pragma once

#include <golos/chain/steem_objects.hpp>

namespace golos { namespace plugins { namespace database_api {

    using golos::chain::asset_object;
    using namespace golos::protocol;

    struct assets_query {
        bool system = false; // GOLOS, GBG
    };

    struct asset_api_object final {
        asset_api_object() = default;

        asset_api_object(const asset_object& p, const golos::chain::database& _db)
                : id(p.id), creator(p.creator), max_supply(p.max_supply), supply(p.supply),
                allow_fee(p.allow_fee), allow_override_transfer(p.allow_override_transfer),
                json_metadata(golos::chain::to_string(p.json_metadata)),
                created(p.created), modified(p.modified), marketed(p.marketed), fee_percent(p.fee_percent),
                market_depth(p.market_depth) {
            can_issue = max_supply - supply;
            precision = supply.decimals();
            for (const auto sym : p.symbols_whitelist) {
                if (sym == STEEM_SYMBOL) {
                    symbols_whitelist.insert("GOLOS");
                } else if (sym == SBD_SYMBOL) {
                    symbols_whitelist.insert("GBG");
                } else {
                    symbols_whitelist.insert(_db.get_asset(sym).symbol_name());
                }
            }
        }

        static asset_api_object golos(const golos::chain::database& _db) {
            asset_api_object o;
            o.max_supply = asset(INT64_MAX, STEEM_SYMBOL);
            const auto& props = _db.get_dynamic_global_properties();
            o.supply = props.current_supply;
            o.can_issue = o.max_supply - o.supply;
            o.precision = o.supply.decimals();
            o.fee_percent = 0;
            o.market_depth = props.golos_market_depth;
            return o;
        }

        static asset_api_object gbg(const golos::chain::database& _db) {
            asset_api_object o;
            o.max_supply = asset(INT64_MAX, SBD_SYMBOL);
            const auto& props = _db.get_dynamic_global_properties();
            o.supply = props.current_sbd_supply;
            o.can_issue = o.max_supply - o.supply;
            o.precision = o.supply.decimals();
            o.fee_percent = 0;
            o.market_depth = props.gbg_market_depth;
            return o;
        }

        asset_object::id_type id;

        account_name_type creator;
        asset max_supply;
        asset supply;
        asset can_issue;
        uint8_t precision = 0;
        bool allow_fee = false;
        bool allow_override_transfer = false;
        std::string json_metadata;
        time_point_sec created;
        time_point_sec modified;
        time_point_sec marketed;
        std::set<std::string> symbols_whitelist;
        uint16_t fee_percent;
        asset market_depth;
    };

}}} // golos::plugins::database_api

FC_REFLECT(
    (golos::plugins::database_api::assets_query),
    (system)
)

FC_REFLECT(
    (golos::plugins::database_api::asset_api_object),
    (id)(creator)(max_supply)(supply)(can_issue)(precision)(allow_fee)(allow_override_transfer)
    (json_metadata)(created)(modified)(marketed)(symbols_whitelist)(fee_percent)
    (market_depth)
)
