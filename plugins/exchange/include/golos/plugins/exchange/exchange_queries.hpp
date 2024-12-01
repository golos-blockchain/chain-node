#pragma once

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

                void validate(const exchange_query& query) const;

                bool ignore_chain(bool is_buy, asset param, asset res, bool is_direct) const {
                    auto thr = is_direct ? direct : multi;
                    if (thr.amount.value != 0 &&
                        (is_buy ? param : res) < thr) return true;
                    return false;
                }
            };

            struct exchange_query {
                asset amount;
                std::string symbol;
                asset_symbol_type sym2;
                exchange_direction direction = exchange_direction::sell;

                exchange_remain remain;
                exchange_excess_protect excess_protect = exchange_excess_protect::fix_input;
                fc::optional<exchange_min_to_receive> min_to_receive;

                std::set<std::string> hidden_assets;

                void initialize_validate(database& _db) {
                    GOLOS_CHECK_PARAM(amount, {
                        GOLOS_CHECK_VALUE_GT(amount.amount.value, 0);
                        if (amount.symbol != STEEM_SYMBOL && amount.symbol != SBD_SYMBOL) {
                            _db.with_weak_read_lock([&]() {
                                _db.get_asset(amount.symbol);
                            });
                        }
                    });

                    GOLOS_CHECK_PARAM(symbol, {
                        GOLOS_CHECK_VALUE(symbol != amount.symbol_name(),
                            "amount should have another symbol than query.symbol");

                        _db.with_weak_read_lock([&]() {
                            sym2 = _db.symbol_from_str(symbol);
                        });
                    });

                    GOLOS_CHECK_PARAM(min_to_receive, {
                        if (!!min_to_receive) {
                            min_to_receive->validate(*this);
                        }
                    });
                }
            };

            void exchange_min_to_receive::validate(const exchange_query& query) const {
                #define VALIDATE_AMOUNT(FIELD) \
                    if (FIELD.amount.value != 0) { \
                        GOLOS_CHECK_VALUE_GT(FIELD.amount.value, 0); \
                        if (query.direction == exchange_direction::sell) { \
                            GOLOS_CHECK_VALUE(FIELD.symbol == query.sym2, \
                                "min_to_receive should have same symbol as 'symbol' field"); \
                        } else { \
                            GOLOS_CHECK_VALUE(FIELD.symbol == query.amount.symbol, \
                                "min_to_receive should have same symbol as 'amount' symbol"); \
                        } \
                    }
                VALIDATE_AMOUNT(direct);
                VALIDATE_AMOUNT(multi);
                #undef VALIDATE_AMOUNT
            }
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
    (direct)(multi)
)

FC_REFLECT(
    (golos::plugins::exchange::exchange_query),
    (amount)(symbol)(direction)
    (remain)(excess_protect)(min_to_receive)
    (hidden_assets)
)
