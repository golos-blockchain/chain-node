#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/paid_subscription_objects.hpp>
#include <golos/chain/steem_evaluator.hpp>

namespace golos { namespace chain {

    void paid_subscription_create_evaluator::do_apply(const paid_subscription_create_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__230, "paid_subscription_create_operation");

        _db.get_account(op.author);

        if (op.cost.symbol != STEEM_SYMBOL && op.cost.symbol != SBD_SYMBOL) {
            auto& a = _db.get_asset(op.cost.symbol);
            if (op.tip_cost) {
                GOLOS_CHECK_VALUE(!a.allow_override_transfer, "This asset do not supports TIP balances");
            }
        }

        GOLOS_CHECK_OBJECT_MISSING(_db, paid_subscription, op.author, op.oid);

        _db.create<paid_subscription_object>([&](auto& pso) {
            pso.author = op.author;
            pso.oid = op.oid;
            pso.cost = op.cost;
            pso.tip_cost = op.tip_cost;
            pso.allow_prepaid = op.allow_prepaid;
            pso.interval = op.interval;
            pso.executions = op.executions;
            pso.created = _db.head_block_time();
        });
    }

    void paid_subscription_update_evaluator::do_apply(const paid_subscription_update_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__230, "paid_subscription_update_operation");

        const auto& pso = _db.get_paid_subscription(op.author, op.oid);

        if (op.cost.symbol != STEEM_SYMBOL && op.cost.symbol != SBD_SYMBOL) {
            auto& a = _db.get_asset(op.cost.symbol);
            if (op.tip_cost) {
                GOLOS_CHECK_VALUE(!a.allow_override_transfer, "This asset do not supports TIP balances");
            }
        }

        if (pso.executions == 0) {
            GOLOS_CHECK_VALUE(op.executions == 0, "You cannot set executions if subscription is created as single-executed (0 executions)");
        } else {
            GOLOS_CHECK_VALUE(op.executions > 0, "You cannot make subscription single-executed");
        }

        uint32_t inactivated_subscribers = 0;

        if (!pso.allow_prepaid) {
            GOLOS_CHECK_VALUE(op.tip_cost == pso.tip_cost, "Subscription not supports prepaid, so you cannot change tip_cost");
            GOLOS_CHECK_VALUE(op.interval == pso.interval, "Subscription not supports prepaid, so you cannot change interval");
            GOLOS_CHECK_VALUE(op.executions == pso.executions, "Subscription not supports prepaid, so you cannot change executions");

            const auto& idx = _db.get_index<paid_subscriber_index, by_author_oid_subscriber>();
            auto itr = idx.find(std::make_tuple(op.author, op.oid));
            for (; itr != idx.end() && itr->author == op.author && itr->oid == op.oid; ++itr) {
                if (itr->active) {
                    _db.push_event(subscription_inactive_operation(itr->subscriber, itr->author, itr->oid,
                        psro_inactive_reason::subscription_update, "{}"));
                }

                _db.modify(*itr, [&](auto& psro) {
                    if (op.cost.symbol != psro.cost.symbol) {
                        psro.prepaid = asset(0, op.cost.symbol);
                    }
                    psro.cost = op.cost;

                    if (psro.active) {
                        psro.active = false;
                        psro.inactive_reason = psro_inactive_reason::subscription_update;
                        psro.next_payment = time_point_sec(0);
                        ++inactivated_subscribers;
                    }
                });
            }
        }

        _db.modify(pso, [&](auto& pso) {
            pso.cost = op.cost;
            pso.tip_cost = op.tip_cost;
            pso.interval = op.interval;
            pso.executions = op.executions;

            pso.active_subscribers -= inactivated_subscribers;
        });
    }

    void paid_subscription_delete_evaluator::do_apply(const paid_subscription_delete_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__230, "paid_subscription_delete_operation");

        const auto& pso = _db.get_paid_subscription(op.author, op.oid);

        const auto& idx = _db.get_index<paid_subscriber_index, by_author_oid_subscriber>();
        uint32_t removed_subscribers = 0;
        for (auto itr = idx.find(std::make_tuple(op.author, op.oid));
            itr != idx.end() && itr->author == op.author && itr->oid == op.oid;) {
            const auto& current = *itr;

            if (current.prepaid.amount != 0)
                _db.pay_for_subscription(_db.get_account(current.subscriber), current.prepaid, current.tip_cost);

            ++itr;
            _db.remove(current);
            ++removed_subscribers;
        }

        _db.remove(pso);

        _db.modify(_db.get_account(op.author), [&](auto& acc) {
            acc.sponsor_count -= removed_subscribers;
        });
    }

    void paid_subscription_transfer_evaluator::do_apply(const paid_subscription_transfer_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__230, "paid_subscription_transfer_operation");

        const auto& from_account = _db.get_account(op.from);

        const auto& pso = _db.get_paid_subscription(op.to, op.oid);

        if (pso.tip_cost) {
            GOLOS_CHECK_VALUE(op.from_tip, "It should be TIP token");
        } else {
            GOLOS_CHECK_VALUE(!op.from_tip, "It should be liquid (not TIP) token");
        }

        GOLOS_CHECK_OP_PARAM(op, amount, {
            if (op.from_tip) {
                GOLOS_CHECK_BALANCE(_db, from_account, TIP_BALANCE, op.amount);
            } else {
                GOLOS_CHECK_BALANCE(_db, from_account, MAIN_BALANCE, op.amount);
            }

            GOLOS_CHECK_VALUE(op.amount.symbol == pso.cost.symbol, "Wrong token used");
            GOLOS_CHECK_VALUE(op.amount >= pso.cost, "Too low amount");

            if (pso.executions != 0 && !pso.allow_prepaid) {
                GOLOS_CHECK_VALUE(op.amount == pso.cost, "You cannot pay more than cost, if subscription is not single-executed and forbids prepayment");
            }
        });

        _db.claim_for_subscription(from_account, op.amount, op.from_tip);

        const auto& now = _db.head_block_time();

        const auto* psro = _db.find_paid_subscriber(op.from, op.to, op.oid);
        if (!psro) {
            const auto& to_account = _db.get_account(op.to);

            auto pay_now = pso.executions == 0 ? op.amount : pso.cost;
            _db.pay_for_subscription(to_account, pay_now, op.from_tip);

            auto to_prepaid = op.amount - pay_now;

            const auto& psro = _db.create<paid_subscriber_object>([&](auto& psro) {
                psro.subscriber = op.from;
                psro.subscribed = now;

                psro.author = op.to;
                psro.oid = op.oid;
                psro.cost = pso.cost;
                psro.tip_cost = pso.tip_cost;
                psro.interval = pso.interval;
                psro.executions = pso.executions;
                psro.executions_left = psro.executions;

                psro.prepaid = to_prepaid;

                if (pso.executions > 0) {
                    psro.next_payment = now + fc::seconds(psro.interval);
                }
            });

            asset rest(0, op.amount.symbol);
            if (pso.executions == 0) {
                rest = op.amount - pso.cost;
            }
            _db.push_payment_event(psro, sponsor_payment::first, asset(0, op.amount.symbol), pso.cost, rest, to_prepaid);

            _db.modify(pso, [&](auto& pso) {
                ++pso.subscribers;
                ++pso.active_subscribers;
            });

            _db.modify(to_account, [&](auto& acc) {
                ++acc.sponsor_count;
            });
        } else {
            if (psro->active) {
                GOLOS_CHECK_VALUE(pso.executions != 0, "You do not need to prolong single-executed subscription");
            }

            if (!pso.allow_prepaid) {
                GOLOS_CHECK_VALUE(!psro->active, "Subscription forbids prepayment, and it is active, so you cannot transfer tokens onto it");

                _db.pay_for_subscription(_db.get_account(op.to), op.amount, op.from_tip);

                _db.push_payment_event(*psro, sponsor_payment::prolong,
                    asset(0, op.amount.symbol), pso.cost, op.amount - pso.cost, asset(0, op.amount.symbol));
            } else {
                auto e_type = psro->active ? sponsor_payment::increase_prepaid : sponsor_payment::prolong;
                _db.push_payment_event(*psro, e_type,
                    op.amount, asset(0, op.amount.symbol), asset(0, op.amount.symbol), asset(0, op.amount.symbol));
            }

            if (!psro->active) {
                _db.modify(pso, [&](auto& pso) {
                    ++pso.active_subscribers;
                });
            }

            if (psro->prepaid.amount > 0 && psro->prepaid.symbol != pso.cost.symbol) {
                _db.pay_for_subscription(_db.get_account(op.from), psro->prepaid, psro->tip_cost);

                _db.push_event(subscription_prepaid_return_operation(op.from, op.to, op.oid,
                    psro->prepaid, psro->tip_cost, "{}"));
            }

            _db.modify(*psro, [&](auto& psro) {
                if (!psro.active) {
                    psro.active = true;
                    psro.inactive_reason = psro_inactive_reason::none;

                    psro.interval = pso.interval;
                    psro.executions = pso.executions;

                    if (pso.executions > 0)
                        psro.next_payment = now + fc::seconds(psro.interval);
                }
                if (pso.allow_prepaid) {
                    psro.cost = pso.cost;
                    psro.tip_cost = pso.tip_cost;
                    psro.interval = pso.interval;
                    psro.executions = pso.executions;

                    if (psro.prepaid.symbol != pso.cost.symbol) {
                        psro.prepaid = asset(0, pso.cost.symbol); // Update symbol if author changed it
                    }

                    psro.prepaid += op.amount;
                }
                psro.executions_left = psro.executions;
            });
        }
    }

    void paid_subscription_cancel_evaluator::do_apply(const paid_subscription_cancel_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__230, "paid_subscription_cancel_operation");

        const auto& psro = _db.get_paid_subscriber(op.subscriber, op.author, op.oid);

        if (psro.prepaid.amount != 0) {
            _db.pay_for_subscription(_db.get_account(op.subscriber), psro.prepaid, psro.tip_cost);
        
            _db.push_event(subscription_prepaid_return_operation(op.subscriber, op.author, op.oid,
                psro.prepaid, psro.tip_cost, "{}"));
        }

        const auto& pso = _db.get_paid_subscription(op.author, op.oid);

        _db.modify(pso, [&](auto& pso) {
            --pso.subscribers;
            if (psro.active) {
                --pso.active_subscribers;
            }
        });

        _db.modify(_db.get_account(op.author), [&](auto& acc) {
            --acc.sponsor_count;
        });

        _db.remove(psro);
    }

} } // golos::chain
