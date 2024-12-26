#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/exchange/exchange_queries.hpp>
#include <golos/plugins/exchange/exchange_types.hpp>
#include <golos/plugins/exchange/exchange.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/donate_targets.hpp>
#include <golos/chain/operation_notification.hpp>

namespace golos { namespace plugins { namespace exchange {

class exchange::exchange_impl final {
public:
    exchange_impl(uint16_t hds = 0)
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()),
            hybrid_discrete_step(hds) {
    }

    ~exchange_impl() {
    }

    asset_info get_asset(asset_map& assets, asset_symbol_type symbol) const;

    asset subtract_fee(asset& a, uint16_t fee_pct) const;

    template<typename OrderIndex>
    void exchange_go_chain(ex_chain chain, exchange_step add_me,
        std::vector<ex_chain>& chains, asset_symbol_type sym2,
        const exchange_query& query, const OrderIndex& idx, std::function<void(const limit_order_object&)> add_to_cache, asset_map& assets, const ex_stat& stat) const;
    fc::mutable_variant_object get_exchange(exchange_query query) const;

    ex_chain fix_receive(const ex_chain& chain, const ex_stat& stat) const;

    database& _db;

    uint16_t hybrid_discrete_step = 0;
};

exchange::exchange() = default;

exchange::~exchange() = default;

const std::string& exchange::name() {
    static std::string name = "exchange";
    return name;
}

void exchange::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
}

void exchange::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing exchange plugin");

    my = std::make_unique<exchange::exchange_impl>(this->hybrid_discrete_step);

    JSON_RPC_REGISTER_API(name())
} 

void exchange::plugin_startup() {
    ilog("Starting up exchange plugin");
}

void exchange::plugin_shutdown() {
    ilog("Shutting down exchange plugin");
}

asset_info exchange::exchange_impl::get_asset(asset_map& assets, asset_symbol_type symbol) const {
    auto a_itr = assets.find(symbol);
    if (a_itr != assets.end()) {
        return a_itr->second;
    } else {
        if (symbol == STEEM_SYMBOL) {
            assets[symbol] = asset_info("GOLOS");
        } else if (symbol == SBD_SYMBOL) {
            assets[symbol] = asset_info("GBG");
        } else {
            const auto& a = _db.get_asset(symbol);                           
            assets[symbol] = asset_info(a);
        }
        return assets[symbol];
    }
}

asset exchange::exchange_impl::subtract_fee(asset& a, uint16_t fee_pct) const {
    auto fee = asset((fc::uint128_t(a.amount.value) * fee_pct / STEEMIT_100_PERCENT).to_uint64(),
        a.symbol);
    // for HF 25 fix
    if (fee_pct && fee.amount.value == 0 && a.amount.value > 0) {
        fee.amount.value = 1;
    }
    a -= fee;
    return fee;
}

template<typename OrderIndex>
void exchange::exchange_impl::exchange_go_chain(
    ex_chain chain, exchange_step add_me,
    std::vector<ex_chain>& chains, asset_symbol_type sym2,
    const exchange_query& query, const OrderIndex& idx, std::function<void(const limit_order_object&)> add_to_cache, asset_map& assets, const ex_stat& stat
) const {
    if (stat.msec() > SEARCH_TIMEOUT) throw ex_timeout_exception();

    if (add_me.param().symbol == sym2) {
        if (query.pct == STEEMIT_100_PERCENT && !!query.min_to_receive
                && query.min_to_receive->ignore_chain(add_me.is_buy, chain.param(), chain.res(), chain.size() == 1)) {
            return;
        }
        if (add_me.is_buy) {
            chain.reverse();
        }
        if (chain.size() == 1) {
            if (chain.has_remain && query.remain.direct == exchange_remain_policy::ignore) return;
        } else {
            if (chain.has_remain && query.remain.multi == exchange_remain_policy::ignore) return;
        }
        chains.push_back(chain);
        return;
    }

    chain.push_back(add_me);

    auto& step = chain[chain.size() - 1];

    asset par = step.param();
    auto receive0 = par;

    auto make_remain = [&]() {
        if (par.amount.value) {
            step.remain = par;
            chain.has_remain = true;
        }
    };

    if (add_me.is_buy) {
        const auto& a = get_asset(assets, par.symbol);
        if (a.fee_pct) {
            step.fee_pct = a.fee_pct;
        } else if (!!step.fee_pct) {
            step.fee_pct.reset();
        }
    }

    auto prc = price::min(par.symbol, 0);
    if (add_me.is_buy) {
        prc = price::max(par.symbol, asset::max_symbol());
    }

    auto itr = idx.lower_bound(prc);
    while (itr != idx.end()) {
        if (stat.msec() > SEARCH_TIMEOUT) throw ex_timeout_exception();

        add_to_cache(*itr);

        auto itr_prc = add_me.is_buy ? itr->sell_price : itr->buy_price();
        if (itr_prc.base.symbol != par.symbol) {
            break;
        }

        auto quote_symbol = itr_prc.quote.symbol;

        if (chain.has_sym(quote_symbol)) {
            ++itr;
            continue;
        }

        auto& res = step.res();

        if (res.symbol != quote_symbol) {
            const auto& a = get_asset(assets, quote_symbol);

            if (query.hidden_assets.count(a.symbol_name)) {
                ++itr;
                continue;
            }

            if (res.amount.value || res.symbol == sym2) {
                make_remain();
                exchange_go_chain(chain, exchange_step::from(res, add_me.is_buy),
                    chains, sym2, query, idx, add_to_cache, assets, stat);
                chain.has_remain = false;
            }
            res.amount.value = 0;
            res.symbol = quote_symbol;
            if (step.remain) step.remain.reset();
            if (add_me.is_buy) {
                par = receive0;
                step.receive.amount = 0;
            } else {
                par = step.param();
                if (step.fee) step.fee.reset();

                if (a.fee_pct) {
                    step.fee_pct = a.fee_pct;
                } else if (!!step.fee_pct) {
                    step.fee_pct.reset();
                }
            }
            step.best_price = ~itr_prc;
        } else if (par.amount.value == 0) {
            ++itr;
            continue;
        }

        step.limit_price = ~itr_prc;

        auto fee_pct = !!step.fee_pct ? *(step.fee_pct) : 0;

        if (add_me.is_buy) {
            auto ord_orig = itr->amount_for_sale();
            auto ord = ord_orig;

            auto fee = asset((fc::uint128_t(ord.amount.value) * fee_pct / STEEMIT_100_PERCENT).to_uint64(),
                ord.symbol);
            if (fee_pct && fee.amount.value == 0) { // for HF 25 fix
                fee.amount.value = 1;
            }
            ord -= fee;

            if (par >= ord) {
                step.receive += ord;
                step.add_fee(fee);
                par -= ord;
                step.add_res(ord_orig, itr->sell_price);
            } else {
                step.receive += par;

                auto am_pct = STEEMIT_100_PERCENT - fee_pct;
                auto am = par;
                if (am_pct) {
                    am = asset((fc::uint128_t(am.amount.value) * STEEMIT_100_PERCENT / am_pct).to_uint64(),
                        am.symbol);
                    if (fee_pct && am == par) { // for HF 25 fix
                        am += asset(1, am.symbol);
                    }
                } else {
                    ++itr;
                    continue;
                }

                step.add_fee(am - par);

                par -= par;

                step.add_res(am, itr->sell_price);
            }
        } else {
            auto o_par = std::min(par, itr->amount_to_receive());
            auto r = o_par * itr->sell_price;
            par -= o_par;

            if (fee_pct) {
                auto fee = subtract_fee(r, fee_pct);
                step.add_fee(fee);
            }
            res += r;
        }

        ++itr;
    }

    if (step.res().amount.value || step.res().symbol == sym2) {
        make_remain();
        exchange_go_chain(chain, exchange_step::from(step.res(), add_me.is_buy),
            chains, sym2, query, idx, add_to_cache, assets, stat);
        chain.has_remain = false;
    }
}

ex_chain exchange::exchange_impl::fix_receive(const ex_chain& chain, const ex_stat& stat) const {
    ex_chain result;
    result.reversed = true;

    const auto& idx = _db.get_index<limit_order_index, by_buy_price>();

    asset par(0, asset::min_symbol());

    bool rev = chain.reversed;
    int64_t size = chain.size();
    int64_t first = rev ? 0 : size - 1;
    int64_t i = first;
    int64_t last = rev ? size - 1 : 0;
    while (rev ? (i <= last) : (i >= last)) {
        const auto& st = chain[i];

        if (i == first) {
            par = st.sell;
        }

        auto step = exchange_step::from_sell(par);
        step.receive = asset(0, st.receive.symbol);
        if (!!st.fee_pct) step.fee_pct = st.fee_pct;

        auto prc = price::min(par.symbol, st.receive.symbol);
        auto itr = idx.lower_bound(prc);
        auto end = idx.upper_bound(prc.max());
        while (itr != end && par.amount.value > 0) {
            if (stat.msec() > SEARCH_TIMEOUT) throw ex_timeout_exception();

            if (!step.best_price.base.amount.value) {
                step.best_price = ~itr->sell_price;
            }
            step.limit_price = ~itr->sell_price;

            auto sell = std::min(par, itr->amount_to_receive());

            auto r = sell * itr->sell_price;
            par -= sell;

            if (!!step.fee_pct) {
                auto fee = subtract_fee(r, *step.fee_pct);
                step.add_fee(fee);
            }
            step.receive += r;

            ++itr;
        }

        if (!step.receive.amount.value && i != last) {
            result.steps.clear();
            return result;
        }

        step.is_buy = true;
        result.push_back(step);

        par = step.receive;

        rev ? i++ : i--;
    }

    return result;
}

fc::mutable_variant_object exchange::exchange_impl::get_exchange(exchange_query query) const {
    fc::mutable_variant_object result;

    query.initialize_validate(_db);

    std::map<uint16_t, std::vector<ex_chain>> chain_map;

    bool is_buy = query.direction == exchange_direction::buy;

    ex_stat stat;

    ex_chain chain;
    order_cache cache;
    asset_map assets;

    std::vector<exchange_query> queries;
    if (!!query.hybrid && query.hybrid->strategy == exchange_hybrid_strategy::discrete) {
        queries = query.discrete_split(hybrid_discrete_step);
    } else {
        queries = {query};
    }

    for (size_t i = 0; i < queries.size(); ++i) {
        const auto& curr = queries[i];
        auto step = exchange_step::from(curr.amount, is_buy);
        bool from_cache = i > 0;

        std::vector<ex_chain> chains;

        try {
            auto add_to_cache = [&](const limit_order_object& loo) {
                if (from_cache) return;
                auto& id = cache.get<by_id>();
                if (id.find(loo.id) != id.end()) return;
                id.emplace(loo);
            };

            auto use_indexes = [&](const auto& idx, const auto& cache_idx) {
                if (from_cache) {
                    exchange_go_chain(
                        chain, step,
                        chains, curr.sym2, curr, cache_idx, add_to_cache, assets, stat);
                } else {
                    _db.with_weak_read_lock([&]() {
                        exchange_go_chain(
                            chain, step,
                            chains, curr.sym2, curr, idx, add_to_cache, assets, stat);
                    });
                }
            };

            if (is_buy) {
                const auto& idx = _db.get_index<limit_order_index, by_price>();
                const auto& cache_idx = cache.get<by_price>();

                use_indexes(idx, cache_idx);
            } else {
                const auto& idx = _db.get_index<limit_order_index, by_buy_price>();
                const auto& cache_idx = cache.get<by_buy_price>();

                use_indexes(idx, cache_idx);
            }
        } catch (const ex_timeout_exception& ex) {
            result["error"] = "timeout";
            elog("get_exchange timeout - " + fc::json::to_string(curr));
            return result;
        }

        chain_map[curr.pct] = chains;
    }

    std::vector<ex_chain> chains;
    for (auto itr = chain_map.rbegin(); itr != chain_map.rend(); ++itr) {
        auto pct = itr->first;
        const auto& vec = itr->second;
        if (pct == STEEMIT_100_PERCENT) {
            std::copy(vec.begin(), vec.end(), std::back_inserter(chains));
        } else {
            auto dir_pct = STEEMIT_100_PERCENT - pct;
            auto dirs = chain_map.find(dir_pct);
            const ex_chain* direct = nullptr;
            if (dirs != chain_map.end()) {
                direct = get_direct_chain(dirs->second);
            }
            if (!direct) {
                continue;
            }
            for (auto chain : vec) {
                if (chain.size() == 1) {
                    continue;
                }
                chain.subchains.push_back(*direct);
                if (!!query.min_to_receive
                        && query.min_to_receive->ignore_chain(chain.is_buy(), chain.param(), chain.res(), false)) {
                    continue;
                }
                chains.push_back(chain);
            }
        }
    }

    if (is_buy && query.excess_protect == exchange_excess_protect::fix_input) {
        std::vector<ex_chain> new_chains;

        _db.with_weak_read_lock([&]() {
            for (const auto& c : chains) {
                auto c2 = fix_receive(c, stat);
                if (c2.steps.size()) {
                    if (!!query.min_to_receive
                            && query.min_to_receive->ignore_chain(c2.is_buy(), c2.param(), c2.res(), c2.size() == 1)) {
                        continue;
                    }
                    new_chains.push_back(c2);
                }
            }
        });

        chains = new_chains;
    }

    std::sort(chains.begin(), chains.end(), [&](ex_chain a, ex_chain b) {
        if (is_buy) return a.get_price() < b.get_price();
        return a.get_price() > b.get_price();
    });

    result["direct"] = false;
    for (const auto& chain : chains) {
        if (chain.size() == 1) {
            result["direct"] = chain;
            break;
        }
    }

    if (chains.size()) {
        result["best"] = chains[0];
    } else {
        result["best"] = false;
    }

    result["all_chains"] = chains;

    result["_msec"] = stat.msec();

    return result;
}

DEFINE_API(exchange, get_exchange) {
    PLUGIN_API_VALIDATE_ARGS(
        (exchange_query, query)
    );

    fc::mutable_variant_object result;
    result = my->get_exchange(query);
    return result;
}

} } } // golos::plugins::exchange
