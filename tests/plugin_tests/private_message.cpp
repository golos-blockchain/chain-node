#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/chain/account_object.hpp>

#include <fc/crypto/aes.hpp>

#include <golos/plugins/private_message/private_message_plugin.hpp>
#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/private_message/private_message_exceptions.hpp>

using golos::plugins::json_rpc::msg_pack;
using golos::logic_exception;
using golos::missing_object;
using golos::object_already_exist;
using namespace golos::protocol;
using namespace golos::plugins::private_message;

struct private_message_fixture : public golos::chain::clean_database_fixture_wrap {
    private_message_fixture() : golos::chain::clean_database_fixture_wrap(true, [&]() {
        // WARNING: no work with private_message_fixture fields here
        initialize<private_message_plugin>();
        open_database();
        startup();
    }) {
        pm_plugin = appbase::app().find_plugin<private_message_plugin>();
    }

    private_message_plugin* pm_plugin = nullptr;
};

fc::variant_object make_private_message_id(const std::string& from, const std::string& to, const uint64_t nonce) {
    auto res = fc::mutable_variant_object()("from",from)("to",to)("nonce",nonce);
    return fc::variant_object(res);
}

#define _CHECK_EQUAL_MESSAGE_API_OBJECT(L, R) \
    BOOST_CHECK_EQUAL((L).from, (R).from); \
    BOOST_CHECK_EQUAL((L).to, (R).to); \
    BOOST_CHECK_EQUAL((L).nonce, (R).nonce); \
    BOOST_CHECK_EQUAL((L).from_memo_key, (R).from_memo_key); \
    BOOST_CHECK_EQUAL((L).to_memo_key, (R).to_memo_key); \
    BOOST_CHECK_EQUAL((L).checksum, (R).checksum); \
    BOOST_CHECK_EQUAL((L).encrypted_message.size(), (R).encrypted_message.size()); \
    BOOST_CHECK_EQUAL(std::equal( \
       (L).encrypted_message.begin(), (L).encrypted_message.end(), \
       (R).encrypted_message.begin(), (R).encrypted_message.end()), true); \
    BOOST_CHECK_EQUAL((L).create_date, (R).create_date); \
    BOOST_CHECK_EQUAL((L).receive_date, (R).receive_date); \
    BOOST_CHECK_EQUAL((L).read_date, (R).read_date);


BOOST_FIXTURE_TEST_SUITE(private_message_plugin, private_message_fixture)

    BOOST_AUTO_TEST_CASE(private_outbox_message) {
        BOOST_TEST_MESSAGE("Testing: private_message_operation");

        ACTORS((alice)(bob));

        BOOST_TEST_MESSAGE("--- Compose alice -> bob message");

        auto alice_bob_nonce = fc::time_point::now().time_since_epoch().count();
        auto alice_bob_secret = alice_private_key.get_shared_secret(bob_private_key.get_public_key());
        std::string alice_bob_msg = "Hello, Bob! My name is Alice.";
        std::vector<char> alice_bob_msg_data(alice_bob_msg.begin(), alice_bob_msg.end());

        BOOST_TEST_MESSAGE("--- Compose bob -> alice message");

        auto bob_alice_nonce = alice_bob_nonce + 10;
        auto bob_alice_secret = bob_private_key.get_shared_secret(alice_private_key.get_public_key());
        std::string bob_alice_msg = "Hello, Alice!";
        std::vector<char> bob_alice_msg_data(bob_alice_msg.begin(), bob_alice_msg.end());

        BOOST_TEST_MESSAGE("--- Check alice -> bob and bob -> alice secrets are equal");

        BOOST_CHECK_EQUAL(alice_bob_secret, bob_alice_secret);

        BOOST_TEST_MESSAGE("--- Encrypt alice -> bob message");

        fc::sha512::encoder enc;
        fc::raw::pack(enc, alice_bob_nonce);
        fc::raw::pack(enc, alice_bob_secret);
        auto alice_bob_encrypt_key = enc.result();
        uint32_t alice_bob_checksum = fc::sha256::hash(alice_bob_encrypt_key)._hash[0];

        private_message_operation mop;

        mop.from = "alice";
        mop.from_memo_key = alice_private_key.get_public_key();
        mop.to = "bob";
        mop.to_memo_key = bob_private_key.get_public_key();
        mop.nonce = alice_bob_nonce;
        mop.encrypted_message = fc::aes_encrypt(alice_bob_encrypt_key, alice_bob_msg_data);
        mop.checksum = alice_bob_checksum;

        BOOST_TEST_MESSAGE("--- Send alice -> bob message");

        private_message_plugin_operation pop = mop;

        custom_json_operation jop;
        jop.id = "private_message";
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"alice"};

        signed_transaction trx;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

        BOOST_TEST_MESSAGE("--- Encrypt bob -> alice message");

        enc.reset();
        fc::raw::pack(enc, bob_alice_nonce);
        fc::raw::pack(enc, bob_alice_secret);
        auto bob_alice_encrypt_key = enc.result();
        uint32_t bob_alice_checksum = fc::sha256::hash(bob_alice_encrypt_key)._hash[0];

        mop.from = "bob";
        mop.from_memo_key = bob_private_key.get_public_key();
        mop.to = "alice";
        mop.to_memo_key = alice_private_key.get_public_key();
        mop.nonce = bob_alice_nonce;
        mop.encrypted_message = fc::aes_encrypt(bob_alice_encrypt_key, bob_alice_msg_data);
        mop.checksum = bob_alice_checksum;

        BOOST_TEST_MESSAGE("--- Send bob -> alice message");

        pop = mop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"bob"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, bob_private_key, jop));

        BOOST_TEST_MESSAGE("--- Get messages by alice and bob with all API methods");

        msg_pack mp;
        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant(message_box_query())});
        auto bob_inbox = pm_plugin->get_inbox(mp);

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant(message_box_query())});
        auto bob_outbox = pm_plugin->get_outbox(mp);

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant(message_box_query())});
        auto alice_inbox = pm_plugin->get_inbox(mp);

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant(message_box_query())});
        auto alice_outbox = pm_plugin->get_outbox(mp);

        mp.args = std::vector<fc::variant>(
            {fc::variant("alice"), fc::variant("bob"), fc::variant(message_thread_query())});
        auto alice_bob_thread = pm_plugin->get_thread(mp);

        mp.args = std::vector<fc::variant>(
            {fc::variant("bob"), fc::variant("alice"), fc::variant(message_thread_query())});
        auto bob_alice_thread = pm_plugin->get_thread(mp);

        BOOST_TEST_MESSAGE("--- Check messages count");

        BOOST_CHECK_EQUAL(bob_outbox.size(), 1);
        BOOST_CHECK_EQUAL(bob_inbox.size(), 1);
        BOOST_CHECK_EQUAL(alice_inbox.size(), 1);
        BOOST_CHECK_EQUAL(alice_outbox.size(), 1);
        BOOST_CHECK_EQUAL(alice_bob_thread.size(), 2);
        BOOST_CHECK_EQUAL(bob_alice_thread.size(), 2);

        BOOST_TEST_MESSAGE("--- Check messages");

        _CHECK_EQUAL_MESSAGE_API_OBJECT(bob_outbox[0], alice_inbox[0]);
        _CHECK_EQUAL_MESSAGE_API_OBJECT(bob_outbox[0], alice_bob_thread[0]);
        _CHECK_EQUAL_MESSAGE_API_OBJECT(bob_outbox[0], bob_alice_thread[0]);
        BOOST_CHECK_EQUAL(bob_outbox[0].nonce, bob_alice_nonce);
        BOOST_CHECK_EQUAL(bob_outbox[0].checksum, bob_alice_checksum);

        _CHECK_EQUAL_MESSAGE_API_OBJECT(alice_outbox[0], bob_inbox[0]);
        _CHECK_EQUAL_MESSAGE_API_OBJECT(alice_outbox[0], alice_bob_thread[1]);
        _CHECK_EQUAL_MESSAGE_API_OBJECT(alice_outbox[0], bob_alice_thread[1]);
        BOOST_CHECK_EQUAL(alice_outbox[0].nonce, alice_bob_nonce);
        BOOST_CHECK_EQUAL(alice_outbox[0].checksum, alice_bob_checksum);

        BOOST_TEST_MESSAGE("--- Decrypt message by alice");

        enc.reset();
        fc::raw::pack(enc, alice_inbox[0].nonce);
        fc::raw::pack(enc, alice_private_key.get_shared_secret(alice_inbox[0].from_memo_key));
        auto alice_bob_decrypt_key = enc.result();
        uint32_t alice_bob_decrypt_checksum = fc::sha256::hash(alice_bob_decrypt_key)._hash[0];

        BOOST_CHECK_EQUAL(alice_bob_decrypt_checksum, alice_inbox[0].checksum);

        auto alice_bob_decrypt_msg_data = fc::aes_decrypt(alice_bob_decrypt_key, alice_inbox[0].encrypted_message);

        BOOST_CHECK_EQUAL(alice_bob_decrypt_msg_data.size(), bob_alice_msg_data.size());
        BOOST_CHECK_EQUAL(std::equal(
            alice_bob_decrypt_msg_data.begin(), alice_bob_decrypt_msg_data.end(),
            bob_alice_msg_data.begin(), bob_alice_msg_data.end()), true);

        BOOST_TEST_MESSAGE("--- Decrypt message by bob");

        enc.reset();
        fc::raw::pack(enc, bob_inbox[0].nonce);
        fc::raw::pack(enc, bob_private_key.get_shared_secret(bob_inbox[0].from_memo_key));
        auto bob_alice_decrypt_key = enc.result();
        uint32_t bob_alice_decrypt_checksum = fc::sha256::hash(bob_alice_decrypt_key)._hash[0];

        BOOST_CHECK_EQUAL(bob_alice_decrypt_checksum, bob_inbox[0].checksum);

        auto bob_alice_decrypt_msg_data = fc::aes_decrypt(bob_alice_decrypt_key, bob_inbox[0].encrypted_message);

        BOOST_CHECK_EQUAL(bob_alice_decrypt_msg_data.size(), alice_bob_msg_data.size());
        BOOST_CHECK_EQUAL(std::equal(
            bob_alice_decrypt_msg_data.begin(), bob_alice_decrypt_msg_data.end(),
            alice_bob_msg_data.begin(), alice_bob_msg_data.end()), true);

        BOOST_TEST_MESSAGE("--- Test failure with edit alice -> bob message, but wrong nonce");

        std::string alice_bob_edited_msg = "Edited: Hello, Bob! My name is Alice.";
        std::vector<char> alice_bob_edited_msg_data(alice_bob_edited_msg.begin(), alice_bob_edited_msg.end());

        mop.from = "alice";
        mop.from_memo_key = alice_private_key.get_public_key();
        mop.to = "bob";
        mop.to_memo_key = bob_private_key.get_public_key();
        mop.nonce = alice_bob_nonce + 1000;
        mop.update = true;
        mop.encrypted_message = fc::aes_encrypt(alice_bob_encrypt_key, alice_bob_edited_msg_data);
        mop.checksum = alice_bob_checksum;

        pop = mop;

        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"alice"};

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(trx, alice_private_key, jop),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(missing_object, "private_message",
                    make_private_message_id("alice", "bob", mop.nonce))));

        BOOST_TEST_MESSAGE("--- Test failure with edit alice -> bob message, but update = false");

        mop.nonce = alice_bob_nonce;
        mop.update = false;
        pop = mop;
        jop.json = fc::json::to_string(pop);

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(trx, alice_private_key, jop),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(object_already_exist, "private_message",
                    make_private_message_id("alice", "bob", mop.nonce))));

        BOOST_TEST_MESSAGE("--- Test success edit alice -> bob message");

        mop.update = true;
        pop = mop;
        jop.json = fc::json::to_string(pop);
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

        BOOST_TEST_MESSAGE("--- Check alice -> bob message edited in API");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant(message_box_query())});
        bob_inbox = pm_plugin->get_inbox(mp);

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant(message_box_query())});
        alice_outbox = pm_plugin->get_outbox(mp);

        BOOST_CHECK_EQUAL(bob_inbox.size(), 1);
        BOOST_CHECK_EQUAL(alice_outbox.size(), 1);
        _CHECK_EQUAL_MESSAGE_API_OBJECT(bob_inbox[0], alice_outbox[0]);

        BOOST_TEST_MESSAGE("--- Decrypt alice -> bob edited message");

        auto bob_alice_edited_decrypt_msg_data = fc::aes_decrypt(bob_alice_decrypt_key, bob_inbox[0].encrypted_message);

        BOOST_CHECK_EQUAL(bob_alice_edited_decrypt_msg_data.size(), alice_bob_edited_msg_data.size());
        BOOST_CHECK_EQUAL(std::equal(
            bob_alice_edited_decrypt_msg_data.begin(), bob_alice_edited_decrypt_msg_data.end(),
            alice_bob_edited_msg_data.begin(), alice_bob_edited_msg_data.end()), true);
    }

    // private_contact_operation is disabled
    // BOOST_AUTO_TEST_CASE(private_contact) {
    //     BOOST_TEST_MESSAGE("Testing: private_contact_operation");

    //     ACTORS((alice)(bob)(sam)(dave));

    //     BOOST_TEST_MESSAGE("--- unknown contact");

    //     private_contact_operation cop;

    //     cop.owner = "alice";
    //     cop.contact = "bob";
    //     cop.type = unknown;

    //     private_message_plugin_operation pop = cop;

    //     custom_json_operation jop;
    //     jop.id = "private_message";
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"alice"};

    //     signed_transaction trx;

    //     GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(trx, alice_private_key, jop),
    //         CHECK_ERROR(tx_invalid_operation, 0,
    //             CHECK_ERROR(logic_exception, logic_errors::add_unknown_contact)));

    //     BOOST_TEST_MESSAGE("--- Ignored contact");

    //     cop.type = ignored;
    //     cop.json_metadata = "{}";
    //     pop = cop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"alice"};

    //     GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

    //     msg_pack mp;
    //     mp.args = std::vector<fc::variant>(
    //         {fc::variant("alice"), fc::variant(ignored), fc::variant(100), fc::variant(0)});
    //     auto alice_contacts = pm_plugin->get_contacts(mp);

    //     BOOST_CHECK_EQUAL(alice_contacts.size(), 1);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].owner, cop.owner);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].contact, cop.contact);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].local_type, cop.type);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].remote_type, unknown);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].json_metadata, cop.json_metadata);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].size.total_outbox_messages, 0);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].size.unread_outbox_messages, 0);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].size.total_inbox_messages, 0);
    //     BOOST_CHECK_EQUAL(alice_contacts[0].size.unread_inbox_messages, 0);

    //     generate_block();

    //     BOOST_TEST_MESSAGE("--- Contact hasn't changed");

    //     GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(trx, alice_private_key, jop),
    //         CHECK_ERROR(tx_invalid_operation, 0,
    //             CHECK_ERROR(logic_exception, logic_errors::contact_has_not_changed)));

    //     cop.json_metadata = "{\"name\":\"Mark\"}";
    //     pop = cop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"alice"};

    //     GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

    //     BOOST_TEST_MESSAGE("--- Send message from ignored contact");

    //     auto base_nonce = fc::time_point::now().time_since_epoch().count();

    //     fc::sha512::encoder enc;
    //     fc::raw::pack(enc, base_nonce);
    //     auto encrypt_key = enc.result();

    //     private_message_operation mop;

    //     mop.from = "alice";
    //     mop.from_memo_key = alice_private_key.get_public_key();
    //     mop.to = "bob";
    //     mop.to_memo_key = bob_private_key.get_public_key();
    //     mop.nonce = base_nonce;
    //     mop.encrypted_message = fc::aes_encrypt(encrypt_key, {});
    //     mop.checksum = encrypt_key._hash[0];

    //     pop = mop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"alice"};

    //     GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

    //     mop.from = "bob";
    //     mop.to = "alice";
    //     mop.nonce += 10;
    //     pop = mop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"bob"};

    //     GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(trx, bob_private_key, jop),
    //         CHECK_ERROR(tx_invalid_operation, 0,
    //             CHECK_ERROR(logic_exception, logic_errors::sender_in_ignore_list)));

    //     mop.from = "sam";
    //     mop.to = "alice";
    //     mop.nonce += 10;
    //     pop = mop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"sam"};

    //     GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, sam_private_key, jop));

    //     BOOST_TEST_MESSAGE("--- Receive messages only from pinned contacts");

    //     mp.args = std::vector<fc::variant>({fc::variant("alice")});
    //     auto alice_settings = pm_plugin->get_settings(mp);

    //     BOOST_CHECK_EQUAL(alice_settings.ignore_messages_from_unknown_contact, false);

    //     private_settings_operation sop;
    //     sop.owner = "alice";
    //     sop.settings.insert(ignore_messages_from_unknown_contact{true});
    //     pop = sop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"alice"};

    //     GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

    //     alice_settings = pm_plugin->get_settings(mp);

    //     BOOST_CHECK_EQUAL(alice_settings.ignore_messages_from_unknown_contact, true);

    //     mop.from = "sam";
    //     mop.to = "alice";
    //     mop.nonce += 10;
    //     pop = mop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"sam"};

    //     GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(trx, sam_private_key, jop),
    //         CHECK_ERROR(tx_invalid_operation, 0,
    //             CHECK_ERROR(logic_exception, logic_errors::recipient_ignores_messages_from_unknown_contact)));

    //     cop.owner = "alice";
    //     cop.contact = "dave";
    //     cop.type = pinned;
    //     cop.json_metadata = "{}";
    //     pop = cop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"alice"};

    //     GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

    //     mop.from = "dave";
    //     mop.to = "alice";
    //     mop.nonce += 10;
    //     pop = mop;
    //     jop.json = fc::json::to_string(pop);
    //     jop.required_posting_auths = {"dave"};

    //     GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, dave_private_key, jop));

    //     mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("dave")});
    //     auto alice_dave_contact = pm_plugin->get_contact_info(mp);

    //     BOOST_CHECK_EQUAL(alice_dave_contact.owner, "alice");
    //     BOOST_CHECK_EQUAL(alice_dave_contact.contact, "dave");
    //     BOOST_CHECK_EQUAL(alice_dave_contact.local_type, pinned);
    //     BOOST_CHECK_EQUAL(alice_dave_contact.remote_type, pinned);
    //     BOOST_CHECK_EQUAL(alice_dave_contact.json_metadata, "{}");
    //     BOOST_CHECK_EQUAL(alice_dave_contact.size.total_outbox_messages, 0);
    //     BOOST_CHECK_EQUAL(alice_dave_contact.size.unread_outbox_messages, 0);
    //     BOOST_CHECK_EQUAL(alice_dave_contact.size.total_inbox_messages, 1);
    //     BOOST_CHECK_EQUAL(alice_dave_contact.size.unread_inbox_messages, 1);
    // }

    BOOST_AUTO_TEST_CASE(private_mark) {
        BOOST_TEST_MESSAGE("Testing: private_mark_message_operation");

        ACTORS((alice)(bob)(sam));

        BOOST_TEST_MESSAGE("--- bob -> alice message");

        signed_transaction trx;

        auto base_nonce = fc::time_point::now().time_since_epoch().count();

        fc::sha512::encoder enc;
        fc::raw::pack(enc, base_nonce);
        auto encrypt_key = enc.result();

        private_message_operation mop;

        mop.from = "bob";
        mop.from_memo_key = bob_private_key.get_public_key();
        mop.to = "alice";
        mop.to_memo_key = alice_private_key.get_public_key();
        mop.nonce = base_nonce;
        mop.encrypted_message = fc::aes_encrypt(encrypt_key, {});
        mop.checksum = encrypt_key._hash[0];

        private_message_plugin_operation pop = mop;

        custom_json_operation jop;
        jop.id = "private_message";
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"bob"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, bob_private_key, jop));
        generate_block();
        
        BOOST_TEST_MESSAGE("--- Another bob -> alice message");

        mop.from = "bob";
        mop.to = "alice";
        mop.nonce = base_nonce + 10;
        pop = mop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"bob"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, bob_private_key, jop));
        generate_block();

        BOOST_TEST_MESSAGE("--- same -> alice message");

        mop.from = "sam";
        mop.to = "alice";
        mop.nonce = base_nonce + 20;
        pop = mop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"sam"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, sam_private_key, jop));
        generate_block();

        BOOST_TEST_MESSAGE("--- Another same -> alice message");

        mop.from = "sam";
        mop.to = "alice";
        mop.nonce = base_nonce + 30;
        pop = mop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"sam"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, sam_private_key, jop));
        generate_block();

        BOOST_TEST_MESSAGE("--- get_contacts_size alice");

        msg_pack mp;
        mp.args = std::vector<fc::variant>({fc::variant("alice")});
        auto alice_contacts_size = pm_plugin->get_contacts_size(mp);
        
        BOOST_CHECK_EQUAL(alice_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_contacts, 2);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_inbox_messages, 4);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_inbox_messages, 4);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[pinned].total_contacts, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[pinned].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[pinned].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[pinned].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[pinned].unread_inbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[ignored].total_contacts, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[ignored].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[ignored].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[ignored].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[ignored].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contacts_info alice bob");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("bob")});
        auto alice_bob_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_inbox_messages, 2);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_inbox_messages, 2);

        BOOST_TEST_MESSAGE("--- get_contacts_size bob");

        mp.args = std::vector<fc::variant>({fc::variant("bob")});
        auto bob_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(bob_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_contacts, 1);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[pinned].total_contacts, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[pinned].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[pinned].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[pinned].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[pinned].unread_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[ignored].total_contacts, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[ignored].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[ignored].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[ignored].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[ignored].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contacts_info bob alice");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant("alice")});
        auto bob_alice_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- Mark one bob -> alice message by alice");

        private_mark_message_operation rop;

        rop.from = "bob";
        rop.to = "alice";
        rop.nonce = base_nonce;
        pop = rop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"alice"};
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

        BOOST_TEST_MESSAGE("--- get_contacts_size alice");

        mp.args = std::vector<fc::variant>({fc::variant("alice")});
        alice_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(alice_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_contacts, 2);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_inbox_messages, 4);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_inbox_messages, 3);

        BOOST_TEST_MESSAGE("--- get_contact_info alice bob");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("bob")});
        alice_bob_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_inbox_messages, 2);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_inbox_messages, 1);

        BOOST_TEST_MESSAGE("--- get_contacts_size bob");

        mp.args = std::vector<fc::variant>({fc::variant("bob")});
        bob_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(bob_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_contacts, 1);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_outbox_messages, 1);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info bob alice");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant("alice")});
        bob_alice_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_outbox_messages, 1);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- Mark all messages from sam");

        generate_block();
        
        rop.from = "sam";
        rop.to = "alice";
        rop.nonce = 0;
        rop.stop_date = _db.head_block_time();
        pop = rop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"alice"};
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

        BOOST_TEST_MESSAGE("--- get_contacts_size alice");

        mp.args = std::vector<fc::variant>({fc::variant("alice")});
        alice_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(alice_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_contacts, 2);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_inbox_messages, 4);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_inbox_messages, 1);

        BOOST_TEST_MESSAGE("--- get_contact_info alice sam");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("sam")});
        auto alice_sam_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.total_inbox_messages, 2);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contacts_size sam");

        mp.args = std::vector<fc::variant>({fc::variant("sam")});
        auto sam_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(sam_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].total_contacts, 1);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info sam alice");

        mp.args = std::vector<fc::variant>({fc::variant("sam"), fc::variant("alice")});
        auto sam_alice_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- Mark all messages to alice");

        generate_block();

        rop.from = "";
        rop.to = "alice";
        rop.nonce = 0;
        rop.stop_date = _db.head_block_time();
        pop = rop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"alice"};
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, alice_private_key, jop));

        BOOST_TEST_MESSAGE("--- get_contacts_size alice");

        mp.args = std::vector<fc::variant>({fc::variant("alice")});
        alice_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(alice_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_contacts, 2);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_inbox_messages, 4);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info alice sam");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("sam")});
        alice_sam_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.total_inbox_messages, 2);
        BOOST_CHECK_EQUAL(alice_sam_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contacts_size sam");

        mp.args = std::vector<fc::variant>({fc::variant("sam")});
        sam_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(sam_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].total_contacts, 1);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_contacts_size.size[unknown].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info sam alice");

        mp.args = std::vector<fc::variant>({fc::variant("sam"), fc::variant("alice")});
        sam_alice_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(sam_alice_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info alice sam");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("bob")});
        alice_bob_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_inbox_messages, 2);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contacts_size bob");

        mp.args = std::vector<fc::variant>({fc::variant("bob")});
        bob_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(bob_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_contacts, 1);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info bob alice");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant("alice")});
        bob_alice_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_outbox_messages, 2);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_inbox_messages, 0);
    }

    BOOST_AUTO_TEST_CASE(private_delete) {
        BOOST_TEST_MESSAGE("Testing: private_delete_message_operation");

        ACTORS((alice)(bob)(sam));

        BOOST_TEST_MESSAGE("--- bob -> alice message");

        signed_transaction trx;

        auto base_nonce = fc::time_point::now().time_since_epoch().count();

        fc::sha512::encoder enc;
        fc::raw::pack(enc, base_nonce);
        auto encrypt_key = enc.result();

        private_message_operation mop;

        mop.from = "bob";
        mop.from_memo_key = bob_private_key.get_public_key();
        mop.to = "alice";
        mop.to_memo_key = alice_private_key.get_public_key();
        mop.nonce = base_nonce;
        mop.encrypted_message = fc::aes_encrypt(encrypt_key, {});
        mop.checksum = encrypt_key._hash[0];

        private_message_plugin_operation pop = mop;

        custom_json_operation jop;
        jop.id = "private_message";
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"bob"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, bob_private_key, jop));

        generate_block();

        BOOST_TEST_MESSAGE("--- Another bob -> alice message");

        mop.from = "bob";
        mop.to = "alice";
        mop.nonce = base_nonce + 10;
        pop = mop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"bob"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, bob_private_key, jop));

        generate_block();

        BOOST_TEST_MESSAGE("--- sam -> alice message");

        mop.from = "sam";
        mop.to = "alice";
        mop.nonce = base_nonce + 20;
        pop = mop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"sam"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, sam_private_key, jop));

        generate_block();

        BOOST_TEST_MESSAGE("--- Another sam -> alice message");

        mop.from = "sam";
        mop.to = "alice";
        mop.nonce = base_nonce + 30;
        pop = mop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"sam"};

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, sam_private_key, jop));

        generate_block();

        BOOST_TEST_MESSAGE("--- Delete bob -> alice message by bob");

        private_delete_message_operation dop;

        dop.requester = "bob";
        dop.from = "bob";
        dop.to = "alice";
        dop.nonce = base_nonce;
        pop = dop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"bob"};
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, bob_private_key, jop));

        BOOST_TEST_MESSAGE("--- get_contacts_size alice");

        msg_pack mp;
        mp.args = std::vector<fc::variant>({fc::variant("alice")});
        auto alice_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(alice_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_contacts, 2);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_inbox_messages, 3);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_inbox_messages, 3);

        BOOST_TEST_MESSAGE("--- get_contact_info alice bob");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("bob")});
        auto alice_bob_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_inbox_messages, 1);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_inbox_messages, 1);

        BOOST_TEST_MESSAGE("--- get_contacts_size bob");

        mp.args = std::vector<fc::variant>({fc::variant("bob")});
        auto bob_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(bob_contacts_size.size.size(), 3);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_contacts, 1);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_outbox_messages, 1);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_outbox_messages, 1);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info bob alice");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant("alice")});
        auto bob_alice_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_outbox_messages, 1);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_outbox_messages, 1);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_inbox alice");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant(message_box_query())});
        auto alice_inbox = pm_plugin->get_inbox(mp);
        BOOST_CHECK_EQUAL(alice_inbox.size(), 3);

        BOOST_TEST_MESSAGE("--- get_thread alice bob");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("bob"), fc::variant(message_thread_query())});
        auto alice_bob_thread = pm_plugin->get_thread(mp);
        BOOST_CHECK_EQUAL(alice_bob_thread.size(), 1);

        BOOST_TEST_MESSAGE("--- get_outbox bob");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant(message_box_query())});
        auto bob_outbox = pm_plugin->get_outbox(mp);
        BOOST_CHECK_EQUAL(bob_outbox.size(), 1);

        BOOST_TEST_MESSAGE("--- get_thread bob alice");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant("alice"), fc::variant(message_thread_query())});
        auto bob_alice_thread = pm_plugin->get_thread(mp);
        BOOST_CHECK_EQUAL(bob_alice_thread.size(), 1);

        BOOST_TEST_MESSAGE("--- get_outbox sam");

        mp.args = std::vector<fc::variant>({fc::variant("sam"), fc::variant(message_box_query())});
        auto sam_outbox = pm_plugin->get_outbox(mp);
        BOOST_CHECK_EQUAL(sam_outbox.size(), 2);

        BOOST_TEST_MESSAGE("--- Delete all messages by bob");
        
        dop.requester = "bob";
        dop.from = "bob";
        dop.to = "alice";
        dop.nonce = 0;
        dop.stop_date = _db.head_block_time();
        pop = dop;
        jop.json = fc::json::to_string(pop);
        jop.required_posting_auths = {"bob"};
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(trx, bob_private_key, jop));

        BOOST_TEST_MESSAGE("--- get_contacts_size alice");

        mp.args = std::vector<fc::variant>({fc::variant("alice")});
        alice_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_contacts, 1);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].total_inbox_messages, 2);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[unknown].unread_inbox_messages, 2);

        BOOST_TEST_MESSAGE("--- get_contact_info alice bob");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant("bob")});
        alice_bob_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_bob_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contacts_size bob");

        mp.args = std::vector<fc::variant>({fc::variant("bob")});
        bob_contacts_size = pm_plugin->get_contacts_size(mp);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_contacts, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_contacts_size.size[unknown].unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_contact_info bob alice");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant("alice")});
        bob_alice_contact = pm_plugin->get_contact_info(mp);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.total_inbox_messages, 0);
        BOOST_CHECK_EQUAL(bob_alice_contact.size.unread_inbox_messages, 0);

        BOOST_TEST_MESSAGE("--- get_inbox alice");

        mp.args = std::vector<fc::variant>({fc::variant("alice"), fc::variant(message_box_query())});
        alice_inbox = pm_plugin->get_inbox(mp);
        BOOST_CHECK_EQUAL(alice_inbox.size(), 2);

        BOOST_TEST_MESSAGE("--- get_outbox bob");

        mp.args = std::vector<fc::variant>({fc::variant("bob"), fc::variant(message_box_query())});
        bob_outbox = pm_plugin->get_outbox(mp);
        BOOST_CHECK_EQUAL(bob_outbox.size(), 0);

        BOOST_TEST_MESSAGE("--- get_outbox sam");

        mp.args = std::vector<fc::variant>({fc::variant("sam"), fc::variant(message_box_query())});
        sam_outbox = pm_plugin->get_outbox(mp);
        BOOST_CHECK_EQUAL(sam_outbox.size(), 2);
    }

    BOOST_AUTO_TEST_CASE(remove_private_messages_bug_990) try {
        BOOST_TEST_MESSAGE("Testing: remove_private_messages_bug_990");

        BOOST_TEST_MESSAGE("--- Prepare users");

        constexpr char alice_user_name[] = "alice";
        constexpr char bob_user_name[] = "bob";
        constexpr char clara_user_name[] = "clara";

        ACTORS((alice)(bob)(clara))

        const auto alice_memo_private_key = generate_private_key("alice_memo");
        const auto alice_memo_public_key = alice_memo_private_key.get_public_key();

        const auto bob_memo_private_key = generate_private_key("bob_memo");
        const auto bob_memo_public_key = bob_memo_private_key.get_public_key();

        const auto clara_memo_private_key = generate_private_key("clara_memo");
        const auto clara_memo_public_key = bob_memo_private_key.get_public_key();

        const auto alice_to_bob_shared_secret = alice_memo_private_key.get_shared_secret(bob.memo_key);
        const auto clara_to_alice_shared_secret = clara_memo_private_key.get_shared_secret(alice.memo_key);

        BOOST_TEST_MESSAGE("--- Prepare private message common data");

        golos::plugins::private_message::private_message_operation private_message;

        private_message.update = false;

        const auto msg_json = fc::json::to_string("test message");
        const auto msg_data = std::vector<char>(msg_json.begin(), msg_json.end());

        custom_json_operation custom_operation;

        custom_operation.id   = "private_message";

        uint64_t nonce(100911024604926323);

        BOOST_TEST_MESSAGE("--- Send private message from \"alice\" to \"bob\"");

        private_message.from = alice_user_name;;
        private_message.from_memo_key = alice_memo_public_key;
        private_message.to = bob_user_name;
        private_message.to_memo_key = bob_memo_public_key;

        custom_operation.required_posting_auths.insert(alice_user_name);

        fc::sha512::encoder alice_to_bob_enc;
        fc::raw::pack(alice_to_bob_enc, nonce);
        fc::raw::pack(alice_to_bob_enc, alice_to_bob_shared_secret);

        const auto alice_bob_encrypt_key = alice_to_bob_enc.result();

        private_message.encrypted_message = fc::aes_encrypt(alice_bob_encrypt_key, msg_data);
        private_message.checksum = fc::sha256::hash(alice_bob_encrypt_key)._hash[0];
        private_message.nonce = nonce++;

        custom_operation.json = fc::json::to_string(golos::plugins::private_message::private_message_plugin_operation(private_message));

        signed_transaction tx;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, custom_operation));

        BOOST_TEST_MESSAGE("--- Send private message from \"clara\" to \"alice\"");

        private_message.from              = clara_user_name;
        private_message.from_memo_key     = clara_memo_public_key;
        private_message.to                = alice_user_name;
        private_message.to_memo_key       = alice_memo_public_key;

        custom_operation.required_posting_auths.clear();
        custom_operation.required_posting_auths.insert(clara_user_name);

        fc::sha512::encoder clara_to_alice_enc;
        fc::raw::pack(clara_to_alice_enc, nonce);
        fc::raw::pack(clara_to_alice_enc, clara_to_alice_shared_secret);

        const auto clara_to_alice_encrypt_key = alice_to_bob_enc.result();
        private_message.encrypted_message = fc::aes_encrypt(clara_to_alice_encrypt_key, msg_data);
        private_message.checksum = fc::sha256::hash(clara_to_alice_encrypt_key)._hash[0];
        private_message.nonce = nonce++;

        custom_operation.json = fc::json::to_string(golos::plugins::private_message::private_message_plugin_operation(private_message));

        tx.clear();

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, clara_private_key, custom_operation));

        BOOST_TEST_MESSAGE("--- Delete private messages from \"alice\" to \"bob\"");

        custom_operation.required_posting_auths.clear();
        custom_operation.required_posting_auths.insert(alice_user_name);

        golos::plugins::private_message::private_delete_message_operation delete_operation;

        delete_operation.requester = alice_user_name;
        delete_operation.from = alice_user_name;
        delete_operation.to = bob_user_name;
        delete_operation.nonce = 0;
        delete_operation.start_date = fc::time_point_sec::min();
        delete_operation.stop_date = fc::time_point_sec::maximum();

        custom_operation.json = fc::json::to_string(golos::plugins::private_message::private_message_plugin_operation(delete_operation));
        custom_operation.required_posting_auths.insert(alice_user_name);

        tx.clear();

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, custom_operation));

        BOOST_TEST_MESSAGE("--- Verify removed messages written by \"alice\" to \"bob\"");

        golos::plugins::json_rpc::msg_pack get_size_message;
        get_size_message.args = std::vector<fc::variant>({fc::variant(alice_user_name)});
        auto alice_contacts_size = pm_plugin->get_contacts_size(get_size_message);

        BOOST_CHECK_EQUAL(alice_contacts_size.size[golos::plugins::private_message::pinned].unread_outbox_messages, 0);
        BOOST_CHECK_EQUAL(alice_contacts_size.size[golos::plugins::private_message::unknown].unread_outbox_messages, 0);

    } FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
