#include <golos/plugins/account_notes/account_notes_operations.hpp>
#include <golos/plugins/account_notes/account_notes_plugin.hpp>
#include <golos/protocol/operation_util_impl.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace golos { namespace plugins { namespace account_notes {

void set_value_operation::validate() const {
    GOLOS_CHECK_PARAM_ACCOUNT(account);
    GOLOS_CHECK_PARAM(key, GOLOS_CHECK_VALUE_LEGE(key.length(), 1, 128));

     // TODO: optimize
    auto is_global = boost::algorithm::starts_with(key, "g.");
    auto is_acs = boost::algorithm::ends_with(key, ".acs");
    auto is_lst = boost::algorithm::ends_with(key, ".lst");

    GOLOS_CHECK_PARAM(key, GOLOS_CHECK_VALUE(!is_acs || is_global, ".acs requires g."));

    auto key_length = key.length();
    if (is_global) key_length -= 2;
    if (is_acs || is_lst) key_length -= 4;
    GOLOS_CHECK_PARAM(key, GOLOS_CHECK_VALUE(key_length, "Key should not be empty"));

    fc::variant v;
    if (is_lst || is_acs) {
        try {
            v = fc::json::from_string(value);
        } catch (...) {
            GOLOS_CHECK_PARAM(value, GOLOS_CHECK_VALUE(false, "Value should be valid JSON for .lst, .acs"));
        }
    }

    if (is_lst) { // should be JSON object
        GOLOS_CHECK_PARAM(value, GOLOS_CHECK_VALUE(v.is_object(), "Value should be JSON object for .lst"));
    } else if (is_acs) { // should be array of JSON values which are valid account names
        GOLOS_CHECK_PARAM(value, GOLOS_CHECK_VALUE(v.is_array(), "Value should be JSON array for .acs"));

        if (v.size()) {
            std::vector<std::string> value_acs;
            try {
                fc::from_variant(v, value_acs);
            } catch (...) {
                GOLOS_CHECK_PARAM(value, GOLOS_CHECK_VALUE(false, "Value should be JSON array of strings for .acs"));
            }

            for (size_t i = 0; i < value_acs.size(); i++) {
                GOLOS_CHECK_PARAM_ACCOUNT(value_acs[i]);
            }
        }
    }
}

} } } // golos::plugins::account_notes

DEFINE_OPERATION_TYPE(golos::plugins::account_notes::account_notes_plugin_operation);