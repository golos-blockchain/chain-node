#pragma once

#include <golos/chain/account_object.hpp>
#include <golos/chain/steem_objects.hpp>

namespace golos { namespace plugins { namespace database_api {

    using golos::chain::account_balance_object;
    using namespace golos::protocol;

    struct balances_query {
        bool system = false; // GOLOS, GBG
        std::set<std::string> symbols;
        std::string token_holders;
    };

    struct account_balance_api_object {
        account_balance_object::id_type id;

        account_name_type account;
        asset balance;
        asset tip_balance;
        asset market_balance;

        account_balance_api_object() = default;

        account_balance_api_object(const account_balance_object& p) :
            id(p.id), account(p.account), balance(p.balance), tip_balance(p.tip_balance),
            market_balance(p.market_balance) {

        }

        static account_balance_api_object golos(const account_name_type& account, const golos::chain::database& _db) {
            account_balance_api_object o;
            auto* acc = _db.find_account(account);
            if (acc) o.account = account;
            o.balance = acc ? acc->balance : asset(0, STEEM_SYMBOL);
            o.tip_balance = acc ? acc->tip_balance : asset(0, STEEM_SYMBOL);
            o.market_balance = acc ? acc->market_balance : asset(0, STEEM_SYMBOL);
            return o;
        }

        static account_balance_api_object gbg(const account_name_type& account, const golos::chain::database& _db) {
            account_balance_api_object o;
            auto* acc = _db.find_account(account);
            if (acc) o.account = account;
            o.balance = acc ? acc->sbd_balance : asset(0, SBD_SYMBOL);
            o.tip_balance = asset(0, SBD_SYMBOL);
            o.market_balance = acc ? acc->market_sbd_balance : asset(0, SBD_SYMBOL);
            return o;
        }
    };

    using account_balances_map_api_object = std::map<std::string, account_balance_api_object>;

}}} // golos::plugins::database_api

FC_REFLECT(
    (golos::plugins::database_api::balances_query),
    (system)(symbols)(token_holders)
)

FC_REFLECT((golos::plugins::database_api::account_balance_api_object),
    (id)(account)(balance)(tip_balance)(market_balance)
)
