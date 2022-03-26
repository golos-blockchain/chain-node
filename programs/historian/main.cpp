#include <fc/io/json.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/variant_object.hpp>
#include <fc/stacktrace.hpp>
#include <fc/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>

#include <golos/chain/database.hpp>
#include <golos/protocol/operation_util_impl.hpp>

#include "./program_options.hpp"
#include "./progress_bar.hpp"
#include "./special_operation_visitor.hpp"

namespace bfs = boost::filesystem;
namespace bpo = boost::program_options;
using bpo::options_description;
using bpo::variables_map;
using namespace golos::chain;

int unsafe_main(int argc, char** argv);

int main(int argc, char** argv) {
    try {
        return unsafe_main(argc, argv);
    } catch (const fc::exception& e) {
        std::cout << e.to_detail_string() << "\n";
        return -1;
    }
}

int unsafe_main(int argc, char** argv) {
    auto po = get_program_options(argc, argv);

    if (!po.proceed) {
        return po.exit_code;
    }

    fc::install_stacktrace_crash_handler();

    std::cout << "Opening block_log: " << po.blocklog_file.string() << "..." << std::endl;

    golos::chain::block_log bg;
    bg.open(po.blocklog_file);

    std::cout << "Opened. Processing blocks..." << std::endl;

    auto head_block = bg.head();
    if (!head_block.valid()) {
        std::cerr << "Blocklog is empty." << std::endl;
        return -3;
    }
    auto head_block_num = head_block->block_num();

    auto start_time = fc::time_point::now();

    uint32_t start_block = std::stol(po.start);
    uint32_t end_block = std::stol(po.end);
    end_block = std::min(end_block, head_block_num);
    uint32_t block_num = start_block;

    uint64_t found = 0;
    uint64_t old_found = 0;

    progress_bar p;

    uint32_t old_pct = 0;

    auto save_found = [&](const std::string& str) {
        std::ofstream output;
        output.open(po.output_file, std::ios_base::app);
        output << str << std::endl;
        ++found;
    };

    special_operation_visitor sov(po, save_found);

    while (block_num <= end_block) {
        auto block = bg.read_block_by_num(block_num);
        if (!block) {
            break;
        }

        std::string op_name;
        for (const auto& tx : block->transactions) {
            for (const auto& op : tx.operations) {
                op_name = "";
                op.visit(fc::get_operation_name(op_name));
                if (po.search_json.size()) {
                    if (!po.search_op.size() || op_name.find(po.search_op) != std::string::npos) {
                        auto json_str = fc::json::to_string(op);
                        if (json_str.find(po.search_json) != std::string::npos) {
                            save_found(op_name + " " + json_str);
                        }
                    }
                } else if (po.search_text.size()) {
                    if (!po.search_op.size() || op_name.find(po.search_op) != std::string::npos) {
                        op.visit(sov);
                    }
                } else {
                    if (op_name.find(po.search_op) != std::string::npos) {
                        save_found(op_name + " " + fc::json::to_string(op));
                    } else {
                        op.visit(sov);
                    }
                }
            }
        }
        ++block_num;

        p.update_progress(found, &old_found, *block, end_block, &old_pct);
    }

    p.complete(*head_block);

    std::cout << "Found: " << found << std::endl;

    auto end_time = fc::time_point::now();

    std::cout << "Elapsed: " << fc::get_approximate_relative_time_string(start_time, end_time, "") << std::endl;

    return 0;
}
