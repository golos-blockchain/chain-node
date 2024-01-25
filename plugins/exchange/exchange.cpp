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
    exchange_impl()
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    ~exchange_impl() {
    }

    template<typename OrderIndex>
    void exchange_go_chain(ex_chain chain, exchange_step add_me,
        std::vector<ex_chain>& chains, asset_symbol_type sym2,
        exchange_query& query, asset_map& assets) const;
    fc::mutable_variant_object get_exchange(exchange_query query) const;

    database& _db;
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

    my = std::make_unique<exchange::exchange_impl>();

    JSON_RPC_REGISTER_API(name())
} 

void exchange::plugin_startup() {
    ilog("Starting up exchange plugin");
}

void exchange::plugin_shutdown() {
    ilog("Shutting down exchange plugin");
}

template<typename OrderIndex>
void exchange::exchange_impl::exchange_go_chain(
    ex_chain chain, exchange_step add_me,
    std::vector<ex_chain>& chains, asset_symbol_type sym2,
    exchange_query& query, asset_map& assets
) const {
    if (add_me.param().symbol == sym2) {
        if (add_me.is_buy) {
            chain.reverse();
        } else if (query.min_to_receive.amount.value > 0 &&
            chain.res() < query.min_to_receive) {
            return;
        }
        if (query.remain_policy != exchange_remain_policy::ignore
            || !chain.has_remain) {
            chains.push_back(chain);
        }
        return;
    }

    chain.push_back(add_me);

    auto& step = chain[chain.size() - 1];

    auto get_asset = [&](asset_symbol_type symbol) {
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
    };

    asset par = step.param();

    auto make_remain_fee = [&]() {
        if (par.amount.value) {
            step.remain = par;
            chain.has_remain = true;
        }
        if (!!step.fee_pct) {
            auto fee_pct = *(step.fee_pct);
            if (!add_me.is_buy) {
                auto fee = asset((fc::uint128_t(step.receive.amount.value) * fee_pct / STEEMIT_100_PERCENT).to_uint64(),
                    step.receive.symbol);
                step.receive -= fee;
                step.fee = fee;
            }
        }
    };

    auto chain_has = [&](const ex_chain& c, asset_symbol_type sym) {
        bool has = false;
        for (size_t i = 0; i < c.size(); ++i) {
            const auto& st = c[i];
            if (st.param().symbol == sym) {
                has = true;
            }
            if (has) break;
        }
        return has;
    };


    if (add_me.is_buy) {
        const auto& a = get_asset(par.symbol);
        if (a.fee_pct) {
            step.fee_pct = a.fee_pct;
        } else if (!!step.fee_pct) {
            step.fee_pct.reset();
        }
    }

    const auto& idx = _db.get_index<limit_order_index, OrderIndex>();

    auto prc = price::min(par.symbol, 0);
    if (add_me.is_buy) {
        prc = price::max(par.symbol, asset::max_symbol());
    }

    auto itr = idx.lower_bound(prc);
    while (itr != idx.end()) {
        auto itr_prc = add_me.is_buy ? itr->sell_price : itr->buy_price();
        if (itr_prc.base.symbol != par.symbol) {
            break;
        }

        auto quote_symbol = itr_prc.quote.symbol;

        if (chain_has(chain, quote_symbol)) {
            ++itr;
            continue;
        }

        auto& res = step.res();

        if (res.symbol != quote_symbol) {
            const auto& a = get_asset(quote_symbol);

            if (query.hidden_assets.count(a.symbol_name)) {
                ++itr;
                continue;
            }

            if (res.amount.value) {
                make_remain_fee();
                exchange_go_chain<OrderIndex>(chain, exchange_step::from(res, add_me.is_buy),
                    chains, sym2, query, assets);
                chain.has_remain = false;
            }
            par = step.param();
            res.amount.value = 0;
            res.symbol = quote_symbol;
            if (step.remain) step.remain.reset();
            if (!add_me.is_buy) {
                if (step.fee) step.fee.reset();

                if (a.fee_pct) {
                    step.fee_pct = a.fee_pct;
                } else if (!!step.fee_pct) {
                    step.fee_pct.reset();
                }
            }
            step.best_price = itr_prc;
        }

        step.limit_price = itr_prc;

        auto o_par = std::min(par, add_me.is_buy ? itr->amount_for_sale() : itr->amount_to_receive());
        auto r = o_par * itr->sell_price;
        par -= o_par;

        if (add_me.is_buy && !!step.fee_pct) {
            auto am_pct = STEEMIT_100_PERCENT - *(step.fee_pct);
            asset r_f(0, r.symbol);
            if (am_pct) {
                r_f = asset((fc::uint128_t(r.amount.value) * STEEMIT_100_PERCENT / am_pct).to_uint64(),
                    r.symbol);
            } else {
                ++itr;
                continue;
            }
            res += r_f;

            auto p_f = r_f * itr->sell_price;
            if (!step.fee) step.fee = asset(0, p_f.symbol);
            *(step.fee) += (p_f - o_par);
        } else {
            res += r;
        }

        ++itr;
    }

    if (step.res().amount.value) {
        make_remain_fee();
        exchange_go_chain<OrderIndex>(chain, exchange_step::from(step.res(), add_me.is_buy),
            chains, sym2, query, assets);
        chain.has_remain = false;
    }
}

fc::mutable_variant_object exchange::exchange_impl::get_exchange(exchange_query query) const {
    fc::mutable_variant_object result;

    asset_symbol_type sym2 = 0;
    _db.with_weak_read_lock([&]() {
        sym2 = _db.symbol_from_str(query.symbol);

        GOLOS_CHECK_PARAM(query.min_to_receive, {
            if (query.min_to_receive.amount.value) {
                GOLOS_CHECK_VALUE(query.direction == exchange_direction::sell,
                    "min_to_receive works only if direction is sell");
                GOLOS_CHECK_VALUE(query.min_to_receive.symbol == sym2,
                    "min_to_receive should have same symbol as 'symbol' field");
            }
        });
    });

    std::vector<ex_chain> chains;

    bool is_buy = query.direction == exchange_direction::buy;

    _db.with_weak_read_lock([&]() {
        ex_chain chain;
        auto step = exchange_step::from(query.amount, is_buy);
        asset_map assets;
        if (is_buy) {
            exchange_go_chain<by_price>(
                chain, step,
                chains, sym2, query, assets);
        } else {
            exchange_go_chain<by_buy_price>(
                chain, step,
                chains, sym2, query, assets);
        }
    });

    std::sort(chains.begin(), chains.end(), [&](ex_chain a, ex_chain b) {
        if (is_buy) return a.res() < b.res();
        return a.res() > b.res();
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
