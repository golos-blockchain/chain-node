#include <golos/plugins/witness_api/plugin.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/protocol/exceptions.hpp>


namespace golos { namespace plugins { namespace witness_api {

using namespace golos::protocol;
using namespace golos::chain;

struct plugin::witness_plugin_impl {
public:
    witness_plugin_impl() : database(appbase::app().get_plugin<chain::plugin>().db()) {
    }

    ~witness_plugin_impl() = default;

    std::vector<account_name_type> get_miner_queue() const;
    std::vector<optional<witness_api_object>> get_witnesses(const std::vector<witness_object::id_type> &witness_ids) const;
    fc::optional<witness_api_object> get_witness_by_account(std::string account_name) const;
    std::vector<witness_api_object> get_witnesses_by_vote(std::string from, uint32_t limit) const;
    uint64_t get_witness_count() const;
    std::set<account_name_type> lookup_witness_accounts(const std::string &lower_bound_name, uint32_t limit) const;

    golos::chain::database& database;
};


DEFINE_API(plugin, get_current_median_history_price) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database.with_weak_read_lock([&]() {
        return my->database.get_feed_history().current_median_history;
    });
}

DEFINE_API(plugin, get_feed_history) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database.with_weak_read_lock([&]() {
        return feed_history_api_object(my->database.get_feed_history());
    });
}

std::vector<account_name_type> plugin::witness_plugin_impl::get_miner_queue() const {
    std::vector<account_name_type> result;
    const auto &pow_idx = database.get_index<witness_index>().indices().get<by_pow>();

    auto itr = pow_idx.upper_bound(0);
    while (itr != pow_idx.end()) {
        if (itr->pow_worker) {
            result.push_back(itr->owner);
        }
        ++itr;
    }
    return result;
}

DEFINE_API(plugin, get_miner_queue) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database.with_weak_read_lock([&]() {
        return my->get_miner_queue();
    });
}


DEFINE_API(plugin, get_active_witnesses) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database.with_weak_read_lock([&]() {
        const auto &wso = my->database.get_witness_schedule_object();
        size_t n = wso.current_shuffled_witnesses.size();
        vector<account_name_type> result;
        result.reserve(n);
        for (size_t i = 0; i < n; i++) {
            if (wso.current_shuffled_witnesses[i] != "") {
                result.push_back(wso.current_shuffled_witnesses[i]);
            }
        }
        return result;
    });
}

DEFINE_API(plugin, get_witness_schedule) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database.with_weak_read_lock([&]() {
        return my->database.get(witness_schedule_object::id_type());
    });
}

std::vector<optional<witness_api_object>> plugin::witness_plugin_impl::get_witnesses(
    const std::vector<witness_object::id_type> &witness_ids
) const {
    std::vector<optional<witness_api_object>> result;
    result.reserve(witness_ids.size());
    std::transform(
        witness_ids.begin(), witness_ids.end(), std::back_inserter(result),
        [&](witness_object::id_type id) -> optional<witness_api_object> {
           if (auto o = database.find(id)) {
               return witness_api_object(*o, database);
           }
           return {};
        });
    return result;
}

DEFINE_API(plugin, get_witnesses) {
    PLUGIN_API_VALIDATE_ARGS(
        (vector<witness_object::id_type>, witness_ids)
    );
    return my->database.with_weak_read_lock([&]() {
        return my->get_witnesses(witness_ids);
    });
}

DEFINE_API(plugin, get_witness_by_account) {
    PLUGIN_API_VALIDATE_ARGS(
        (string, account_name)
    );
    return my->database.with_weak_read_lock([&]() {
        return my->get_witness_by_account(account_name);
    });
}


fc::optional<witness_api_object> plugin::witness_plugin_impl::get_witness_by_account(std::string account_name) const {
    const auto& idx = database.get_index<witness_index>().indices().get<by_name>();
    auto itr = idx.find(account_name);
    if (itr != idx.end()) {
        return witness_api_object(*itr, database);
    }
    return {};
}

DEFINE_API(plugin, get_witnesses_by_vote) {
    PLUGIN_API_VALIDATE_ARGS(
        (string,   from)
        (uint32_t, limit)
    );
    return my->database.with_weak_read_lock([&]() {
        return my->get_witnesses_by_vote(from, limit);
    });
}

std::vector<witness_api_object> plugin::witness_plugin_impl::get_witnesses_by_vote(
        std::string from, uint32_t limit
) const {
    GOLOS_CHECK_LIMIT_PARAM(limit, 100);

    std::vector<witness_api_object> result;
    result.reserve(limit);

    const auto &name_idx = database.get_index<witness_index>().indices().get<by_name>();
    const auto &vote_idx = database.get_index<witness_index>().indices().get<by_vote_name>();

    auto itr = vote_idx.begin();
    if (from.size()) {
        auto nameitr = name_idx.find(from);
        GOLOS_CHECK_PARAM(from,
            GOLOS_CHECK_VALUE(nameitr != name_idx.end(), "Witness name after last witness"));
        itr = vote_idx.iterator_to(*nameitr);
    }

    while (itr != vote_idx.end() && result.size() < limit && itr->votes > 0) {
        result.emplace_back(*itr, database);
        ++itr;
    }
    return result;
}

DEFINE_API(plugin, get_witness_count) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database.with_weak_read_lock([&]() {
        return my->get_witness_count();
    });
}

uint64_t plugin::witness_plugin_impl::get_witness_count() const {
    return database.get_index<witness_index>().indices().size();
}

DEFINE_API(plugin, lookup_witness_accounts) {
    PLUGIN_API_VALIDATE_ARGS(
        (string,   lower_bound_name)
        (uint32_t, limit)
    );
    return my->database.with_weak_read_lock([&]() {
        return my->lookup_witness_accounts(lower_bound_name, limit);
    });
}

std::set<account_name_type> plugin::witness_plugin_impl::lookup_witness_accounts(
    const std::string &lower_bound_name,
    uint32_t limit
) const {
    GOLOS_CHECK_LIMIT_PARAM(limit, 1000);
    const auto &witnesses_by_id = database.get_index<witness_index>().indices().get<by_id>();

    // get all the names and look them all up, sort them, then figure out what
    // records to return.  This could be optimized, but we expect the
    // number of witnesses to be few and the frequency of calls to be rare
    std::set<account_name_type> witnesses_by_account_name;
    for (const auto& witness : witnesses_by_id) {
        if (witness.owner >= lower_bound_name) { // we can ignore anything below lower_bound_name
            witnesses_by_account_name.insert(witness.owner);
        }
    }

    auto end_iter = witnesses_by_account_name.begin();
    while (end_iter != witnesses_by_account_name.end() && limit--) {
        ++end_iter;
    }
    witnesses_by_account_name.erase(end_iter, witnesses_by_account_name.end());
    return witnesses_by_account_name;
}

void plugin::set_program_options(
    boost::program_options::options_description &cli,
    boost::program_options::options_description &cfg
) {
}

void plugin::plugin_initialize(const boost::program_options::variables_map &options) {
    ilog("witness_api plugin: plugin_initialize() begin");

    try {
        my = std::make_unique<witness_plugin_impl>();

        JSON_RPC_REGISTER_API(name());
    } FC_CAPTURE_AND_RETHROW()

    ilog("witness_api plugin: plugin_initialize() end");
}

plugin::plugin() = default;

plugin::~plugin() = default;

void plugin::plugin_startup() {
    ilog("witness_api plugin: plugin_startup() begin");

    ilog("witness_api plugin: plugin_startup() end");
}


} } } // golos::plugins::witness_api
