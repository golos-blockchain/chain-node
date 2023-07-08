#include <golos/chain/database.hpp>
#include <golos/chain/evaluator.hpp>
#include <golos/protocol/exceptions.hpp>

namespace golos { namespace chain {

const paid_subscription_object& database::get_paid_subscription(const account_name_type& author, const paid_subscription_id& id) const {
    try {
        return get<paid_subscription_object, by_author_oid>(std::make_tuple(author, id));
    } catch(const std::out_of_range& e) {
        GOLOS_THROW_MISSING_OBJECT("paid_subscription", fc::mutable_variant_object()("author", author)("id", id));
    } FC_CAPTURE_AND_RETHROW((author)(id))
}

const paid_subscription_object* database::find_paid_subscription(const account_name_type& author, const paid_subscription_id& id) const {
    return find<paid_subscription_object, by_author_oid>(std::make_tuple(author, id));
}

void database::throw_if_exists_paid_subscription(const account_name_type& author, const paid_subscription_id& id) const {
    if (find_paid_subscription(author, id)) {
        GOLOS_THROW_OBJECT_ALREADY_EXIST("paid_subscription", fc::mutable_variant_object()("author", author)("id", id));
    }
}

const paid_subscriber_object& database::get_paid_subscriber(const account_name_type& subscriber, const account_name_type& author, const paid_subscription_id& id) const {
    try {
        return get<paid_subscriber_object, by_author_oid_subscriber>(std::make_tuple(author, id, subscriber));
    } catch(const std::out_of_range& e) {
        GOLOS_THROW_MISSING_OBJECT("paid_subscriber", fc::mutable_variant_object()("subscriber", subscriber)("author", author)("id", id));
    } FC_CAPTURE_AND_RETHROW((author)(id))
}

const paid_subscriber_object* database::find_paid_subscriber(const account_name_type& subscriber, const account_name_type& author, const paid_subscription_id& id) const {
    return find<paid_subscriber_object, by_author_oid_subscriber>(std::make_tuple(author, id, subscriber));
}

void database::throw_if_exists_paid_subscriber(const account_name_type& subscriber, const account_name_type& author, const paid_subscription_id& id) const {
    if (find_paid_subscriber(subscriber, author, id)) {
        GOLOS_THROW_OBJECT_ALREADY_EXIST("paid_subscriber", fc::mutable_variant_object()("subscriber", subscriber)("author", author)("id", id));
    }
}

bool database::claim_for_subscription(const account_object& subscriber, const asset& amount, bool from_tip) {
    auto bal = golos::chain::get_balance(*this, subscriber, from_tip ? TIP_BALANCE : MAIN_BALANCE, amount.symbol);
    if (bal < amount) return false;
    if (from_tip) {
        if (amount.symbol == STEEM_SYMBOL) {
            modify(subscriber, [&](auto& acnt) {
                acnt.tip_balance -= amount;
            });
        } else {
            adjust_account_balance(subscriber.name, asset(0, amount.symbol), -amount);
        }
    } else {
        adjust_balance(subscriber, -amount);
    }
    return true;
}

void database::pay_for_subscription(const account_object& author, const asset& amount, bool to_tip) {
    if (to_tip) {
        if (amount.symbol == STEEM_SYMBOL) {
            modify(author, [&](auto& acnt) {
                acnt.tip_balance += amount;
            });
        } else {
            adjust_account_balance(author.name, asset(0, amount.symbol), amount);
        }
    } else {
        adjust_balance(author, amount);
    }
}

void database::push_payment_event(const paid_subscriber_object& psro, asset prepaid, asset amount, asset rest) {
    push_event(subscription_payment_operation(psro.subscriber, psro.author, psro.oid,
        prepaid, amount, rest, psro.tip_cost, "{}"));
}

void database::process_paid_subscribers() { try {
    if (!has_hardfork(STEEMIT_HARDFORK_0_29__230)) {
        return;
    }

    const auto& idx = get_index<paid_subscriber_index, by_next_payment>();

    const auto now = head_block_time();

    for (auto itr = idx.upper_bound(fc::time_point_sec(0)); itr != idx.end() && itr->next_payment <= now; ) {
        const auto& psro = *itr;

        if (psro.executions_left == 0) {
            ++itr;

            modify(psro, [&](auto& psro) {
                psro.active = false;
                psro.next_payment = time_point_sec(0);
            });

            const auto& pso = get_paid_subscription(psro.author, psro.oid);
            modify(pso, [&](auto& pso) {
                --pso.active_subscribers;
            });
            continue;
        }

        asset prepaid = std::min(psro.cost, psro.prepaid);
        asset amount(0, psro.cost.symbol);
        if (prepaid < psro.cost) {
            amount = psro.cost - prepaid;
            if (!claim_for_subscription(get_account(psro.subscriber), amount, psro.tip_cost)) {
                ++itr;

                modify(psro, [&](auto& psro) {
                    psro.active = false;
                    psro.next_payment = time_point_sec(0);
                });

                const auto& pso = get_paid_subscription(psro.author, psro.oid);
                modify(pso, [&](auto& pso) {
                    --pso.active_subscribers;
                });

                push_virtual_operation(subscription_payment_failure_operation(psro.subscriber, psro.author, psro.oid, "{}"));
                continue;
            }
        }

        asset rest(0, amount.symbol);
        if (psro.executions_left == 1 && psro.prepaid > prepaid) {
            rest = psro.prepaid - prepaid;
        }

        pay_for_subscription(get_account(psro.author), prepaid + amount + rest, psro.tip_cost);

        push_payment_event(psro, prepaid, amount, rest);

        ++itr;

        modify(psro, [&](auto& psro) {
            psro.prepaid -= (prepaid + rest);
            --psro.executions_left;
            psro.next_payment = psro.next_payment + fc::seconds(psro.interval);
        });
    }
} FC_CAPTURE_AND_RETHROW() }

}} // golos::chain
