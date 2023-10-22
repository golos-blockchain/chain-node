#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/nft_api/nft_api_queries.hpp>
#include <golos/plugins/nft_api/nft_api.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace plugins { namespace nft_api {

class nft_api_plugin::nft_api_plugin_impl final {
public:
    nft_api_plugin_impl()
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    ~nft_api_plugin_impl() {
    }

    template<typename SortIndex>
    std::vector<nft_collection_api_object> get_nft_collections_by_index(
        const nft_collections_query& query
    ) {
        std::vector<nft_collection_api_object> result;

        const auto& idx = _db.get_index<nft_collection_index, SortIndex>();

        auto handle = [&](const nft_collection_api_object& no) {
            if (query.creator != account_name_type() && no.creator != query.creator)
                return;
            if (query.filter_creators.size() && query.filter_creators.count(no.creator))
                return;
            if (query.filter_names.size() && query.filter_names.count(nft_name_to_string(no.name)))
                return;
            result.push_back(no);
        };

        auto itr = query.reverse_sort ? idx.end() : idx.begin();
        if (query.reverse_sort) {
            auto ritr = boost::make_reverse_iterator(itr);

            if (query.start_name.size()) {
                auto start_name = nft_name_from_string(query.start_name);
                for (; ritr != idx.rend() && ritr->name != start_name; ++ritr) {}
            }

            for (; ritr != idx.rend() && result.size() < query.limit; ++ritr) {
                handle(*ritr);
            }
        } else {
            if (query.start_name.size()) {
                auto start_name = nft_name_from_string(query.start_name);
                for (; itr != idx.end() && itr->name != start_name; ++itr) {}
            }

            for (; itr != idx.end() && result.size() < query.limit; ++itr) {
                handle(*itr);
            }
        }

        return result;
    }

    std::vector<nft_collection_api_object> get_nft_collections(
        const nft_collections_query& query
    ) {
        std::vector<nft_collection_api_object> result;

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        auto push = [&](const nft_collection_api_object& obj) {
            if (!query.filter_names.count(obj.name) && !query.filter_creators.count(obj.creator))
                result.push_back(obj);
        };

        if (query.select_names.size()) {
            for (auto& name : query.select_names) {
                const auto* nco = _db.find_nft_collection(nft_name_from_string(name));
                if (nco) {
                    push(*nco);
                } else {
                    push(nft_collection_api_object());
                }
            }
        } else {
            if (query.sort == nft_collection_sort::by_created) {
                return get_nft_collections_by_index<by_created>(query);
            } else if (query.sort == nft_collection_sort::by_last_price) {
                return get_nft_collections_by_index<by_last_price>(query);
            } else if (query.sort == nft_collection_sort::by_token_count) {
                return get_nft_collections_by_index<by_token_count>(query);
            } else if (query.sort == nft_collection_sort::by_market_depth) {
                return get_nft_collections_by_index<by_market_depth>(query);
            } else if (query.sort == nft_collection_sort::by_market_asks) {
                return get_nft_collections_by_index<by_market_asks>(query);
            } else if (query.sort == nft_collection_sort::by_market_volume) {
                return get_nft_collections_by_index<by_market_volume>(query);
            } else {
                return get_nft_collections_by_index<by_name_str>(query);
            }
        }

        return result;
    }

    bool good_collection_count(const asset_symbol_type& name, std::map<asset_symbol_type, uint32_t>& collection_count, uint32_t max_count) {
        auto& record = collection_count[name];
        if (record == max_count) {
            return false;
        }
        ++record;
        return true;
    }

    bool good_collection_count(const std::string& name, std::map<asset_symbol_type, uint32_t>& collection_count, uint32_t max_count) {
        return good_collection_count(nft_name_from_string(name), collection_count, max_count);
    }

    template<typename SortIndex>
    std::vector<nft_extended_api_object> get_nft_tokens_by_index(
        const nft_tokens_query& query
    ) {
        std::vector<nft_extended_api_object> result;

        const auto& idx = _db.get_index<nft_index, SortIndex>();

        std::map<asset_symbol_type, uint32_t> collection_count;

        auto handle = [&](nft_api_object no) {
            if (query.illformed == nft_token_illformed::ignore) {
                no.check_illformed();
            }
            if (!query.is_good(no) || !good_collection_count(no.name, collection_count, query.collection_limit)) {
                return;
            }
            nft_extended_api_object obj(no, _db, query.collections, query.orders);
            result.push_back(std::move(obj));
        };

        auto itr = query.reverse_sort ? idx.end() : idx.begin();
        if (query.reverse_sort) {
            auto ritr = boost::make_reverse_iterator(itr);

            if (query.start_token_id)
                for (; ritr != idx.rend() && ritr->token_id != query.start_token_id; ++ritr) {}

            for (; ritr != idx.rend() && result.size() < query.limit; ++ritr) {
                auto obj = nft_api_object(*ritr);
                handle(std::move(obj));
            }
        } else {
            if (query.start_token_id)
                for (; itr != idx.end() && itr->token_id != query.start_token_id; ++itr) {}

            for (; itr != idx.end() && result.size() < query.limit; ++itr) {
                auto obj = nft_api_object(*itr);
                handle(std::move(obj));
            }
        }

        return result;
    }

    std::vector<nft_extended_api_object> get_nft_tokens(
        const nft_tokens_query& query
    ) {
        std::vector<nft_extended_api_object> result;

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        std::map<asset_symbol_type, uint32_t> collection_count;

        auto push = [&](nft_extended_api_object& obj) {
            obj.fill_extensions(_db, query.collections, query.orders);
            result.push_back(std::move(obj));
        };

        if (query.select_token_ids.size()) {
            for (const auto& token_id : query.select_token_ids) {
                const auto* no = _db.find_nft(token_id);
                if (no) {
                    nft_extended_api_object obj = nft_api_object(*no);
                    if (query.illformed == nft_token_illformed::ignore) {
                        obj.check_illformed();
                    }
                    if (query.is_good(obj) && good_collection_count(obj.name, collection_count, query.collection_limit)) {
                        push(obj);
                    }
                } else {
                    nft_extended_api_object empty;
                    push(empty);
                }
            }
        } else {
            std::vector<nft_extended_api_object> unsorted;

            // slow implementation, but rich behaviour
            if (query.owner.size() ||
                    query.sort_selling() ||
                    query.illformed == nft_token_illformed::sort_down) {
                const auto& idx = _db.get_index<nft_index, by_owner>();
                auto nitr = idx.begin();
                if (query.owner.size()) {
                    nitr = idx.lower_bound(query.owner);
                }
                for (; nitr != idx.end(); ++nitr) {
                    if (query.owner.size() && nitr->owner != query.owner) {
                        break;
                    }

                    nft_extended_api_object obj = nft_api_object(*nitr);
                    if (query.illformed != nft_token_illformed::nothing) {
                        obj.check_illformed();
                    }
                    if (query.is_good(obj) &&
                        good_collection_count(nitr->name, collection_count, query.collection_limit)) {
                        if (query.sort == nft_tokens_sort::by_last_price || query.sort_selling()) {
                            obj.fill_order(_db);
                        }
                        unsorted.push_back(std::move(obj));
                    }
                }

                auto compare_illformeds = [&](nft_extended_api_object& lhs, nft_extended_api_object& rhs) -> fc::optional<bool> {
                    if (!lhs.illformed && rhs.illformed) return true;
                    if (lhs.illformed && !rhs.illformed) return false;

                    return fc::optional<bool>();
                };

                auto compare_sellings = [&](nft_extended_api_object& lhs, nft_extended_api_object& rhs) -> fc::optional<bool> {
                    auto lpr = lhs.current_price_real();
                    auto rpr = rhs.current_price_real();

                    if (query.selling_sorting == nft_token_selling::sort_up_by_price) {
                        if (query.reverse_sort) {
                            if (lpr || rpr) return lpr < rpr;
                            return fc::optional<bool>();
                        }

                       if (lpr || rpr) return lpr > rpr;
                    } else if (query.selling_sorting == nft_token_selling::sort_up) {
                        if (lpr && !rpr) return true;
                        if (!lpr && rpr) return false;
                    }

                    return fc::optional<bool>();
                };

                auto compare_sortings = [&](nft_extended_api_object& lhs, nft_extended_api_object& rhs) -> fc::optional<bool> {
                    if (query.sorting_priority == nft_token_sorting_priority::illformed) {
                        auto ic = compare_illformeds(lhs, rhs);
                        if (!!ic) return *ic;

                        auto sc = compare_sellings(lhs, rhs);
                        if (!!sc) return *sc;
                    } else {
                        auto sc = compare_sellings(lhs, rhs);
                        if (!!sc) return *sc;

                        auto ic = compare_illformeds(lhs, rhs);
                        if (!!ic) return *ic;
                    }

                    return fc::optional<bool>();
                };

                if (query.sort == nft_tokens_sort::by_name)
                    std::sort(unsorted.begin(), unsorted.end(), [&](auto& lhs, auto& rhs) {
                        auto sc = compare_sortings(lhs, rhs);
                        if (!!sc) return *sc;

                        if (query.reverse_sort) return lhs.name > rhs.name;
                        return lhs.name < rhs.name;
                    });
                else if (query.sort == nft_tokens_sort::by_last_price)
                    std::sort(unsorted.begin(), unsorted.end(), [&](auto& lhs, auto& rhs) {
                        auto sc = compare_sortings(lhs, rhs);
                        if (!!sc) return *sc;

                        if (query.reverse_sort) return lhs.price_real() < rhs.price_real();
                        return lhs.price_real() > rhs.price_real();
                    });
                else if (query.sort == nft_tokens_sort::by_last_update)
                    std::sort(unsorted.begin(), unsorted.end(), [&](auto& lhs, auto& rhs) {
                        auto sc = compare_sortings(lhs, rhs);
                        if (!!sc) return *sc;

                        if (query.reverse_sort) return lhs.last_update < rhs.last_update;
                        return lhs.last_update > rhs.last_update;
                    });
                else
                    std::sort(unsorted.begin(), unsorted.end(), [&](auto& lhs, auto& rhs) {
                        auto sc = compare_sortings(lhs, rhs);
                        if (!!sc) return *sc;

                        if (query.reverse_sort) return lhs.issued < rhs.issued;
                        return lhs.issued > rhs.issued;
                    });

                auto itr = unsorted.begin();

                if (query.start_token_id)
                    for (; itr != unsorted.end(); ++itr) {
                        if (itr->token_id == query.start_token_id) {
                            break;
                        }
                    }

                for (; itr != unsorted.end() && result.size() < query.limit; ++itr) {
                    push(*itr);
                }
            } else { // fast, but poor :)
                if (query.sort == nft_tokens_sort::by_name) {
                    return get_nft_tokens_by_index<by_asset_str_owner>(query);
                } else if (query.sort == nft_tokens_sort::by_issued) {
                    return get_nft_tokens_by_index<by_issued>(query);
                } else if (query.sort == nft_tokens_sort::by_last_price) {
                    return get_nft_tokens_by_index<by_last_price>(query);
                } else {
                    return get_nft_tokens_by_index<by_last_update>(query);
                }
            }
        }

        return result;
    }

    template<typename SortIndex>
    std::vector<nft_order_api_object> get_nft_orders_by_index(
        const nft_orders_query& query
    ) {
        std::vector<nft_order_api_object> result;

        const auto& idx = _db.get_index<nft_order_index, SortIndex>();

        std::map<asset_symbol_type, uint32_t> collection_count;

        auto handle = [&](const nft_order_api_object& no) {
            if (!query.is_good(no) || !good_collection_count(no.name, collection_count, query.collection_limit)) {
                return;
            }
            nft_order_api_object obj = std::move(no);
            if (query.tokens) {
                const auto& no = _db.get_nft(obj.token_id);
                obj.token = no;
            }
            result.push_back(std::move(obj));
        };

        auto itr = query.reverse_sort ? idx.end() : idx.begin();
        if (query.reverse_sort) {
            auto ritr = boost::make_reverse_iterator(itr);

            for (; ritr != idx.rend() && (query.start_order_id && ritr->order_id != query.start_order_id); ++ritr) {}

            for (; ritr != idx.rend() && result.size() < query.limit; ++ritr) {
                handle(*ritr);
            }
        } else {
            for (; itr != idx.end() && (query.start_order_id && itr->order_id != query.start_order_id); ++itr) {}

            for (; itr != idx.end() && result.size() < query.limit; ++itr) {
                handle(*itr);
            }
        }

        return result;
    }

    std::vector<nft_order_api_object> get_nft_orders(
        const nft_orders_query& query
    ) {
        std::vector<nft_order_api_object> result;

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        std::vector<nft_order_api_object> unsorted;

        std::map<asset_symbol_type, uint32_t> collection_count;

        if (query.owner != account_name_type()) {
            const auto& idx = _db.get_index<nft_order_index, by_owner_order_id>();
            for (auto itr = idx.lower_bound(query.owner); itr != idx.end() && itr->owner == query.owner; ++itr) {
                nft_order_api_object obj = *itr;
                if (query.is_good(obj) && good_collection_count(itr->name, collection_count, query.collection_limit))
                    unsorted.push_back(std::move(obj));
            }

            if (query.sort == nft_orders_sort::by_price)
                std::sort(unsorted.begin(), unsorted.end(), [&](auto& lhs, auto& rhs) {
                    if (query.reverse_sort) return lhs.price_real() < rhs.price_real();
                    return lhs.price_real() > rhs.price_real();
                });
            else
                std::sort(unsorted.begin(), unsorted.end(), [&](auto& lhs, auto& rhs) {
                    if (query.reverse_sort) return lhs.created < rhs.created;
                    return lhs.created > rhs.created;
                });

            auto itr = unsorted.begin();

            if (query.start_order_id)
                for (; itr != unsorted.end(); ++itr) {
                    if (itr->order_id == query.start_order_id) {
                        break;
                    }
                }

            for (; itr != unsorted.end() && result.size() < query.limit; ++itr) {
                nft_order_api_object obj = *itr;
                if (query.tokens) {
                    const auto& no = _db.get_nft(obj.token_id);
                    obj.token = no;
                }
                result.push_back(std::move(obj));
            }
        } else {
            if (query.sort == nft_orders_sort::by_price) {
                return get_nft_orders_by_index<by_price>(query);
            } else {
                return get_nft_orders_by_index<by_created>(query);
            }
        }

        return result;
    }

    database& _db;
};

nft_api_plugin::nft_api_plugin() = default;

nft_api_plugin::~nft_api_plugin() = default;

const std::string& nft_api_plugin::name() {
    static std::string name = "nft_api";
    return name;
}

void nft_api_plugin::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
}

void nft_api_plugin::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing nft_api plugin");

    my = std::make_unique<nft_api_plugin::nft_api_plugin_impl>();

    JSON_RPC_REGISTER_API(name())
} 

void nft_api_plugin::plugin_startup() {
    ilog("Starting up nft_api plugin");
}

void nft_api_plugin::plugin_shutdown() {
    ilog("Shutting down nft_api plugin");
}

DEFINE_API(nft_api_plugin, get_nft_collections) {
    PLUGIN_API_VALIDATE_ARGS(
        (nft_collections_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_nft_collections(query);
    });
}

DEFINE_API(nft_api_plugin, get_nft_tokens) {
    PLUGIN_API_VALIDATE_ARGS(
        (nft_tokens_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_nft_tokens(query);
    });
}

DEFINE_API(nft_api_plugin, get_nft_orders) {
    PLUGIN_API_VALIDATE_ARGS(
        (nft_orders_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_nft_orders(query);
    });
}

} } } // golos::plugins::nft_api
