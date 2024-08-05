#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/base.hpp>
#include <golos/chain/database.hpp>

#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/private_message/private_message_queries.hpp>

namespace golos { namespace plugins { namespace private_message {

    using namespace golos::protocol;

    struct message_api_object {
        message_api_object(const message_object& o) 
            : from(o.from), to(o.to),
            nonce(o.nonce),
            from_memo_key(o.from_memo_key), to_memo_key(o.to_memo_key),
            checksum(o.checksum),
            encrypted_message(o.encrypted_message.begin(), o.encrypted_message.end()),
            create_date(std::max(o.inbox_create_date, o.outbox_create_date)),
            receive_date(o.receive_date),
            read_date(o.read_date), remove_date(o.remove_date),
            donates(o.donates), donates_uia(o.donates_uia) {
        }

        message_api_object() = default;

        account_name_type from;
        account_name_type to;
        uint64_t nonce = 0;
        public_key_type from_memo_key;
        public_key_type to_memo_key;
        uint32_t checksum = 0;
        std::vector<char> encrypted_message;

        time_point_sec create_date;
        time_point_sec receive_date;
        time_point_sec read_date;
        time_point_sec remove_date;

        asset donates = asset(0, STEEM_SYMBOL);
        share_type donates_uia = 0;
    };

    struct settings_api_object final {
        settings_api_object(const settings_object& o)
            : ignore_messages_from_unknown_contact(o.ignore_messages_from_unknown_contact) {
        }

        settings_api_object() = default;

        bool ignore_messages_from_unknown_contact = false;
    };

    /**
     * Contact item
     */
    struct contact_api_object final {
        contact_api_object(const contact_object& o)
            : owner(o.owner), contact(o.contact.begin(), o.contact.end()),
            kind(o.kind),
            json_metadata(o.json_metadata.begin(), o.json_metadata.end()),
            local_type(o.type),
            size(o.size) {
            trim_dog(contact);
        }

        contact_api_object() = default;

        account_name_type owner;
        std::string contact;
        contact_kind kind = contact_kind::account;
        std::string json_metadata;
        private_contact_type local_type = unknown;
        private_contact_type remote_type = unknown;
        contact_size_info size;
        message_api_object last_message;
    };

    /**
     * Counters for account contact lists
     */
    struct contacts_size_api_object {
        fc::flat_map<private_contact_type, contacts_size_info> size;
    };

    /**
     * Private group member item
     */
    struct private_group_member_api_object final {
        private_group_member_api_object(const private_group_member_object& o)
            : group(o.group.begin(), o.group.end()), account(o.account),
            json_metadata(o.json_metadata.begin(), o.json_metadata.end()),
            member_type(o.member_type), invited(o.invited),
            joined(o.joined), updated(o.updated) {
        }

        private_group_member_api_object() = default;

        std::string group;
        account_name_type account;
        std::string json_metadata;
        private_group_member_type member_type = private_group_member_type::member;
        account_name_type invited;
        time_point_sec joined;
        time_point_sec updated;
    };

    /**
     * Private group item
     */
    struct private_group_api_object final {
        private_group_api_object(const private_group_object& o, const golos::chain::database& _db,
            const fc::optional<private_group_members>& pgms)
            : owner(o.owner), name(o.name.begin(), o.name.end()),
            json_metadata(o.json_metadata.begin(), o.json_metadata.end()),
            is_encrypted(o.is_encrypted), privacy(o.privacy), created(o.created),
            moders(o.moders), members(o.members), pendings(o.pendings), banneds(o.banneds) {
            if (!!pgms) {
                const auto& idx = _db.get_index<private_group_member_index, by_group_account>();
                if (pgms->accounts.size()) {
                    for (const auto& acc : pgms->accounts) {
                        auto itr = idx.find(std::make_tuple(o.name, acc));
                        if (itr == idx.end()) {
                            member_list.emplace_back();
                            continue;
                        }
                        member_list.emplace_back(*itr);
                    }
                } else { // Can also support different sorts
                    auto itr = idx.lower_bound(o.name);
                    for (; itr != idx.end() && itr->group == o.name
                        && member_list.size() < pgms->limit; ++itr) {
                        if (pgms->start != account_name_type() && itr->account != pgms->start) {
                            continue;
                        }
                        member_list.emplace_back(*itr);
                    }
                }
            }
        }

        private_group_api_object() = default;

        account_name_type owner;
        std::string name;
        std::string json_metadata;
        bool is_encrypted = false;
        private_group_privacy privacy;
        time_point_sec created;
        uint32_t moders = 0;
        uint32_t members = 0;
        uint32_t pendings = 0;
        uint32_t banneds = 0;

        std::vector<private_group_member_api_object> member_list;
    };

    /**
     * Events for callbacks
     */
    enum class callback_event_type: uint8_t {
        message,
        mark,
        remove_inbox,
        remove_outbox,
        contact,
    };

    /**
     * Query for callback
     */
    struct callback_query final {
        fc::flat_set<account_name_type> select_accounts;
        fc::flat_set<account_name_type> filter_accounts;
        fc::flat_set<callback_event_type> select_events;
        fc::flat_set<callback_event_type> filter_events;
    };

    /**
     * Callback event about message
     */
    struct callback_message_event final {
        callback_event_type type;
        message_api_object message;
    };

    /**
     * Callback event about contact
     */
    struct callback_contact_event final {
        callback_event_type type;
        contact_api_object contact;
    };

} } } // golos::plugins::private_message

FC_REFLECT(
    (golos::plugins::private_message::message_api_object),
    (from)(to)(from_memo_key)(to_memo_key)(nonce)(checksum)(encrypted_message)
    (create_date)(receive_date)(read_date)(remove_date)
    (donates)(donates_uia)
)

FC_REFLECT(
    (golos::plugins::private_message::settings_api_object),
    (ignore_messages_from_unknown_contact))

FC_REFLECT(
    (golos::plugins::private_message::contact_size_info),
    (total_outbox_messages)(unread_outbox_messages)(total_inbox_messages)(unread_inbox_messages))

FC_REFLECT_DERIVED(
    (golos::plugins::private_message::contacts_size_info), ((golos::plugins::private_message::contact_size_info)),
    (total_contacts))

FC_REFLECT(
    (golos::plugins::private_message::contact_api_object),
    (contact)(json_metadata)(local_type)(remote_type)(size)(last_message))

FC_REFLECT(
    (golos::plugins::private_message::contacts_size_api_object),
    (size))

FC_REFLECT_ENUM(
    golos::plugins::private_message::callback_event_type,
    (message)(mark)(remove_inbox)(remove_outbox)(contact))

FC_REFLECT(
    (golos::plugins::private_message::callback_query),
    (select_accounts)(filter_accounts)(select_events)(filter_events))

FC_REFLECT(
    (golos::plugins::private_message::callback_message_event),
    (type)(message))

FC_REFLECT(
    (golos::plugins::private_message::callback_contact_event),
    (type)(contact))

FC_REFLECT(
    (golos::plugins::private_message::private_group_member_api_object),
    (group)(account)(json_metadata)(member_type)
    (invited)(joined)(updated)
)

FC_REFLECT(
    (golos::plugins::private_message::private_group_api_object),
    (owner)(name)(json_metadata)(is_encrypted)(privacy)(created)
    (moders)(members)(pendings)(banneds)(member_list)
)
