#include <golos/chain/freezing_utils.hpp>
#include <golos/chain/steem_objects.hpp>

namespace golos { namespace chain {

freezing_utils::freezing_utils(database& db) : _db(db) {
    hardfork = _db.get_hardfork_property_object().last_hardfork;
}

//fc::time_point_sec inactive = fc::time_point_sec::from_iso_string("2021-05-30T00:00:00");

fc::time_point_sec inactive = fc::time_point_sec::from_iso_string("2022-05-10T00:00:00");

bool freezing_utils::is_inactive(const account_object& acc) {
    if (is_system_account(acc.name)) {
        return false;
    }

    switch (hardfork) {
        case STEEMIT_HARDFORK_0_27:
        {
            /*return !acc.post_count && !acc.comment_count
                && acc.balance.amount < 10000 && acc.vesting_shares.amount < 29000000000
                && acc.last_active_operation < inactive;*/
            return acc.balance.amount < 100000 && acc.vesting_shares.amount < 300000000000
                && acc.created < inactive;
        }
        default:
            return false;
    }
}

std::set<account_name_type> system_accounts = {
    STEEMIT_MINER_ACCOUNT, STEEMIT_NULL_ACCOUNT, STEEMIT_TEMP_ACCOUNT,
    STEEMIT_WORKER_POOL_ACCOUNT, STEEMIT_OAUTH_ACCOUNT,
    STEEMIT_REGISTRATOR_ACCOUNT, STEEMIT_NOTIFY_ACCOUNT
};

bool freezing_utils::is_system_account(account_name_type name) {
    return system_accounts.find(name) != system_accounts.end();
}

void freezing_utils::unfreeze(const account_object& acc) {
    auto fr = _db.get<account_freeze_object, by_account>(acc.name);
    _db.modify(acc, [&](auto& acc) {
        acc.memo_key = fr.memo_key;
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
}

}} // golos::chain
