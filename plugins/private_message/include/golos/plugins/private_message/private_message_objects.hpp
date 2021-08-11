#pragma once

#include <golos/protocol/base.hpp>
#include <golos/protocol/types.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <chainbase/chainbase.hpp>

namespace golos { namespace plugins { namespace private_message {

    using namespace golos::protocol;
    using namespace chainbase;
    using namespace golos::chain;
    using namespace boost::multi_index;

#ifndef PRIVATE_MESSAGE_SPACE_ID
#define PRIVATE_MESSAGE_SPACE_ID 6
#endif

    enum private_message_object_type {
        message_object_type = (PRIVATE_MESSAGE_SPACE_ID << 8),
        settings_object_type = (PRIVATE_MESSAGE_SPACE_ID << 8) + 1,
        contact_object_type = (PRIVATE_MESSAGE_SPACE_ID << 8) + 2,
        contact_size_object_type = (PRIVATE_MESSAGE_SPACE_ID << 8) + 3,
    };

    /**
     * Message
     */
    class message_object: public object<message_object_type, message_object> {
    public:
        template<typename Constructor, typename Allocator>
        message_object(Constructor&& c, allocator <Allocator> a)
            : encrypted_message(a) {
            c(*this);
        }

        id_type id;

        account_name_type from;
        account_name_type to;
        uint64_t nonce;
        public_key_type from_memo_key;
        public_key_type to_memo_key;
        uint32_t checksum = 0;
        buffer_type encrypted_message;

        time_point_sec inbox_create_date; // == time_point_sec::min() means removed message
        time_point_sec outbox_create_date; // == time_point_sec::min() means removed message
        time_point_sec receive_date;
        time_point_sec read_date;
        time_point_sec remove_date;
    };

    using message_id_type = message_object::id_type;

    struct by_inbox;
    struct by_inbox_account;
    struct by_outbox;
    struct by_outbox_account;
    struct by_nonce;

    struct by_owner;
    struct by_contact;

    using message_index = multi_index_container<
        message_object,
        indexed_by<
            ordered_unique<tag<by_id>,
                member<message_object, message_id_type, &message_object::id>
            >,
            ordered_unique<tag<by_inbox>,
                composite_key<
                    message_object,
                    member<message_object, account_name_type, &message_object::to>,
                    member<message_object, time_point_sec, &message_object::inbox_create_date>,
                    member<message_object, message_id_type, &message_object::id>
                >,
                composite_key_compare<
                    string_less,
                    std::greater<time_point_sec>,
                    std::greater<message_id_type>
                >
            >,
            ordered_unique<tag<by_inbox_account>,
                composite_key<
                    message_object,
                    member<message_object, account_name_type, &message_object::to>,
                    member<message_object, account_name_type, &message_object::from>,
                    member<message_object, time_point_sec, &message_object::inbox_create_date>,
                    member<message_object, message_id_type, &message_object::id>
                >,
                composite_key_compare<
                    string_less,
                    string_less,
                    std::greater<time_point_sec>,
                    std::greater<message_id_type>
                >
            >,
            ordered_unique<tag<by_outbox>,
                composite_key<
                    message_object,
                    member<message_object, account_name_type, &message_object::from>,
                    member<message_object, time_point_sec, &message_object::outbox_create_date>,
                    member<message_object, message_id_type, &message_object::id>
                >,
                composite_key_compare<
                    string_less,
                    std::greater<time_point_sec>,
                    std::greater<message_id_type>
                >
            >,
            ordered_unique<tag<by_outbox_account>,
                composite_key<
                    message_object,
                    member<message_object, account_name_type, &message_object::from>,
                    member<message_object, account_name_type, &message_object::to>,
                    member<message_object, time_point_sec, &message_object::outbox_create_date>,
                    member<message_object, message_id_type, &message_object::id>
                >,
                composite_key_compare<
                    string_less,
                    string_less,
                    std::greater<time_point_sec>,
                    std::greater<message_id_type>
                >
            >,
            ordered_unique<tag<by_nonce>,
                composite_key<
                    message_object,
                    member<message_object, account_name_type, &message_object::from>,
                    member<message_object, account_name_type, &message_object::to>,
                    member<message_object, uint64_t, &message_object::nonce>
                >,
                composite_key_compare<
                    string_less,
                    string_less,
                    std::less<uint64_t>
                >
            >
        >, allocator<message_object>>;

    /**
     * Settings
     */
    class settings_object: public object<settings_object_type, settings_object> {
    public:
        template<typename Constructor, typename Allocator>
        settings_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        }

        id_type id;

        account_name_type owner;
        bool ignore_messages_from_unknown_contact = false;
    };

    using settings_id_type = settings_object::id_type;

    using settings_index = multi_index_container<
        settings_object,
        indexed_by<
            ordered_unique<tag<by_id>,
                member<settings_object, settings_id_type, &settings_object::id>
            >,
            ordered_unique<tag<by_owner>,
                member<settings_object, account_name_type, &settings_object::owner>
            >
        >, allocator<settings_object>>;

    struct contact_size_info {
        uint32_t total_outbox_messages = 0;
        uint32_t unread_outbox_messages = 0;
        uint32_t total_inbox_messages = 0;
        uint32_t unread_inbox_messages = 0;

        bool empty() const {
            return !total_outbox_messages && !total_inbox_messages;
        }

        contact_size_info& operator-=(const contact_size_info& s) {
            total_outbox_messages -= s.total_outbox_messages;
            unread_outbox_messages -= s.unread_outbox_messages;
            total_inbox_messages -= s.total_inbox_messages;
            unread_inbox_messages -= s.unread_inbox_messages;
            return *this;
        }

        contact_size_info& operator+=(const contact_size_info& s) {
            total_outbox_messages += s.total_outbox_messages;
            unread_outbox_messages += s.unread_outbox_messages;
            total_inbox_messages += s.total_inbox_messages;
            unread_inbox_messages += s.unread_inbox_messages;
            return *this;
        }

        bool operator==(const contact_size_info& s) const {
            return
                total_outbox_messages == s.total_outbox_messages &&
                unread_outbox_messages == s.unread_outbox_messages &&
                total_inbox_messages == s.total_inbox_messages &&
                unread_inbox_messages == s.unread_inbox_messages;
        }

        bool operator!=(const contact_size_info& s) const {
            return !(this->operator==(s));
        }
    };

    struct contacts_size_info final: public contact_size_info {
        uint32_t total_contacts = 0;
    };

    /**
     * Contact item
     */
    class contact_object: public object<contact_object_type, contact_object> {
    public:
        template<typename Constructor, typename Allocator>
        contact_object(Constructor&& c, allocator <Allocator> a): json_metadata(a) {
            c(*this);
        }

        id_type id;

        account_name_type owner;
        account_name_type contact;
        private_contact_type type;
        shared_string json_metadata;
        contact_size_info size;
        bool is_hidden = false;
    };

    using contact_id_type = contact_object::id_type;

    using contact_index = multi_index_container<
        contact_object,
        indexed_by<
            ordered_unique<tag<by_id>,
                member<contact_object, contact_id_type, &contact_object::id>
            >,
            ordered_unique<tag<by_owner>,
                composite_key<
                    contact_object,
                    member<contact_object, account_name_type, &contact_object::owner>,
                    member<contact_object, private_contact_type, &contact_object::type>,
                    member<contact_object, account_name_type, &contact_object::contact>
                >,
                composite_key_compare<
                    string_less,
                    std::greater<private_contact_type>,
                    string_less
                >
            >,
            ordered_unique<tag<by_contact>,
                composite_key<
                    contact_object,
                    member<contact_object, account_name_type, &contact_object::owner>,
                    member<contact_object, account_name_type, &contact_object::contact>
                >,
                composite_key_compare<
                    string_less,
                    string_less
                >
            >
        >, allocator<contact_object>>;

    /**
     * Counters for account contact lists
     */
    struct contact_size_object: public object<contact_size_object_type, contact_size_object> {
        template<typename Constructor, typename Allocator>
        contact_size_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        }

        id_type id;

        account_name_type owner;
        private_contact_type type;
        contacts_size_info size;
    };

    using contact_size_id_type = contact_size_object::id_type;

    using contact_size_index = multi_index_container<
        contact_size_object,
        indexed_by<
            ordered_unique<tag<by_id>,
                member<contact_size_object, contact_size_id_type, &contact_size_object::id>
            >,
            ordered_unique<tag<by_owner>,
                composite_key<
                    contact_size_object,
                    member<contact_size_object, account_name_type, &contact_size_object::owner>,
                    member<contact_size_object, private_contact_type, &contact_size_object::type>
                >,
                composite_key_compare<
                    string_less,
                    std::less<private_contact_type>
                >
            >
        >, allocator<contact_size_object>>;

} } } // golos::plugins::private_message

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::private_message::message_object,
    golos::plugins::private_message::message_index)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::private_message::settings_object,
    golos::plugins::private_message::settings_index)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::private_message::contact_object,
    golos::plugins::private_message::contact_index)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::private_message::contact_size_object,
    golos::plugins::private_message::contact_size_index)
