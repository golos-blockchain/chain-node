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

            // Non-API internal usage only

            struct asset_info {
                uint16_t fee_pct = 0;
                std::string symbol_name;

                asset_info() {}

                asset_info(const asset_object& a) : fee_pct(a.fee_percent),
                    symbol_name(a.symbol_name()) {}

                asset_info(const std::string& s) : fee_pct(0), symbol_name(s) {}
            };

            using asset_map = std::map<asset_symbol_type, asset_info>;

            struct exchange_step {
                asset sell{0, asset::min_symbol()};
                asset receive{0, asset::min_symbol()};
                fc::optional<asset> remain;
                fc::optional<asset> fee;
                bool is_buy = false;
                fc::optional<uint16_t> fee_pct;

                price best_price{asset(0, STEEM_SYMBOL), asset(0, SBD_SYMBOL)};
                price limit_price{asset(0, STEEM_SYMBOL), asset(0, SBD_SYMBOL)};

                static exchange_step from_sell(asset s) {
                    exchange_step step;
                    step.sell = s;
                    step.is_buy = false;
                    return step;
                }

                static exchange_step from_receive(asset r) {
                    exchange_step step;
                    step.receive = r;
                    step.is_buy = true;
                    return step;
                }

                static exchange_step from(asset a, bool is_buy = false) {
                    if (is_buy) {
                        return from_receive(a);
                    } else {
                        return from_sell(a);
                    }
                }

                const asset& param() const {
                    return is_buy ? receive : sell;
                }

                asset& param() {
                    return is_buy ? receive : sell;
                }

                const asset& res() const {
                    return is_buy ? sell : receive;
                }

                asset& res() {
                    return is_buy ? sell : receive;
                }
            };

            struct ex_chain {
                void push_back(const exchange_step& step) {
                    steps.push_back(step);
                }

                exchange_step& operator[](size_t pos) {
                    return steps[pos];
                }

                const exchange_step& operator[](size_t pos) const {
                    return steps[pos];
                }

                size_t size() const {
                    return steps.size();
                }

                void reverse() {
                    std::reverse(steps.begin(), steps.end());
                    reversed = true;
                }

                bool is_buy() const {
                    if (steps.size()) {
                        return steps[0].is_buy;
                    }
                    return false;
                }

                const asset& res() const {
                    auto idx = reversed ? 0 : steps.size() - 1;
                    return steps[idx].res();
                }

                std::vector<exchange_step> steps;
                std::vector<std::string> syms;
                bool has_remain = false; // Not reflected - internal
                bool reversed = false; //
            };
        }
    }
} // golos::plugins::exchange

FC_REFLECT(
    (golos::plugins::exchange::exchange_step),
    (sell)(receive)(remain)(fee)(fee_pct)(best_price)(limit_price)
)

namespace fc {
    using namespace std;

    void to_variant(const golos::plugins::exchange::ex_chain &var, fc::variant &vo) {
        fc::mutable_variant_object res;
        res["res"] = var.res();

        fc::variant steps;
        to_variant(var.steps, steps);
        res["steps"] = steps;

        std::vector<std::string> syms;
        for (size_t i = 0; i < var.steps.size(); ++i) {
            const auto& s = var.steps[i];
            if (i == 0) {
                syms.push_back(s.param().symbol_name());
            }
            syms.push_back(s.res().symbol_name());
        }
        fc::variant syms_v;
        to_variant(syms, syms_v);
        res["syms"] = syms_v;

        vo = res;
    }
} // fc


/*FC_REFLECT(
    (golos::plugins::exchange::ex_chain),
    (steps)(syms)
)*/

