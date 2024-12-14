#pragma once

#include <golos/protocol/exceptions.hpp>
#include <golos/plugins/private_message/private_message_plugin.hpp>

namespace golos { namespace plugins { namespace private_message {

    struct logic_errors {
        enum types {
            cannot_send_to_yourself,
            from_and_to_memo_keys_must_be_different,
            cannot_add_contact_to_yourself,
            sender_in_ignore_list,
            recipient_ignores_messages_from_unknown_contact,
            add_unknown_contact,
            contact_has_not_changed,
            no_unread_messages,
            too_low_gp,
            already_exists,
            cannot_change_group_encrypted,
            unauthorized,
            too_many_groups,
            too_many_moders,
            group_is_public
        };
    };

} } } // golos::plugins::private_message

namespace golos {
    template<>
    inline std::string get_logic_error_namespace<golos::plugins::private_message::logic_errors::types>() {
        return golos::plugins::private_message::private_message_plugin::name();
    }
}

FC_REFLECT_ENUM(golos::plugins::private_message::logic_errors::types,
    (cannot_send_to_yourself)
    (from_and_to_memo_keys_must_be_different)
    (cannot_add_contact_to_yourself)
    (sender_in_ignore_list)
    (recipient_ignores_messages_from_unknown_contact)
    (add_unknown_contact)
    (contact_has_not_changed)
    (no_unread_messages)
    (too_low_gp)
    (already_exists)
    (cannot_change_group_encrypted)
    (unauthorized)
    (too_many_groups)
    (too_many_moders)
    (group_is_public)
);
