#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/exchange/exchange_queries.hpp>
#include <golos/plugins/exchange/exchange.hpp>
#include <golos/plugins/exchange/order_cache.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/donate_targets.hpp>
#include <golos/chain/operation_notification.hpp>

namespace golos { namespace plugins { namespace exchange {

using sym_path = std::vector<asset_symbol_type>;
bool sym_exists(const auto& path, auto sym) {
    return std::find(path.begin(), path.end(), sym) != path.end();
};

class exchange::exchange_impl final {
public:
    exchange_impl(uint16_t hds = 0)
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()),
            hybrid_discrete_step(hds) {
    }

    ~exchange_impl() {
    }

    asset_info get_asset(asset_map& assets, asset_symbol_type symbol, bool cached = false) const;

    asset subtract_fee(asset& a, uint16_t fee_pct) const;

    asset include_fee(asset& a, uint16_t fee_pct) const;

    struct cache_helper {
        order_cache cache;
        bool from_cache = false;

        void add_to_cache(const limit_order_object& loo) {
            if (from_cache) return;
            auto& id = cache.get<by_id>();
            if (id.find(loo.id) != id.end()) return;
            id.emplace(loo);
        }
    };

    template<typename OrderIndex>
    void exchange_go_chain(ex_chain chain, exchange_step add_me,
        std::vector<ex_chain>& chains, asset_symbol_type sym2,
        const exchange_query& query, const OrderIndex& idx, cache_helper& ch, asset_map& assets, const ex_stat& stat) const;

    ex_chain fix_receive(const ex_chain& chain, const ex_stat& stat) const;

    // -- spread

    asset apply_direct(exchange_step& dir, const limit_order_object& ord, asset param) const;

    ex_rows map_rows(const ex_rows& curr, ex_rows prev, const ex_stat& stat) const;

    template<typename OrderIndex>
    std::pair<ex_chain, asset> optimize_chain(asset start, const ex_chain& chain, const OrderIndex& idx, const ex_stat& stat, bool optimize = true) const;

    std::vector<ex_chain> spread_chains(const std::vector<ex_chain>& chains, const exchange_query& query,
        const cache_helper& ch, const ex_stat& stat) const;

    // -- spread end

    fc::mutable_variant_object get_exchange(exchange_query query) const;

    template<typename OrderIndex>
    void go_value_path(const exchange_path_query& query, exchange_path& res,
        sym_path path, value_path path_str,
        asset_symbol_type add_me, const std::string& add_me_str, const OrderIndex& idx, const ex_stat& stat) const;

    exchange_path get_exchange_path(exchange_path_query query) const;

    database& _db;

    uint16_t hybrid_discrete_step = 0;

    fc::log_level log_lev = fc::log_level::info;
};

exchange::exchange() = default;

exchange::~exchange() = default;

const std::string& exchange::name() {
    static std::string name = "exchange";
    return name;
}

void exchange::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
    cfg.add_options()
        ("exchange-log-level", bpo::value<std::string>()->default_value("info"), "info or all");
}

void exchange::plugin_initialize(const bpo::variables_map& options) {
    ilog("Initializing exchange plugin");

    my = std::make_unique<exchange::exchange_impl>(this->hybrid_discrete_step);

    if (options.count("exchange-log-level")) {
        auto log_lev = options.at("exchange-log-level").as<std::string>();
        if (log_lev == "info") {
            my->log_lev = fc::log_level::info;
        } else if (log_lev == "all") {
            my->log_lev = fc::log_level::all;
        } else {
            FC_ASSERT(false, "'" + log_lev + "' exchange-log-level not supported");
        }
    }

    JSON_RPC_REGISTER_API(name())
} 

void exchange::plugin_startup() {
    ilog("Starting up exchange plugin");
}

void exchange::plugin_shutdown() {
    ilog("Shutting down exchange plugin");
}

ex_rows exchange::map_rows(const ex_rows& curr, ex_rows prev) const {
    ex_stat stat;
    return my->map_rows(curr, prev, stat);
}

asset_info exchange::exchange_impl::get_asset(asset_map& assets, asset_symbol_type symbol, bool cached) const {
    auto a_itr = assets.find(symbol);
    if (a_itr != assets.end()) {
        return a_itr->second;
    } else {
        if (symbol == STEEM_SYMBOL) {
            assets[symbol] = asset_info("GOLOS");
        } else if (symbol == SBD_SYMBOL) {
            assets[symbol] = asset_info("GBG");
        } else {
            FC_ASSERT(!cached, "Cannot access not cached asset when using cached orders");
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

asset exchange::exchange_impl::include_fee(asset& a, uint16_t fee_pct) const {
    auto orig = a;
    auto am_pct = STEEMIT_100_PERCENT - fee_pct;
    if (am_pct) {
        a = asset((fc::uint128_t(a.amount.value) * STEEMIT_100_PERCENT / am_pct).to_uint64(),
            a.symbol);
        if (fee_pct && a == orig) { // for HF 25 fix
            a += asset(1, a.symbol);
        }
        return a - orig;
    } else {
        a.amount.value = 0;
        return orig;
    }
}

template<typename OrderIndex>
void exchange::exchange_impl::exchange_go_chain(
    ex_chain chain, exchange_step add_me,
    std::vector<ex_chain>& chains, asset_symbol_type sym2,
    const exchange_query& query, const OrderIndex& idx, cache_helper& ch, asset_map& assets, const ex_stat& stat
) const {
    CHECK_API_TIMEOUT(stat, GOLOS_SEARCH_TIMEOUT_MS);

    if (add_me.param().symbol == sym2) {
        if (query.pct == STEEMIT_100_PERCENT
                && (!query.is_spread() || chain.is_direct())
                && query.min_to_receive.ignore_chain(add_me.is_buy, chain.param(), chain.res(), chain.is_direct())) {
            return;
        }
        if (chain.is_direct()) {
            if (chain.has_remain && query.remain.direct == exchange_remain_policy::ignore) return;
        } else if (!query.is_spread()) {
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
        const auto& a = get_asset(assets, par.symbol, ch.from_cache);
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
        CHECK_API_TIMEOUT(stat, GOLOS_SEARCH_TIMEOUT_MS);

        ch.add_to_cache(*itr);

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

        if (query.pct != STEEMIT_100_PERCENT) {
            if ((query.pct_direct && quote_symbol != query.sym2) ||
                (!query.pct_direct && chain.size() == 1 && quote_symbol == sym2)) {
                ++itr;
                continue;
            }
        }

        if (res.symbol != quote_symbol) {
            const auto& a = get_asset(assets, quote_symbol, ch.from_cache);

            if (query.hidden_assets.count(a.symbol_name)) {
                ++itr;
                continue;
            }

            if (res.amount.value || res.symbol == sym2) {
                make_remain();
                exchange_go_chain(chain, exchange_step::from(res, add_me.is_buy),
                    chains, sym2, query, idx, ch, assets, stat);
                chain.has_remain = false;
            }
            res.amount.value = 0;
            res.symbol = quote_symbol;
            if (chain._log_lev < fc::log_level::off) step.impacted_orders.clear();
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

            auto fee = subtract_fee(ord, fee_pct);

            if (par >= ord) {
                step.receive += ord;
                step.add_fee(fee);
                par -= ord;
                step.add_res(ord_orig, itr->sell_price);
            } else {
                step.receive += par;

                auto am = par;
                include_fee(am, fee_pct);
                if (am.amount == 0) {
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

        if (chain._log_lev < fc::log_level::off) step.impacted_orders.push_back(*itr);

        ++itr;
    }

    if (step.res().amount.value || step.res().symbol == sym2) {
        make_remain();
        exchange_go_chain(chain, exchange_step::from(step.res(), add_me.is_buy),
            chains, sym2, query, idx, ch, assets, stat);
        chain.has_remain = false;
    }
}

ex_chain exchange::exchange_impl::fix_receive(const ex_chain& chain, const ex_stat& stat) const {
    ex_chain result(chain._log_lev);
    result.reversed = true;
    result.subchains = chain.subchains;

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
            CHECK_API_TIMEOUT(stat, GOLOS_SEARCH_TIMEOUT_MS);

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

        if (!!st.remain) {
            step.remain = st.remain;
            result.has_remain = true;
        }

        step.is_buy = true;
        result.push_back(step);

        par = step.receive;

        rev ? i++ : i--;
    }

    return result;
}

asset exchange::exchange_impl::apply_direct(exchange_step& dir, const limit_order_object& ord, asset param) const {
    auto par = param;

    uint16_t fee_pct = 0;
    if (!!dir.fee_pct) fee_pct = *(dir.fee_pct);

    if (dir.is_buy) {
        auto ord_orig = ord.amount_for_sale();
        auto full = ord_orig;

        auto fee = subtract_fee(full, fee_pct);
        if (par >= full) {
            dir.add_fee(fee);
            par -= full;
            dir.add_res(ord_orig, ord.sell_price);
        } else {
            auto am = par;
            include_fee(am, fee_pct);
            if (am.amount == 0) {
                //TODO
                //++itr;
                //continue;
            }

            dir.add_fee(am - par);

            par -= par;

            dir.add_res(am, ord.sell_price);
        }
    } else {
        auto o_par = std::min(par, ord.amount_to_receive());
        auto r = o_par * ord.sell_price;
        par -= o_par;

        if (fee_pct) {
            auto fee = subtract_fee(r, fee_pct);
            dir.add_fee(fee);
        }
        dir.res() += r;
    }

    dir.param() += (param - par);

    auto itr_prc = dir.is_buy ? ord.buy_price() : ord.sell_price;
    if (!dir.best_price.base.amount.value) {
        dir.best_price = itr_prc;
    }
    dir.limit_price = itr_prc;

    return par;
}

ex_rows exchange::exchange_impl::map_rows(const ex_rows& curr, ex_rows prev, const ex_stat& stat) const {
    ex_rows res;

    for (size_t i = 0; i < curr.size(); ++i) {
        auto row = curr[i];
        auto row_price = row.get_price();

        auto safe_price = [&](asset a, price p) {
            if (a.symbol == p.base.symbol && p.base.amount == 0) {
                return asset(0, p.quote.symbol);
            }
            if (a.symbol == p.quote.symbol && p.quote.amount == 0) {
                return asset(0, p.base.symbol);
            }
            return a * p;
        };

        asset new_res(0, row.res.symbol);
        auto first_i = res.size();

        while (true) {
            CHECK_API_TIMEOUT(stat, GOLOS_SPREAD_PER_CHAIN_TIMEOUT_MS);

            if (row.par.amount <= 0) break;

            FC_ASSERT(prev.size(), "No enough prev row found");

            auto& pr = prev[0];

            ex_row nr(pr.par.symbol, row.res.symbol);

            auto par0 = safe_price(row.par, pr.get_price());
            if (row.par >= pr.res || par0 >= pr.par) {
                nr.par = pr.par;
                nr.res = safe_price(pr.res, row_price);
                res.push_back(nr);
                new_res += nr.res;

                row.par -= pr.res;
                row.res = safe_price(row.par, row_price);

                prev.erase(prev.begin());
                continue;
            } else {
                nr.par = par0;
                nr.res = row.res;
                new_res += nr.res;

                pr.par -= par0;
                pr.res -= row.par;
            }

            res.push_back(nr);

            break;
        }

        auto error = curr[i].res - new_res;
        res[first_i].res += error;
    }

    return res;
}

template<typename OrderIndex>
std::pair<ex_chain, asset> exchange::exchange_impl::optimize_chain(asset start, const ex_chain& chain, const OrderIndex& idx, const ex_stat& stat, bool optimize) const {
    ex_chain new_chain(chain._log_lev);
    new_chain.copy_logs(chain);
    new_chain.log_d([&]() { return "--- optimize_chain"; });
    asset next;

    int64_t first = 0;
    int64_t end = chain.size();

    for (int64_t i = first; i != end; ++i) {
        const auto& step = chain[i];

        auto copy = step;
        if (i == first) {
            copy.param() = start;
        } else {
            copy.param() = new_chain[i - 1].res();
        }
        copy.remain.reset();
        if (new_chain._log_lev < fc::log_level::off) copy.impacted_orders.clear();

        auto par = copy.param();
        auto& res = copy.res();
        res.amount = 0;

        ex_rows rows;

        auto prc = price::min(par.symbol, res.symbol);
        if (copy.is_buy) {
            prc = price::max(par.symbol, res.symbol);
        }

        bool first_ord = true;
        auto itr = idx.lower_bound(prc);
        while (itr != idx.end()) {
            CHECK_API_TIMEOUT(stat, GOLOS_SPREAD_PER_CHAIN_TIMEOUT_MS);

            if (par.amount == 0) break;

            auto itr_prc = copy.is_buy ? itr->sell_price : itr->buy_price();
            if (itr_prc.base.symbol != par.symbol || itr_prc.quote.symbol != res.symbol) {
                break;
            }

            ex_row row(par.symbol, res.symbol);

            uint16_t fee_pct = 0;
            if (!!copy.fee_pct) fee_pct = *(copy.fee_pct);

            if (copy.is_buy) {
                auto ord_orig = itr->amount_for_sale();
                auto full = ord_orig;

                auto fee = subtract_fee(full, fee_pct);
                if (par >= full) {
                    copy.add_fee(fee);
                    par -= full;
                    auto add = copy.add_res(ord_orig, itr->sell_price);

                    row.par += full;
                    row.res += add;
                } else {
                    auto am = par;
                    include_fee(am, fee_pct);
                    if (am.amount == 0) {
                        //TODO
                        //++itr;
                        //continue;
                    }

                    copy.add_fee(am - par);

                    auto par0 = par;
                    par -= par;

                    auto add = copy.add_res(am, itr->sell_price);

                    row.par += par0;
                    row.res += add;
                }
            } else {
                bool do_fix = par <= itr->amount_to_receive();

                auto o_par = std::min(par, itr->amount_to_receive());
                auto r = o_par * itr->sell_price;

                // fix_sell
                bool fixed = false;
                if (!copy.is_buy && i == first && r.amount > 0 && do_fix && optimize) {
                    auto fix = r * itr->sell_price;
                    if (fix < o_par) {
                        fixed = true;
                        par -= fix;

                        row.par += fix;
                    }
                }

                if (!fixed) {
                    par -= o_par;

                    row.par += o_par;
                }

                if (fee_pct) {
                    auto fee = subtract_fee(r, fee_pct);
                    copy.add_fee(fee);
                }
                res += r;

                row.res += r;
            }

            if (first_ord) {
                copy.best_price = ~itr_prc;
            }
            first_ord = false;
            copy.limit_price = ~itr_prc;

            rows.push_back(row);

            if (new_chain._log_lev < fc::log_level::off) copy.impacted_orders.push_back(*itr);

            ++itr;
        }

        if (par.amount.value != 0) {
            if (i == first) {
                next = par;
            } else {
                auto set_remain = [&]() {
                    copy.remain = par;
                    new_chain.has_remain = true;
                };

                set_remain();
            }
        }

        if (i == first && next.amount.value) {
            copy.param() -= next;
        }

        if (i == first) {
            new_chain.rows = rows;
        } else {
            #define SUBMIT_ERROR(REPORT) \
                if (!new_chain._err_report) new_chain._err_report = "map_rows: " + std::string(REPORT);

            try {
                new_chain.rows = map_rows(rows, new_chain.rows, stat);
            } catch (fc::exception& err) {
                SUBMIT_ERROR(err.to_detail_string());
            } catch (const std::exception& err) {
                SUBMIT_ERROR(err.what());
            } catch (...) {
                SUBMIT_ERROR("Unknown error");
            }
        }

        new_chain.log_d([&]() { return "rows: " + fc::json::to_string(new_chain.rows)
            + " | cur_rows: " + fc::json::to_string(rows); });

        new_chain.push_back(copy);
    }

    return { new_chain, next };
}

std::vector<ex_chain> exchange::exchange_impl::spread_chains(const std::vector<ex_chain>& chains, const exchange_query& query,
    const cache_helper& ch, const ex_stat& stat) const {
    std::vector<ex_chain> res;

    for (auto chain : chains) {
        CHECK_API_TIMEOUT(stat, GOLOS_SEARCH_TIMEOUT_MS);

        ex_stat per_chain;

        if (chain.size() > 1) {
            auto orig = chain;

            #define HANDLE_ERROR(REPORT) \
                orig._err_report = REPORT; \
                res.push_back(orig); \
                continue;

            try {
                auto is_buy = chain.is_buy();
                auto par = query.amount;
                bool chain_empty = true;

                exchange_step dir;
                dir = exchange_step::from(par, is_buy);
                dir.param().amount = 0;
                dir.res() = asset(0, chain.res().symbol);
                dir.fee_pct = chain.last().fee_pct;

                auto process_idx = [&](const auto& idx) {
                    #define RETURN_ORIG_WITH_REPORT(OPTIM) \
                        orig._logs = OPTIM._logs; \
                        if (OPTIM._err_report) { \
                            orig._err_report = OPTIM._err_report; \
                            return; \
                        }

                    auto optim = optimize_chain(par, chain, idx, per_chain, query.hybrid.fix_sell);
                    RETURN_ORIG_WITH_REPORT(optim.first);

                    auto rows = optim.first.rows;
                    int64_t r = rows.size() - 1;

                    auto prc = price::min(query.amount.symbol, query.sym2);
                    auto prc_max = prc.max();
                    auto itr = idx.lower_bound(is_buy ? prc_max : prc);
                    auto end = idx.upper_bound(is_buy ? prc : prc_max);

                    asset ord;
                    asset last_ord;
                    asset dir_per_ord;
                    while (itr != end && r >= 0) {
                        CHECK_API_TIMEOUT(per_chain, GOLOS_SPREAD_PER_CHAIN_TIMEOUT_MS);

                        if (dir_per_ord.amount == 0) dir_per_ord = asset(0, rows[r].par.symbol);

                        auto itr_prc = is_buy ? itr->buy_price() : itr->sell_price;
                        auto prc = rows[r].get_price();
                        bool better = is_buy ? itr_prc < prc : itr_prc > prc;

                        optim.first.log_d([&]() { return "sc: " + fc::json::to_string(prc) + " " + fc::json::to_string(itr_prc) + " " + std::to_string(better); });

                        last_ord = is_buy ? itr->amount_for_sale() : itr->amount_to_receive();

                        if (better) {
                            if (ord.amount == 0) {
                                ord = last_ord;
                            }

                            asset param;
                            if (rows[r].par < ord) {
                                param = rows[r].par;
                            } else {
                                param = ord;
                            }

                            // TODO: what if res = 0 because price?
                            rows[r].minus_par(param);
                            if (rows[r].par.amount == 0) {
                                --r;
                            }
                            ord -= param;
                            last_ord -= param;
                            dir_per_ord += param;

                            if (ord.amount == 0) {
                                apply_direct(dir, *itr, dir_per_ord);
                                optim.first.log_i([&]() { return "dir0: " + fc::json::to_string(*itr); });

                                par -= dir_per_ord;
                                dir_per_ord.amount = 0;

                                itr++;
                                if (itr != end)
                                    last_ord = is_buy ? itr->amount_for_sale() : itr->amount_to_receive();
                            }
                        } else {
                            break;
                        }
                    }

                    if (dir_per_ord.amount > 0 && itr != end) {
                        apply_direct(dir, *itr, dir_per_ord);
                        optim.first.log_i([&]() { return "dir_last: " + fc::json::to_string(*itr); });
                        par -= dir_per_ord;
                    }

                    if (par.amount == 0) {
                        // chain empty
                        orig._err_report = optim.first._err_report;
                        return;
                    }

                    auto mult = optimize_chain(par, optim.first, idx, per_chain, query.hybrid.fix_sell);
                    RETURN_ORIG_WITH_REPORT(mult.first);

                    auto next = mult.second;
                    while (next.amount > 0 && itr != end) {
                        // TODO: but it is wrong. last ord should be used
                        next = apply_direct(dir, *itr, next);
                        par -= next;
                        mult.first.log_i([&]() { return "dir_next: " + fc::json::to_string(*itr); });

                        ++itr;
                    }

                    if (next.amount > 0) {
                        mult = optimize_chain(par, optim.first, idx, per_chain, false);
                        RETURN_ORIG_WITH_REPORT(mult.first);
                        next = mult.second;
                    }

                    chain = mult.first;
                    chain_empty = false;

                    if (next.amount > 0) {
                        chain.has_remain = true;
                        chain.steps[0].remain = next;
                    }
                };

                if (is_buy) {
                    const auto& cache_idx = ch.cache.get<by_price>();
                    process_idx(cache_idx);
                } else {
                    const auto& cache_idx = ch.cache.get<by_buy_price>();
                    process_idx(cache_idx);
                }

                if (chain_empty) {
                    res.push_back(orig);
                    continue;
                }

                if (dir.param().amount > 0) {
                    chain.set_direct(dir);
                }

                if (chain.has_remain && query.remain.multi == exchange_remain_policy::ignore) {
                    if (!orig.has_remain) res.push_back(orig);
                    continue;
                }
                if (!query.will_fix_input() &&
                    query.min_to_receive.ignore_chain(chain.is_buy(), chain.param(), chain.res(), false)) {
                    continue;
                }
            } catch (fc::exception& err) {
                elog("BUF_REP" + err.to_detail_string());
                HANDLE_ERROR(err.to_detail_string());
            } catch (const std::exception& err) {
                elog("BUF_REP2" + std::string(err.what()));
                HANDLE_ERROR(err.what());
            } catch (...) {
                HANDLE_ERROR("Unknown error");
            }
        }
        res.push_back(chain);
    }

    return res;
}

fc::mutable_variant_object exchange::exchange_impl::get_exchange(exchange_query query) const {
    fc::mutable_variant_object result;

    query.initialize_validate(_db);

    using chain_map = std::map<uint16_t, std::vector<ex_chain>>;
    chain_map multies;
    chain_map directs;

    bool is_buy = query.direction == exchange_direction::buy;

    ex_stat stat;

    auto ll = query.logs ? log_lev : fc::log_level::off;
    ex_chain chain(ll);
    cache_helper ch;
    asset_map assets;

    std::vector<exchange_query> queries;
    if (query.is_discrete()) {
        GOLOS_CHECK_PARAM(query.hybrid, {
            GOLOS_CHECK_VALUE(query.hybrid.discrete_step <= STEEMIT_100_PERCENT,
                "discrete_step cannot be greater than 10000 (100%)");
            GOLOS_CHECK_VALUE(query.hybrid.discrete_step >= hybrid_discrete_step,
                "discrete_step cannot be less than " + std::to_string(hybrid_discrete_step));
        });
        queries = query.discrete_split(query.hybrid.discrete_step);
    } else {
        queries = {query};
    }

    for (size_t i = 0; i < queries.size(); ++i) {
        const auto& curr = queries[i];
        auto step = exchange_step::from(curr.amount, is_buy);
        bool from_cache = i > 0;

        std::vector<ex_chain> chains;

        try {
            auto use_indexes = [&](const auto& idx, const auto& cache_idx) {
                if (from_cache) {
                    exchange_go_chain(
                        chain, step,
                        chains, curr.sym2, curr, cache_idx, ch, assets, stat);
                } else {
                    _db.with_weak_read_lock([&]() {
                        exchange_go_chain(
                            chain, step,
                            chains, curr.sym2, curr, idx, ch, assets, stat);
                    });
                }
            };

            if (is_buy) {
                const auto& idx = _db.get_index<limit_order_index, by_price>();
                const auto& cache_idx = ch.cache.get<by_price>();

                use_indexes(idx, cache_idx);
            } else {
                const auto& idx = _db.get_index<limit_order_index, by_buy_price>();
                const auto& cache_idx = ch.cache.get<by_buy_price>();

                use_indexes(idx, cache_idx);
            }
        } catch (const api_timeout& ex) {
            result["error"] = "timeout";
            elog("get_exchange timeout - " + fc::json::to_string(curr));
            return result;
        }

        if (curr.pct_direct) {
            directs[curr.pct] = chains;
        } else {
            multies[curr.pct] = chains;
        }
    }

    std::vector<ex_chain> chains;
    for (auto itr = multies.rbegin(); itr != multies.rend(); ++itr) {
        auto pct = itr->first;
        const auto& vec = itr->second;
        if (pct == STEEMIT_100_PERCENT) {
            if (query.is_spread()) {
                chains = spread_chains(vec, query, ch, stat);
            } else {
                std::copy(vec.begin(), vec.end(), std::back_inserter(chains));
            }
        } else {
            auto dirs = directs.find(pct);
            const ex_chain* direct = nullptr;
            if (dirs != directs.end()) {
                direct = get_direct_chain(dirs->second);
            }
            if (!direct) {
                continue;
            }
            for (auto chain : vec) {
                if (chain.size() == 1) {
                    continue;
                }
                if (direct->has_remain && query.remain.multi == exchange_remain_policy::ignore) {
                    continue;
                }
                chain.subchains.push_back(*direct);
                if (!query.will_fix_input() &&
                    query.min_to_receive.ignore_chain(chain.is_buy(), chain.param(), chain.res(), false)) {
                    continue;
                }
                chains.push_back(chain);
            }
        }
    }

    asset dir_receive;
    for (const auto& c : chains) {
        if (c.is_direct()) {
            dir_receive = c.is_buy() ? c.param() : c.res();
            break;
        }
    }

    uint16_t err_count = 0;

    if (query.will_fix_input()) {
        std::vector<ex_chain> new_chains;

        _db.with_weak_read_lock([&]() {
            for (const auto& c : chains) {
                auto c2 = fix_receive(c, stat);
                if (c2.steps.size()) {
                    if (c2.subchains.size()) {
                        auto sub = fix_receive(c2.subchains[0], stat);
                        if (sub.steps.size()) {
                            c2.subchains[0] = sub;
                        } else {
                            c2.subchains.clear();
                        }
                    }
                    if (query.min_to_receive.ignore_chain(c2.is_buy(), c2.param(), c2.res(), c2.size() == 1, dir_receive)) {
                        continue;
                    }
                    if (!!c._err_report) ++err_count;
                    new_chains.push_back(c2);
                }
            }
        });

        chains = new_chains;
    } else {
        std::vector<ex_chain> new_chains;

        for (auto& c : chains) {
            // Need for min_profit_pct only...
            if (query.min_to_receive.ignore_chain(c.is_buy(), c.param(), c.res(), c.is_direct(), dir_receive)) {
                continue;
            }

            if (is_buy) c.reverse();

            if (!!c._err_report) ++err_count;

            new_chains.push_back(c);
        }
        
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
        if (query.remain.multi == exchange_remain_policy::ignore &&
                chains.size() > 1 && !!chains[0].steps[0].remain) {
            result["best"] = chains[1];
        } else {
            result["best"] = chains[0];
        }
    } else {
        result["best"] = false;
    }

    result["all_chains"] = chains;

    result["_msec"] = stat.msec();

    if (err_count)
        result["_err_count"] = err_count;

    if (query.is_spread())
        result["_fix_sell"] = query.hybrid.fix_sell;

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

template<typename OrderIndex>
void exchange::exchange_impl::go_value_path(
    const exchange_path_query& query, exchange_path& res,
    sym_path path, value_path path_str,
    asset_symbol_type add_me, const std::string& add_me_str, const OrderIndex& idx, const ex_stat& stat) const {
    path.push_back(add_me);
    path_str.push_back(add_me_str);

    auto prc = price::min(add_me, 0);
    if (query.is_buy()) {
        prc = price::max(add_me, asset::max_symbol());
    }

    asset_symbol_type prev;

    auto itr = idx.lower_bound(prc);
    while (itr != idx.end()) {
        CHECK_API_TIMEOUT(stat, GOLOS_SEARCH_TIMEOUT_MS);

        auto itr_prc = query.is_buy() ? itr->sell_price : itr->buy_price();
        if (itr_prc.base.symbol != add_me) {
            break;
        }

        auto quote_symbol = itr_prc.quote.symbol;
        if (prev == quote_symbol) {
            ++itr;
            continue;
        }

        auto quote_str = itr_prc.quote.symbol_name();
        if (query.is_bad_symbol(quote_str)) {
            ++itr;
            continue;
        }

        if (!sym_exists(path, quote_symbol)) {
            go_value_path(query, res, path, path_str, quote_symbol, quote_str, idx, stat);
        }

        prev = quote_symbol;

        ++itr;
    }

    if (path.size() > 1 && query.is_good_symbol(add_me_str)) {
        res.result_syms.insert(add_me_str);
        res.paths.push_back(path_str);
    }
}

exchange_path exchange::exchange_impl::get_exchange_path(exchange_path_query query) const {
    query.initialize_validate(_db);

    ex_stat stat;
    exchange_path res;

    _db.with_weak_read_lock([&]() {
        if (query.is_buy()) {
            const auto& idx = _db.get_index<limit_order_index, by_price>();
            go_value_path(query, res, sym_path(), value_path(), query.buy_sym, query.buy, idx, stat);
        } else {
            const auto& idx = _db.get_index<limit_order_index, by_buy_price>();
            go_value_path(query, res, sym_path(), value_path(), query.sell_sym, query.sell, idx, stat);
        }
    });

    if (query.assets) {
        using golos::api::assets_query;
        using database_api::asset_api_sort;

        std::set<std::string> keys;
        keys.insert(res.result_syms.begin(), res.result_syms.end());
        if (query.sell.size())keys.insert(query.sell);
        if (query.buy.size()) keys.insert(query.buy);

        auto& _dapi = appbase::app().get_plugin<golos::plugins::database_api::plugin>();
        auto assets = _dapi.get_assets_inner("",
            std::vector<std::string>(keys.begin(), keys.end()),
            "", 100, asset_api_sort::by_symbol_name, assets_query());
        for (const auto& a : assets) {
            res.assets[a.supply.symbol_name()] = a;
        }
    }

    res._msec = stat.msec();

    return res;
}

DEFINE_API(exchange, get_exchange_path) {
    PLUGIN_API_VALIDATE_ARGS(
        (exchange_path_query, query)
    );

    return my->get_exchange_path(query);
}

} } } // golos::plugins::exchange
