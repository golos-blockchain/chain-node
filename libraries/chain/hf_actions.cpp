#include <golos/chain/hf_actions.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>

namespace golos { namespace chain {

void hf_actions::prepare_for_tests() {
#ifdef STEEMIT_BUILD_LIVETEST
    //Password (of @lex): P5Jw6xSsoDhFtrNp1CGAWyzhVez6dFuxCQ9dn4QC7aZZW9R3WmUq
    //Posting: 5HwQScueMZdELZpjVBD4gm6xhiKiMqGx18g4WtQ6wVr4nBdSxY5
    //Active: 5K67PNheLkmxkgJ5UjvR8Nyt3GVPoLEN1dMZjFuNETzrNyMecPG
    //Owner: 5KD45zFh5WNFaW8mZTSx4eicy8FzwEmm5psNKH7GLg5bVQwUw6s
    //Memo: 5Kek6zP5vQmDRXXNBtZkxUtoMT3iW1xEYXcifkA2UHb2VT5UD7P

    for (const auto &account : _db.get_index<account_index>().indices()) {
        _db.update_owner_authority(account, authority(1, public_key_type("GLS5GfkCE2HQwcE6Gs8pDXvp2PJtF4dwqG8Af9rhn9LKNrJ6PKPMF"), 1));

        _db.modify(_db.get_authority(account.name), [&](account_authority_object &auth) {
            auth.posting = authority(1, public_key_type("GLS8hnaAj3ufXbfFBKqGNhyGvW78EQN5rpeqfcDD2d2tQyhd2dEDb"), 1);
            auth.active = authority(1, public_key_type("GLS8EAm8DhD8tjX3N87RVAvw1o8uzSQkRJ3Tcn4dgmybHwnsyvThb"), 1);
        });

        _db.modify(account, [&](account_object &acc) {
            acc.memo_key = public_key_type("GLS5Zwv6MSYx9agm2GaH3XmWCQT317TR7nBXgos1TV56ziErocqa3");
        });
    }

    _db.modify(_db.get_account(STEEMIT_NOTIFY_ACCOUNT), [&](auto& a) {
        // 5JFZC7AtEe1wF2ce6vPAUxDeevzYkPgmtR14z9ZVgvCCtrFAaLw
        a.memo_key = public_key_type("GLS7Pbawjjr71ybgT6L2yni3B3LXYiJqEGnuFSq1MV9cjnV24dMG3");
    });

    const auto &witness_idx = _db.get_index<witness_index>().indices();
    for (auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr) {
        _db.modify(*itr, [&](witness_object &w) {
            w.signing_key = public_key_type("GLS8EAm8DhD8tjX3N87RVAvw1o8uzSQkRJ3Tcn4dgmybHwnsyvThb");
        });
    }
#endif
#ifdef STEEMIT_BUILD_TESTNET
#define COMMON_POSTING public_key_type("GLS8hnaAj3ufXbfFBKqGNhyGvW78EQN5rpeqfcDD2d2tQyhd2dEDb")
#define COMMON_ACTIVE public_key_type("GLS8EAm8DhD8tjX3N87RVAvw1o8uzSQkRJ3Tcn4dgmybHwnsyvThb")

    _db.adjust_balance(_db.get_account("cyberfounder"), asset(10000000, SBD_SYMBOL));

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

    create_test_account(STEEMIT_RECOVERY_ACCOUNT, [&](auto& posting, auto& active, auto& owner) {
        posting = COMMON_POSTING;
        active = COMMON_ACTIVE;
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

void hf_actions::create_registrator_account() {
#ifdef STEEMIT_BUILD_TESTNET
    _db.create<account_object>([&](auto& a) {
        a.name = STEEMIT_REGISTRATOR_ACCOUNT;
    });
    if (_db.store_metadata_for_account(STEEMIT_REGISTRATOR_ACCOUNT)) {
        _db.create<account_metadata_object>([&](auto& m) {
            m.account = STEEMIT_REGISTRATOR_ACCOUNT;
        });
    }
    _db.create<account_authority_object>([&](auto& auth) {
        auth.account = STEEMIT_REGISTRATOR_ACCOUNT;
        auth.owner.weight_threshold = 1;
        auth.active.weight_threshold = 1;
    });
#else
    const auto& acc = _db.get_account(STEEMIT_REGISTRATOR_ACCOUNT);
    _db.modify(acc, [&](auto& acnt) {
        acnt.recovery_account = account_name_type();
    });
    _db.modify(_db.get_authority(acc.name), [&](auto& o) {
        o.active = authority();
        o.active.weight_threshold = 1;
        o.owner = authority();
        o.owner.weight_threshold = 1;
        o.posting = authority();
        o.posting.weight_threshold = 1;
    });
#endif
}

void hf_actions::convert_min_curate_golos_power() {
    const auto& wso = _db.get_witness_schedule_object();

    const auto& gbg_median = _db.get_feed_history().current_median_history;
    auto convert_to_gbg = [&](auto& a) {
        if (!gbg_median.is_null()) {
            return a * gbg_median;
        }
        return asset(a.amount, SBD_SYMBOL);
    };

    const auto &idx = _db.get_index<witness_index>().indices();
    for (const auto& witness : idx) {
        _db.modify(witness, [&](auto& w) {
            w.props.min_golos_power_to_curate = convert_to_gbg(w.props.min_golos_power_to_curate);
        });
    }

    _db.modify(wso, [&](auto& wso) {
        wso.median_props.min_golos_power_to_curate = convert_to_gbg(wso.median_props.min_golos_power_to_curate);
    });
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
