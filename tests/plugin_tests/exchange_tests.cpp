#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

#include <golos/chain/account_object.hpp>
#include <golos/plugins/exchange/exchange.hpp>
#include <golos/plugins/exchange/exchange_queries.hpp>

#include <string>
#include <cstdint>

using golos::chain::database_fixture;

using namespace golos::protocol;
using namespace golos::chain;
using golos::plugins::json_rpc::msg_pack;
using namespace golos::plugins::exchange;

struct exchange_fixture : public database_fixture {
    template<typename... Plugins>
    void initialize(const database_fixture::plugin_options& opts = {}) {
        database_fixture::initialize<exchange>(opts);
        ex_plugin = find_plugin<exchange>();
        open_database();
        startup();
    }

    fc::mutable_variant_object get_exchange(const exchange_query& q) const {
        msg_pack mp;
        mp.args = std::vector<fc::variant>({fc::variant(q)});
        return ex_plugin->get_exchange(mp);
    }

    void create_GBGF() {
        asset_create_operation op;
        op.creator = STEEMIT_INIT_MINER_NAME;
        op.max_supply = ASSET("1000000.000 GBGF");
        op.json_metadata = "{}";
        op.allow_fee = true;
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, init_account_priv_key, op));
    }

    void create_GOLOSF() {
        asset_create_operation op;
        op.creator = STEEMIT_INIT_MINER_NAME;
        op.max_supply = ASSET("1000000.000 GOLOSF");
        op.json_metadata = "{}";
        op.allow_fee = true;
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, init_account_priv_key, op));
    }

    void create_AAA() {
        asset_create_operation op;
        op.creator = STEEMIT_INIT_MINER_NAME;
        op.max_supply = ASSET("1000000.000 AAA");
        op.json_metadata = "{}";
        op.allow_fee = true;
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, init_account_priv_key, op));
    }

    void issue(asset amount) {
        asset_issue_operation op;
        op.creator = STEEMIT_INIT_MINER_NAME;
        op.amount = amount;
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, init_account_priv_key, op));
    }

    void set_fee(const std::string& symbol, uint16_t fee_percent) {
        asset_update_operation op;
        op.creator = STEEMIT_INIT_MINER_NAME;
        op.symbol = symbol;
        op.fee_percent = fee_percent;
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, init_account_priv_key, op));
    }

    void sell(asset for_sale, asset min_receive) {
        limit_order_create_operation op;
        op.owner = STEEMIT_INIT_MINER_NAME;
        op.amount_to_sell = for_sale;
        op.min_to_receive = min_receive;
        op.orderid = ++last_order;
        signed_transaction tx;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, init_account_priv_key, op));
    }

    exchange* ex_plugin = nullptr;
    uint32_t last_order = 0;
};

#define EX_CHAIN_CHECK_REPORT(CHAIN, REPORT) { \
    auto itr = CHAIN.find("_err_report"); \
    std::string _err_report = (itr == CHAIN.end()) ? "" : itr->value().get_string(); \
    BOOST_CHECK_EQUAL(_err_report, REPORT); \
}

#define EX_CHAIN_CHECK_NO_CHAIN(CHAIN) { \
    BOOST_CHECK_EQUAL(fc::json::to_string(CHAIN), "false"); \
}

#define EX_CHAIN_CHECK_RES_ETC(CHAIN, RES, ETC) { \
    BOOST_CHECK_NE(fc::json::to_string(CHAIN), "false"); \
    auto obj = CHAIN.get_object(); \
    EX_CHAIN_CHECK_REPORT(obj, ""); \
    BOOST_CHECK_EQUAL(obj["res"].get_string(), RES); \
    ETC; \
}

#define EX_CHAIN_CHECK_RES(CHAIN, RES) { \
    EX_CHAIN_CHECK_RES_ETC(CHAIN, RES, {}) \
}

#define EX_CHAIN_CHECK_REMAIN(OBJ, STEP, REMAIN) { \
    auto steps = OBJ["steps"].get_array(); \
    BOOST_CHECK(steps.size() > STEP); \
    auto step = steps[STEP].get_object(); \
    \
    auto rem = asset::from_string(REMAIN); \
    if (rem.amount == 0) { \
        if (step.contains("remain")) { \
            BOOST_CHECK_EQUAL(step["remain"].get_string(), REMAIN); \
        } else { \
            BOOST_CHECK(!step.contains("remain")); \
        } \
    } else { \
        BOOST_CHECK(step.contains("remain")); \
        BOOST_CHECK_EQUAL(step["remain"].get_string(), REMAIN); \
    } \
}

BOOST_FIXTURE_TEST_SUITE(exchange_tests, exchange_fixture)

using namespace golos::plugins::exchange;

BOOST_AUTO_TEST_CASE(rows_simple) { try {
    BOOST_TEST_MESSAGE("Testing: rows_simple");

    initialize({});

    ex_rows rows;
    rows.emplace_back(ASSET("0.250 GBG"), ASSET("0.001 AAA"));

    ex_rows rows2;
    rows2.emplace_back(ex_row{ASSET("0.001 AAA"), ASSET("5.500 GOLOS")});

    ex_rows rows3;
    GOLOS_CHECK_NO_THROW(rows3 = ex_plugin->map_rows(rows2, rows));

    BOOST_CHECK_EQUAL(rows3.size(), 1);
    BOOST_CHECK_EQUAL(rows3[0].par, ASSET("0.250 GBG"));
    BOOST_CHECK_EQUAL(rows3[0].res, ASSET("5.500 GOLOS"));
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(rows_splitting) {
    BOOST_TEST_MESSAGE("Testing: rows_splitting");

    initialize({});

    ex_rows rows;
    rows.emplace_back(ASSET("0.250 GBG"), ASSET("0.002 AAA"));

    ex_rows rows2;
    rows2.emplace_back(ex_row{ASSET("0.001 AAA"), ASSET("5.500 GOLOS")});
    rows2.emplace_back(ex_row{ASSET("0.001 AAA"), ASSET("5.000 GOLOS")});

    ex_rows rows3;
    GOLOS_CHECK_NO_THROW(rows3 = ex_plugin->map_rows(rows2, rows));

    BOOST_CHECK_EQUAL(rows3.size(), 2);
    BOOST_CHECK_EQUAL(rows3[0].par, ASSET("0.125 GBG"));
    BOOST_CHECK_EQUAL(rows3[0].res, ASSET("5.500 GOLOS"));
    BOOST_CHECK_EQUAL(rows3[1].par, ASSET("0.125 GBG"));
    BOOST_CHECK_EQUAL(rows3[1].res, ASSET("5.000 GOLOS"));
}

BOOST_AUTO_TEST_CASE(rows_combining) {
    BOOST_TEST_MESSAGE("Testing: rows_combining");

    initialize({});

    ex_rows rows;
    rows.emplace_back(ASSET("0.250 GBG"), ASSET("0.002 AAA"));
    rows.emplace_back(ASSET("0.250 GBG"), ASSET("0.001 AAA"));

    ex_rows rows2;
    rows2.emplace_back(ex_row{ASSET("0.003 AAA"), ASSET("6.000 GOLOS")});

    ex_rows rows3;
    GOLOS_CHECK_NO_THROW(rows3 = ex_plugin->map_rows(rows2, rows));

    BOOST_CHECK_EQUAL(rows3.size(), 2);
    BOOST_CHECK_EQUAL(rows3[0].par, ASSET("0.250 GBG"));
    BOOST_CHECK_EQUAL(rows3[0].res, ASSET("4.000 GOLOS"));
    BOOST_CHECK_EQUAL(rows3[1].par, ASSET("0.250 GBG"));
    BOOST_CHECK_EQUAL(rows3[1].res, ASSET("2.000 GOLOS"));
}

BOOST_AUTO_TEST_CASE(rows_many_steps) {
    BOOST_TEST_MESSAGE("Testing: rows_many_steps");

    initialize({});

    ex_rows rows;
    rows.emplace_back(ASSET("0.250 GBG"), ASSET("0.002 AAA"));
    rows.emplace_back(ASSET("0.250 GBG"), ASSET("0.001 AAA"));

    ex_rows rows2;
    rows2.emplace_back(ex_row{ASSET("0.003 AAA"), ASSET("6.000 BBB")});

    ex_rows rows3;
    GOLOS_CHECK_NO_THROW(rows3 = ex_plugin->map_rows(rows2, rows));

    BOOST_CHECK_EQUAL(rows3.size(), 2);
    BOOST_CHECK_EQUAL(rows3[0].par, ASSET("0.250 GBG"));
    BOOST_CHECK_EQUAL(rows3[0].res, ASSET("4.000 BBB"));
    BOOST_CHECK_EQUAL(rows3[1].par, ASSET("0.250 GBG"));
    BOOST_CHECK_EQUAL(rows3[1].res, ASSET("2.000 BBB"));

    BOOST_TEST_MESSAGE("-- 2nd map_rows (also, tests integer division remainder)");

    ex_rows rows4;
    rows4.emplace_back(ex_row{ASSET("5.000 BBB"), ASSET("1.000 GOLOS")});
    rows4.emplace_back(ex_row{ASSET("0.500 BBB"), ASSET("0.002 GOLOS")});
    rows4.emplace_back(ex_row{ASSET("0.500 BBB"), ASSET("0.001 GOLOS")});

    ex_rows rows5;
    GOLOS_CHECK_NO_THROW(rows5 = ex_plugin->map_rows(rows4, rows3));
    BOOST_CHECK_EQUAL(rows5.size(), 4);
    BOOST_CHECK_EQUAL(rows5[0].par, ASSET("0.250 GBG"));
    BOOST_CHECK_EQUAL(rows5[0].res, ASSET("0.800 GOLOS"));
    BOOST_CHECK_EQUAL(rows5[1].par, ASSET("0.125 GBG"));
    BOOST_CHECK_EQUAL(rows5[1].res, ASSET("0.200 GOLOS"));
    BOOST_CHECK_EQUAL(rows5[2].par, ASSET("0.062 GBG"));
    BOOST_CHECK_EQUAL(rows5[2].res, ASSET("0.002 GOLOS"));
    BOOST_CHECK_EQUAL(rows5[3].par, ASSET("0.063 GBG"));
    BOOST_CHECK_EQUAL(rows5[3].res, ASSET("0.001 GOLOS"));
}

/*BOOST_AUTO_TEST_CASE(rows_remain_1cent) {
    BOOST_TEST_MESSAGE("Testing: rows_remain_1cent");

    initialize({});

    ex_rows rows;
    rows.emplace_back(ASSET("0.001 GBG"), ASSET("0.003 AAA"));
    rows.emplace_back(ASSET("0.250 GBG"), ASSET("0.001 AAA"));

    ex_rows rows2;
    rows2.emplace_back(ex_row{ASSET("0.002 AAA"), ASSET("6.000 GOLOS")});

    ex_rows rows3;
    GOLOS_CHECK_NO_THROW(rows3 = ex_plugin->map_rows(rows2, rows));
    std::cout << fc::json::to_string(rows3);
}*/

BOOST_AUTO_TEST_CASE(exchange_spread_basic) { try {
    BOOST_TEST_MESSAGE("Testing: exchange_spread_basic");

    initialize({});

    BOOST_TEST_MESSAGE("-- Creating assets");

    create_GBGF();
    create_AAA();

    BOOST_TEST_MESSAGE("-- Try with empty market");

    exchange_query qu;
    qu.amount = ASSET("0.500 GBGF");
    qu.symbol = "GOLOS";
    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    fc::mutable_variant_object res;
    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    BOOST_CHECK_EQUAL(fc::json::to_string(res["direct"]), "false");
    BOOST_CHECK_EQUAL(fc::json::to_string(res["best"]), "false");

    BOOST_TEST_MESSAGE("-- Fill market with direct");

    sell(ASSET("6.000 GOLOS"), ASSET("0.100 GBGF"));
    sell(ASSET("10.000 GOLOS"), ASSET("1.000 GBGF"));
    sell(ASSET("50.000 GOLOS"), ASSET("10.000 GBGF"));

    BOOST_TEST_MESSAGE("-- Try only with direct");

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "10.000 GOLOS");
    }

    BOOST_TEST_MESSAGE("-- Fill market with chained");

    issue(ASSET("100000.000 AAA"));

    sell(ASSET("11.000 GOLOS"), ASSET("0.002 AAA"));
    sell(ASSET("0.002 AAA"), ASSET("0.500 GBGF"));

    BOOST_TEST_MESSAGE("-- Try chained without strategy");

    qu.hybrid.strategy = exchange_hybrid_strategy::none;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "10.000 GOLOS");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        BOOST_CHECK_EQUAL(best["res"].get_string(), "11.000 GOLOS");
    }

    BOOST_TEST_MESSAGE("-- Try chained with spread");

    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        EX_CHAIN_CHECK_REPORT(dir, "");
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "10.000 GOLOS");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        BOOST_CHECK(best.find("_err_report") == best.end());
        EX_CHAIN_CHECK_REPORT(best, "");
        BOOST_CHECK_EQUAL(best["res"].get_string(), "13.000 GOLOS");
    }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(exchange_spread_rows) { try {
    BOOST_TEST_MESSAGE("Testing: exchange_spread_rows");

    initialize({});

    BOOST_TEST_MESSAGE("-- Creating assets");

    create_GBGF();
    create_AAA();

    BOOST_TEST_MESSAGE("-- Fill market with direct");

    sell(ASSET("10.000 GOLOS"), ASSET("1.000 GBGF"));
    sell(ASSET("0.500 GOLOS"), ASSET("0.100 GBGF"));
    sell(ASSET("50.000 GOLOS"), ASSET("10.000 GBGF"));

    BOOST_TEST_MESSAGE("-- Fill market with chained");

    issue(ASSET("100000.000 AAA"));

    sell(ASSET("11.000 GOLOS"), ASSET("1.000 AAA"));
    sell(ASSET("1.000 AAA"), ASSET("0.400 GBGF"));
    sell(ASSET("0.001 GOLOS"), ASSET("0.001 AAA"));
    sell(ASSET("0.001 AAA"), ASSET("0.100 GBGF"));

    // Chain's middle price is better than direct... but its 2nd row should be 'moved out'

    BOOST_TEST_MESSAGE("-- Try chained with spread");

    exchange_query qu;
    qu.amount = ASSET("0.500 GBGF");
    qu.symbol = "GOLOS";
    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    fc::mutable_variant_object res;
    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        EX_CHAIN_CHECK_REPORT(dir, "");
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "5.000 GOLOS");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        EX_CHAIN_CHECK_REPORT(best, "");
        BOOST_CHECK_EQUAL(best["res"].get_string(), "12.000 GOLOS");
    }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(exchange_spread_thin_line) { try {
    BOOST_TEST_MESSAGE("Testing: exchange_spread_thin_line");

    initialize({});

    BOOST_TEST_MESSAGE("-- Creating assets");

    create_GBGF();
    create_AAA();

    BOOST_TEST_MESSAGE("-- Fill market with direct");

    sell(ASSET("6.000 GOLOS"), ASSET("0.100 GBGF"));
    sell(ASSET("3.001 GOLOS"), ASSET("0.300 GBGF")); // it is better... but just 1 cent
    sell(ASSET("50.000 GOLOS"), ASSET("10.000 GBGF"));

    BOOST_TEST_MESSAGE("-- Fill market with chained");

    issue(ASSET("100000.000 AAA"));

    sell(ASSET("5.000 GOLOS"), ASSET("1.000 AAA"));
    sell(ASSET("1.000 AAA"), ASSET("0.500 GBGF"));

    BOOST_TEST_MESSAGE("-- Try sell with spread");

    exchange_query qu;
    qu.amount = ASSET("0.500 GBGF");
    qu.symbol = "GOLOS";
    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    fc::mutable_variant_object res;
    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        EX_CHAIN_CHECK_REPORT(dir, "");
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "9.501 GOLOS");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        EX_CHAIN_CHECK_REPORT(best, "");
        BOOST_CHECK_EQUAL(best["res"].get_string(), "10.001 GOLOS");
    }

    /*BOOST_TEST_MESSAGE("-- Try buy with spread");

    qu.direction = exchange_direction::buy;
    qu.amount = ASSET("10.001 GOLOS");
    qu.symbol = "GBGF";
    qu.excess_protect = exchange_excess_protect::none;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        EX_CHAIN_CHECK_REPORT(dir, "");
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "0.600 GBGF");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        EX_CHAIN_CHECK_REPORT(best, "");
        BOOST_TEST_MESSAGE(fc::json::to_string(best));
        BOOST_CHECK_EQUAL(best["res"].get_string(), "0.500 GBGF");
    }*/
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(exchange_spread_buy) { try {
    BOOST_TEST_MESSAGE("Testing: exchange_spread_buy");

    initialize({});

    BOOST_TEST_MESSAGE("-- Creating assets");

    create_GBGF();
    create_AAA();

    BOOST_TEST_MESSAGE("-- Fill market with direct");

    sell(ASSET("3.000 GOLOS"), ASSET("0.100 GBGF"));
    sell(ASSET("3.001 GOLOS"), ASSET("0.300 GBGF"));
    sell(ASSET("50.000 GOLOS"), ASSET("10.000 GBGF"));

    BOOST_TEST_MESSAGE("-- Fill market with chained");

    issue(ASSET("100000.000 AAA"));

    sell(ASSET("5.000 GOLOS"), ASSET("1.000 AAA"));
    sell(ASSET("1.000 AAA"), ASSET("0.500 GBGF"));

    BOOST_TEST_MESSAGE("-- Try sell with spread");

    exchange_query qu;
    qu.amount = ASSET("0.500 GBGF");
    qu.symbol = "GOLOS";
    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    fc::mutable_variant_object res;
    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        EX_CHAIN_CHECK_REPORT(dir, "");
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "6.501 GOLOS");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        EX_CHAIN_CHECK_REPORT(best, "");
        BOOST_CHECK_EQUAL(best["res"].get_string(), "7.001 GOLOS");
    }

    BOOST_TEST_MESSAGE("-- Try buy with spread (spread should just return orig chain, because direct-only is better)");

    qu.direction = exchange_direction::buy;
    qu.amount = ASSET("5.000 GOLOS");
    qu.symbol = "GBGF";
    qu.excess_protect = exchange_excess_protect::none;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        EX_CHAIN_CHECK_REPORT(dir, "");
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "0.300 GBGF");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        EX_CHAIN_CHECK_REPORT(best, "");
        BOOST_CHECK_EQUAL(best["res"].get_string(), "0.300 GBGF"); // should be same as direct
    }
    {
        BOOST_TEST_MESSAGE("---- Checking orig (no-hybrid) chain present");
        auto chains = res["all_chains"].get_array();
        BOOST_CHECK_EQUAL(chains.size(), 2);
        auto orig = chains[1].get_object();
        EX_CHAIN_CHECK_REPORT(orig, "");
        BOOST_CHECK_EQUAL(orig["res"].get_string(), "0.500 GBGF");
    }

    BOOST_TEST_MESSAGE("-- Try buy with spread (chain hasn't enough depth, so spreading)");

    qu.amount = ASSET("8.000 GOLOS");

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    {
        BOOST_CHECK_NE(fc::json::to_string(res["direct"]), "false");
        auto dir = res["direct"].get_object();
        EX_CHAIN_CHECK_REPORT(dir, "");
        BOOST_CHECK_EQUAL(dir["res"].get_string(), "0.800 GBGF");
    }
    {
        BOOST_CHECK_NE(fc::json::to_string(res["best"]), "false");
        auto best = res["best"].get_object();
        EX_CHAIN_CHECK_REPORT(best, "");
        BOOST_TEST_MESSAGE(fc::json::to_string(best));
        //BOOST_CHECK_EQUAL(best["res"].get_string(), "0.300 GBGF"); // should be same as direct
    }
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(exchange_spread_remain) { try {
    BOOST_TEST_MESSAGE("Testing: exchange_spread_remain");

    initialize({});

    BOOST_TEST_MESSAGE("-- Creating assets");

    create_GBGF();
    create_GOLOSF();
    create_AAA();

    BOOST_TEST_MESSAGE("-- Fill market with direct");

    issue(ASSET("100000.000 GOLOSF"));

    sell(ASSET("0.002 GOLOSF"), ASSET("0.001 GBGF"));
    sell(ASSET("0.001 GOLOSF"), ASSET("0.002 GBGF"));
    sell(ASSET("0.001 GOLOSF"), ASSET("0.003 GBGF"));

    BOOST_TEST_MESSAGE("-- Fill market with chained");

    issue(ASSET("100000.000 AAA"));

    sell(ASSET("0.002 GOLOSF"), ASSET("0.002 AAA"));
    sell(ASSET("0.002 AAA"), ASSET("0.002 GBGF"));

    BOOST_TEST_MESSAGE("-- Try sell with spread");

    exchange_query qu;
    qu.amount = ASSET("0.002 GBGF");
    qu.symbol = "GOLOSF";
    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    fc::mutable_variant_object res;
    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    EX_CHAIN_CHECK_RES(res["direct"], "0.002 GOLOSF");
    EX_CHAIN_CHECK_RES(res["best"], "0.003 GOLOSF");

    BOOST_TEST_MESSAGE("-- Try increase sell");

    qu.amount = ASSET("0.003 GBGF");

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    EX_CHAIN_CHECK_RES(res["direct"], "0.003 GOLOSF");
    EX_CHAIN_CHECK_RES(res["best"], "0.004 GOLOSF");

    BOOST_TEST_MESSAGE("-- Try increase sell #2");

    qu.amount = ASSET("0.004 GBGF");

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    EX_CHAIN_CHECK_RES(res["direct"], "0.003 GOLOSF");
    EX_CHAIN_CHECK_RES(res["best"], "0.004 GOLOSF");

    BOOST_TEST_MESSAGE("-- Try increase sell #3 (direct is full, multi has remain, hybrid hasn't remain)");

    qu.amount = ASSET("0.006 GBGF");

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    EX_CHAIN_CHECK_RES(res["direct"], "0.004 GOLOSF");
    EX_CHAIN_CHECK_RES(res["best"], "0.005 GOLOSF");

    BOOST_TEST_MESSAGE("-- Try increase sell #4.1 (with strategy=none. direct has remain, multi has remain)");

    qu.amount = ASSET("0.009 GBGF");
    qu.hybrid.strategy = exchange_hybrid_strategy::none;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    EX_CHAIN_CHECK_RES_ETC(res["direct"], "0.004 GOLOSF", {
        EX_CHAIN_CHECK_REMAIN(obj, 0, "0.003 GBGF");
    });

    BOOST_TEST_MESSAGE("---- Checking best is direct");
    EX_CHAIN_CHECK_RES(res["best"], "0.004 GOLOSF");

    BOOST_TEST_MESSAGE("---- Checking multi chain - res and remain");
    {
        auto chains = res["all_chains"].get_array();
        BOOST_CHECK_EQUAL(chains.size(), 2);
        EX_CHAIN_CHECK_RES_ETC(chains[1], "0.002 GOLOSF", {
            EX_CHAIN_CHECK_REMAIN(obj, 0, "0.007 GBGF");
        });
    }

    BOOST_TEST_MESSAGE("-- Try increase sell #4.2 (direct has remain, hybrid has remain)");

    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));
    EX_CHAIN_CHECK_RES_ETC(res["direct"], "0.004 GOLOSF", {
        EX_CHAIN_CHECK_REMAIN(obj, 0, "0.003 GBGF");
    });
    EX_CHAIN_CHECK_RES_ETC(res["best"], "0.006 GOLOSF", {
        EX_CHAIN_CHECK_REMAIN(obj, 0, "0.001 GBGF");
    });
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(exchange_spread_remain_in_middle) { try {
    BOOST_TEST_MESSAGE("Testing: exchange_spread_remain_in_middle");

    initialize({});

    BOOST_TEST_MESSAGE("-- Creating assets");

    create_GBGF();
    create_GOLOSF();
    create_AAA();

    BOOST_TEST_MESSAGE("-- Fill market with direct");

    issue(ASSET("100000.000 GOLOSF"));

    sell(ASSET("0.002 GOLOSF"), ASSET("0.001 GBGF"));
    sell(ASSET("0.001 GOLOSF"), ASSET("0.002 GBGF"));
    sell(ASSET("0.001 GOLOSF"), ASSET("0.003 GBGF"));

    BOOST_TEST_MESSAGE("-- Fill market with chained");

    issue(ASSET("100000.000 AAA"));

    sell(ASSET("0.002 GOLOSF"), ASSET("0.002 AAA"));
    sell(ASSET("0.004 AAA"), ASSET("0.004 GBGF"));

    BOOST_TEST_MESSAGE("-- Try sell without strategy");

    exchange_query qu;
    qu.amount = ASSET("0.004 GBGF");
    qu.symbol = "GOLOSF";
    qu.hybrid.strategy = exchange_hybrid_strategy::none;

    fc::mutable_variant_object res;
    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));

    EX_CHAIN_CHECK_RES(res["direct"], "0.003 GOLOSF");
    // same as direct. Multi cannot be best because price = res / param,
    // without treating remainder as valuable
    EX_CHAIN_CHECK_RES(res["best"], "0.003 GOLOSF");

    {
        auto chains = res["all_chains"].get_array();
        BOOST_CHECK_EQUAL(chains.size(), 2);
        EX_CHAIN_CHECK_RES_ETC(chains[1], "0.002 GOLOSF", {
            EX_CHAIN_CHECK_REMAIN(obj, 1, "0.002 AAA");
        });
    }

    BOOST_TEST_MESSAGE("-- Try sell with spread");

    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));

    EX_CHAIN_CHECK_RES(res["direct"], "0.003 GOLOSF");

    EX_CHAIN_CHECK_RES_ETC(res["best"], "0.004 GOLOSF", {
        // will sell 0.003 GBGF, and split 0.001 GBGF to subchain because order #1 is better
        EX_CHAIN_CHECK_REMAIN(obj, 1, "0.001 AAA");
    });
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(exchange_spread_zero_orders) { try {
    BOOST_TEST_MESSAGE("Testing: exchange_spread_zero_orders");

    initialize({});

    BOOST_TEST_MESSAGE("-- Creating assets");

    create_GBGF();
    create_GOLOSF();
    create_AAA();

    BOOST_TEST_MESSAGE("-- Fill market with chained");

    issue(ASSET("100000.000 GOLOSF"));
    issue(ASSET("100000.000 AAA"));

    sell(ASSET("0.002 GOLOSF"), ASSET("0.002 AAA"));
    sell(ASSET("0.001 GOLOSF"), ASSET("0.002 AAA"));
    sell(ASSET("0.001 GOLOSF"), ASSET("1.000 AAA"));
    sell(ASSET("0.002 AAA"), ASSET("0.002 GBGF"));
    sell(ASSET("0.001 AAA"), ASSET("1.000 GBGF"));
    sell(ASSET("0.001 AAA"), ASSET("2.000 GBGF"));
    sell(ASSET("0.001 AAA"), ASSET("2.000 GBGF"));

    BOOST_TEST_MESSAGE("-- Try sell without strategy");

    exchange_query qu;
    qu.amount = ASSET("1.000 GBGF");
    qu.symbol = "GOLOSF";
    qu.hybrid.strategy = exchange_hybrid_strategy::none;

    fc::mutable_variant_object res;
    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));

    EX_CHAIN_CHECK_NO_CHAIN(res["direct"]);

    EX_CHAIN_CHECK_RES_ETC(res["best"], "0.002 GOLOSF", {
        // Can also check limit_price includes zero-order...
        EX_CHAIN_CHECK_REMAIN(obj, 0, "0.000 GBGF");
        EX_CHAIN_CHECK_REMAIN(obj, 1, "0.000 AAA");
    });

    BOOST_TEST_MESSAGE("-- Try sell with strategy");

    qu.hybrid.strategy = exchange_hybrid_strategy::spread;

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));

    EX_CHAIN_CHECK_NO_CHAIN(res["direct"]);

    EX_CHAIN_CHECK_RES_ETC(res["best"], "0.002 GOLOSF", {
        EX_CHAIN_CHECK_REMAIN(obj, 0, "0.000 GBGF");
        EX_CHAIN_CHECK_REMAIN(obj, 1, "0.000 AAA");
        auto subchains = obj["subchains"].get_array();
        BOOST_CHECK_EQUAL(subchains.size(), 0);
    });

    BOOST_TEST_MESSAGE("-- Try sell with strategy #2");

    qu.amount = ASSET("3.002 GBGF");

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));

    EX_CHAIN_CHECK_NO_CHAIN(res["direct"]);

    EX_CHAIN_CHECK_RES_ETC(res["best"], "0.003 GOLOSF", {
        EX_CHAIN_CHECK_REMAIN(obj, 0, "0.000 GBGF");
        EX_CHAIN_CHECK_REMAIN(obj, 1, "0.000 AAA");
        auto subchains = obj["subchains"].get_array();
        BOOST_CHECK_EQUAL(subchains.size(), 0);
    });

    BOOST_TEST_MESSAGE("-- Try sell with strategy #3");

    qu.amount = ASSET("5.002 GBGF");

    GOLOS_CHECK_NO_THROW(res = get_exchange(qu));

    EX_CHAIN_CHECK_NO_CHAIN(res["direct"]);

    EX_CHAIN_CHECK_RES_ETC(res["best"], "0.003 GOLOSF", {
        BOOST_TEST_MESSAGE(fc::json::to_string(obj));
        EX_CHAIN_CHECK_REMAIN(obj, 0, "0.000 GBGF");
        EX_CHAIN_CHECK_REMAIN(obj, 1, "0.000 AAA");
        auto subchains = obj["subchains"].get_array();
        BOOST_CHECK_EQUAL(subchains.size(), 0);
    });
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
