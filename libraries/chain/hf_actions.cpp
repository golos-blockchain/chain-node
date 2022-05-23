#include <golos/chain/hf_actions.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>

namespace golos { namespace chain {

void hf_actions::prepare_for_tests() {
#ifdef STEEMIT_BUILD_LIVETEST
    //Password: P5JZjQT6GWQ5B7mBmcpYQKCuPBwGy1S5RcyUbJiL53eNTU8d1RNa
    //Posting: 5JFLL8mFftgnPwhBjZ5jFzkwMamit8i7qcCbLs3h866EeCzjmcY
    //Active: 5Jez5D9jyZmUf7LBBMnK2KDgzAQJ1TcBtLyTWdTvkZwTkPjq7de
    //Owner: 5KL7LgHzk3upN2x5kbJTKyt2om5jtoCyGEVTzYZrSk529CYZXGi
    //Memo: 5KJH8ubH3Pi7eVdmvbUaegJKkZ2SskMwzd2e4VPtLmZ51dgf2xK

    for (const auto &account : _db.get_index<account_index>().indices()) {
        _db.update_owner_authority(account, authority(1, public_key_type("GLS6ec8WydkchNKpdRdu5NwCN6tJ9BU3zjaLK8d9R7pE4sZFGSPJq"), 1));

        _db.modify(_db.get_authority(account.name), [&](account_authority_object &auth) {
            auth.posting = authority(1, public_key_type("GLS63N7CWwDWw6zgfeZZTugB3S1k9Z4vEv945JWx3e69rW3q4dH6A"), 1);
            auth.active = authority(1, public_key_type("GLS65EokmijFAKniwRBRTgnnTos2Qe5Cbxa2Qc9QQgmfkqooERo8D"), 1);
        });

        _db.modify(account, [&](account_object &acc) {
            acc.memo_key = public_key_type("GLS6abTB9JACr2XqVixYrakusKjUvZPZENgnz9kdAdKwVowMWiN2o");
        });
    }
    
    const auto &witness_idx = _db.get_index<witness_index>().indices();
    for (auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr) {
        _db.modify(*itr, [&](witness_object &w) {
            w.signing_key = public_key_type("GLS65EokmijFAKniwRBRTgnnTos2Qe5Cbxa2Qc9QQgmfkqooERo8D");
        });
    }
#endif
#ifdef STEEMIT_BUILD_TESTNET
#define COMMON_POSTING public_key_type("GLS63N7CWwDWw6zgfeZZTugB3S1k9Z4vEv945JWx3e69rW3q4dH6A")
#define COMMON_ACTIVE public_key_type("GLS65EokmijFAKniwRBRTgnnTos2Qe5Cbxa2Qc9QQgmfkqooERo8D")

    // adjust_balance(get_account("cyberfounder"), asset(10000000, SBD_SYMBOL));

    _db.modify(_db.get_authority(_db.get_account("cyberfounder").name), [&](account_authority_object &auth) {
        auth.posting = authority(1, COMMON_POSTING, 1);
        // and active-owner-memo is STEEMIT_INIT_PUBLIC_KEY:
        // 5JVFFWRLwz6JoP9kguuRFfytToGU6cLgBVTL9t6NB3D3BQLbUBS
    });

    create_test_account("cyberfounder100", [&](auto& posting, auto& active, auto& owner) {
        posting = COMMON_POSTING;
        active = public_key_type(STEEMIT_INIT_PUBLIC_KEY);
        owner = active;
    }, [&](auto& a) {
        a.memo_key = public_key_type(STEEMIT_INIT_PUBLIC_KEY);
    });

    create_test_account(STEEMIT_OAUTH_ACCOUNT, [&](auto& posting, auto& active, auto& owner) {
        posting = COMMON_POSTING;
        active = COMMON_ACTIVE;
    });

    create_test_account(STEEMIT_NOTIFY_ACCOUNT, [&](auto& posting, auto& active, auto& owner) {
        posting = COMMON_POSTING;
        active = COMMON_ACTIVE;
    }, [&](auto& a) {
        // 5JFZC7AtEe1wF2ce6vPAUxDeevzYkPgmtR14z9ZVgvCCtrFAaLw
        a.memo_key = public_key_type("GLS7Pbawjjr71ybgT6L2yni3B3LXYiJqEGnuFSq1MV9cjnV24dMG3");
    });
#endif
}

void hf_actions::create_worker_pool() {
#ifdef STEEMIT_BUILD_TESTNET
    _db.create<account_object>([&](auto& a) {
        a.name = STEEMIT_WORKER_POOL_ACCOUNT;
    });
    if (_db.store_metadata_for_account(STEEMIT_WORKER_POOL_ACCOUNT)) {
        _db.create<account_metadata_object>([&](auto& m) {
            m.account = STEEMIT_WORKER_POOL_ACCOUNT;
        });
    }
    _db.create<account_authority_object>([&](auto& auth) {
        auth.account = STEEMIT_WORKER_POOL_ACCOUNT;
        auth.owner.weight_threshold = 1;
        auth.active.weight_threshold = 1;
    });
#else
    const auto& workers_pool = _db.get_account(STEEMIT_WORKER_POOL_ACCOUNT);
    authority empty_owner;
    empty_owner.weight_threshold = 1;
    _db.update_owner_authority(workers_pool, empty_owner);
    _db.modify(_db.get_authority(workers_pool.name), [&](auto& o) {
        o.active = authority();
        o.active.weight_threshold = 1;
        o.posting = authority();
        o.posting.weight_threshold = 1;
    });
#endif
}

hf_actions::hf_actions(database& db) : _db(db) {
}

template<typename FillAuth, typename FillAccount>
void hf_actions::create_test_account(account_name_type acc, FillAuth&& fillAuth, FillAccount&& fillAcc) {
    _db.create<account_object>([&](auto& a) {
        a.name = acc;
        fillAcc(a);
    });
    _db.create<account_authority_object>([&](auto& auth) {
        auth.account = acc;
        public_key_type posting;
        public_key_type active;
        public_key_type owner;
        fillAuth(posting, active, owner);
        auth.active = authority(1, active, 1);
        auth.posting = authority(1, posting, 1);
        auth.owner = authority(1, owner, 1);
    });
}

template<typename FillAuth>
void hf_actions::create_test_account(account_name_type acc, FillAuth&& fillAuth) {
    create_test_account(acc, fillAuth, [&](auto& a) {});
}

}} // golos::chain
