#include <golos/chain/evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/steem_objects.hpp>

namespace golos { namespace chain {

        asset get_balance(const database& _db, const account_object &account, balance_type type, asset_symbol_type symbol) {
            switch(type) {
                case MAIN_BALANCE:
                    if (symbol == STEEM_SYMBOL) {
                        return account.balance;
                    } else if (symbol == SBD_SYMBOL) {
                        return account.sbd_balance;
                    } else {
                        GOLOS_CHECK_VALUE(_db.has_hardfork(STEEMIT_HARDFORK_0_24__95), "invalid symbol");
                        return _db.get_or_default_account_balance(account.name, symbol).balance;
                    }
                case SAVINGS:
                    if (symbol == STEEM_SYMBOL) {
                        return account.savings_balance;
                    } else if (symbol == SBD_SYMBOL) {
                        return account.savings_sbd_balance;
                    } else {
                        GOLOS_CHECK_VALUE(false, "invalid symbol");
                    }
                case VESTING:
                    GOLOS_CHECK_VALUE(symbol == VESTS_SYMBOL, "invalid symbol");
                    return account.vesting_shares;
                case EFFECTIVE_VESTING:
                    GOLOS_CHECK_VALUE(symbol == VESTS_SYMBOL, "invalid symbol");
                    return account.effective_vesting_shares();
                case HAVING_VESTING:
                    GOLOS_CHECK_VALUE(symbol == VESTS_SYMBOL, "invalid symbol");
                    return account.available_vesting_shares(false);
                case AVAILABLE_VESTING:
                    GOLOS_CHECK_VALUE(symbol == VESTS_SYMBOL, "invalid symbol");
                    return account.available_vesting_shares(true);
                case TIP_BALANCE:
                    if (symbol == STEEM_SYMBOL) {
                        return account.tip_balance;
                    } else {
                        GOLOS_CHECK_VALUE(_db.has_hardfork(STEEMIT_HARDFORK_0_24__95), "invalid symbol");
                        return _db.get_or_default_account_balance(account.name, symbol).tip_balance;
                    }
                default: FC_ASSERT(false, "invalid balance type");
            }
        }

        std::string get_balance_name(balance_type type) {
            switch(type) {
                case MAIN_BALANCE: return "fund";
                case SAVINGS: return "savings";
                case VESTING: return "vesting shares";
                case EFFECTIVE_VESTING: return "effective vesting shares";
                case HAVING_VESTING: return "having vesting shares";
                case AVAILABLE_VESTING: return "available vesting shares";
                case TIP_BALANCE: return "tip balance";
                default: FC_ASSERT(false, "invalid balance type");
            }
        }

} } // golos::chain
