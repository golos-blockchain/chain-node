#include <boost/test/unit_test.hpp>
#include <golos/plugins/cryptor/cryptor_queries.hpp>
#include <graphene/utilities/key_conversion.hpp>

#include "cryptor_fixture.hpp"
#include "helpers.hpp"

using namespace golos;
using namespace golos::protocol;
using namespace golos::plugins::cryptor;

std::string pk_to_str(const fc::ecc::private_key& pk) {
    return golos::utilities::key_to_wif(pk);
}

BOOST_FIXTURE_TEST_SUITE(cryptor_tests, cryptor_fixture)

BOOST_AUTO_TEST_CASE(decrypt_fee_test) {
    BOOST_TEST_MESSAGE("Testing: decrypt_fee_test");

    signed_transaction tx;

    ACTORS((alice)(bob)(eve))
    generate_block();

    validate_database();

    BOOST_TEST_MESSAGE("-- Creating post");

    encrypt_query eq;
    eq.author = "alice";
    eq.body = "Test it";
    auto enc_res = encrypt_body(eq);
    BOOST_CHECK_EQUAL(enc_res.error, "");
    BOOST_CHECK_NE(enc_res.encrypted, "");

    auto body = fc::mutable_variant_object()
        ("t", "e")("v", 2)("c", enc_res.encrypted);
    comment_create("alice", alice_post_key, "test", "", "test", "Hello", 
        fc::json::to_string(body), "");

    validate_database();

    BOOST_TEST_MESSAGE("-- Setting decrypt fee to post");

    comment_options_operation cop;

    cop.author = "alice";
    cop.permlink = "test";

    comment_decrypt_fee cdf;
    cdf.fee = asset{3, STEEM_SYMBOL};
    cop.extensions.insert(cdf);

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, cop));

    validate_database();

    BOOST_TEST_MESSAGE("-- Trying decrypt (fail: empty account)");

    decrypt_query dq;
    auto dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "account_not_exists");

    BOOST_TEST_MESSAGE("-- Trying decrypt (fail: no signature)");

    dq.account = "bob";

    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "illformed_signature");

    BOOST_TEST_MESSAGE("-- Trying decrypt (fail: active key, but only posting allowed)");

    auto block_num = db->head_block_num();
    dq.signed_data.head_block_number = block_num;

    auto sig_hash = fc::sha256::hash(std::to_string(block_num));

    dq.signature = bob_private_key.sign_compact(sig_hash);

    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "wrong_signature");

    BOOST_TEST_MESSAGE("-- Trying decrypt (login success, but empty result because empty query");

    dq.signature = bob_post_key.sign_compact(sig_hash);
    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 0);

    BOOST_TEST_MESSAGE("-- Trying decrypt by author + permlink (ok, result with 'no_sub' and 'decrypt_fee'");

    dq.entries.push_back(fc::mutable_variant_object()
        ("author","alice")("permlink","test"));
    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 1);
    {
        const auto& dr = dec_res.results[0];
        BOOST_CHECK(!dr.body);
        BOOST_CHECK(!!dr.err);
        BOOST_CHECK_EQUAL(*(dr.err), "no_sub");
        BOOST_CHECK(!!dr.decrypt_fee);
        BOOST_CHECK_EQUAL(*(dr.decrypt_fee), cdf.fee);
    }

    BOOST_TEST_MESSAGE("-- Trying decrypt by wrong author + permlink (ok, result with just 'no_sub')");

    dq.entries.clear();
    dq.entries.push_back(fc::mutable_variant_object()
        ("author","alice")("permlink","test2"));
    dec_res = decrypt_comments(dq);
    {
        const auto& dr = dec_res.results[0];
        BOOST_CHECK(!dr.body);
        BOOST_CHECK(!!dr.err);
        BOOST_CHECK_EQUAL(*(dr.err), "no_sub");
        BOOST_CHECK(!dr.decrypt_fee);
    }

    BOOST_TEST_MESSAGE("-- Top up bob TIP balance");

    fund("bob", 1000);
    transfer_to_tip_operation totip;
    totip.from = "bob";
    totip.to = "bob";
    totip.amount = asset(500, STEEM_SYMBOL);
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, totip));

    validate_database();

    BOOST_TEST_MESSAGE("-- Donating decrypt_fee partly");

    donate_operation dop;
    dop.from = "bob";
    dop.to = "alice";
    dop.amount = asset{1, STEEM_SYMBOL};
    dop.memo.app = "best-golos-ui";
    dop.memo.version = 1;
    dop.memo.target = fc::mutable_variant_object()
        ("author", "alice")("permlink", "test");

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_post_key, dop));

    validate_database();

    BOOST_TEST_MESSAGE("-- Trying decrypt again (ok, result with 'no_sub' and 'decrypt_fee'");

    dq.entries.clear();
    dq.entries.push_back(fc::mutable_variant_object()
        ("author","alice")("permlink","test"));
    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 1);
    {
        const auto& dr = dec_res.results[0];
        BOOST_CHECK(!dr.body);
        BOOST_CHECK(!!dr.err);
        BOOST_CHECK_EQUAL(*(dr.err), "no_sub");
        BOOST_CHECK(!!dr.decrypt_fee);
        BOOST_CHECK_EQUAL(*(dr.decrypt_fee), asset(3 - 1, STEEM_SYMBOL));
    }

    BOOST_TEST_MESSAGE("-- Donating decrypt_fee remaining");

    dop.amount = asset{2, STEEM_SYMBOL};

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_post_key, dop));

    validate_database();

    BOOST_TEST_MESSAGE("-- Trying decrypt again (ok)");

    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 1);
    {
        const auto& dr = dec_res.results[0];
        BOOST_CHECK(!!dr.err);
        BOOST_CHECK_EQUAL(*(dr.err), "");
        BOOST_CHECK(!dr.decrypt_fee);
        BOOST_CHECK(!!dr.body);
        BOOST_CHECK_EQUAL(*(dr.body), eq.body);
    }

    BOOST_TEST_MESSAGE("-- Trying decrypt using author + hashlink (ok)");

    const auto& cmt = db->get_comment_by_perm("alice", std::string("test"));

    dq.entries.clear();
    dq.entries.push_back(fc::mutable_variant_object()
        ("author","alice")("hashlink",cmt.hashlink));
    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 1);
    {
        const auto& dr = dec_res.results[0];
        BOOST_CHECK(!!dr.err);
        BOOST_CHECK_EQUAL(*(dr.err), "");
        BOOST_CHECK(!dr.decrypt_fee);
        BOOST_CHECK(!!dr.body);
        BOOST_CHECK_EQUAL(*(dr.body), eq.body);
    }

    BOOST_TEST_MESSAGE("-- Donating more than decrypt_fee");

    generate_block(); // avoids duplicate tx check + allows decrypt after 1 block

    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_post_key, dop));

    validate_database();

    BOOST_TEST_MESSAGE("-- Trying decrypt again");

    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 1);
    {
        const auto& dr = dec_res.results[0];
        BOOST_CHECK(!!dr.err);
        BOOST_CHECK_EQUAL(*(dr.err), "");
        BOOST_CHECK(!dr.decrypt_fee);
        BOOST_CHECK(!!dr.body);
        BOOST_CHECK_EQUAL(*(dr.body), eq.body);
    }

    BOOST_TEST_MESSAGE("-- Generating more blocks");
    generate_blocks(9);

    BOOST_TEST_MESSAGE("-- Trying again");

    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 1);
    {
        const auto& dr = dec_res.results[0];
        BOOST_CHECK(!!dr.body);
        BOOST_CHECK_EQUAL(*(dr.body), eq.body);
    }

    BOOST_TEST_MESSAGE("-- Generating more blocks");

    generate_blocks(1);

    BOOST_TEST_MESSAGE("-- Trying again (fail - head block too old)");

    dec_res = decrypt_comments(dq);
    BOOST_CHECK_EQUAL(dec_res.error, "head_block_num_too_old");
    BOOST_CHECK_EQUAL(dec_res.results.size(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
