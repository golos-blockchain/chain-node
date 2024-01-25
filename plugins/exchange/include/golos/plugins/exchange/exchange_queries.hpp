#pragma once

#include <golos/plugins/json_rpc/utility.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <golos/chain/steem_objects.hpp>
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

            struct exchange_query {
                asset amount;
                std::string symbol;
                exchange_direction direction = exchange_direction::sell;

                exchange_remain_policy remain_policy = exchange_remain_policy::normal;
                asset min_to_receive;

                std::set<std::string> hidden_assets;
                std::set<asset_symbol_type> hidden_asset_syms; // Internal, not reflected
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
    (golos::plugins::exchange::exchange_query),
    (amount)(symbol)(direction)
    (remain_policy)(min_to_receive)
    (hidden_assets)
)
