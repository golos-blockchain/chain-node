#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"

namespace golos { namespace chain {

struct hf26_database_fixture : public database_fixture {
    hf26_database_fixture() {
        try {
            initialize();
            open_database();

            generate_block();
            db->set_hardfork(STEEMIT_HARDFORK_0_25);
            startup(false);
        } catch (const fc::exception& e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    ~hf26_database_fixture() {
    }
};

} } // golos::chain

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

// Requires build with MAX_19_VOTED_WITNESSES=TRUE
BOOST_FIXTURE_TEST_SUITE(hf26_tests, hf26_database_fixture)

    BOOST_AUTO_TEST_CASE(convert_validation) { try {
        BOOST_TEST_MESSAGE("Testing: convert_validation");

        ACTORS((alice))
        generate_block();
        vest("alice", ASSET("10.000 GOLOS"));

        signed_transaction tx;

        BOOST_TEST_MESSAGE("Testing failure with VESTS symbol");

        convert_operation op;
        op.owner = "alice";
        op.amount = ASSET("1.000000 GESTS");
        op.requestid = 1;

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(invalid_parameter, "amount")));

        BOOST_TEST_MESSAGE("Testing failure with GOLOS symbol");

        op.amount = ASSET("1.000 GOLOS");

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(invalid_parameter, "amount")));

        BOOST_TEST_MESSAGE("-- Apply HF");

        db->set_hardfork(STEEMIT_HARDFORK_0_26);
        validate_database();

        BOOST_TEST_MESSAGE("Testing failure with VESTS symbol");

        op.amount = ASSET("1.000000 GESTS");

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(invalid_parameter, "amount")));

        BOOST_TEST_MESSAGE("Testing failure with GOLOS symbol");

        op.amount = ASSET("0.020 GOLOS");

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(insufficient_funds, "alice", "fund", "0.020 GOLOS")));

        BOOST_TEST_MESSAGE("Testing failure with GOLOS + too low amount");

        op.amount = ASSET("0.019 GOLOS");

        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
            CHECK_ERROR(tx_invalid_operation, 0,
                CHECK_ERROR(logic_exception, logic_exception::amount_is_too_low)));
    } FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()