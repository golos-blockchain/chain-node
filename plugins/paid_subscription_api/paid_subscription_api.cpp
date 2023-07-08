#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/paid_subscription_api/paid_subscription_api_queries.hpp>
#include <golos/plugins/paid_subscription_api/paid_subscription_api.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace plugins { namespace paid_subscription_api {

class paid_subscription_api_plugin::paid_subscription_api_plugin_impl final {
public:
    paid_subscription_api_plugin_impl()
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    ~paid_subscription_api_plugin_impl() {
    }

    std::vector<paid_subscription_object> get_paid_subscriptions_by_author(
        const paid_subscriptions_by_author_query& query
    ) {
        std::vector<paid_subscription_object> result;

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        const auto& idx = _db.get_index<paid_subscription_index, by_author_oid>();
        auto itr = idx.lower_bound(std::make_tuple(query.author, query.from));
        for (; itr != idx.end() && result.size() < query.limit && itr->author == query.author; ++itr) {
            result.push_back(*itr);
        }

        return result;
    }

    paid_subscription_object get_paid_subscription_options(
        const paid_subscription_options_query& query
    ) {
        _db.get_account(query.author);

        const auto* pso = _db.find_paid_subscription(query.author, query.oid);

        if (pso) {
            return *pso;
        } else {
            return paid_subscription_object();
        }
    }

    std::vector<paid_subscriber_object> get_paid_subscribers(
        const paid_subscribers_query& query
    ) {
        std::vector<paid_subscriber_object> result;

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        auto push_it = [&](paid_subscriber_object psro) {
            if (query.state == paid_subscribers_state::active_only && !psro.active) return;
            if (query.state == paid_subscribers_state::inactive_only && psro.active) return;
            if (psro.executions == 0) {
                psro.prepaid_until = time_point_sec::maximum();
            } else {
                auto sec = psro.prepaid.amount.value / psro.cost.amount.value * psro.interval;
                psro.prepaid_until = sec ? time_point_sec(psro.next_payment + fc::seconds(sec)) : time_point_sec(0);
            }
            result.push_back(psro);
        };

        if (query.sort == paid_subscribers_sort::by_name) {
            const auto& idx = _db.get_index<paid_subscriber_index, by_author_oid_subscriber>();
            auto itr = idx.lower_bound(std::make_tuple(query.author, query.oid, query.from));

            for (;
                itr != idx.end() && result.size() < query.limit && itr->author == query.author && itr->oid == query.oid;
                ++itr) {
                push_it(*itr);
            }
        } else {
            const auto& idx = _db.get_index<paid_subscriber_index, by_author_oid_subscribed>();
            auto itr = idx.lower_bound(std::make_tuple(query.author, query.oid));

            if (query.from != account_name_type()) {
                for (; itr->subscriber != query.from && itr->author == query.author && itr->oid == query.oid; ++itr) {
                }
            }

            for (;
                itr != idx.end() && result.size() < query.limit && itr->author == query.author && itr->oid == query.oid;
                ++itr) {
                push_it(*itr);
            }
        }

        return result;
    }

    std::vector<paid_subscriber_object> get_paid_subscriptions(
        const paid_subscriptions_query& query
    ) {
        std::vector<paid_subscriber_object> result;

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        auto push_it = [&](paid_subscriber_object psro) {
            if (query.state == paid_subscribers_state::active_only && !psro.active) return;
            if (query.state == paid_subscribers_state::inactive_only && psro.active) return;
            if (psro.executions == 0) {
                psro.prepaid_until = time_point_sec::maximum();
            } else {
                auto sec = psro.prepaid.amount.value / psro.cost.amount.value * psro.interval;
                psro.prepaid_until = sec ? time_point_sec(psro.next_payment + fc::seconds(sec)) : time_point_sec(0);
            }
            result.push_back(psro);
        };

        if (query.sort == paid_subscriptions_sort::by_author_oid) {
            const auto& idx = _db.get_index<paid_subscriber_index, by_subscriber_author_oid>();
            auto itr = idx.lower_bound(std::make_tuple(query.subscriber, query.start_author, query.start_oid));

            for (;
                itr != idx.end() && result.size() < query.limit && itr->subscriber == query.subscriber;
                ++itr) {
                push_it(*itr);
            }
        } else {
            const auto& idx = _db.get_index<paid_subscriber_index, by_subscriber_subscribed>();
            auto itr = idx.lower_bound(std::make_tuple(query.subscriber));

            if (query.start_author != account_name_type() || !(query.start_oid == paid_subscription_id())) {
                for (; (itr->author != query.start_author || !(itr->oid == query.start_oid)) && itr->subscriber == query.subscriber; ++itr) {
                }
            }

            for (;
                itr != idx.end() && result.size() < query.limit && itr->subscriber == query.subscriber;
                ++itr) {
                push_it(*itr);
            }
        }

        return result;
    }

    paid_subscriber_object get_paid_subscribe(
        const paid_subscribe_query& query
    ) {
        _db.get_account(query.subscriber);
        _db.get_account(query.author);

        const auto* psro = _db.find_paid_subscriber(query.subscriber, query.author, query.oid);

        if (psro) {
            return *psro;
        } else {
            return paid_subscriber_object();
        }
    }

    database& _db;
};

paid_subscription_api_plugin::paid_subscription_api_plugin() = default;

paid_subscription_api_plugin::~paid_subscription_api_plugin() = default;

const std::string& paid_subscription_api_plugin::name() {
    static std::string name = "paid_subscription_api";
    return name;
}

void paid_subscription_api_plugin::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
}

void paid_subscription_api_plugin::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing paid_subscription_api plugin");

    my = std::make_unique<paid_subscription_api_plugin::paid_subscription_api_plugin_impl>();

    JSON_RPC_REGISTER_API(name())
} 

void paid_subscription_api_plugin::plugin_startup() {
    ilog("Starting up paid_subscription_api plugin");
}

void paid_subscription_api_plugin::plugin_shutdown() {
    ilog("Shutting down paid_subscription_api plugin");
}

DEFINE_API(paid_subscription_api_plugin, get_paid_subscriptions_by_author) {
    PLUGIN_API_VALIDATE_ARGS(
        (paid_subscriptions_by_author_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_paid_subscriptions_by_author(query);
    });
}

DEFINE_API(paid_subscription_api_plugin, get_paid_subscription_options) {
    PLUGIN_API_VALIDATE_ARGS(
        (paid_subscription_options_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_paid_subscription_options(query);
    });
}

DEFINE_API(paid_subscription_api_plugin, get_paid_subscribers) {
    PLUGIN_API_VALIDATE_ARGS(
        (paid_subscribers_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_paid_subscribers(query);
    });
}

DEFINE_API(paid_subscription_api_plugin, get_paid_subscriptions) {
    PLUGIN_API_VALIDATE_ARGS(
        (paid_subscriptions_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_paid_subscriptions(query);
    });
}

DEFINE_API(paid_subscription_api_plugin, get_paid_subscribe) {
    PLUGIN_API_VALIDATE_ARGS(
        (paid_subscribe_query, query)
    )
    return my->_db.with_weak_read_lock([&](){
        return my->get_paid_subscribe(query);
    });
}

} } } // golos::plugins::paid_subscription_api_plugin
