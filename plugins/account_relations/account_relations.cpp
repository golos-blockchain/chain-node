#include <golos/plugins/account_relations/account_relations.hpp>
#include <golos/plugins/account_relations/account_relation_api_object.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/protocol/block.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/api/operation_history_extender.hpp>
#include <golos/chain/operation_notification.hpp>

namespace golos { namespace plugins { namespace account_relations {

using golos::protocol::signed_block;
using golos::api::operation_history_extender;

class account_relations::impl final {
public:
    impl()
        : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    fc::mutable_variant_object list_account_relations(const list_relation_query& query) const;
    fc::mutable_variant_object get_account_relations(const get_relation_query& query) const;

    database& _db;
};

account_relations::account_relations() = default;

account_relations::~account_relations() = default;

const std::string& account_relations::name() {
    static std::string name = "account_relations";
    return name;
}

void account_relations::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
}

void account_relations::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing account_relations plugin");

    my = std::make_unique<account_relations::impl>();

    JSON_RPC_REGISTER_API(name())
} 

void account_relations::plugin_startup() {
    ilog("Starting up account_relations plugin");
}

void account_relations::plugin_shutdown() {
    ilog("Shutting down account_relations plugin");
}


DEFINE_API(account_relations, list_account_relations) {
    PLUGIN_API_VALIDATE_ARGS(
        (list_relation_query, query)
    );
    return my->_db.with_weak_read_lock([&]() {
        return my->list_account_relations(query);
    });
}

fc::mutable_variant_object account_relations::impl::list_account_relations(
        const list_relation_query& query) const {
    fc::mutable_variant_object result;

    GOLOS_ASSERT(query.direction == relation_direction::me_to_them,
        golos::unsupported_api_method,
        "You can get only who blocked by you"
    );

    for (auto acc : query.my_accounts) {
        std::vector<account_relation_api_object> vec;

        const auto& idx = _db.get_index<account_blocking_index, by_account>();
        auto itr = idx.lower_bound(std::make_tuple(acc, query.from));
        for (; itr != idx.end() && itr->account == acc && vec.size() < size_t(query.limit); ++itr) {
            vec.emplace_back(*itr);
        }

        result[acc] = vec;
    }

    return result;
}

DEFINE_API(account_relations, get_account_relations) {
    PLUGIN_API_VALIDATE_ARGS(
        (get_relation_query, query)
    );
    return my->_db.with_weak_read_lock([&]() {
        return my->get_account_relations(query);
    });
}

fc::mutable_variant_object account_relations::impl::get_account_relations(
        const get_relation_query& query) const {
    fc::mutable_variant_object result;

    GOLOS_CHECK_LIMIT_PARAM(query.with_accounts.size(), 500);

    const auto& idx = _db.get_index<account_blocking_index, by_account>();

    for (auto name : query.with_accounts) {
        auto account = query.my_account;
        auto blocking = name;
        if (query.direction == relation_direction::they_to_me) {
            account = name;
            blocking = query.my_account;
        }
        auto itr = idx.find(std::make_tuple(account, blocking));
        if (itr != idx.end()) {
            result[name] = account_relation_api_object(*itr);
        }
    }

    return result;
}

} } } // golos::plugins::account_relations
