#pragma once

#include <golos/plugins/exchange/exchange.hpp>
#include <golos/plugins/json_rpc/utility.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>
#include <golos/protocol/types.hpp>
#include <golos/protocol/asset.hpp>
#include <golos/protocol/steem_virtual_operations.hpp>

namespace golos {
    namespace plugins {
        namespace exchange {

            using namespace golos::chain;

            // API

            enum class exchange_direction : uint8_t {
                sell,
                buy
            };

            enum class exchange_remain_policy : uint8_t {
                normal,
                ignore
            };

            struct exchange_remain {
                exchange_remain_policy direct = exchange_remain_policy::normal;
                exchange_remain_policy multi = exchange_remain_policy::normal;
            };

            enum class exchange_excess_protect : uint8_t {
                none,
                fix_input
            };

            struct exchange_query;

            struct exchange_min_to_receive {
                asset direct;
                asset multi;
                uint16_t min_profit_pct = 0;

                void validate(const exchange_query& query) const;

                bool ignore_chain(bool is_buy, asset param, asset res, bool is_direct,
                    asset dir_receive = asset()) const;
            };

            enum class exchange_hybrid_strategy : uint8_t {
                none,
                discrete,
                spread
            };

            struct exchange_hybrid {
                exchange_hybrid_strategy strategy = exchange_hybrid_strategy::none;
                uint16_t discrete_step = GOLOS_MIN_DISCRETE_STEP;
                bool fix_sell = false;
            };

            struct exchange_query {
                asset amount;
                std::string symbol;
                asset_symbol_type sym2; // Internal
                exchange_direction direction = exchange_direction::sell;

                exchange_remain remain;
                exchange_excess_protect excess_protect = exchange_excess_protect::fix_input;
                exchange_min_to_receive min_to_receive;
                exchange_hybrid hybrid;
                bool logs = false;
                uint16_t pct = STEEMIT_100_PERCENT; // Internal
                bool pct_direct = false; //

                std::set<std::string> hidden_assets;

                void initialize_validate(database& _db);

                std::vector<exchange_query> discrete_split(uint16_t step);

                bool is_discrete() const;
                bool is_spread() const;
                bool will_fix_input() const;
            };

            struct exchange_path_query {
                std::string sell;
                std::string buy;

                std::set<std::string> select_syms;
                std::set<std::string> filter_syms;

                bool assets = false;

                asset_symbol_type sell_sym; // Internal
                asset_symbol_type buy_sym; // Internal

                bool is_bad_symbol(const std::string& sym) const;
                bool is_good_symbol(const std::string& sym) const;

                bool is_buy() const;

                void initialize_validate(database& _db);
            };
        }
    }
} // golos::plugins::exchange

FC_REFLECT_ENUM(
    golos::plugins::exchange::exchange_direction,
    (sell)(buy)
)

FC_REFLECT_ENUM(
    golos::plugins::exchange::exchange_remain_policy,
    (normal)(ignore)
)

FC_REFLECT(
    (golos::plugins::exchange::exchange_remain),
    (direct)(multi)
)

FC_REFLECT_ENUM(
    golos::plugins::exchange::exchange_excess_protect,
    (none)(fix_input)
)

FC_REFLECT(
    (golos::plugins::exchange::exchange_min_to_receive),
    (direct)(multi)(min_profit_pct)
)

FC_REFLECT_ENUM(
    golos::plugins::exchange::exchange_hybrid_strategy,
    (none)(discrete)(spread)
)

FC_REFLECT(
    (golos::plugins::exchange::exchange_hybrid),
    (strategy)(discrete_step)(fix_sell)
)

FC_REFLECT(
    (golos::plugins::exchange::exchange_query),
    (amount)(symbol)(direction)
    (remain)(excess_protect)(min_to_receive)(hybrid)(logs)
    (pct)
    (hidden_assets)
)

FC_REFLECT(
    (golos::plugins::exchange::exchange_path_query),
    (sell)(buy)(select_syms)(filter_syms)(assets)
)
