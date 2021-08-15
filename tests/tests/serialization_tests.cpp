/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifdef STEEMIT_BUILD_TESTNET

#include <boost/test/unit_test.hpp>

#include <golos/chain/steem_objects.hpp>
#include <golos/chain/database.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/variant.hpp>

#include "database_fixture.hpp"

#include <cmath>

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

BOOST_FIXTURE_TEST_SUITE(serialization_tests, clean_database_fixture)

    BOOST_AUTO_TEST_CASE(serialization_raw_test) {
        BOOST_TEST_MESSAGE("Testing: serialization_raw_test");

        try {
            ACTORS((alice)(bob))
            transfer_operation op;
            op.from = "alice";
            op.to = "bob";
            op.amount = asset(100, STEEM_SYMBOL);

            trx.operations.push_back(op);
            auto packed = fc::raw::pack(trx);
            signed_transaction unpacked = fc::raw::unpack<signed_transaction>(packed);
            unpacked.validate();
            BOOST_CHECK_EQUAL(trx.digest(), unpacked.digest());
        } catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(serialization_json_test) {
        BOOST_TEST_MESSAGE("Testing: serialization_json_test");

        try {
            ACTORS((alice)(bob))
            transfer_operation op;
            op.from = "alice";
            op.to = "bob";
            op.amount = asset(100, STEEM_SYMBOL);

            fc::variant test(op.amount);
            auto tmp = test.as<asset>();
            BOOST_CHECK_EQUAL(tmp, op.amount);

            trx.operations.push_back(op);
            fc::variant packed(trx);
            signed_transaction unpacked = packed.as<signed_transaction>();
            unpacked.validate();
            BOOST_CHECK_EQUAL(trx.digest(), unpacked.digest());
        } catch (fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(asset_test) {
        BOOST_TEST_MESSAGE("Testing: asset_test");

        try {
            BOOST_CHECK_EQUAL(asset().decimals(), 3);
            BOOST_CHECK_EQUAL(asset().symbol_name(), "GOLOS");
            BOOST_CHECK_EQUAL(asset().to_string(), "0.000 GOLOS");

            BOOST_TEST_MESSAGE("-- Basically test from_string");
            asset steem = asset::from_string("123.456 GOLOS");
            asset sbd = asset::from_string("654.321 GBG");
            asset tmp = asset::from_string("0.456 GOLOS");
            BOOST_CHECK_EQUAL(tmp.amount.value, 456);
            tmp = asset::from_string("0.056 GOLOS");
            BOOST_CHECK_EQUAL(tmp.amount.value, 56);

            BOOST_TEST_MESSAGE("-- Test GOLOS-symbol assets");
            BOOST_CHECK(std::abs(steem.to_real() - 123.456) < 0.0005);
            BOOST_CHECK_EQUAL(steem.amount.value, 123456);
            BOOST_CHECK_EQUAL(steem.decimals(), 3);
            BOOST_CHECK_EQUAL(steem.symbol_name(), "GOLOS");
            BOOST_CHECK_EQUAL(steem.to_string(), "123.456 GOLOS");
            BOOST_CHECK_EQUAL(steem.symbol, STEEM_SYMBOL);
            BOOST_CHECK_EQUAL(asset(50, STEEM_SYMBOL).to_string(), "0.050 GOLOS");
            BOOST_CHECK_EQUAL(asset(50000, STEEM_SYMBOL).to_string(), "50.000 GOLOS");

            BOOST_TEST_MESSAGE("-- Test GBG-symbol assets");
            BOOST_CHECK(std::abs(sbd.to_real() - 654.321) < 0.0005);
            BOOST_CHECK_EQUAL(sbd.amount.value, 654321);
            BOOST_CHECK_EQUAL(sbd.decimals(), 3);
            BOOST_CHECK_EQUAL(sbd.symbol_name(), "GBG");
            BOOST_CHECK_EQUAL(sbd.to_string(), "654.321 GBG");
            BOOST_CHECK_EQUAL(sbd.symbol, SBD_SYMBOL);
            BOOST_CHECK_EQUAL(asset(50, SBD_SYMBOL).to_string(), "0.050 GBG");
            BOOST_CHECK_EQUAL(asset(50000, SBD_SYMBOL).to_string(), "50.000 GBG");

            BOOST_TEST_MESSAGE("-- Test failure when asset exceeded (by hack) 15 decimals limit");
            BOOST_CHECK_THROW(steem.set_decimals(20), fc::exception);
            char *steem_sy = (char *) &steem.symbol;
            steem_sy[0] = 20;
            BOOST_CHECK_THROW(steem.decimals(), fc::exception);
            steem_sy[6] = 'A';
            steem_sy[7] = 'A';

            auto check_sym = [](const asset &a) -> std::string {
                auto symbol = a.symbol_name();
                wlog("symbol_name is ${s}", ("s", symbol));
                return symbol;
            };

            BOOST_CHECK_THROW(check_sym(steem), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when exceeded 15 decimals limit (with from_string)");
            BOOST_CHECK_THROW(asset::from_string("1.00000000000000000000 GOLOS"), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when no space between amount and symbol");
            BOOST_CHECK_THROW(asset::from_string("1.000GOLOS"), fc::exception);

            BOOST_TEST_MESSAGE("-- Spaces around amount dot");
            // Fails because symbol is '12345678901 GOLOS', which is too long
            BOOST_CHECK_THROW(asset::from_string("1. 12345678901 GOLOS"), fc::exception);
            BOOST_CHECK_THROW(asset::from_string("1 .333 GOLOS"), fc::exception);
            asset unusual = asset::from_string("1. 333 X"); // Passes because symbol '333 X' is short enough
            FC_ASSERT(unusual.decimals() == 0);
            FC_ASSERT(unusual.symbol_name() == "333 X");
            BOOST_CHECK_THROW(asset::from_string("1 .333 X"), fc::exception);
            BOOST_CHECK_THROW(asset::from_string("1 .333"), fc::exception);
            BOOST_CHECK_THROW(asset::from_string("1 1.1"), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure with too big integer part of asset");
            BOOST_CHECK_THROW(
                asset::from_string("11111111111111111111111111111111111111111111111 GOLOS"),
                fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure with wrong amount");
            BOOST_CHECK_THROW(asset::from_string("1.1.1 GOLOS"), fc::exception);
            BOOST_CHECK_THROW(asset::from_string("1.abc GOLOS"), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure with no amount");
            BOOST_CHECK_THROW(asset::from_string(" GOLOS"), fc::exception);
            BOOST_CHECK_THROW(asset::from_string("GOLOS"), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure with no symbol");
            BOOST_CHECK_THROW(asset::from_string("1.333"), fc::exception);
            BOOST_CHECK_THROW(asset::from_string("1.333 "), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure with empty string");
            BOOST_CHECK_THROW(asset::from_string(""), fc::exception);
            BOOST_CHECK_THROW(asset::from_string(" "), fc::exception);
            BOOST_CHECK_THROW(asset::from_string("  "), fc::exception);

            BOOST_TEST_MESSAGE("-- Test integer-amount asset");
            BOOST_CHECK_EQUAL(asset::from_string("100 GOLOS").amount.value, 100);
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(asset_uia_test) {
        BOOST_TEST_MESSAGE("Testing: asset_uia_test");

        try {
            asset cats;

            BOOST_TEST_MESSAGE("-- Basically test");
            cats = asset::from_string("123.456789 CATS");
            BOOST_CHECK_EQUAL(cats.amount.value, 123456789);
            BOOST_CHECK_EQUAL(cats.decimals(), 6);
            BOOST_CHECK_EQUAL(cats.symbol_name(), "CATS");
            BOOST_CHECK_EQUAL(cats.to_string(), "123.456789 CATS");

            BOOST_TEST_MESSAGE("-- Test with 2-part symbol");
            cats = asset::from_string("123.456789 CATS.CATS");
            BOOST_CHECK_EQUAL(cats.amount.value, 123456789);
            BOOST_CHECK_EQUAL(cats.decimals(), 6);
            BOOST_CHECK_EQUAL(cats.symbol_name(), "CATS.CATS");
            BOOST_CHECK_EQUAL(cats.to_string(), "123.456789 CATS.CATS");
        }
        FC_LOG_AND_RETHROW()
    }

    BOOST_AUTO_TEST_CASE(json_tests) {
        BOOST_TEST_MESSAGE("Testing: json_tests");

        try {
            auto var = fc::json::variants_from_string("10.6 ");
            var = fc::json::variants_from_string("10.5");
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(extended_private_key_type_test) {
        BOOST_TEST_MESSAGE("Testing: extended_private_key_type_test");

        try {
            fc::ecc::extended_private_key key =
                fc::ecc::extended_private_key(
                    fc::ecc::private_key::generate(),
                    fc::sha256(),
                    0, 0, 0);
            extended_private_key_type type = extended_private_key_type(key);
            std::string packed = std::string(type);
            extended_private_key_type unpacked = extended_private_key_type(packed);
            BOOST_CHECK(type == unpacked);
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(extended_public_key_type_test) {
        BOOST_TEST_MESSAGE("Testing: extended_public_key_type_test");

        try {
            fc::ecc::extended_public_key key =
                fc::ecc::extended_public_key(
                    fc::ecc::private_key::generate().get_public_key(),
                    fc::sha256(),
                    0, 0, 0);
            extended_public_key_type type = extended_public_key_type(key);
            std::string packed = std::string(type);
            extended_public_key_type unpacked = extended_public_key_type(packed);
            BOOST_CHECK(type == unpacked);
        } catch (const fc::exception &e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    BOOST_AUTO_TEST_CASE(version_test) {
        BOOST_TEST_MESSAGE("Testing: version_test");

        try {
            BOOST_REQUIRE_EQUAL(string(version(1, 2, 3)), "1.2.3");

            fc::variant ver_str("3.0.0");
            version ver;
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == version(3, 0, 0));

            ver_str = fc::variant("0.0.0");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == version());

            ver_str = fc::variant("1.0.1");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == version(1, 0, 1));

            ver_str = fc::variant("1_0_1");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == version(1, 0, 1));

            ver_str = fc::variant("12.34.56");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == version(12, 34, 56));

            BOOST_TEST_MESSAGE("-- Test failure when major version > 0xFF");
            ver_str = fc::variant("256.0.0");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when hardfork version > 0xFF");
            ver_str = fc::variant("0.256.0");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when revision version > 0xFFFF");
            ver_str = fc::variant("0.0.65536");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when too few parts");
            ver_str = fc::variant("1.0");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when too many parts");
            ver_str = fc::variant("1.0.0.1");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);
            ver_str = fc::variant("1.0.0.");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);
        }
        FC_LOG_AND_RETHROW();
    }

    BOOST_AUTO_TEST_CASE(hardfork_version_test) {
        BOOST_TEST_MESSAGE("Testing: hardfork_version_test");

        try {
            BOOST_REQUIRE_EQUAL(string(hardfork_version(1, 2)), "1.2.0");

            fc::variant ver_str("3.0.0");
            hardfork_version ver;
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == hardfork_version(3, 0));

            ver_str = fc::variant("0.0.0");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == hardfork_version());

            ver_str = fc::variant("1.0.0");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == hardfork_version(1, 0));

            ver_str = fc::variant("1_0_0");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == hardfork_version(1, 0));

            ver_str = fc::variant("12.34.00");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == hardfork_version(12, 34));

            BOOST_TEST_MESSAGE("-- Test failure when major version > 0xFF");
            ver_str = fc::variant("256.0.0");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when hardfork version > 0xFF");
            ver_str = fc::variant("0.256.0");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);

            ver_str = fc::variant("0.0.1");
            fc::from_variant(ver_str, ver);
            BOOST_CHECK(ver == hardfork_version(0, 0));

            BOOST_TEST_MESSAGE("-- Test failure when too few parts");
            ver_str = fc::variant("1.0");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);

            BOOST_TEST_MESSAGE("-- Test failure when too many parts");
            ver_str = fc::variant("1.0.0.1");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);
            ver_str = fc::variant("1.0.0.");
            STEEMIT_CHECK_THROW(fc::from_variant(ver_str, ver), fc::exception);
        }
        FC_LOG_AND_RETHROW();
    }

BOOST_AUTO_TEST_CASE(unpack_clear_test) { try {
    BOOST_TEST_MESSAGE("Testing: unpack_clear_test");

    signed_block b1;

    for (int i = 0; i < 10; i++) {
        signed_transaction tx;

        vote_operation op;
        op.voter = "alice";
        op.author = "bob";
        op.permlink = "permlink1";
        op.weight = STEEMIT_100_PERCENT;
        tx.operations.push_back(op);

        vote_operation op2;
        op2.voter = "charlie";
        op2.author = "sam";
        op2.permlink = "permlink2";
        op2.weight = STEEMIT_100_PERCENT;
        tx.operations.push_back(op2);

        tx.ref_block_num = 1000;
        tx.ref_block_prefix = 1000000000;
        tx.expiration = fc::time_point_sec(1514764800 + i);

        b1.transactions.push_back(tx);
    }

    signed_block b2;

    for (int i = 0; i < 20; i++) {
        signed_transaction tx;
        vote_operation op;
        op.voter = "dave";
        op.author = "greg";
        op.permlink = "foobar";
        op.weight = STEEMIT_100_PERCENT/2;
        tx.ref_block_num = 4000;
        tx.ref_block_prefix = 4000000000;
        tx.expiration = fc::time_point_sec(1714764800 + i);
        tx.operations.push_back(op);

        b2.transactions.push_back(tx);
    }

    std::stringstream ss2;
    fc::raw::pack(ss2, b2);

    std::stringstream ss1;
    fc::raw::pack(ss1, b1);

    signed_block unpacked_block;
    fc::raw::unpack(ss2, unpacked_block);

    // This operation should completely overwrite signed block 'b2'
    fc::raw::unpack(ss1, unpacked_block);

    BOOST_CHECK_EQUAL(b1.transactions.size(), unpacked_block.transactions.size());
    for (size_t i = 0; i < unpacked_block.transactions.size(); i++) {
        signed_transaction tx = unpacked_block.transactions[i];
        BOOST_CHECK_EQUAL(unpacked_block.transactions[i].operations.size(), b1.transactions[i].operations.size());

        vote_operation op = tx.operations[0].get<vote_operation>();
        BOOST_CHECK_EQUAL(op.voter, "alice");
        BOOST_CHECK_EQUAL(op.author, "bob");
        BOOST_CHECK_EQUAL(op.permlink, "permlink1");
        BOOST_CHECK_EQUAL(op.weight, STEEMIT_100_PERCENT);

        vote_operation op2 = tx.operations[1].get<vote_operation>();
        BOOST_CHECK_EQUAL(op2.voter, "charlie");
        BOOST_CHECK_EQUAL(op2.author, "sam");
        BOOST_CHECK_EQUAL(op2.permlink, "permlink2");
        BOOST_CHECK_EQUAL(op2.weight, STEEMIT_100_PERCENT);

        BOOST_CHECK_EQUAL(tx.ref_block_num, 1000);
        BOOST_CHECK_EQUAL(tx.ref_block_prefix, 1000000000);
        BOOST_CHECK_EQUAL(tx.expiration, fc::time_point_sec(1514764800 + i));
    }
} FC_LOG_AND_RETHROW(); }

BOOST_AUTO_TEST_CASE(unpack_recursion_test) { try {
    BOOST_TEST_MESSAGE("Testing: unpack_recursion_test");

    std::stringstream ss;
    int recursion_level = 100000;
    uint64_t allocation_per_level = 500000;
    for (int i = 0; i < recursion_level; i++) {
        fc::raw::pack(ss, unsigned_int(allocation_per_level));
        fc::raw::pack(ss, static_cast<uint8_t>(variant::array_type));
    }
    std::vector<fc::variant> v;
    STEEMIT_CHECK_THROW(fc::raw::unpack(ss, v), fc::assert_exception);
} FC_LOG_AND_RETHROW(); }


BOOST_AUTO_TEST_SUITE_END()
#endif
