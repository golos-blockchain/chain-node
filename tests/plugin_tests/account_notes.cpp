#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/chain/account_object.hpp>

#include <golos/plugins/account_notes/account_notes_plugin.hpp>
#include <golos/plugins/account_notes/account_notes_objects.hpp>
#include <golos/plugins/account_notes/account_notes_operations.hpp>

using boost::container::flat_set;

using golos::plugins::json_rpc::msg_pack;

using golos::logic_exception;
using golos::protocol::tx_invalid_operation;
using golos::invalid_parameter;
using golos::protocol::public_key_type;
using golos::protocol::signed_transaction;
using golos::protocol::custom_binary_operation;

using golos::chain::account_name_set;

using namespace golos::plugins::account_notes;


struct account_notes_fixture : public golos::chain::clean_database_fixture_wrap {
    account_notes_fixture() : golos::chain::clean_database_fixture_wrap(true, [&]() {
        initialize<golos::plugins::account_notes::account_notes_plugin>();
        open_database();
        startup();
    }) {
    }
};


BOOST_FIXTURE_TEST_SUITE(account_notes_plugin, account_notes_fixture)

BOOST_AUTO_TEST_CASE(account_notes_validate_size_limits) {
    BOOST_TEST_MESSAGE("Testing: account_notes_validate_size_limits");

    BOOST_TEST_MESSAGE("-- Normal key case");

    set_value_operation op;
    op.account = "alice";
    for (int i = 0; i < 128; i++) {
        op.key += "1";
    }
    op.value = "ipsum";
    GOLOS_CHECK_NO_THROW(op.validate());

    BOOST_TEST_MESSAGE("-- Long key case");

    op.key += "1";
    GOLOS_CHECK_ERROR_PROPS(op.validate(),
        CHECK_ERROR(invalid_parameter, "key"));

    BOOST_TEST_MESSAGE("-- Normal value case");

    op.key = "lorem";
    op.value = "";
    for (int i = 0; i < UINT16_MAX; i++) {
        op.value += "1";
    }
    GOLOS_CHECK_NO_THROW(op.validate());
}

BOOST_AUTO_TEST_CASE(account_notes_apply) {
    BOOST_TEST_MESSAGE("Testing: account_notes_apply");

    ACTORS((alice));

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL);

    BOOST_TEST_MESSAGE("-- Create case");

    custom_binary_operation cop;
    cop.required_active_auths.insert("alice");
    cop.id = "account_notes";

    boost::container::vector<account_notes_plugin_operation> vec;
    signed_transaction tx;

    set_value_operation op;
    op.account = "alice";
    op.key = "lorem";
    op.value = "ipsum";

    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();

    const auto& notes_idx = _db.get_index<account_note_index, by_account_key>();
    auto notes_itr = notes_idx.find(std::make_tuple(op.account, op.key));
    BOOST_CHECK(notes_itr != notes_idx.end());

    BOOST_TEST_MESSAGE("-- Edit case");

    op.value = "edited";

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();

    notes_itr = notes_idx.find(std::make_tuple(op.account, op.key));
    BOOST_CHECK_EQUAL(to_string(notes_itr->value), op.value);
}

BOOST_AUTO_TEST_CASE(account_notes_size_limits) {
    BOOST_TEST_MESSAGE("Testing: account_notes_size_limits");

    ACTORS((alice));

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL);

    BOOST_TEST_MESSAGE("-- Long key case");

    custom_binary_operation cop;
    cop.required_active_auths.insert("alice");
    cop.id = "account_notes";

    boost::container::vector<account_notes_plugin_operation> vec;
    signed_transaction tx;

    set_value_operation op;
    op.account = "alice";
    op.key = "123456789012345678901234567890123456789012345678901";
    op.value = "ipsum";

    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();

    const auto& notes_idx = _db.get_index<account_note_index, by_account_key>();
    auto notes_itr = notes_idx.find(std::make_tuple(op.account, op.key));
    BOOST_CHECK(notes_itr == notes_idx.end());

    BOOST_TEST_MESSAGE("-- Long value case");

    op.key = "lorem";
    op.value = std::string(size_t(UINT16_MAX) + 1, '1');

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();

    notes_itr = notes_idx.find(std::make_tuple(op.account, op.key));
    BOOST_CHECK(notes_itr == notes_idx.end());
}

BOOST_AUTO_TEST_CASE(account_notes_count_limit_delete) {
    BOOST_TEST_MESSAGE("Testing: account_notes_count_limit_delete");

    ACTORS((bob)(alice));

    generate_blocks(60 / STEEMIT_BLOCK_INTERVAL);

    BOOST_TEST_MESSAGE("-- Create one note as bob");

    custom_binary_operation cop;
    cop.required_active_auths.insert("bob");
    cop.id = "account_notes";

    boost::container::vector<account_notes_plugin_operation> vec;
    signed_transaction tx;

    set_value_operation op;
    op.account = "bob";
    op.key = "lorem0";
    op.value = "ipsum";

    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, cop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Create 10 alice notes");

    cop.required_active_auths.clear();
    cop.required_active_auths.insert("alice");

    op.account = "alice";
    for (auto i = 1; i <= 10; i++) {
        op.key = std::string("lorem").append(std::to_string(i));

        vec.clear();
        vec.push_back(op);
        cop.data = fc::raw::pack(vec);

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Create 11th alice note (failure)");

    op.key = "lorem11";

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();

    const auto& notes_idx = _db.get_index<account_note_index, golos::plugins::account_notes::by_account_key>();
    BOOST_CHECK_EQUAL(notes_idx.count(op.account), 10);

    const auto& stats_idx = _db.get_index<account_note_stats_index, golos::plugins::account_notes::by_account>();
    auto stats_itr = stats_idx.find(op.account);
    BOOST_CHECK(stats_itr != stats_idx.end());
    BOOST_CHECK_EQUAL(stats_itr->note_count, 10);

    BOOST_TEST_MESSAGE("-- Delete one of notes");

    op.key = "lorem10";
    op.value = "";

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();
    BOOST_CHECK_EQUAL(notes_idx.count(op.account), 9);
    BOOST_CHECK_EQUAL(stats_itr->note_count, 9);

    BOOST_TEST_MESSAGE("-- Try create note again");

    op.key = "lorem11";
    op.value = "ipsum";

    vec.clear();
    vec.push_back(op);
    cop.data = fc::raw::pack(vec);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));
    generate_block();
    BOOST_CHECK_EQUAL(notes_idx.count(op.account), 10);
    BOOST_CHECK_EQUAL(stats_itr->note_count, 10);
}

BOOST_AUTO_TEST_SUITE_END()
