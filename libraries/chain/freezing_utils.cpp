#include <golos/chain/freezing_utils.hpp>
#include <golos/chain/steem_objects.hpp>

namespace golos { namespace chain {

#ifdef STEEMIT_BUILD_TESTNET
#define FORCE_FREEZE_STOP 40 // 120 sec
#else
#define FORCE_FREEZE_STOP STEEMIT_BLOCKS_PER_HOUR*6
#endif

freezing_utils::freezing_utils(database& db) : _db(db) {
    const auto& hpo = _db.get_hardfork_property_object();
    hardfork = hpo.last_hardfork;
    auto now = _db.head_block_num();
    if (now - hpo.hf27_applied_block == FORCE_FREEZE_STOP) {
        hf_ago_ended_now = true;
    } else if (now - hpo.hf27_applied_block > FORCE_FREEZE_STOP) {
        hf_long_ago = true;
    }
}

bool freezing_utils::is_inactive(const account_object& acc) {
    if (is_system_account(acc.name)) {
        return false;
    }

    switch (hardfork) {
        case STEEMIT_HARDFORK_0_27:
        {
            auto vesting = 290000000000;
#ifdef STEEMIT_BUILD_TESTNET
            if (!_db._test_freezing) return false; else vesting = 600000000000;
#endif
            return acc.balance.amount < 100000 && acc.vesting_shares.amount < vesting;
        }
        default:
            return false;
    }
}

std::set<account_name_type> system_accounts = {
    STEEMIT_INIT_MINER_NAME, STEEMIT_MINER_ACCOUNT, STEEMIT_NULL_ACCOUNT, STEEMIT_TEMP_ACCOUNT,
    STEEMIT_WORKER_POOL_ACCOUNT, STEEMIT_OAUTH_ACCOUNT,
    STEEMIT_REGISTRATOR_ACCOUNT, STEEMIT_NOTIFY_ACCOUNT
};

bool freezing_utils::is_system_account(account_name_type name) {
    return system_accounts.find(name) != system_accounts.end();
}

void freezing_utils::unfreeze(const account_object& acc, const asset& fee) {
    auto& fr = _db.get<account_freeze_object, by_account>(acc.name);
    _db.modify(acc, [&](auto& acc) {
        acc.frozen = false;
        acc.proved_hf = hardfork;
    });
    auto& auth = _db.get_authority(acc.name);
    _db.modify(auth, [&](auto& auth) {
        auth.posting = fr.posting;
        auth.active = fr.active;
        auth.owner = fr.owner;
    });
    _db.remove(fr);
    account_freeze_operation event(acc.name, false, fee);
    _db.push_event(event);
}

}} // golos::chain
