#include <golos/plugins/account_notes/account_notes_operations.hpp>
#include <golos/plugins/account_notes/account_notes_plugin.hpp>
#include <golos/protocol/operation_util_impl.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>

namespace golos { namespace plugins { namespace account_notes {

void set_value_operation::validate() const {
    GOLOS_CHECK_PARAM_ACCOUNT(account);
    GOLOS_CHECK_PARAM(key, GOLOS_CHECK_VALUE_LEGE(key.length(), 1, 128));
    GOLOS_CHECK_PARAM(value, GOLOS_CHECK_VALUE_LE(value.length(), 4096));
}

} } } // golos::plugins::account_notes

DEFINE_OPERATION_TYPE(golos::plugins::account_notes::account_notes_plugin_operation);