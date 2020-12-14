#pragma once

#include <golos/protocol/base.hpp>
#include <golos/chain/evaluator.hpp>

namespace golos { namespace plugins { namespace account_notes {

using golos::protocol::base_operation;
using golos::protocol::account_name_type;

struct set_value_operation : base_operation {
    account_name_type account;
    string key;
    string value;

    void validate() const;

    bool is_posting() const {
        std::string pst = "pst.";
        std::string g_pst = "g.pst.";
        if (key.size() >= pst.size()) {
            if (key.substr(0, pst.size()) == pst) return true;
        } else {
            return false;
        }
        if (key.substr(0, g_pst.size()) == g_pst) return true;
        return false;
    }

    void get_required_active_authorities(flat_set<account_name_type>& a) const {
        if (!is_posting()) a.insert(account);
    }

    void get_required_posting_authorities(flat_set<account_name_type>& a) const {
        if (is_posting()) a.insert(account);
    }
};

using account_notes_plugin_operation = fc::static_variant<set_value_operation>;

} } } // golos::plugins::account_notes

FC_REFLECT((golos::plugins::account_notes::set_value_operation), (account)(key)(value));

FC_REFLECT_TYPENAME((golos::plugins::account_notes::account_notes_plugin_operation));
DECLARE_OPERATION_TYPE(golos::plugins::account_notes::account_notes_plugin_operation)
