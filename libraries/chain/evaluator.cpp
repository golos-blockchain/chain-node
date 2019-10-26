#include <golos/chain/evaluator.hpp>

namespace golos { namespace chain {

        asset get_balance(const account_object &account, balance_type type, asset_symbol_type symbol) {
            switch(type) {
                case MAIN_BALANCE:
                    switch (symbol) {
                        case STEEM_SYMBOL:
                            return account.balance;
                        case SBD_SYMBOL:
                            return account.sbd_balance;
                        default:
                            GOLOS_CHECK_VALUE(false, "invalid symbol");
                    }
                case SAVINGS:
                    switch (symbol) {
                        case STEEM_SYMBOL:
                            return account.savings_balance;
                        case SBD_SYMBOL:
                            return account.savings_sbd_balance;
                        default:
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
                default: FC_ASSERT(false, "invalid balance type");
            }
        }

} } // golos::chain
