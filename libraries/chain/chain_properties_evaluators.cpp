#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/protocol/validate_helper.hpp>

namespace golos { namespace chain {

    void witness_update_evaluator::do_apply(const witness_update_operation& o) {
        _db.get_account(o.owner); // verify owner exists

        if (_db.has_hardfork(STEEMIT_HARDFORK_0_1)) {
            GOLOS_CHECK_OP_PARAM(o, url, {
                GOLOS_CHECK_VALUE_MAX_SIZE(o.url, STEEMIT_MAX_WITNESS_URL_LENGTH);
            });
        } else if (o.url.size() > STEEMIT_MAX_WITNESS_URL_LENGTH) {
            // after HF, above check can be moved to validate() if reindex doesn't show this warning
            wlog("URL is too long in block ${b}", ("b", _db.head_block_num() + 1));
        }

        const bool has_hf18 = _db.has_hardfork(STEEMIT_HARDFORK_0_18__673);

        auto update_witness = [&](witness_object& w) {
            from_string(w.url, o.url);
            w.signing_key = o.block_signing_key;
            if (!has_hf18) {
                w.props = o.props;
            }
        };

        const auto& idx = _db.get_index<witness_index>().indices().get<by_name>();
        auto itr = idx.find(o.owner);
        if (itr != idx.end()) {
            _db.modify(*itr, update_witness);
        } else {
            _db.create<witness_object>([&](witness_object& w) {
                w.owner = o.owner;
                w.created = _db.head_block_time();
                update_witness(w);
                if (_db.has_hardfork(STEEMIT_HARDFORK_0_26__168)) {
                    w.props.hf26_windows_sec_to_min();
                }
                if (_db.has_hardfork(STEEMIT_HARDFORK_0_28)) {
                    w.props.min_golos_power_to_curate = _db.get_witness_schedule_object().median_props.min_golos_power_to_curate;
                }
            });
        }
    }

    struct chain_properties_update {
        using result_type = void;

        const database& _db;
        chain_properties& _wprops;

        chain_properties_update(const database& db, chain_properties& wprops)
                : _db(db), _wprops(wprops) {
        }

        result_type operator()(const chain_properties_19& props) const {
            GOLOS_CHECK_PARAM(props, {
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_22__65)) {
                    auto max_delegated_vesting_interest_rate = props.max_delegated_vesting_interest_rate;
                    GOLOS_CHECK_VALUE_LE(max_delegated_vesting_interest_rate, STEEMIT_MAX_DELEGATED_VESTING_INTEREST_RATE_PRE_HF22);
                }
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_22__66)) {
                    auto min_curation_percent = props.min_curation_percent;
                    auto max_curation_percent = props.max_curation_percent;
                    GOLOS_CHECK_VALUE_LEGE(min_curation_percent, STEEMIT_MIN_CURATION_PERCENT_PRE_HF22, max_curation_percent);
                }
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_23__93)) {
                    auto max_referral_break_fee = props.max_referral_break_fee;
                    GOLOS_CHECK_PARAM(max_referral_break_fee, {
                        GOLOS_CHECK_VALUE(max_referral_break_fee <= GOLOS_MAX_REFERRAL_BREAK_FEE_PRE_HF22,
                            "Maximum break free cann't be more than ${max}", ("max", GOLOS_MAX_REFERRAL_BREAK_FEE_PRE_HF22));
                    });
                }
            });
            _wprops = props;
        }

        result_type operator()(const chain_properties_22& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_22, "chain_properties_22");
            _wprops = props;
        }

        void hf23_check(const chain_properties_23& props) const {
            GOLOS_CHECK_PARAM(props, {
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_27)) {
                    auto claim_idleness_time = props.claim_idleness_time;
                    GOLOS_CHECK_VALUE_GE(claim_idleness_time, GOLOS_MIN_CLAIM_IDLENESS_TIME);
                }
            });
        }

        result_type operator()(const chain_properties_23& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_23, "chain_properties_23");
            hf23_check(props);
            _wprops = props;
        }

        result_type operator()(const chain_properties_24& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_24, "chain_properties_24");
            hf23_check(props);
            _wprops = props;
        }

        void hf26_check(const chain_properties_26& props) const {
            GOLOS_CHECK_PARAM(props, {
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_27)) {
                    auto negrep_posting_window = props.negrep_posting_window;
                    GOLOS_CHECK_VALUE_LEGE(negrep_posting_window, 1, std::numeric_limits<uint16_t>::max() / 2);
                }
                // GOLOS_MIN_GOLOS_POWER_TO_CURATE is just 0
                auto min_golos_power_to_curate = props.min_golos_power_to_curate;
                if (!_db.has_hardfork(STEEMIT_HARDFORK_0_28)) {
                    GOLOS_CHECK_ASSET_GE(min_golos_power_to_curate, GOLOS, GOLOS_MIN_GOLOS_POWER_TO_CURATE);
                } else {
                    GOLOS_CHECK_ASSET_GE(min_golos_power_to_curate, GBG, GOLOS_MIN_GOLOS_POWER_TO_CURATE);
                }
            });
        }

        result_type operator()(const chain_properties_26& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_26, "chain_properties_26");
            hf23_check(props);
            hf26_check(props);
            _wprops = props;
        }

        result_type operator()(const chain_properties_27& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_27, "chain_properties_27");
            hf23_check(props);
            hf26_check(props);
            _wprops = props;
        }

        result_type operator()(const chain_properties_28& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_28, "chain_properties_28");
            hf23_check(props);
            hf26_check(props);
            _wprops = props;
        }

        result_type operator()(const chain_properties_29& props) const {
            ASSERT_REQ_HF(STEEMIT_HARDFORK_0_29, "chain_properties_29");
            hf23_check(props);
            hf26_check(props);
            _wprops = props;
        }

        template<typename Props>
        result_type operator()(Props&& props) const {
            _wprops = props;
        }
    };

    void chain_properties_update_evaluator::do_apply(const chain_properties_update_operation& o) {
        _db.get_account(o.owner); // verify owner exists

        const auto& idx = _db.get_index<witness_index>().indices().get<by_name>();
        auto itr = idx.find(o.owner);
        if (itr != idx.end()) {
            _db.modify(*itr, [&](witness_object& w) {
                o.props.visit(chain_properties_update(_db, w.props));
            });
        } else {
            _db.create<witness_object>([&](witness_object& w) {
                w.owner = o.owner;
                w.created = _db.head_block_time();
                o.props.visit(chain_properties_update(_db, w.props));
            });
        }
    }

    void transit_to_cyberway_evaluator::do_apply(const transit_to_cyberway_operation& o) {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_21__1348, "transit_to_cyberway");

        _db.get_account(o.owner); // verify owner exists
        auto& witness = _db.get_witness(o.owner);

        GOLOS_CHECK_OP_PARAM(o, vote_to_transit, {
            GOLOS_CHECK_VALUE(o.vote_to_transit != (witness.transit_to_cyberway_vote != STEEMIT_GENESIS_TIME),
                "You should change your vote.");
        });

        _db.modify(witness, [&](witness_object& w) {
            if (o.vote_to_transit) {
                w.transit_to_cyberway_vote = _db.head_block_time();
            } else {
                w.transit_to_cyberway_vote = STEEMIT_GENESIS_TIME;
            }
        });
    }

} } // golos::chain
