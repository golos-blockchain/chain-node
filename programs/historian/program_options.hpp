#pragma once

#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>

namespace bfs = boost::filesystem;
namespace bpo = boost::program_options;
using bpo::options_description;
using bpo::variables_map;

struct program_options {
    bfs::path blocklog_file;
    std::string search_op;
    std::string search_text;
    std::string search_json;
    std::string output_file;
    std::string start;
    std::string end;
    int exit_code = 0;
    bool proceed = false;
};

program_options get_program_options(int argc, char** argv) {
    program_options po;

    options_description cli("historian allows you to check if some operation present in block_log.\n"
        "\n"
        "It requires block_log file. block_log.index is optional.\n"
        "\n"
        "Example of usage:\n"
        "historian -b /home/block_log -o private_message\n"
        "\n"
        "Command line options");

    cli.add_options()
        ("blocklog,b", bpo::value<bfs::path>(&po.blocklog_file), "Path to block_log.")
        ("operation,o", bpo::value<std::string>(&po.search_op), "Name of searching operation.")
        ("text,t", bpo::value<std::string>(&po.search_text), "Search compromised text. Can be used with -o or without.")
        ("json,j", bpo::value<std::string>(&po.search_json), "Search raw operation JSON. Examples: '\"author\":\"lex\"' or '\\\"follower\\\":\\\"lex' Can be used with -o or without.")
        ("log,l", bpo::value<std::string>(&po.output_file)->default_value("log.txt"), "Name of output file. Default is log.txt")
        ("start,s", bpo::value<std::string>(&po.start)->default_value("2"), "Start block number.")
        ("end,e", bpo::value<std::string>(&po.end)->default_value("4294967295"), "End block number.")
        ("help,h", "Print this help message and exit.")
        ;

    variables_map vmap;
    bpo::store(bpo::parse_command_line(argc, argv, cli), vmap);
    bpo::notify(vmap);
    if (vmap.count("help") > 0 || vmap.count("blocklog") == 0 ||
        (vmap.count("operation") == 0 && vmap.count("text") == 0 && vmap.count("json") == 0)) {
        cli.print(std::cerr);
        return po;
    }

    if (!bfs::exists(po.blocklog_file)) {
        std::cerr << po.blocklog_file.string() << " not exists." << std::endl;
        po.exit_code = -1;
        return po;
    }

    if (!bfs::is_regular_file(po.blocklog_file)) {
        std::cerr << po.blocklog_file.string() << " is not a file, it is a directory or something another." << std::endl;
        po.exit_code = -2;
        return po;
    }

    if (bfs::exists(po.output_file)) {
        std::cerr << "Log file " << po.output_file << " already exists." << std::endl;
        po.exit_code = -3;
        return po;
    }

    po.proceed = true;
    return po;
}