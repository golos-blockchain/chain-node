#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/variant_object.hpp>
#include <fc/stacktrace.hpp>
#include <fc/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <golos/chain/database.hpp>
#include <golos/protocol/operation_util_impl.hpp>

#include "plugins.hpp"

using namespace golos::chain;

int unsafe_main(int argc, char** argv);

int main(int argc, char** argv) {
    fc::install_stacktrace_crash_handler();
    try {
        return unsafe_main(argc, argv);
    } catch (const fc::exception& e) {
        std::cout << e.to_detail_string() << "\n";
        return -1;
    }
}

size_t get_used(const database& _db) {
    auto free_size = _db.free_memory();
    auto total_size = _db.max_memory();
    auto reserved_size = _db.reserved_memory();
    return total_size - free_size - reserved_size;
}

uint64_t print_size(uint64_t size) {
    return size / (1024 * 1024);
}

void process_indexes(database& _db) {
    std::cout << "-------- Processing indexes.... ----------" << std::endl;
    for (auto it = _db.index_list_begin(), et = _db.index_list_end(); et != it; ++it) {
        auto name = (*it)->name();
        boost::replace_all(name, "golos::chain::", "");
        boost::replace_all(name, "golos::plugins::", "- ");
        
        auto before = get_used(_db);
        int64_t i = 0;
        while ((*it)->size()) {
            try {
                (*it)->remove_object(i);
            } catch (const std::exception&) {}
            i++;
        }
        auto after = get_used(_db);
        std::cout << " " << print_size(before - after);
        std::cout << " MB\t\t" << name << std::endl;
    }
}

struct op_stat {
    uint64_t total_size = 0;
    uint64_t count = 0;
    uint64_t heaviest_size = 0;
    std::string heaviest_op = "";
};

std::map<std::string, op_stat> ops_stat;

void process_op(golos::protocol::operation& op, uint64_t s) {
    std::string name;
    fc::get_operation_name g(name);
    op.visit(g);
    ops_stat[name].count++;
    ops_stat[name].total_size += s;
    if (ops_stat[name].heaviest_size < s) {
        ops_stat[name].heaviest_size = s;
        ops_stat[name].heaviest_op = fc::json::to_string(op);
    }
}

void show_ops() {
    for (const auto& op : ops_stat) {
        std::cout << print_size(op.second.total_size);
        std::cout << " MB\t\t" << op.first << " - ";
        std::cout << op.second.count << " pcs \n";
        std::cout << "heaviest is: " << op.second.heaviest_op << "\n\n";
    }
}

void process_events(database& _db) {
    using namespace golos::plugins::event_plugin;
    std::cout << "-------- Processing events.... ----------" << std::endl;
    ops_stat.clear();
    if (!_db.has_index<op_note_index>()) {
        std::cout << "Canceled - no event_plugin data found." << std::endl;
        return;
    }
    const auto& idx = _db.get_index<op_note_index>().indices();
    for (auto itr = idx.begin(); itr != idx.end(); ++itr) {
        golos::protocol::operation op;
        op = fc::raw::unpack<golos::protocol::operation>(itr->serialized_op);

        uint64_t s = itr->serialized_op.size();

        process_op(op, s);
    }
    show_ops();
}

void process_ops(database& _db) {
    using namespace golos::plugins::operation_history;
    std::cout << "-------- Processing operation history.... ----------" << std::endl;
    ops_stat.clear();
    if (!_db.has_index<operation_index>()) {
        std::cout << "Canceled - no operation_history data found." << std::endl;
        return;
    }
    const auto& idx = _db.get_index<operation_index>().indices();
    for (auto itr = idx.begin(); itr != idx.end(); ++itr) {
        golos::protocol::operation op;
        op = fc::raw::unpack<golos::protocol::operation>(itr->serialized_op);

        uint64_t s = itr->serialized_op.size();

        process_op(op, s);
    }
    show_ops();
}

void wait_pause() {
    std::cout << "\nSave the report (because next it can be over-scrolled), and press Enter to continue...";
    std::cin.get();
}

int unsafe_main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cout << "Usage is:\n";
        std::cout << "meter /path/to/folder/with/shared_memory_file\n";
        return 1;
    }

    database _db;

    fc::path p(argv[1]);

    wlog("Adding plugin indexes...");

    add_plugins(_db);

    wlog("Opening shm...");

    // _db.set_init_block_log(false);

    _db.open(p, p, STEEMIT_INIT_SUPPLY, 0, chainbase::database::read_write);

    wlog("Shm is open! Processing...");

    process_events(_db);
    wait_pause();
    process_ops(_db);
    wait_pause();
    process_indexes(_db);

    return 0;
}
