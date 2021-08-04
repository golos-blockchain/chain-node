#include <golos/plugins/event_plugin/event_object.hpp>
#include <golos/plugins/event_plugin/event_plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/protocol/block.hpp>
#include <golos/chain/operation_notification.hpp>

namespace golos { namespace plugins { namespace event_plugin {

using golos::protocol::signed_block;

class event_plugin::event_plugin_impl final {
public:
    event_plugin_impl()
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    ~event_plugin_impl() {
    }

    void on_operation(const operation_notification& note) {
        if (_db.is_generating() || _db.is_producing())
            return;

        if (_db.head_block_num() < start_block)
            return;
    }

    void send_event() {

    }

    void erase_old_blocks() {

    }

    database& _db;
    uint32_t start_block = 0;
    uint32_t history_blocks = UINT32_MAX;
};

event_plugin::event_plugin() = default;

event_plugin::~event_plugin() = default;

const std::string& event_plugin::name() {
    static std::string name = "event_plugin";
    return name;
}

void event_plugin::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
    cfg.add_options() (
        "event-start-block",
        boost::program_options::value<uint32_t>(),
        "Defines starting block from which recording events."
    ) (
        "event-blocks",
        boost::program_options::value<uint32_t>(),
        "Defines depth of history for recording events."
    );
}

void event_plugin::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing event plugin");

    my = std::make_unique<event_plugin::event_plugin_impl>();

    add_plugin_index<op_note_index>(my->_db);

    if (options.count("event-start-block")) {
        my->start_block = options.at("event-start-block").as<uint32_t>();
    }

    if (options.count("event-blocks")) {
        my->history_blocks = options.at("event-blocks").as<uint32_t>();
        my->_db.applied_block.connect([&](const signed_block& block){
            my->erase_old_blocks();
        });
    }

    my->_db.post_apply_operation.connect([&](const operation_notification& note) {
        my->on_operation(note);
    });
} 

void event_plugin::plugin_startup() {
    ilog("Starting up event plugin");
}

void event_plugin::plugin_shutdown() {
    ilog("Shutting down event plugin");
}

} } } // golos::plugins::event_plugin
