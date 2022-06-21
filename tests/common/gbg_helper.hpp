#pragma once

#include <golos/chain/database.hpp>

namespace golos { namespace chain {

    struct gbg_info {
        asset balance;
        time_point_sec last_update;
        asset payout;
        asset savings;
        time_point_sec last_savings_update;
        asset savings_payout;
    };

    class gbg_helper {
    public:
        gbg_helper(const database* _db) : db(_db) {
            for (const auto& acc : db->get_index<account_index>().indices()) {
                gbg_infos[acc.name] = gbg_info{
                    acc.sbd_balance,
                    acc.sbd_seconds_last_update,
                    asset(0, SBD_SYMBOL),
                    acc.savings_sbd_balance,
                    acc.savings_sbd_seconds_last_update,
                    asset(0, SBD_SYMBOL)
                };
            }
        }

        const asset& payout(account_name_type name) {
            return gbg_infos[name].payout;
        }

        const asset& savings_payout(account_name_type name) {
            return gbg_infos[name].savings_payout;
        }

        void validate_balances() {
            for (const auto& acc : db->get_index<account_index>().indices()) {
                auto itr = gbg_infos.find(acc.name);
                if (itr != gbg_infos.end()) {
                    BOOST_TEST_CONTEXT(std::string(acc.name)) {
                        BOOST_CHECK_EQUAL(itr->second.balance, acc.sbd_balance);
                        BOOST_CHECK_EQUAL(itr->second.savings, acc.savings_sbd_balance);
                    }
                }
            }
        }

        void trigger_convert() {
            for (auto& pair : gbg_infos) {
                auto& info = pair.second;

                auto sub = auto_convert(info.balance);
                auto savings_sub = auto_convert(info.savings);

                if (!db->has_hardfork(STEEMIT_HARDFORK_0_27)) {
                    auto interest = pay_interest(info.balance, info.last_update);
                    info.balance += interest;
                    info.payout += interest;
                }
                auto interest = pay_interest(info.savings, info.last_savings_update);
                info.savings += interest;
                info.savings_payout += interest;

                info.balance -= sub;
                info.savings -= savings_sub;
            }

            validate_balances();
        }
    private:
        asset auto_convert(const asset& balance) {
            return balance * STEEMIT_SBD_DEBT_CONVERT_RATE / STEEMIT_100_PERCENT;
        }

        asset pay_interest(const asset& balance, const time_point_sec& last_update) {
            auto sbd_seconds = balance.amount.value * (db->head_block_time() - last_update).to_seconds();
            auto interest = sbd_seconds * STEEMIT_DEFAULT_SBD_INTEREST_RATE / (STEEMIT_SECONDS_PER_YEAR * STEEMIT_100_PERCENT);
            return asset(interest, SBD_SYMBOL);
        }

        const database* db;
        std::map<account_name_type, gbg_info> gbg_infos;
    };

}}
