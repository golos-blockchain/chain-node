#include <golos/plugins/account_notes/account_notes_plugin.hpp>
#include <golos/plugins/account_notes/account_notes_evaluators.hpp>
#include <golos/plugins/account_notes/account_notes_objects.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <appbase/application.hpp>

#include <golos/chain/index.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>
#include <golos/chain/generic_custom_operation_interpreter.hpp>

namespace golos { namespace plugins { namespace account_notes {

namespace bpo = boost::program_options;

class account_notes_plugin::account_notes_plugin_impl final {
public:
    account_notes_plugin_impl(account_notes_plugin& plugin)
            : plugin_(plugin), _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
        // Each plugin needs its own evaluator registry.
        custom_operation_interpreter_ = std::make_shared<
            generic_custom_operation_interpreter<account_notes_plugin_operation>>(_db);

        auto coi = custom_operation_interpreter_.get();

        // Add each operation evaluator to the registry
        coi->register_evaluator<set_value_evaluator>(&settings_);

        // Add the registry to the database so the database can delegate custom ops to the plugin
        _db.set_custom_operation_interpreter(plugin.name(), custom_operation_interpreter_);
    }

    ~account_notes_plugin_impl() = default;

    key_values get_values(
        account_name_type account, std::set<std::string> keys
    ) const;

    account_notes_plugin& plugin_;

    account_notes_settings_api_object settings_;

    golos::chain::database& _db;

    std::shared_ptr<generic_custom_operation_interpreter<account_notes_plugin_operation>> custom_operation_interpreter_;
};

key_values account_notes_plugin::account_notes_plugin_impl::get_values(
    account_name_type account,
    std::set<std::string> keys
) const {
    key_values result;

    auto has_keys = !!keys.size();
    auto key_itr = keys.begin();

    const auto& idx = _db.get_index<account_note_index, by_account_key>();
    auto itr = idx.lower_bound(account);
    for (; itr != idx.end() && itr->account == account; ) {
        if (has_keys) {
            if (key_itr == keys.end() || result.size() == keys.size()) break;
        }

        const auto key = to_string(itr->key);

        if (has_keys && key != *key_itr) {
            if (key < *key_itr) {
                ++itr;
            } else {
                key_itr = keys.erase(key_itr);
            }
            continue;
        }

        result[key] = to_string(itr->value);
        if (has_keys) ++key_itr;
        ++itr;
    }

    return result;
}

account_notes_plugin::account_notes_plugin() = default;

account_notes_plugin::~account_notes_plugin() = default;

const std::string& account_notes_plugin::name() {
    static std::string name = "account_notes";
    return name;
}

void account_notes_plugin::set_program_options(
    bpo::options_description& cli,
    bpo::options_description& cfg
) {
    cfg.add_options() (
        "an-tracked-accounts",
        bpo::value<string>()->default_value("[]"),
        "Defines a count of accounts to store notes"
    ) (
        "an-untracked-accounts",
        bpo::value<string>()->default_value("[]"),
        "Defines a count of accounts to do not store notes"
    ) (
        "an-max-key-length",
        bpo::value<uint16_t>()->default_value(50),
        "Maximum length of note key"
    ) (
        "an-max-value-length",
        bpo::value<uint16_t>()->default_value(UINT16_MAX),
        "Maximum length of note value"
    ) (
        "an-max-note-count",
        bpo::value<uint16_t>()->default_value(10),
        "Maximum count of key-value notes per account"
    );
}

void account_notes_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
    ilog("Intializing account notes plugin");

    my = std::make_unique<account_notes_plugin::account_notes_plugin_impl>(*this);

    add_plugin_index<account_note_index>(my->_db);

    add_plugin_index<account_note_stats_index>(my->_db);

    auto& settings = my->settings_;

    settings.max_key_length = options.at("an-max-key-length").as<uint16_t>();

    settings.max_value_length = options.at("an-max-value-length").as<uint16_t>();

    settings.max_note_count = options.at("an-max-note-count").as<uint16_t>();

    if (options.count("an-tracked-accounts")) {
        auto tracked_accounts = options["an-tracked-accounts"].as<string>();
        settings.tracked_accounts = fc::json::from_string(tracked_accounts).as<flat_set<std::string>>();
    }

    if (options.count("an-untracked-accounts")) {
        auto untracked_accounts = options["an-untracked-accounts"].as<string>();
        settings.untracked_accounts = fc::json::from_string(untracked_accounts).as<flat_set<std::string>>();
    }

    JSON_RPC_REGISTER_API(name())
}

void account_notes_plugin::plugin_startup() {
    ilog("Starting up account notes plugin");
}

void account_notes_plugin::plugin_shutdown() {
    ilog("Shutting down account notes plugin");
}

// Api Defines

DEFINE_API(account_notes_plugin, get_values) {
    PLUGIN_API_VALIDATE_ARGS(
        (account_name_type, account)
        (std::set<std::string>,       keys, std::set<std::string>())
    )
    return my->_db.with_weak_read_lock([&]() {
        return my->get_values(account, keys);
    });
}

DEFINE_API(account_notes_plugin, get_values_settings) {
    return my->settings_;
}

} } } // golos::plugins::account_notes
