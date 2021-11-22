#pragma once

#include <golos/chain/steem_objects.hpp>

namespace golos { namespace plugins { namespace database_api {

    using golos::chain::asset_object;
    using namespace golos::protocol;

    struct asset_api_object final {
        asset_api_object() = default;

        asset_api_object(const asset_object& p, const golos::chain::database& _db)
                : id(p.id), creator(p.creator), max_supply(p.max_supply), supply(p.supply),
                allow_fee(p.allow_fee), allow_override_transfer(p.allow_override_transfer),
                json_metadata(golos::chain::to_string(p.json_metadata)),
                created(p.created), modified(p.modified), marketed(p.marketed), fee_percent(p.fee_percent) {
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
    };

}}} // golos::plugins::database_api

FC_REFLECT(
    (golos::plugins::database_api::asset_api_object),
    (id)(creator)(max_supply)(supply)(can_issue)(precision)(allow_fee)(allow_override_transfer)
    (json_metadata)(created)(modified)(marketed)(symbols_whitelist)(fee_percent)
)
