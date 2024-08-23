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

    struct group_extension_validate_visitor {
        group_extension_validate_visitor(std::string& _group, account_name_type& _requester)
            : group(_group), requester(_requester) {
        }

        using result_type = void;

        std::string& group;
        account_name_type& requester;

        void operator()(const private_group_options& pgo) const {
            pgo.validate();
            group = pgo.group;
            requester = pgo.requester;
        }
    };

    void private_group_options::validate() const {
        validate_private_group_name(group);
        if (requester != account_name_type()) {
            GOLOS_CHECK_PARAM_ACCOUNT(requester);
        }
    }

    void private_message_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(from);

        std::string group;
        account_name_type requester;
        for (auto& e : extensions) {
            e.visit(group_extension_validate_visitor(group, requester));
        }
        bool is_group = group.size();

        if (requester != account_name_type()) {
            GOLOS_CHECK_PARAM(update, {
                GOLOS_CHECK_VALUE(update, "Moderator can only edit messages, not send them as another user.");
            });
        }

        if (!is_group) {
            GOLOS_CHECK_PARAM(to, {
                GOLOS_CHECK_VALUE(to != account_name_type(), "`to` can't be empty");
            });
        }
        if (to.size()) {
            GOLOS_CHECK_PARAM_ACCOUNT(to);
        }

        GOLOS_CHECK_LOGIC(from != to,
            logic_errors::cannot_send_to_yourself,
            "You cannot write to yourself");

        if (!is_group) {
            // memo_keys should be GLS1111111111111111111111111111111114T1Anm

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
        } else {
            GOLOS_CHECK_PARAM(to_memo_key, {
                GOLOS_CHECK_VALUE(to_memo_key == public_key_type(), "`to_key` should be empty, it is only for private chats, not groups");
            });

            GOLOS_CHECK_PARAM(from_memo_key, {
                GOLOS_CHECK_VALUE(from_memo_key == public_key_type(), "`from_key` should be empty, it is only for private chats, not groups");
            });
        }

        GOLOS_CHECK_PARAM(nonce, {
            GOLOS_CHECK_VALUE(nonce != 0, "`nonce` can't be zero");
        });

        GOLOS_CHECK_PARAM(encrypted_message, {
            GOLOS_CHECK_VALUE(encrypted_message.size() >= 16, "Encrypted message is too small");
        });
    }

    struct delete_extension_validate_visitor : group_extension_validate_visitor {
        delete_extension_validate_visitor(std::string& _group, account_name_type& _requester, fc::optional<bool>& _delete_contact)
            : group_extension_validate_visitor(_group, _requester), delete_contact(_delete_contact) {
        }

        fc::optional<bool>& delete_contact;

        using group_extension_validate_visitor::operator();

        void operator()(const delete_options& dop) const {
            delete_contact = dop.delete_contact;
        }
    };

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

        std::string group;
        account_name_type ext_requester;
        fc::optional<bool> delete_contact;
        for (auto& e : extensions) {
            e.visit(delete_extension_validate_visitor(group, ext_requester, delete_contact));
        }
        if (!delete_contact.valid()) {
            delete_contact = group.size() ? false : true;
        }
        GOLOS_CHECK_PARAM(extensions, {
            GOLOS_CHECK_VALUE(!ext_requester.size(),
                "For delete operations, use `requester` only from operation, not extension.");
            if (group.size()) {
                GOLOS_CHECK_VALUE(!(*delete_contact), "Cannot delete contact of group.");
            }
        });

        GOLOS_CHECK_PARAM(requester, {
            validate_account_name(requester);
            if (to.size() || from.size()) {
                GOLOS_CHECK_VALUE(group.size() || requester == to || requester == from,
                    "`requester` can delete messages only from his inbox/outbox");
            }
        });

        GOLOS_CHECK_PARAM(start_date, {
            GOLOS_CHECK_VALUE(start_date <= stop_date, "`start_date` can't be greater then `stop_date`");
        });

        GOLOS_CHECK_PARAM(nonce, {
            if (nonce != 0) {
                GOLOS_CHECK_VALUE(from.size(), "Non-zero `nonce` requires `from` to be set too");
                GOLOS_CHECK_VALUE(group.size() || to.size(), "Non-zero `nonce` requires `to`/`group` to be set too");
                GOLOS_CHECK_VALUE(start_date == time_point_sec::min(), "Non-zero `nonce` can't be used with `start_date`");
                GOLOS_CHECK_VALUE(stop_date == time_point_sec::min(), "Non-zero `nonce` can't be used with `stop_date`");
            }
        });
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
                GOLOS_CHECK_VALUE(from.size(), "Non-zero `nonce` requires `from` to be set too");
                GOLOS_CHECK_VALUE(start_date == time_point_sec::min(), "Non-zero `nonce` can't be used with `start_date`");
                GOLOS_CHECK_VALUE(stop_date == time_point_sec::min(), "Non-zero `nonce` can't be used with `stop_date`");
            }
        });
    }

    void ignore_messages_from_unknown_contact::validate() const {
    }

    struct private_setting_validate_visitor {
        private_setting_validate_visitor() {
        }

        using result_type = void;

        template <typename Setting>
        void operator()(const Setting& p) const {
            p.validate();
        }
    };

    void private_settings_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(owner);
        GOLOS_CHECK_VALUE_NOT_EMPTY(settings);

        private_setting_validate_visitor visitor;
        for (const auto& s : settings) {
            s.visit(visitor);
        }
    }

    void private_contact_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(contact);
        GOLOS_CHECK_PARAM_ACCOUNT(owner);

        GOLOS_CHECK_LOGIC(contact != owner,
            logic_errors::cannot_add_contact_to_yourself,
            "You cannot add contact to yourself");

        GOLOS_CHECK_PARAM(type, {
            GOLOS_CHECK_VALUE(is_valid_contact_type(type), "Unknown contact type");
        });

        if (json_metadata.size() > 0) {
            GOLOS_CHECK_PARAM(json_metadata, {
                GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
                GOLOS_CHECK_VALUE(type != unknown, "JSON Metadata can't be set for unknown contact");
            });
        }
    }

    void validate_private_group_name(const std::string& name) {
        GOLOS_CHECK_VALUE(name.size(), "Private group name should not be empty");
        GOLOS_CHECK_VALUE(name.size() <= 32, "Private group name should not be longer than 32 bytes");
        for (size_t i = 0; i < name.size(); ++i) {
            const auto& c = name[i];
            bool is_alpha = c >= 'a' && c <= 'z';
            bool is_digit = c >= '0' && c <= '9';
            bool is_dash = c == '-';
            bool is_ul = c == '_';
            if (i == 0) {
                GOLOS_CHECK_VALUE(is_alpha || is_digit,
                    "Private group name can start only from a-z symbol, or 0-9 digit.");
            } else {
                GOLOS_CHECK_VALUE(is_alpha || is_digit || is_dash || is_ul,
                    "Private group name can contain only a-z, 0-9 symbols, - and _.");
            }
        }
    }

    void private_group_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(creator);
        GOLOS_CHECK_PARAM(name, {
            validate_private_group_name(name);
        });
        if (json_metadata.size() > 0) {
            GOLOS_CHECK_PARAM(json_metadata, {
                GOLOS_CHECK_VALUE_MAX_SIZE(json_metadata, 512);
                GOLOS_CHECK_VALUE_UTF8(json_metadata);
                GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            });
        }
        GOLOS_CHECK_PARAM(privacy, {
            GOLOS_CHECK_VALUE(privacy != private_group_privacy::_size, "Wrong privacy value.");
        });
        if (privacy == private_group_privacy::private_group) {
            GOLOS_CHECK_PARAM(is_encrypted, {
                GOLOS_CHECK_VALUE(is_encrypted, "Private group cannot be not encrypted.");
            });
        }
    }

    void private_group_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(owner);
        GOLOS_CHECK_PARAM(name, {
            validate_private_group_name(name);
        });
    }

    void private_group_member_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(requester);
        GOLOS_CHECK_PARAM(name, {
            validate_private_group_name(name);
        });
        GOLOS_CHECK_PARAM_ACCOUNT(member);
        GOLOS_CHECK_PARAM(member_type, {
            GOLOS_CHECK_VALUE(member_type != private_group_member_type::_size, "Wrong member type.");

            if (member == requester) {
                GOLOS_CHECK_VALUE(member_type != private_group_member_type::banned, "You cannot ban yourself.");
                GOLOS_CHECK_VALUE(member_type != private_group_member_type::moder, "You cannot make moder yourself.");
            }
        });
        if (json_metadata.size() > 0) {
            GOLOS_CHECK_PARAM(json_metadata, {
                GOLOS_CHECK_VALUE_MAX_SIZE(json_metadata, 512);
                GOLOS_CHECK_VALUE_UTF8(json_metadata);
                GOLOS_CHECK_VALUE(fc::json::is_valid(json_metadata), "JSON Metadata not valid JSON");
            });
        }
    }
} } } // golos::plugins::private_message

DEFINE_OPERATION_TYPE(golos::plugins::private_message::private_message_plugin_operation);
