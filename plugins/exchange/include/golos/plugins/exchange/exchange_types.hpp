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

                void add_fee(asset f) {
                    if (!f.amount.value) return;
                    if (!fee) fee = asset(0, f.symbol);
                    *(fee) += f;
                }

                void add_res(asset a, price p) {
                    auto add = a * p;
                    if ((add * p) < a) { // fixes test.2: case
                        add += asset(1, add.symbol);
                    }
                    res() += add; // TODO: using only in buy
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

                bool is_direct() const {
                    return steps.size() == 1;
                }

                void reverse() {
                    std::reverse(steps.begin(), steps.end());
                    reversed = !reversed;
                }

                bool is_buy() const {
                    if (steps.size()) {
                        return steps[0].is_buy;
                    }
                    return false;
                }

                const exchange_step& last() const {
                    if (steps.size()) {
                        auto idx = reversed ? 0 : steps.size() - 1;
                        return steps[idx];
                    }
                    return empty_step;
                }

                asset res() const {
                    if (steps.size()) {
                        auto main = last().res();
                        if (subchains.size())
                        {
                            main += subchains[0].res();
                        }
                        return main;
                    }
                    return empty;
                }

                const exchange_step& first() const {
                    if (steps.size()) {
                        auto idx = reversed ? steps.size() - 1 : 0;
                        return steps[idx];
                    }
                    return empty_step;
                }

                asset param() const {
                    if (steps.size()) {
                        auto main = first().param();
                        if (subchains.size())
                        {
                            main += subchains[0].param();
                        }
                        return main;
                    }
                    return empty;
                }

                price get_price() const {
                    return price{res(), param()};
                }

                bool has_sym(asset_symbol_type sym) const {
                    for (const auto& st : steps) {
                        if (st.param().symbol == sym) return true;
                    }
                    return false;
                }

                std::vector<exchange_step> steps;
                std::vector<std::string> syms;
                std::vector<ex_chain> subchains;
                bool has_remain = false; // Not reflected - internal
                bool reversed = false; //
                asset empty{0, asset::min_symbol()}; //
                exchange_step empty_step; //
            };

            const ex_chain* get_direct_chain(const std::vector<ex_chain>& chains) {
                auto itr = std::find_if(chains.begin(), chains.end(), [&](const auto& chain) {
                    return chain.size() == 1;
                });
                if (itr != chains.end()) {
                    return &(*itr);
                }
                return nullptr;
            }

            struct ex_stat {
                fc::time_point start;

                ex_stat() {
                    start = fc::time_point::now();
                }

                uint32_t msec() const {
                    auto now = fc::time_point::now();
                    return (now - start).count() / 1000;
                }
            };

            GOLOS_DECLARE_DERIVED_EXCEPTION(
                api_timeout, golos::golos_exception,
                6000000, "API timeout");
#define CHECK_API_TIMEOUT(TIMEOUT) \
    if (stat.msec() > TIMEOUT) throw api_timeout();

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

FC_REFLECT(
    (golos::plugins::exchange::exchange_step),
    (sell)(receive)(remain)(fee)(fee_pct)(best_price)(limit_price)
)

namespace fc {
    using namespace std;
    using golos::protocol::price;

    void to_variant(const golos::plugins::exchange::ex_chain &var, fc::variant &vo) {
        fc::mutable_variant_object res;
        res["res"] = var.res();
elog("tov1");
        fc::variant steps;
        to_variant(var.steps, steps);
        res["steps"] = steps;
elog("tov0");
        price best_price;
        price limit_price;

        std::vector<std::string> syms;
        try {
            for (size_t i = 0; i < var.steps.size(); ++i) {
                const auto& s = var.steps[i];
                if (i == 0) {
                    syms.push_back(s.sell.symbol_name());
                    best_price = s.best_price;
                    limit_price = s.limit_price;
                } else {
                    if (var.is_buy()) { // TODO: if not reversed,works wrong
                        best_price.quote = best_price.quote * s.best_price;
                        limit_price.quote = limit_price.quote * s.limit_price;
                    } else {
                        best_price.base = best_price.base * s.best_price;
                        limit_price.base = limit_price.base * s.limit_price;
                    }
                }
                syms.push_back(s.receive.symbol_name());
            }
        } catch (...) {
            FC_ASSERT(false, "Chain has wrong best_price, limit_price");
        }

        fc::variant syms_v;
        to_variant(syms, syms_v);
        res["syms"] = syms_v;

        fc::variant subchains;
        to_variant(var.subchains, subchains);
        res["subchains"] = subchains;

        res["best_price"] = best_price;
        res["limit_price"] = limit_price;

        res["buy"] = var.is_buy();

        vo = res;
    }
} // fc
