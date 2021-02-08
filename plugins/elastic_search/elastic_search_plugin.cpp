#include <golos/plugins/elastic_search/elastic_search_plugin.hpp>
#include <golos/plugins/elastic_search/elastic_search_state.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/protocol/block.hpp>
#include <golos/chain/operation_notification.hpp>

namespace golos { namespace plugins { namespace elastic_search {

using golos::protocol::signed_block;

class elastic_search_plugin::elastic_search_plugin_impl final {
public:
    elastic_search_plugin_impl()
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    ~elastic_search_plugin_impl() {
    }

    void on_operation(const operation_notification& note) {
        if (!_db.is_generating() && !_db.is_producing()) {
            note.op.visit(elastic_search_state_writer(_db));
        }
    }

    database& _db;
};

elastic_search_plugin::elastic_search_plugin() = default;

elastic_search_plugin::~elastic_search_plugin() = default;

const std::string& elastic_search_plugin::name() {
    static std::string name = "elastic_search";
    return name;
}

void elastic_search_plugin::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
}

void elastic_search_plugin::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing elastic search plugin");

    my = std::make_unique<elastic_search_plugin::elastic_search_plugin_impl>();

    my->_db.post_apply_operation.connect([&](const operation_notification& note) {
        my->on_operation(note);
    });
} 

void elastic_search_plugin::plugin_startup() {
    ilog("Starting up elastic search plugin");
}

void elastic_search_plugin::plugin_shutdown() {
    ilog("Shutting down elastic search plugin");
}

} } } // golos::plugins::elastic_search
