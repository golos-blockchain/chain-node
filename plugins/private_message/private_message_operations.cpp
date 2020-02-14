#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/private_message/private_message_exceptions.hpp>
#include <golos/protocol/operation_util_impl.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>
#include <fc/io/json.hpp>

namespace golos { namespace plugins { namespace private_message {

    static inline bool is_valid_contact_type(private_contact_type type) {
        switch(type) {
            case unknown:
            case pinned:
            case ignored:
                return true;

            default:
                break;
        }
        return false;
    }


    void private_settings_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(owner);
    }
    void private_settings_operation::get_required_posting_authorities(flat_set<account_name_type>& a) const {
        a.insert(owner);
    }


    void private_message_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(to);
        GOLOS_CHECK_PARAM_ACCOUNT(from);

        GOLOS_CHECK_LOGIC(from != to,
            logic_errors::cannot_send_to_yourself,
            "You cannot write to yourself");

        GOLOS_CHECK_PARAM(to_memo_key, {
            GOLOS_CHECK_VALUE(to_memo_key != public_key_type(), "`to_key` can't be empty");
        });

        GOLOS_CHECK_PARAM(from_memo_key, {
            GOLOS_CHECK_VALUE(from_memo_key != public_key_type(), "`from_key` can't be empty");
        });

        GOLOS_CHECK_LOGIC(
            from_memo_key != to_memo_key,
            logic_errors::from_and_to_memo_keys_must_be_different,
            "`from_key` can't be equal to `to_key`");

        GOLOS_CHECK_PARAM(nonce, {
            GOLOS_CHECK_VALUE(nonce != 0, "Nonce can't be zero");
        });

        GOLOS_CHECK_PARAM(encrypted_message, {
            GOLOS_CHECK_VALUE(encrypted_message.size() >= 16, "Encrypted message is too small");
        });
    }

    void private_message_operation::get_required_posting_authorities(flat_set<account_name_type>& a) const {
        a.insert(from);
    }


    void private_delete_message_operation::validate() const {
        GOLOS_CHECK_PARAM(from, {
            if (from.size()) {
                validate_account_name(from);
            }
        });

        GOLOS_CHECK_PARAM(to, {
            if (to.size()) {
                validate_account_name(to);
                GOLOS_CHECK_VALUE(to != from, "You cannot delete messages to yourself");
            }
        });

        GOLOS_CHECK_PARAM(requester, {
            validate_account_name(requester);
            if (to.size() || from.size()) {
                GOLOS_CHECK_VALUE(requester == to || requester == from,
                    "`requester` can delete messages only from his inbox/outbox");
            }
        });

        GOLOS_CHECK_PARAM(start_date, {
            GOLOS_CHECK_VALUE(start_date <= stop_date, "`start_date` can't be greater then to_time");
        });

        GOLOS_CHECK_PARAM(nonce, {
            if (nonce != 0) {
                GOLOS_CHECK_VALUE(to.size(), "to and nonce should be set both");
                GOLOS_CHECK_VALUE(start_date == time_point_sec::min(), "nonce and start_date can't be used together");
                GOLOS_CHECK_VALUE(stop_date == time_point_sec::min(), "nonce and stop_date can't be used together");
            }
        });
    }

    void private_delete_message_operation::get_required_posting_authorities(flat_set<account_name_type>& a) const {
        a.insert(requester);
    }


    void private_mark_message_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(to);

        GOLOS_CHECK_PARAM(from, {
            if (from.size()) {
                validate_account_name(from);
                GOLOS_CHECK_VALUE(to != from, "You cannot mark messages to yourself");
            }
        });

        GOLOS_CHECK_PARAM(start_date, {
            GOLOS_CHECK_VALUE(start_date <= stop_date, "`start_date` can't be greater then `stop_date`");
        });

        GOLOS_CHECK_PARAM(nonce, {
            if (nonce != 0) {
                GOLOS_CHECK_VALUE(to.size(), "Non-zero 'nonce' requires 'to' to be set too");
                GOLOS_CHECK_VALUE(start_date == time_point_sec::min(), "Non-zero `nonce` can't be used with `start_date`");
                GOLOS_CHECK_VALUE(stop_date == time_point_sec::min(), "Non-zero `nonce` can't be used with `stop_date`");
            }
        });
    }

    void private_mark_message_operation::get_required_posting_authorities(flat_set<account_name_type>& a) const {
        a.insert(to);
    }


    void private_contact_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(contact);
        GOLOS_CHECK_PARAM_ACCOUNT(owner);

        GOLOS_CHECK_LOGIC(contact != owner,
            logic_errors::cannot_add_contact_to_yourself,
            "You add contact to yourself");

        GOLOS_CHECK_PARAM(type, {
            GOLOS_CHECK_VALUE(is_valid_contact_type(type), "Unknown contact type");
        });

        if (json_metadata.size() > 0) {
            GOLOS_CHECK_PARAM(json_metadata, {
                GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
                GOLOS_CHECK_VALUE(type != unknown, "JSON Metadata can't be set for undefined contact");
            });
        }
    }

    void private_contact_operation::get_required_posting_authorities(flat_set<account_name_type>& a) const {
        a.insert(owner);
    }

} } } // golos::plugins::private_message

DEFINE_OPERATION_TYPE(golos::plugins::private_message::private_message_plugin_operation);