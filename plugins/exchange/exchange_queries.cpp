#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/plugins/exchange/exchange_queries.hpp>

namespace golos { namespace plugins { namespace exchange {

bool exchange_min_to_receive::ignore_chain(bool is_buy, asset param, asset res, bool is_direct) const {
    auto thr = is_direct ? direct : multi;
    if (thr.amount.value != 0 &&
        (is_buy ? param : res) < thr) return true;
    return false;
}

void exchange_min_to_receive::validate(const exchange_query& query) const {
    #define VALIDATE_AMOUNT(FIELD) \
        if (FIELD.amount.value != 0) { \
            GOLOS_CHECK_VALUE_GT(FIELD.amount.value, 0); \
            if (query.direction == exchange_direction::sell) { \
                GOLOS_CHECK_VALUE(FIELD.symbol == query.sym2, \
                    "min_to_receive should have same symbol as 'symbol' field"); \
            } else { \
                GOLOS_CHECK_VALUE(FIELD.symbol == query.amount.symbol, \
                    "min_to_receive should have same symbol as 'amount' symbol"); \
            } \
        }
    VALIDATE_AMOUNT(direct);
    VALIDATE_AMOUNT(multi);
    #undef VALIDATE_AMOUNT
}

void exchange_query::initialize_validate(database& _db) {
    GOLOS_CHECK_PARAM(amount, {
        GOLOS_CHECK_VALUE_GT(amount.amount.value, 0);
        if (amount.symbol != STEEM_SYMBOL && amount.symbol != SBD_SYMBOL) {
            _db.with_weak_read_lock([&]() {
                _db.get_asset(amount.symbol);
            });
        }
    });

    GOLOS_CHECK_PARAM(symbol, {
        GOLOS_CHECK_VALUE(symbol != amount.symbol_name(),
            "amount should have another symbol than query.symbol");

        _db.with_weak_read_lock([&]() {
            sym2 = _db.symbol_from_str(symbol);
        });
    });

    GOLOS_CHECK_PARAM(min_to_receive, {
        if (!!min_to_receive) {
            min_to_receive->validate(*this);
        }
    });
}

std::vector<exchange_query> exchange_query::discrete_split(uint16_t step) {
    std::vector<exchange_query> qus;
    FC_ASSERT(step <= STEEMIT_100_PERCENT, "Discrete split cannot be > 10000 (100%)");
    if (step == 0 || step == STEEMIT_100_PERCENT) {
        qus.emplace_back(*this);
        return qus;
    }
    asset prev_amount(0, this->amount.symbol);
    for (uint16_t pct = STEEMIT_100_PERCENT; pct > 0; pct -= step) {
        auto chunk = *this;
        chunk.pct = pct;
        chunk.amount = asset((fc::uint128_t(chunk.amount.amount.value) * pct / STEEMIT_100_PERCENT).to_uint64(),
            chunk.amount.symbol);
        if (chunk.amount.amount == 0 || chunk.amount == prev_amount) {
            continue;
        }
        prev_amount = chunk.amount;
        qus.push_back(chunk);

        if (pct != STEEMIT_100_PERCENT) {
            auto dir = *this;
            dir.pct = pct;
            dir.pct_direct = true;
            dir.amount -= chunk.amount;
            qus.push_back(dir);
        }
    }
    return qus;
}

bool exchange_path_query::is_bad_symbol(const std::string& sym) const {
    return filter_syms.size() && filter_syms.count(sym);
}

bool exchange_path_query::is_good_symbol(const std::string& sym) const {
    return !is_bad_symbol(sym) && (!select_syms.size() || select_syms.count(sym));
}

bool exchange_path_query::is_buy() const {
    return sell.empty();
}

void exchange_path_query::initialize_validate(database& _db) {
    GOLOS_CHECK_PARAM(sell, {
        GOLOS_CHECK_VALUE(sell != buy, "`sell` cannot be same as `buy`");
        GOLOS_CHECK_VALUE(!sell.size() || !buy.size(), "`sell` and `buy` cannot be set both (in current implementation)");
        GOLOS_CHECK_VALUE(sell.size() || buy.size(), "`sell` and `buy` cannot be empty both");
    });

    _db.with_weak_read_lock([&]() {
        if (sell.size()) {
            sell_sym = _db.symbol_from_str(sell);
        }
        if (buy.size()) {
            buy_sym = _db.symbol_from_str(buy);
        }
    });
}

} } } // golos::plugins::exchange
