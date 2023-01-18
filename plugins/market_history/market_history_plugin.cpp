#include <golos/plugins/market_history/market_history_plugin.hpp>
#include <golos/plugins/market_history/market_history_visitor.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>

#include <golos/chain/index.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/account_object.hpp>

#include <golos/protocol/exceptions.hpp>

#include <boost/algorithm/string.hpp>
#include <golos/api/callback_info.hpp>
#include <golos/plugins/market_history/market_history_callbacks.hpp>


namespace golos {
    namespace plugins {
        namespace market_history {

            using golos::protocol::fill_order_operation;
            using golos::chain::operation_notification;

            using golos::api::callback_info;
            using plugins::json_rpc::msg_pack_transfer;

            using market_callback_info = callback_info<const callback_arg&>;
            using market_callback = market_callback_info::callback_t;

            class market_history_plugin::market_history_plugin_impl {
            public:
                market_history_plugin_impl(market_history_plugin &plugin)
                        : _my(plugin),
                          _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()){
                }

                ~market_history_plugin_impl() {
                }

                void on_operation(const operation_notification& note) {
                    try {
                        /// plugins shouldn't ever throw
                        update_market_histories(note);
                        note.op.visit(operation_visitor(_my, _db));
                    } catch (const fc::exception& e) {
                        edump((e.to_detail_string()));
                    } catch (...) {
                        elog("unhandled exception");
                    }
                }

                symbol_type_pair get_symbol_type_pair(asset asset1, asset asset2, bool* pair_reversed = nullptr) const;
                symbol_type_pair get_symbol_type_pair(const symbol_name_pair& pair, bool* pair_reversed = nullptr, bool allow_partial = false) const;
                void reverse_price(double& price);

                market_ticker get_ticker(const symbol_type_pair& pair) const;
                market_volume get_volume(const symbol_type_pair& pair) const;
                market_depth get_depth(const symbol_type_pair& pair) const;
                order_book get_order_book(const symbol_type_pair& pair, uint32_t limit) const;
                order_book_extended get_order_book_extended(const symbol_type_pair& pair, uint32_t limit) const;
                vector<market_trade> get_trade_history(const symbol_type_pair& pair, time_point_sec start, time_point_sec end, uint32_t limit) const;
                vector<market_trade> get_recent_trades(const symbol_type_pair& pair, uint32_t limit) const;
                vector<bucket_object> get_market_history(const symbol_type_pair& pair, uint32_t bucket_seconds, time_point_sec start, time_point_sec end) const;
                flat_set<uint32_t> get_market_history_buckets() const;
                std::vector<limit_order> get_open_orders(const symbol_type_pair& pair, bool pair_reversed, const std::string& owner, bool sort_by_price, bool sort_lesser) const;
                std::vector<limit_order> get_orders(const std::vector<std::string>& order_ids) const;

                void subscribe_to_market(market_callback cb);

                void update_market_histories(const golos::chain::operation_notification &o);

                golos::chain::database &database() const {
                    return _db;
                }

                market_history_plugin &_my;
                flat_set<uint32_t> _tracked_buckets = flat_set<uint32_t>  {15, 60, 300, 3600, 86400 };

                int32_t _maximum_history_per_bucket_size = 1000;

                golos::chain::database &_db;

                market_callback_info::cont active_market_callback;
                market_callback_info::cont free_market_callback;
            };

            void market_history_plugin::market_history_plugin_impl::update_market_histories(const operation_notification &o) {
                if (o.op.which() ==
                    operation::tag<fill_order_operation>::value) {
                    fill_order_operation op = o.op.get<fill_order_operation>();

                    auto &db = database();
                    const auto& bucket_idx = db.get_index<bucket_index, by_bucket>();

                    db.create<order_history_object>([&](auto& ho) {
                        ho.time = db.head_block_time();
                        ho.op = op;
                    });

                    if (!_maximum_history_per_bucket_size) {
                        return;
                    }
                    if (!_tracked_buckets.size()) {
                        return;
                    }

                    asset_symbol_type sym1;
                    asset_symbol_type sym2;
                    std::tie(sym1, sym2) = get_symbol_type_pair(op.open_pays, op.current_pays);

                    for (auto bucket : _tracked_buckets) {
                        auto cutoff = db.head_block_time() - fc::seconds(
                                bucket * _maximum_history_per_bucket_size);

                        auto open = fc::time_point_sec(
                                (db.head_block_time().sec_since_epoch() /
                                 bucket) * bucket);
                        auto seconds = bucket;

                        auto itr = bucket_idx.find(std::make_tuple(sym1, sym2, seconds, open));
                        if (itr == bucket_idx.end()) {
                            db.create<bucket_object>([&](auto& b) {
                                b.sym1 = sym1;
                                b.sym2 = sym2;

                                b.open = open;
                                b.seconds = bucket;

                                if (op.open_pays.symbol == sym1) {
                                    b.high_asset1 = op.open_pays.amount;
                                    b.high_asset2 = op.current_pays.amount;
                                    b.low_asset1 = op.open_pays.amount;
                                    b.low_asset2 = op.current_pays.amount;
                                    b.open_asset1 = op.open_pays.amount;
                                    b.open_asset2 = op.current_pays.amount;
                                    b.close_asset1 = op.open_pays.amount;
                                    b.close_asset2 = op.current_pays.amount;
                                    b.asset1_volume = op.open_pays.amount;
                                    b.asset2_volume = op.current_pays.amount;
                                } else {
                                    b.high_asset1 = op.current_pays.amount;
                                    b.high_asset2 = op.open_pays.amount;
                                    b.low_asset1 = op.current_pays.amount;
                                    b.low_asset2 = op.open_pays.amount;
                                    b.open_asset1 = op.current_pays.amount;
                                    b.open_asset2 = op.open_pays.amount;
                                    b.close_asset1 = op.current_pays.amount;
                                    b.close_asset2 = op.open_pays.amount;
                                    b.asset1_volume = op.current_pays.amount;
                                    b.asset2_volume = op.open_pays.amount;
                                }
                            });
                        } else {
                            db.modify(*itr, [&](auto& b) {
                                if (op.open_pays.symbol == sym1) {
                                    b.asset1_volume += op.open_pays.amount;
                                    b.asset2_volume += op.current_pays.amount;
                                    b.close_asset1 = op.open_pays.amount;
                                    b.close_asset2 = op.current_pays.amount;

                                    if (b.high() <
                                        price(op.current_pays, op.open_pays)) {
                                        b.high_asset1 = op.open_pays.amount;
                                        b.high_asset2 = op.current_pays.amount;
                                    }

                                    if (b.low() >
                                        price(op.current_pays, op.open_pays)) {
                                        b.low_asset1 = op.open_pays.amount;
                                        b.low_asset2 = op.current_pays.amount;
                                    }
                                } else {
                                    b.asset1_volume += op.current_pays.amount;
                                    b.asset2_volume += op.open_pays.amount;
                                    b.close_asset1 = op.current_pays.amount;
                                    b.close_asset2 = op.open_pays.amount;

                                    if (b.high() <
                                        price(op.open_pays, op.current_pays)) {
                                        b.high_asset1 = op.current_pays.amount;
                                        b.high_asset2 = op.open_pays.amount;
                                    }

                                    if (b.low() >
                                        price(op.open_pays, op.current_pays)) {
                                        b.low_asset1 = op.current_pays.amount;
                                        b.low_asset2 = op.open_pays.amount;
                                    }
                                }
                            });

                            if (_maximum_history_per_bucket_size > 0) {
                                open = fc::time_point_sec();
                                itr = bucket_idx.lower_bound(std::make_tuple(sym1, sym2, seconds, open));

                                while (itr->sym1 == sym1 && itr->sym2 == sym2 &&
                                       itr->seconds == seconds &&
                                       itr->open < cutoff) {
                                    auto old_itr = itr;
                                    ++itr;
                                    db.remove(*old_itr);
                                }
                            }
                        }
                    }
                }
            }

            symbol_type_pair market_history_plugin::market_history_plugin_impl::get_symbol_type_pair(asset asset1, asset asset2, bool* pair_reversed) const {
                if (pair_reversed) *pair_reversed = false;
                auto sym1 = asset1.symbol;
                auto sym2 = asset2.symbol;
                if (sym2 == STEEM_SYMBOL
                    || (asset2.symbol_name() < asset1.symbol_name() && sym1 != STEEM_SYMBOL)) {
                    std::swap(sym1, sym2);
                    if (pair_reversed) *pair_reversed = true;
                }
                return symbol_type_pair(sym1, sym2);
            }

            symbol_type_pair market_history_plugin::market_history_plugin_impl::get_symbol_type_pair(const symbol_name_pair& pair, bool* pair_reversed, bool allow_partial) const {
                GOLOS_CHECK_PARAM(pair, {
                    if (!allow_partial || pair.first.size())
                        GOLOS_CHECK_VALUE(pair.first.size() >= 3 && pair.first.size() <= 14, "pair.first must be between 3 and 14");
                    if (!allow_partial || pair.second.size())
                        GOLOS_CHECK_VALUE(pair.second.size() >= 3 && pair.second.size() <= 14, "pair.second must be between 3 and 14");
                });
                auto sym_from_str = [&](std::string str) {
                    boost::to_upper(str);
                    asset_symbol_type sym = 0;
                    if (str == "GOLOS") {
                        sym = STEEM_SYMBOL;
                    } else if (str == "GBG") {
                        sym = SBD_SYMBOL;
                    } else if (!allow_partial || str != "") {
                        sym = _db.get_asset(str).symbol();
                    }
                    return sym;
                };
                return get_symbol_type_pair(
                    asset(0, sym_from_str(pair.first)),
                    asset(0, sym_from_str(pair.second)),
                    pair_reversed
                );
            }

            void market_history_plugin::market_history_plugin_impl::reverse_price(double& price) {
                if (price != 0) price = 1 / price;
            }

            market_ticker market_history_plugin::market_history_plugin_impl::get_ticker(const symbol_type_pair& pair) const {
                const auto& bucket_idx = _db.get_index<bucket_index, by_bucket>();
                auto itr = bucket_idx.lower_bound(std::make_tuple(pair.first, pair.second, 86400, database().head_block_time() - 86400));

                market_ticker result;

                if (itr != bucket_idx.end() && itr->seconds == 86400) {
                    auto open1 = (asset(itr->open_asset2, pair.second) /
                                 asset(itr->open_asset1, pair.first)).to_real();
                    result.latest1 = (asset(itr->close_asset2, pair.second) /
                                     asset(itr->close_asset1, pair.first)).to_real();
                    result.percent_change1 =
                            ((result.latest1 - open1) / open1) * 100;

                    auto open2 = (asset(itr->open_asset1, pair.first) /
                                 asset(itr->open_asset2, pair.second)).to_real();
                    result.latest2 = (asset(itr->close_asset1, pair.first) /
                                     asset(itr->close_asset2, pair.second)).to_real();
                    result.percent_change2 =
                            ((result.latest2 - open2) / open2) * 100;
                } else {
                    result.latest1 = 0;
                    result.percent_change1 = 0;
                    result.latest2 = 0;
                    result.percent_change2 = 0;
                }

                auto orders = get_order_book(pair, 1);
                if (orders.bids.size()) {
                    result.highest_bid = orders.bids[0].price;
                }
                if (orders.asks.size()) {
                    result.lowest_ask = orders.asks[0].price;
                }

                auto volume = get_volume(pair);
                result.asset1_volume = volume.asset1_volume;
                result.asset2_volume = volume.asset2_volume;

                auto depth = get_depth(pair);
                result.asset1_depth = depth.asset1_depth;
                result.asset2_depth = depth.asset2_depth;

                return result;
            }

            market_volume market_history_plugin::market_history_plugin_impl::get_volume(const symbol_type_pair& pair) const {
                market_volume result;

                const auto& bucket_idx = _db.get_index<bucket_index, by_bucket>();
                auto itr = bucket_idx.lower_bound(std::make_tuple(pair.first, pair.second, 86400, database().head_block_time() - 86400));
                result.asset1_volume = asset(0, pair.first);
                result.asset2_volume = asset(0, pair.second);

                while (itr != bucket_idx.end() &&
                        itr->sym1 == pair.first && itr->sym2 == pair.second &&
                        itr->seconds == 86400) {
                    result.asset1_volume.amount += itr->asset1_volume;
                    result.asset2_volume.amount += itr->asset2_volume;

                    ++itr;
                }

                return result;
            }

            market_depth market_history_plugin::market_history_plugin_impl::get_depth(const symbol_type_pair& pair) const {
                market_depth result;
                result.asset1_depth = asset(0, pair.first);
                result.asset2_depth = asset(0, pair.second);

                const auto& order_idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_price>();
                auto itr = order_idx.lower_bound(price::max(pair.second, pair.first));

                while (itr != order_idx.end() &&
                       itr->sell_price.base.symbol == pair.second &&
                       itr->sell_price.quote.symbol == pair.first) {
                    result.asset2_depth += itr->amount_for_sale();
                    ++itr;
                }

                itr = order_idx.lower_bound(price::max(pair.first, pair.second));

                while (itr != order_idx.end() &&
                       itr->sell_price.base.symbol == pair.first &&
                       itr->sell_price.quote.symbol == pair.second) {
                    result.asset1_depth += itr->amount_for_sale();
                    ++itr;
                }
                return result;
            }

            order_book market_history_plugin::market_history_plugin_impl::get_order_book(const symbol_type_pair& pair, uint32_t limit) const {
                const auto& order_idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_price>();
                auto itr = order_idx.lower_bound(price::max(pair.second, pair.first));

                order_book result;

                while (itr != order_idx.end() &&
                       itr->sell_price.base.symbol == pair.second &&
                       itr->sell_price.quote.symbol == pair.first &&
                       result.bids.size() < limit) {
                    order cur;
                    cur.price = itr->sell_price.base.to_real() / itr->sell_price.quote.to_real();
                    cur.asset1 = (asset(itr->for_sale, pair.second) * itr->sell_price).amount;
                    cur.asset2 = itr->for_sale;
                    result.bids.push_back(cur);
                    ++itr;
                }

                itr = order_idx.lower_bound(price::max(pair.first, pair.second));

                while (itr != order_idx.end() &&
                       itr->sell_price.base.symbol == pair.first &&
                       itr->sell_price.quote.symbol == pair.second &&
                       result.asks.size() < limit) {
                    order cur;
                    cur.price = itr->sell_price.quote.to_real() / itr->sell_price.base.to_real();
                    cur.asset1 = itr->for_sale;
                    cur.asset2 = (asset(itr->for_sale, pair.first) * itr->sell_price).amount;
                    result.asks.push_back(cur);
                    ++itr;
                }

                return result;
            }

            order_book_extended market_history_plugin::market_history_plugin_impl::get_order_book_extended(const symbol_type_pair& pair, uint32_t limit) const {
                order_book_extended result;

                auto max_sell = price::max(pair.second, pair.first);
                auto max_buy = price::max(pair.first, pair.second);
                const auto& limit_price_idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_price>();
                auto sell_itr = limit_price_idx.lower_bound(max_sell);
                auto buy_itr = limit_price_idx.lower_bound(max_buy);
                auto end = limit_price_idx.end();

                while (sell_itr != end &&
                       sell_itr->sell_price.base.symbol == pair.second &&
                       sell_itr->sell_price.quote.symbol == pair.first &&
                       result.bids.size() < limit) {
                    auto itr = sell_itr;
                    order_extended cur;
                    cur.orderid = itr->orderid;
                    cur.order_price = itr->sell_price;
                    cur.real_price = (cur.order_price).to_real();
                    cur.asset2 = itr->for_sale;
                    cur.asset1 = (asset(itr->for_sale, pair.second) * cur.order_price).amount;
                    cur.created = itr->created;
                    cur.seller = itr->seller;
                    result.bids.push_back(cur);
                    ++sell_itr;
                }
                while (buy_itr != end &&
                       buy_itr->sell_price.base.symbol == pair.first &&
                       buy_itr->sell_price.quote.symbol == pair.second &&
                       result.asks.size() < limit) {
                    auto itr = buy_itr;
                    order_extended cur;
                    cur.orderid = itr->orderid;
                    cur.order_price = itr->sell_price;
                    cur.real_price = (~cur.order_price).to_real();
                    cur.asset1 = itr->for_sale;
                    cur.asset2 = (asset(itr->for_sale, pair.first) * cur.order_price).amount;
                    cur.created = itr->created;
                    cur.seller = itr->seller;
                    result.asks.push_back(cur);
                    ++buy_itr;
                }

                return result;
            }


            vector<market_trade> market_history_plugin::market_history_plugin_impl::get_trade_history(
                    const symbol_type_pair& pair, time_point_sec start, time_point_sec end, uint32_t limit) const {
                const auto& bucket_idx = _db.get_index<order_history_index, by_time>();
                auto itr = bucket_idx.lower_bound(start);

                std::vector<market_trade> result;

                while (itr != bucket_idx.end() && itr->time <= end &&
                       result.size() < limit) {
                    if (!(itr->op.open_pays.symbol == pair.first && itr->op.current_pays.symbol == pair.second)
                        && !(itr->op.open_pays.symbol == pair.second && itr->op.current_pays.symbol == pair.first)) {
                        ++itr;
                        continue;
                    }
                    market_trade trade;
                    trade.id = itr->id;
                    trade.date = itr->time;
                    trade.current_pays = itr->op.current_pays;
                    trade.open_pays = itr->op.open_pays;
                    result.push_back(trade);
                    ++itr;
                }

                return result;
            }

            vector<market_trade> market_history_plugin::market_history_plugin_impl::get_recent_trades(const symbol_type_pair& pair, uint32_t limit) const {
                const auto& order_idx = _db.get_index<order_history_index, by_time>();
                auto itr = order_idx.rbegin();

                vector<market_trade> result;

                while (itr != order_idx.rend() && result.size() < limit) {
                    if (!(itr->op.open_pays.symbol == pair.first && itr->op.current_pays.symbol == pair.second)
                        && !(itr->op.open_pays.symbol == pair.second && itr->op.current_pays.symbol == pair.first)) {
                        ++itr;
                        continue;
                    }
                    market_trade trade;
                    trade.id = itr->id;
                    trade.date = itr->time;
                    trade.current_pays = itr->op.current_pays;
                    trade.open_pays = itr->op.open_pays;
                    result.push_back(trade);
                    ++itr;
                }

                return result;
            }

            vector<bucket_object> market_history_plugin::market_history_plugin_impl::get_market_history(
                    const symbol_type_pair& pair, uint32_t bucket_seconds, time_point_sec start, time_point_sec end) const {
                const auto& bucket_idx = _db.get_index<bucket_index, by_bucket>();
                auto itr = bucket_idx.lower_bound(std::make_tuple(pair.first, pair.second, bucket_seconds, start));

                std::vector<bucket_object> result;

                while (itr != bucket_idx.end() &&
                       itr->sym1 == pair.first && itr->sym2 == pair.second &&
                       itr->seconds == bucket_seconds && itr->open < end) {
                    result.push_back(*itr);

                    ++itr;
                }

                return result;
            }

            flat_set<uint32_t> market_history_plugin::market_history_plugin_impl::get_market_history_buckets() const {
                return appbase::app().get_plugin<market_history_plugin>().get_tracked_buckets();
            }

            std::vector<limit_order> market_history_plugin::market_history_plugin_impl::get_open_orders(const symbol_type_pair& pair, bool pair_reversed, const string& owner, bool sort_by_price, bool sort_lesser) const {
                return _db.with_weak_read_lock([&]() {
                    std::vector<limit_order> result;
                    const auto& idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_account>();
                    auto itr = idx.lower_bound(owner);
                    while (itr != idx.end() && itr->seller == owner) {
                        bool ask = true;
                        bool bid = true;
                        if (pair.first != 0 && pair.second != 0) {
                            ask = itr->sell_price.base.symbol == pair.first && itr->sell_price.quote.symbol == pair.second;
                            bid = itr->sell_price.base.symbol == pair.second && itr->sell_price.quote.symbol == pair.first;
                        } else if (pair.first != 0) {
                            ask = itr->sell_price.base.symbol == pair.first;
                            bid = false;
                        } else if (pair.second != 0) {
                            ask = false;
                            bid = itr->sell_price.base.symbol == pair.second;
                        }
                        if (ask || bid) {
                            result.push_back(*itr);
                            auto& last = result.back();
                            last.asset1 = asset(last.for_sale, last.sell_price.base.symbol);
                            if (ask) {
                                last.real_price = (~last.sell_price).to_real();
                                last.asset2 = ~last.sell_price * last.asset1;
                            } else if (bid) {
                                last.real_price = (last.sell_price).to_real();
                                last.asset2 = last.sell_price * last.asset1;
                            }
                            if (pair_reversed) {
                                std::swap(last.asset1, last.asset2);
                            }
                        }
                        ++itr;
                    }
                    if (sort_by_price) {
                        if (sort_lesser) {
                            std::sort(result.begin(), result.end(), [&](auto& lhs, auto& rhs) {
                                return lhs.sell_price < rhs.sell_price;
                            });
                        } else {
                            std::sort(result.begin(), result.end(), [&](auto& lhs, auto& rhs) {
                                return lhs.sell_price > rhs.sell_price;
                            });
                        }
                    }
                    return result;
                });
            }

            std::vector<limit_order> market_history_plugin::market_history_plugin_impl::get_orders(const std::vector<std::string>& order_ids) const {
                return _db.with_weak_read_lock([&]() {
                    std::vector<limit_order> result;

                    const auto& idx = _db.get_index<golos::chain::limit_order_index, golos::chain::by_account>();

                    for (const auto& id : order_ids) {
                        std::vector<std::string> parts;
                        parts.reserve(2);
                        boost::split(parts, id, boost::is_any_of("|"));
                        if (parts.size() == 2) {
                            auto orderid = std::stoul(parts[1]);
                            auto itr = idx.find(std::make_tuple(parts[0], orderid));
                            if (itr != idx.end()) {
                                result.push_back(*itr);
                                continue;
                            }
                        }
                        result.emplace_back();
                    }

                    return result;
                });
            }

            void market_history_plugin::market_history_plugin_impl::subscribe_to_market(market_callback cb) {
                auto info_ptr = std::make_shared<market_callback_info>();
                active_market_callback.push_back(info_ptr);
                info_ptr->it = std::prev(active_market_callback.end());
                info_ptr->connect(_my.create_order_signal, free_market_callback, cb);
            }

            market_history_plugin::market_history_plugin() {
            }

            market_history_plugin::~market_history_plugin() {
            }

            void market_history_plugin::set_program_options(
                    boost::program_options::options_description &cli,
                    boost::program_options::options_description &cfg
            ) {
                cfg.add_options()
                        ("market-history-bucket-size",
                         boost::program_options::value<string>()->default_value("[15,60,300,3600,86400]"),
                         "Track market history by grouping orders into buckets of equal size measured in seconds specified as a JSON array of numbers")
                        ("market-history-buckets-per-size",
                         boost::program_options::value<uint32_t>()->default_value(5760),
                         "How far back in time to track history for each bucket size, measured in the number of buckets (default: 5760)");
            }

            void market_history_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
                try {
                    ilog("market_history plugin: plugin_initialize() begin");
                    _my.reset(new market_history_plugin_impl(*this));
                    golos::chain::database& db = _my->database();

                    db.post_apply_operation.connect(
                            [&](const golos::chain::operation_notification &o) {
                                _my->on_operation(o);
                            });
                    golos::chain::add_plugin_index<bucket_index>(db);
                    golos::chain::add_plugin_index<order_history_index>(db);

                    if (options.count("bucket-size")) {
                        std::string buckets = options["bucket-size"].as<string>();
                        _my->_tracked_buckets = fc::json::from_string(buckets).as<flat_set<uint32_t>>();
                    }
                    if (options.count("history-per-size")) {
                        _my->_maximum_history_per_bucket_size = options["history-per-size"].as<uint32_t>();
                    }

                    wlog("bucket-size ${b}", ("b", _my->_tracked_buckets));
                    wlog("history-per-size ${h}", ("h", _my->_maximum_history_per_bucket_size));

                    ilog("market_history plugin: plugin_initialize() end");
                    JSON_RPC_REGISTER_API ( name() ) ;
                } FC_CAPTURE_AND_RETHROW()
            }

            void market_history_plugin::plugin_startup() {
                ilog("market_history plugin: plugin_startup() begin");

                ilog("market_history plugin: plugin_startup() end");
            }

            void market_history_plugin::plugin_shutdown() {
                ilog("market_history plugin: plugin_shutdown() begin");

                ilog("market_history plugin: plugin_shutdown() end");
            }

            flat_set<uint32_t> market_history_plugin::get_tracked_buckets() const {
                return _my->_tracked_buckets;
            }

            uint32_t market_history_plugin::get_max_history_per_bucket() const {
                return _my->_maximum_history_per_bucket_size;
            }


            // Api Defines

            DEFINE_API(market_history_plugin, subscribe_to_market) {
                msg_pack_transfer transfer(args);

                auto& _db = _my->database();
                _db.with_weak_read_lock([&]{
                    _my->subscribe_to_market([this,msg = transfer.msg()](const auto& block) {
                        fc::variant r = fc::variant(block);
                        wlog(fc::json::to_string(r));
                        msg->unsafe_result(r);
                    });
                });

                transfer.complete();

                return {};
            }

            DEFINE_API(market_history_plugin, get_ticker) {
                PLUGIN_API_VALIDATE_ARGS(
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    bool reversed;
                    auto res = _my->get_ticker(_my->get_symbol_type_pair(pair, &reversed));
                    if (reversed) {
                        std::swap(res.latest1, res.latest2);

                        std::swap(res.lowest_ask, res.highest_bid);
                        _my->reverse_price(res.lowest_ask);
                        _my->reverse_price(res.highest_bid);

                        std::swap(res.percent_change1, res.percent_change2);

                        std::swap(res.asset1_volume, res.asset2_volume);
                        std::swap(res.asset1_depth, res.asset2_depth);
                    }
                    return res;
                });
            }

            DEFINE_API(market_history_plugin, get_volume) {
                PLUGIN_API_VALIDATE_ARGS(
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    bool reversed;
                    auto res = _my->get_volume(_my->get_symbol_type_pair(pair, &reversed));
                    if (reversed) {
                        std::swap(res.asset1_volume, res.asset2_volume);
                    }
                    return res;
                });
            }

            DEFINE_API(market_history_plugin, get_depth) {
                PLUGIN_API_VALIDATE_ARGS(
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    bool reversed;
                    auto res = _my->get_depth(_my->get_symbol_type_pair(pair, &reversed));
                    if (reversed) {
                        std::swap(res.asset1_depth, res.asset2_depth);
                    }
                    return res;
                });
            }

            DEFINE_API(market_history_plugin, get_order_book) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t, limit)
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 500);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    bool reversed;
                    auto res = _my->get_order_book(_my->get_symbol_type_pair(pair, &reversed), limit);
                    if (reversed) {
                        std::swap(res.bids, res.asks);
                        for (auto& order : res.bids) {
                            std::swap(order.asset1, order.asset2);
                            _my->reverse_price(order.price);
                        }
                        for (auto& order : res.asks) {
                            std::swap(order.asset1, order.asset2);
                            _my->reverse_price(order.price);
                        }
                    }
                    return res;
                });
            }

            DEFINE_API(market_history_plugin, get_order_book_extended) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t, limit)
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    bool reversed;
                    auto res = _my->get_order_book_extended(_my->get_symbol_type_pair(pair, &reversed), limit);
                    if (reversed) {
                        std::swap(res.bids, res.asks);
                        for (auto& order : res.bids) {
                            std::swap(order.asset1, order.asset2);
                            _my->reverse_price(order.real_price);
                            order.order_price = ~order.order_price;
                        }
                        for (auto& order : res.asks) {
                            std::swap(order.asset1, order.asset2);
                            _my->reverse_price(order.real_price);
                            order.order_price = ~order.order_price;
                        }
                    }
                    return res;
                });
            }


            DEFINE_API(market_history_plugin, get_trade_history) {
                PLUGIN_API_VALIDATE_ARGS(
                    (time_point_sec, start)
                    (time_point_sec, end)
                    (uint32_t,       limit)
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_trade_history(_my->get_symbol_type_pair(pair), start, end, limit);
                });
            }

            DEFINE_API(market_history_plugin, get_recent_trades) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t, limit)
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                );
                GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_recent_trades(_my->get_symbol_type_pair(pair), limit);
                });
            }

            DEFINE_API(market_history_plugin, get_market_history) {
                PLUGIN_API_VALIDATE_ARGS(
                    (uint32_t,       bucket_seconds)
                    (time_point_sec, start)
                    (time_point_sec, end)
                    (symbol_name_pair,    pair, symbol_name_pair("GOLOS", "GBG"))
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_market_history(_my->get_symbol_type_pair(pair), bucket_seconds, start, end);
                });
            }

            DEFINE_API(market_history_plugin, get_market_history_buckets) {
                PLUGIN_API_VALIDATE_ARGS();
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_market_history_buckets();
                });
            }

            DEFINE_API(market_history_plugin, get_open_orders) {
                PLUGIN_API_VALIDATE_ARGS(
                    (string, account)
                    (symbol_name_pair, pair, symbol_name_pair("GOLOS", "GBG"))
                    (bool, sort_by_price, true)
                    (bool, sort_lesser, false)
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    bool reversed;
                    auto type_pair = _my->get_symbol_type_pair(pair, &reversed, true);
                    auto res = _my->get_open_orders(type_pair, reversed, account, sort_by_price, sort_lesser);
                    if (reversed) {
                        for (auto& order : res) {
                            _my->reverse_price(order.real_price);
                            order.sell_price = ~order.sell_price;
                        }
                    }
                    return res;
                });
            }

            DEFINE_API(market_history_plugin, get_orders) {
                PLUGIN_API_VALIDATE_ARGS(
                    (std::vector<std::string>, order_ids)
                );
                auto &db = _my->database();
                return db.with_weak_read_lock([&]() {
                    return _my->get_orders(order_ids);
                });
            }

        }
    }
} // golos::plugins::market_history
