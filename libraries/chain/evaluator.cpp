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
                        GOLOS_CHECK_VALUE(symbol != VESTS_SYMBOL
                            && _db.has_hardfork(STEEMIT_HARDFORK_0_24__95), "invalid symbol");

                        const auto& idx = _db.get_index<account_balance_index, by_symbol_account>();
                        auto itr = idx.find(std::make_tuple(symbol, account.name));
                        if (itr != idx.end()) {
                            return itr->balance;
                        } else {
                            const auto& sym_idx = _db.get_index<asset_index, by_symbol>();
                            auto sym_itr = sym_idx.find(symbol);
                            GOLOS_CHECK_VALUE(sym_itr != sym_idx.end(), "invalid symbol");

                            return asset(0, symbol);
                        }
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
                    GOLOS_CHECK_VALUE(symbol == STEEM_SYMBOL, "invalid symbol");
                    return account.tip_balance;
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
