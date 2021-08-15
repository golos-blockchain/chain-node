#include <golos/plugins/event_plugin/event_plugin.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/protocol/block.hpp>
#include <golos/protocol/exceptions.hpp>
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

        _db.create<op_note_object>([&](auto& o) {
            o.trx_id = note.trx_id;
            o.block = note.block;
            o.trx_in_block = note.trx_in_block;
            o.op_in_trx = note.op_in_trx;
            o.virtual_op = note.virtual_op;
            o.timestamp = _db.head_block_time();

            const auto size = fc::raw::pack_size(note.op);
            o.serialized_op.resize(size);
            fc::datastream<char*> ds(o.serialized_op.data(), size);
            fc::raw::pack(ds, note.op);
        });
    }

    void send_event() {

    }

    void erase_old_blocks() {
        uint32_t head_block = _db.head_block_num();
        if (history_blocks <= head_block) {
            uint32_t need_block = head_block - history_blocks;
            const auto& idx = _db.get_index<op_note_index, by_location>();
            auto itr = idx.begin();
            while (itr != idx.end() && itr->block <= need_block) {
                auto next_itr = itr;
                ++next_itr;
                _db.remove(*itr);
                itr = next_itr;
            }
        }
    }

    std::vector<op_note_api_object> get_events_in_block(
        uint32_t block_num,
        bool only_virtual
    ) {
        std::vector<op_note_api_object> result;

        const auto& idx = _db.get_index<op_note_index, by_location>();
        auto itr = idx.lower_bound(block_num);
        for (; itr != idx.end() && itr->block == block_num; ++itr) {
            op_note_api_object operation(*itr);
            if (!only_virtual || operation.virtual_op != 0) {
                result.push_back(std::move(operation));
            }
        }
        return result;
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

    JSON_RPC_REGISTER_API(name())
} 

void event_plugin::plugin_startup() {
    ilog("Starting up event plugin");
}

void event_plugin::plugin_shutdown() {
    ilog("Shutting down event plugin");
}

DEFINE_API(event_plugin, get_events_in_block) {
    PLUGIN_API_VALIDATE_ARGS(
        (uint32_t, block_num)
        (bool,     only_virtual)
    );
    return my->_db.with_weak_read_lock([&](){
        return my->get_events_in_block(block_num, only_virtual);
    });
}

} } } // golos::plugins::event_plugin
