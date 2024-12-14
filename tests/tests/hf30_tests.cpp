#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

struct hf30_fixture : public clean_database_fixture_wrap {

    hf30_fixture() : clean_database_fixture_wrap() {
    }

    void issue_nft(account_name_type creator, const private_key_type& creator_active_key,
        const std::string& name) {
        signed_transaction tx;

        nft_issue_operation niop;
        niop.creator = creator;
        niop.name = name;
        niop.to = creator;
        niop.json_metadata = "{\"health\":10}";

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, creator_active_key, niop));

        validate_database();
    }

    void create_nft(const private_key_type& alice_active_key) {
        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create NFT collection");

        nft_collection_operation ncop;
        ncop.creator = "alice";
        ncop.name = "COOLGAME";
        ncop.json_metadata = "{}";
        ncop.max_token_count = 4294967295;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_active_key, ncop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Issue NFT");

        fund("alice", ASSET("20.000 GBG"));

        issue_nft("alice", alice_active_key, "COOLGAME");
    }
};

BOOST_FIXTURE_TEST_SUITE(hf30_tests, hf30_fixture)

    BOOST_AUTO_TEST_CASE(nft_offer_test) { try {
        BOOST_TEST_MESSAGE("Testing: nft_offer_test");
        signed_transaction tx;

        ACTORS((alice)(bob)(carol))
        generate_block();

        create_nft(alice_active_key);

        BOOST_TEST_MESSAGE("-- Placing offer (fail - without fund)");

        nft_buy_operation nbop;
        nbop.buyer = "bob";
        nbop.name = "COOLGAME"; // for offer it isn't omitting
        nbop.token_id = 1;
        nbop.order_id = 1;
        nbop.price = asset(1000, SBD_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(insufficient_funds, "bob", "fund", "1.000 GBG")
            )
        );

        BOOST_TEST_MESSAGE("-- Placing offer");

        fund("bob", ASSET("1.000 GBG"));

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_active_key, nbop));

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Placing offer again (fail; same price)");

        fund("bob", ASSET("3.000 GBG"));

        nbop.order_id = 2;
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Placing offer again (with another price)");

        nbop.order_id = 2;
        nbop.price = asset(2000, SBD_SYMBOL);
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_active_key, nbop));

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check token and orders");
        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.owner, "alice");

            const auto* order = _db.find_nft_order("bob", 1);
            BOOST_CHECK_NE(order, nullptr);
            BOOST_CHECK_EQUAL(order->order_id, 1);
            order = _db.find_nft_order("bob", 2);
            BOOST_CHECK_NE(order, nullptr);
            BOOST_CHECK_EQUAL(order->order_id, 2);
        }

        BOOST_TEST_MESSAGE("-- Placing auction bet (fail - no auction)");

        nbop.order_id = 0;
        nbop.price = asset(1000, SBD_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Selling token to offer (fail - price is greater than offer)");

        nft_sell_operation nsop;
        nsop.seller = "alice";
        nsop.token_id = 1;
        nsop.buyer = "bob";
        nsop.price = asset(3000, SBD_SYMBOL);
        nsop.order_id = 2;

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Selling token to offer (fail; wrong price symbol)");

        nsop.price = asset(2000, STEEM_SYMBOL);

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Selling token to offer");

        nsop.price = asset(2000, SBD_SYMBOL);
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_active_key, nsop));

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check token and order (should be no order)");

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.owner, "bob");

            const auto* order = _db.find_nft_order("bob", 1);
            BOOST_CHECK_EQUAL(order, nullptr);
            order = _db.find_nft_order("bob", 2);
            BOOST_CHECK_EQUAL(order, nullptr);
        }

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(nft_auction_test) { try {
        BOOST_TEST_MESSAGE("Testing: nft_auction_test");
        signed_transaction tx;

        ACTORS((alice)(bob)(carol))
        generate_block();

        create_nft(alice_active_key);

        BOOST_TEST_MESSAGE("-- Placing auction bet (fail - no auction)");

        fund("bob", ASSET("1.000 GBG"));

        nft_buy_operation nbop;
        nbop.buyer = "bob";
        nbop.name = ""; // for bet it can be omitted or not
        nbop.token_id = 1;
        nbop.order_id = 0;
        nbop.price = asset(1000, SBD_SYMBOL);

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Start auction (fail: not by token owner)");

        nft_auction_operation naop;
        naop.owner = "bob";
        naop.token_id = 1;
        naop.min_price = asset{1, STEEM_SYMBOL};
        naop.expiration = _db.head_block_time() + 180;

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, naop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Start auction (fail: non-existant asset)");

        naop.owner = "alice";
        naop.min_price = asset::from_string("1.0000000 FAKE");

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_active_key, naop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Start auction (fail: expiration in past)");

        naop.min_price = asset{1, STEEM_SYMBOL};
        naop.expiration = _db.head_block_time() - 180;

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_active_key, naop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Start auction (fail: zero-amount which cancels but no auction)");

        naop.expiration = _db.head_block_time() + 180;
        naop.min_price = asset{0, STEEM_SYMBOL};

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_active_key, naop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Start auction (fail: zero-expiration which cancels but no auction)");

        naop.min_price = asset{1, STEEM_SYMBOL};
        naop.expiration = time_point_sec();

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_active_key, naop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Start auction");

        naop.expiration = _db.head_block_time() + 180;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_active_key, naop));

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Placing offer (fail - selling by auction)");

        nbop.name = "COOLGAME"; // for offer it isn't omitting
        nbop.order_id = 1;

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Check token");

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.selling, false);
            BOOST_CHECK_EQUAL(nft.auction_min_price, naop.min_price);
            BOOST_CHECK_EQUAL(nft.auction_expiration, naop.expiration);
        }

        BOOST_TEST_MESSAGE("-- Trying to sell with fixed price (should be success)");

        nft_sell_operation nsop;
        nsop.seller = "alice";
        nsop.token_id = 1;
        nsop.buyer = "";
        nsop.price = asset(1000, SBD_SYMBOL);
        nsop.order_id = 1;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_active_key, nsop));

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check token");

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.selling, true);
            BOOST_CHECK_EQUAL(nft.auction_min_price, naop.min_price);
            BOOST_CHECK_EQUAL(nft.auction_expiration, naop.expiration);
        }

        BOOST_TEST_MESSAGE("-- Place bet (fail: without funds)");

        fund("bob", ASSET("0.600 GOLOS"));

        nbop.order_id = 0;
        nbop.price = asset(1000, STEEM_SYMBOL);

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(insufficient_funds, "bob", "fund", "1.000 GOLOS")
            )
        );

        fund("bob", ASSET("0.400 GOLOS"));

        BOOST_TEST_MESSAGE("-- Place bet");

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_active_key, nbop));

        generate_block();
        validate_database();

        BOOST_TEST_MESSAGE("-- Place same bet again (fail: price is same)");

        fund("bob", ASSET("1.000 GOLOS"));

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Place same bet by same account, but another price (fail)");

        nbop.price = asset(800, STEEM_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, bob_active_key, nbop),
            CHECK_ERROR(tx_invalid_operation, 0)
        );

        BOOST_TEST_MESSAGE("-- Place bet by carol");

        fund("carol", ASSET("2.100 GOLOS"));

        _op["nft_buy"]({ "carol", "", 1, 0, "2.000 GOLOS" }).push(carol_active_key);

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Waiting (expiration - 1 block)");

        generate_blocks(naop.expiration + (20*60));

        BOOST_TEST_MESSAGE("-- Checking auction result");

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_NE(nft.auction_expiration, fc::time_point_sec());
            BOOST_CHECK_EQUAL(nft.owner, "alice");
        }

        BOOST_TEST_MESSAGE("-- Generate 1 block");

        generate_block();

        BOOST_TEST_MESSAGE("-- Checking auction result - token is by carol");

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.auction_expiration, fc::time_point_sec());
            BOOST_CHECK_EQUAL(nft.auction_min_price, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(nft.owner, "carol");
        }

        BOOST_TEST_MESSAGE("-- Creating auction by carol");

        auto ex = _db.head_block_time() + 40*60;
        _op["nft_auction"]({ "carol", 1, "2.000 GBG", _var(ex) }).push(carol_active_key);

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Placing bet by alice");

        fund("alice", ASSET("2.001 GBG"));

        _op["nft_buy"]({ "alice", "", 1, 0, "2.001 GBG" }).push(alice_active_key);

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Placing bet by bob");

        fund("bob", ASSET("2.000 GBG"));

        _op["nft_buy"]({ "bob", "", 1, 0, "2.000 GBG" }).push(bob_active_key);

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Waiting (expiration)");

        generate_blocks(ex);
        generate_block();

        BOOST_TEST_MESSAGE("-- Checking auction result - token is alice");

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.auction_expiration, fc::time_point_sec());
            BOOST_CHECK_EQUAL(nft.auction_min_price, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(nft.owner, "alice");
        }

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(nft_auction_sell_offer_test) { try {
        BOOST_TEST_MESSAGE("Testing: nft_auction_sell_offer_test");
        signed_transaction tx;

        ACTORS((alice)(bob)(carol)(dave))
        generate_block();

        create_nft(alice_active_key);

        BOOST_TEST_MESSAGE("-- Selling by alice");

        _op["nft_sell"]({ "alice", 1, "", 1, "2.000 GBG" }).push(alice_active_key);

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Creating offer by bob");

        fund("bob", ASSET("3.000 GOLOS"));

        _op["nft_buy"]({ "bob", "COOLGAME", 1, 2, "3.000 GOLOS" }).push(bob_active_key);

        validate_database();
        generate_block();

        {
            GET_ACTOR(bob);
            BOOST_CHECK_EQUAL(bob.nft_hold_balance, ASSET("3.000 GOLOS"));
        }

        BOOST_TEST_MESSAGE("-- Creating auction");

        auto ex = _db.head_block_time() + 40*60;
        _op["nft_auction"]({ "alice", 1, "3.000 GOLOS", _var(ex) }).push(alice_active_key);

        validate_database();
        generate_block();

        {
            GET_ACTOR(bob);
            BOOST_CHECK_EQUAL(bob.nft_hold_balance, ASSET("0.000 GOLOS"));
        }

        BOOST_TEST_MESSAGE("-- Placing bet by dave");

        fund("dave", ASSET("3.001 GOLOS"));

        _op["nft_buy"]({ "dave", "", 1, 0, "3.001 GOLOS" }).push(dave_active_key);

        validate_database();
        generate_block();

        {
            GET_ACTOR(dave);
            BOOST_CHECK_EQUAL(dave.nft_hold_balance, ASSET("3.001 GOLOS"));
        }

        BOOST_TEST_MESSAGE("-- Buying by carol via fixed order (NOT auction)");

        fund("carol", ASSET("2.000 GBG"));

        _op["nft_buy"]({ "carol", "", 1, 1, "2.000 GBG" }).push(carol_active_key);

        validate_database();
        generate_block();

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.owner, "carol");
            BOOST_CHECK_EQUAL(nft.auction_expiration, ex);
            BOOST_CHECK_EQUAL(nft.auction_min_price, asset(3000, STEEM_SYMBOL));
        }

        {
            GET_ACTOR(dave);
            BOOST_CHECK_EQUAL(dave.nft_hold_balance, ASSET("3.001 GOLOS"));
        }

        BOOST_TEST_MESSAGE("-- Placing selling order by carol");

        _op["nft_sell"]({ "carol", 1, "", 1, "3.000 GBG" }).push(carol_active_key);

        validate_database();
        generate_block();

        {
            const auto& nft = _db.get_nft(1);
            const auto& nco = _db.get_nft_collection(nft.name);
            BOOST_CHECK_EQUAL(nco.sell_order_count, 1);
            BOOST_CHECK_EQUAL(nco.auction_count, 1);
        }

        BOOST_TEST_MESSAGE("-- Waiting for auction expiration");

        generate_blocks(ex);
        validate_database();
        generate_block();
        validate_database();

        BOOST_TEST_MESSAGE("-- Checking result");

        {
            const auto& nft = _db.get_nft(1);
            BOOST_CHECK_EQUAL(nft.owner, "dave");
            BOOST_CHECK_EQUAL(nft.auction_expiration, fc::time_point_sec());
            BOOST_CHECK_EQUAL(nft.auction_min_price, asset(0, STEEM_SYMBOL));

            const auto& nco = _db.get_nft_collection(nft.name);
            BOOST_CHECK_EQUAL(nco.sell_order_count, 0);
            BOOST_CHECK_EQUAL(nco.auction_count, 0);
        }

        {
            GET_ACTOR(dave);
            BOOST_CHECK_EQUAL(dave.nft_hold_balance, ASSET("0.000 GOLOS"));
        }
    } FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
