#pragma once

#include <golos/chain/steem_object_types.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/protocol/types.hpp>
#include <golos/protocol/asset.hpp>

// Non-API internal usage only

namespace golos {
    namespace plugins {
        namespace exchange {

            using namespace golos::chain;

            struct asset_info {
                uint16_t fee_pct = 0;
                std::string symbol_name;

                asset_info() {}

                asset_info(const asset_object& a) : fee_pct(a.fee_percent),
                    symbol_name(a.symbol_name()) {}

                asset_info(const std::string& s) : fee_pct(0), symbol_name(s) {}
            };

            using asset_map = std::map<asset_symbol_type, asset_info>;

            using order_cache = multi_index_container<
                limit_order_object,
                indexed_by<
                    ordered_unique<tag<by_id>,
                        member<limit_order_object, limit_order_id_type, &limit_order_object::id>
                    >,
                    ordered_unique<tag<by_price>, composite_key<limit_order_object,
                        member<limit_order_object, price, &limit_order_object::sell_price>,
                        member<limit_order_object, limit_order_id_type, &limit_order_object::id>
                    >, composite_key_compare<
                        std::greater<price>,
                        std::less<limit_order_id_type>
                    >>,
                    ordered_unique<tag<by_buy_price>, composite_key<limit_order_object,
                        const_mem_fun<limit_order_object, price, &limit_order_object::buy_price>,
                        member<limit_order_object, limit_order_id_type, &limit_order_object::id>
                    >, composite_key_compare<
                        std::less<price>,
                        std::less<limit_order_id_type>
                    >>
                >
            >;
        }
    }
} // golos::plugins::exchange
