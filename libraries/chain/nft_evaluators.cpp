#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/nft_objects.hpp>
#include <golos/chain/steem_evaluator.hpp>

namespace golos { namespace chain {

    void nft_collection_evaluator::do_apply(const nft_collection_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__201, "nft_collection_operation");

        auto name = nft_name_from_string(op.name);

        GOLOS_CHECK_OBJECT_MISSING(_db, nft_collection, name);

        uint8_t total = 0;
        uint8_t total_empty = 0;
        const auto& idx = _db.get_index<nft_collection_index, by_creator_name>();
        for (auto itr = idx.lower_bound(op.creator); itr != idx.end() && itr->creator == op.creator; ++itr) {
            ++total;
            if (!itr->token_count) ++total_empty;
        }

        GOLOS_CHECK_VALUE(total + 1 <= 200, "Cannot create more than 200 collections");
        GOLOS_CHECK_VALUE(total_empty + 1 <= 5, "Cannot create more than 5 empty collections (without tokens)");

        _db.create<nft_collection_object>([&](auto& nco) {
            nco.creator = op.creator;
            nco.name = name;
            from_string(nco.json_metadata, op.json_metadata);
            nco.max_token_count = op.max_token_count;
            nco.created = _db.head_block_time();
        });
    }

    void nft_collection_delete_evaluator::do_apply(const nft_collection_delete_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__201, "nft_collection_delete_operation");

        auto name = nft_name_from_string(op.name);

        const auto& nft_coll = _db.get_nft_collection(name);

        GOLOS_CHECK_VALUE(nft_coll.token_count == 0, "Collection has tokens.");

        _db.remove(nft_coll);
    }

    void nft_issue_evaluator::do_apply(const nft_issue_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__201, "nft_issue_operation");

        auto name = nft_name_from_string(op.name);

        const auto& nft_coll = _db.get_nft_collection(name);

        uint32_t token_id = 1;
        const auto& idx = _db.get_index<nft_index, by_token_id>();
        auto itr = idx.begin();
        if (itr != idx.end()) token_id = itr->token_id + 1;

        _db.get_account(op.to);

        auto fee = _db.get_witness_schedule_object().median_props.nft_issue_cost;
        if (fee.amount != 0) {
            auto multiplier = op.json_metadata.size() / 1024;
            fee += (fee * multiplier);

            const auto& creator = _db.get_account(op.creator);
            GOLOS_CHECK_BALANCE(_db, creator, MAIN_BALANCE, fee);
            _db.adjust_balance(creator, -fee);
            _db.adjust_balance(_db.get_account(STEEMIT_WORKER_POOL_ACCOUNT), fee);
        }

        GOLOS_CHECK_VALUE(nft_coll.token_count + 1 <= nft_coll.max_token_count, "Cannot issue more tokens of this collection.");

        auto now = _db.head_block_time();

        _db.create<nft_object>([&](auto& no) {
            no.creator = op.creator;
            no.name = name;

            no.owner = op.to;
            no.token_id = token_id;

            no.issue_cost = fee;
            from_string(no.json_metadata, op.json_metadata);
            no.issued = now;
            no.last_update = now;
        });

        _db.modify(nft_coll, [&](auto& nco) {
            ++nco.token_count;
        });

        _db.push_event(nft_token_operation(op.creator, op.name, op.to, token_id, fee));
    }

    void nft_transfer_evaluator::do_apply(const nft_transfer_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__201, "nft_transfer_operation");

        const auto& no = _db.get_nft(op.token_id);
        GOLOS_CHECK_VALUE(op.from == no.owner, "Cannot transfer not your token.");
        GOLOS_CHECK_VALUE(!no.burnt, "Cannot transfer burnt token.");

        _db.get_account(op.to);

        if (op.to == STEEMIT_NULL_ACCOUNT) { // burning
            if (no.issue_cost.amount > 1) {
                auto cost = asset(no.issue_cost.amount / 2, SBD_SYMBOL);
                const auto& from =_db.get_account(op.from);
                const auto& workers = _db.get_account(STEEMIT_WORKER_POOL_ACCOUNT);
                if (workers.sbd_balance >= cost) {
                    _db.adjust_balance(workers, -cost);
                    _db.adjust_balance(from, cost);
                } else {
                    _db.adjust_supply(cost);
                    _db.adjust_balance(from, cost);
                }
            }

            const auto& nft_coll = _db.get_nft_collection(no.name);

            _db.modify(nft_coll, [&](auto& nco) {
                --nco.token_count;
            });

            const auto& order_idx = _db.get_index<nft_order_index, by_token_id>();
            auto order_itr = order_idx.find(no.token_id);
            if (order_itr != order_idx.end()) {
                _db.remove(*order_itr);
            }

            _db.modify(no, [&](auto& no) {
                no.burnt = true;
            });
        } else {
            _db.modify(no, [&](auto& no) {
                no.owner = op.to;
                no.last_update = _db.head_block_time();
            });
        }
    }

    void nft_sell_evaluator::do_apply(const nft_sell_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__201, "nft_sell_operation");

        const auto& no = _db.get_nft(op.token_id);
        GOLOS_CHECK_VALUE(no.owner == op.seller, "Cannot sell not your token.");
        GOLOS_CHECK_VALUE(!no.burnt, "Cannot sell burnt token.");

        GOLOS_CHECK_VALUE(!no.selling, "You already selling this token.");
        GOLOS_CHECK_OBJECT_MISSING(_db, nft_order, op.token_id);
        GOLOS_CHECK_OBJECT_MISSING(_db, nft_order, op.seller, op.order_id);

        if (op.price.symbol != STEEM_SYMBOL && op.price.symbol != SBD_SYMBOL) {
            _db.get_asset(op.price.symbol);
        }

        const auto& nco = _db.get_nft_collection(no.name);

        if (op.buyer == account_name_type()) {
            _db.modify(no, [&](auto& no) {
                no.last_update = _db.head_block_time();
                no.selling = true;
            });

            _db.create<nft_order_object>([&](auto& noo) {
                noo.creator = no.creator;
                noo.name = no.name;
                noo.token_id = no.token_id;

                noo.owner = no.owner;
                noo.order_id = op.order_id;
                noo.price = op.price;
                noo.selling = true;

                noo.created = _db.head_block_time();
            });

            _db.modify(nco, [&](auto& nco) {
                ++nco.sell_order_count;
                nco.market_depth += op.price.to_real();
            });

            return;
        }

        const auto& noo = _db.get_nft_order(op.buyer, op.order_id);
        GOLOS_CHECK_VALUE(!noo.selling, "Cannot sell because this order is not buying - it is selling");
        GOLOS_CHECK_VALUE(noo.name == no.name, "Wrong NFT collection name");

        if (op.price.amount != 0) {
            GOLOS_CHECK_VALUE(op.price.symbol == noo.price.symbol, "Wrong price token");
            GOLOS_CHECK_VALUE(op.price <= noo.price, "Cannot sell with price greater than buyer's one");
        }

        auto price = op.price;
        if (price.amount == 0) {
            price = noo.price;
        }

        _db.modify(no, [&](auto& no) {
            no.last_update = _db.head_block_time();
            no.owner = noo.owner;
            no.selling = false;
            no.last_buy_price = price;
        });

        _db.remove(noo);

        auto price_real = price.to_real();

        _db.modify(nco, [&](auto& nco) {
            nco.last_buy_price = price;
            --nco.buy_order_count;
            nco.market_volume += price_real;
            nco.market_asks -= price_real;
        });
    }

    void nft_buy_evaluator::do_apply(const nft_buy_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__201, "nft_buy_operation");

        if (op.price.symbol != STEEM_SYMBOL && op.price.symbol != SBD_SYMBOL) {
            _db.get_asset(op.price.symbol);
        }

        if (op.token_id) {
            const auto& noo = _db.get_nft_order(op.token_id);

            GOLOS_CHECK_VALUE(noo.selling, "Cannot buy because this order is not selling - it is buying");

            auto& no = _db.get_nft(noo.token_id);

            if (op.price.amount != 0) {
                GOLOS_CHECK_VALUE(op.price.symbol == noo.price.symbol, "Wrong price token");
                GOLOS_CHECK_VALUE(op.price >= noo.price, "Cannot buy with price lesser than seller's one");
            }

            auto price = op.price;
            if (price.amount == 0) {
                price = noo.price;
            }

            _db.adjust_balance(_db.get_account(noo.owner), price);
            _db.adjust_balance(_db.get_account(op.buyer), -price);

            _db.modify(no, [&](auto& no) {
                no.last_update = _db.head_block_time();
                no.owner = op.buyer;
                no.selling = false;
                no.last_buy_price = price;
            });

            const auto& nco = _db.get_nft_collection(no.name);

            _db.remove(noo);

            auto price_real = price.to_real();

            _db.modify(nco, [&](auto& nco) {
                nco.last_buy_price = price;
                --nco.sell_order_count;
                nco.market_depth -= price_real;
                nco.market_volume += price_real;
            });

            return;
        }

        GOLOS_CHECK_OBJECT_MISSING(_db, nft_order, op.buyer, op.order_id);

        auto name = nft_name_from_string(op.name);

        auto& nco = _db.get_nft_collection(name);

        _db.create<nft_order_object>([&](auto& noo) {
            noo.creator = nco.creator;
            noo.name = name;
            noo.token_id = 0;

            noo.owner = op.buyer;
            noo.order_id = op.order_id;
            noo.price = op.price;
            noo.selling = false;

            noo.created = _db.head_block_time();
        });

        _db.modify(nco, [&](auto& nco) {
            ++nco.buy_order_count;
            nco.market_asks += op.price.to_real();
        });
    }

    void nft_cancel_order_evaluator::do_apply(const nft_cancel_order_operation& op) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29__201, "nft_cancel_order_operation");

        const auto& noo = _db.get_nft_order(op.owner, op.order_id);

        if (noo.selling) {
            auto& no = _db.get_nft(noo.token_id);

            _db.modify(no, [&](auto& no) {
                no.last_update = _db.head_block_time();
                no.selling = false;
            });
        }

        const auto& nco = _db.get_nft_collection(noo.name);

        auto price_real = noo.price.to_real();

        _db.modify(nco, [&](auto& nco) {
            if (noo.selling) {
                --nco.sell_order_count;
                nco.market_depth -= price_real;
            } else {
                --nco.buy_order_count;
                nco.market_asks -= price_real;
            }
        });

        _db.remove(noo);
    }
} } // golos::chain
