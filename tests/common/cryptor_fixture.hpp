#include <golos/plugins/cryptor/cryptor.hpp>
#include "database_fixture.hpp"

using namespace golos::plugins::cryptor;
using golos::plugins::json_rpc::msg_pack;

struct cryptor_fixture : public clean_database_fixture_wrap {
    cryptor_fixture() : clean_database_fixture_wrap(true, [&]() {
        initialize();
    }) {
        c_plugin = &(appbase::app().register_plugin<cryptor>());

        std::vector<const char*> args;
        args.push_back("golosd");
        args.push_back("--cryptor-key");
        args.push_back("1234567890123456");

        bpo::options_description desc;
        c_plugin->set_program_options(desc, desc);

        bpo::variables_map options;
        bpo::store(parse_command_line(args.size(), (char**)(args.data()), desc), options);
        bpo::notify(options);

        c_plugin->plugin_initialize(options);

        open_database();
        startup();
        c_plugin->plugin_startup();
    }

    encrypted_api_object encrypt_body(const encrypt_query& eq) {
        msg_pack mp;
        mp.args = std::vector<fc::variant>({fc::variant(eq)});
        return c_plugin->encrypt_body(mp);
    }

    decrypted_api_object decrypt_comments(const decrypt_query& dq) {
        msg_pack mp;
        mp.args = std::vector<fc::variant>({fc::variant(dq)});
        return c_plugin->decrypt_comments(mp);
    }

    cryptor* c_plugin = nullptr;
};
