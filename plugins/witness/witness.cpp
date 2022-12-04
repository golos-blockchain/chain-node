
#include <golos/plugins/witness/witness.hpp>

#include <golos/chain/database_exceptions.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/witness_objects.hpp>
#include <golos/time/time.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <fc/smart_ref_impl.hpp>

#include <memory>
#include <thread>

#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using std::string;
using std::vector;

namespace bpo = boost::program_options;

void new_chain_banner(const golos::chain::database &db) {
    std::cerr << "\n"
            "********************************\n"
            "*                              *\n"
            "*   ------- NEW CHAIN ------   *\n"
            "*   -   Welcome to Golos!  -   *\n"
            "*   ------------------------   *\n"
            "*                              *\n"
            "********************************\n"
            "\n";
    return;
}

template<typename T>
T dejsonify(const string &s) {
    return fc::json::from_string(s).as<T>();
}

#define DEFAULT_VALUE_VECTOR(value) default_value({fc::json::to_string(value)}, fc::json::to_string(value))
#define LOAD_VALUE_SET(options, name, container, type) \
            if( options.count(name) ) { \
                  const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
                  std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &dejsonify<type>); \
            }

namespace golos {
    namespace plugins {
        namespace witness_plugin {

            namespace asio = boost::asio;
            namespace posix_time = boost::posix_time;
            namespace system = boost::system;

            struct witness_plugin::impl final {
                impl():
                    p2p_(appbase::app().get_plugin<golos::plugins::p2p::p2p_plugin>()),
                    chain_(appbase::app().get_plugin<golos::plugins::chain::plugin>()),
                    production_timer_(appbase::app().get_io_service()) {

                }

                ~impl(){}

                golos::chain::database& database() {
                    return chain_.db();
                }

                golos::chain::database& database() const {
                    return chain_.db();
                }

                golos::plugins::chain::plugin& chain() {
                    return chain_;
                }

                golos::plugins::chain::plugin& chain() const {
                    return chain_;
                }

                golos::plugins::p2p::p2p_plugin& p2p(){
                    return p2p_;
                };

                golos::plugins::p2p::p2p_plugin& p2p() const {
                    return p2p_;
                };

                golos::plugins::p2p::p2p_plugin& p2p_;

                golos::plugins::chain::plugin& chain_;

                void on_applied_block(const signed_block &b);

                void schedule_production_loop();

                block_production_condition::block_production_condition_enum block_production_loop();

                block_production_condition::block_production_condition_enum maybe_produce_block(fc::mutable_variant_object &capture);

                boost::program_options::variables_map _options;
                uint32_t _required_witness_participation = 33 * STEEMIT_1_PERCENT;

                uint32_t _production_skip_flags = golos::chain::database::skip_nothing;
                bool _production_enabled = false;
                asio::deadline_timer production_timer_;

                std::map<public_key_type, fc::ecc::private_key> _private_keys;
                std::set<string> _witnesses;

                bool _warn_miner = false;
            };

            void witness_plugin::set_program_options(
                    boost::program_options::options_description &command_line_options,
                    boost::program_options::options_description &config_file_options) {
                    string witness_id_example = "initwitness";

                config_file_options.add_options()
                        ("enable-stale-production", bpo::value<bool>()->implicit_value(false) , "Enable block production, even if the chain is stale.")
                        ("required-participation", bpo::value<int>()->implicit_value(uint32_t(3 * STEEMIT_1_PERCENT)), "Percent of witnesses (0-99) that must be participating in order to produce blocks")
                        ("witness,w", bpo::value<vector<string>>()->composing()->multitoken(), ("name of witness controlled by this node (e.g. " + witness_id_example + " )").c_str())
                        ("miner,m", bpo::value<vector<string>>()->composing()->multitoken(), "name of miner and its private key (e.g. [\"account\",\"WIF PRIVATE KEY\"] )")
                        ("mining-threads,t", bpo::value<uint32_t>(), "Number of threads to use for proof of work mining")
                        ("private-key", bpo::value<vector<string>>()->composing()->multitoken(), "WIF PRIVATE KEY to be used by one or more witnesses or miners")
                        ("miner-account-creation-fee", bpo::value<uint64_t>()->implicit_value(100000), "Account creation fee to be voted on upon successful POW - Minimum fee is 100.000 STEEM (written as 100000)")
                        ("miner-maximum-block-size", bpo::value<uint32_t>()->implicit_value(131072), "Maximum block size (in bytes) to be voted on upon successful POW - Max block size must be between 128 KB and 750 MB")
                        ("miner-sbd-interest-rate", bpo::value<uint32_t>()->implicit_value(1000), "SBD interest rate to be vote on upon successful POW - Default interest rate is 10% (written as 1000)")
                        ;
            }

            using std::vector;
            using std::pair;
            using std::string;

            void witness_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
                try {
                    ilog("witness plugin:  plugin_initialize() begin");
                    pimpl = std::make_unique<witness_plugin::impl>();

                    pimpl->_options = &options;
                    LOAD_VALUE_SET(options, "witness", pimpl->_witnesses, string)
                    edump((pimpl->_witnesses));

                    if (options.count("miner")) {
                        pimpl->_warn_miner = true;
                    }

                    if(options.count("enable-stale-production")){
                        pimpl->_production_enabled = options["enable-stale-production"].as<bool>();
                    }

                    if(options.count("required-participation")){
                        int e = static_cast<int>(options["required-participation"].as<int>());
                        pimpl->_required_witness_participation = uint32_t(e * STEEMIT_1_PERCENT);
                    }

                    if (options.count("private-key")) {
                        const std::vector<std::string> keys = options["private-key"].as<std::vector<std::string>>();
                        for (const std::string &wif_key : keys) {
                            fc::optional<fc::ecc::private_key> private_key = golos::utilities::wif_to_key(wif_key);
                            GOLOS_CHECK_OPTION(private_key.valid(), "unable to parse private key");
                            pimpl->_private_keys[private_key->get_public_key()] = *private_key;
                        }
                    }

                    ilog("witness plugin:  plugin_initialize() end");
                } FC_LOG_AND_RETHROW()
            }

            void witness_plugin::plugin_startup() {
                try {
                    ilog("witness plugin:  plugin_startup() begin");

                    if (pimpl->_warn_miner) {
                        wlog("Miners are not supported in HF28. Please remove following parameters from config: miner, mining-threads, miner-account-creation-fee, miner-maximum-block-size, miner-sbd-interest-rate");
                    }

                    auto &d = pimpl->database();
                    //Start NTP time client
                    golos::time::now();

                    if (!pimpl->_witnesses.empty()) {
                        ilog("Launching block production for ${n} witnesses.", ("n", pimpl->_witnesses.size()));
                        pimpl->p2p().set_block_production(true);
                        if (pimpl->_production_enabled) {
                            if (d.head_block_num() == 0) {
                                new_chain_banner(d);
                            }
                            pimpl->_production_skip_flags |= golos::chain::database::skip_undo_history_check;
                        }
                        pimpl->schedule_production_loop();
                    } else
                        elog("No witnesses configured! Please add witness names and private keys to configuration.");
                    d.applied_block.connect([this](const protocol::signed_block &b) { pimpl->on_applied_block(b); });
                    ilog("witness plugin:  plugin_startup() end");
                } FC_CAPTURE_AND_RETHROW()
            }

            void witness_plugin::plugin_shutdown() {
                if (!pimpl->_witnesses.empty()) {
                    ilog("shutting downing production timer");
                    pimpl->production_timer_.cancel();
                }
            }

            witness_plugin::witness_plugin() {}

            witness_plugin::~witness_plugin() {}

            void witness_plugin::impl::schedule_production_loop() {
                //Schedule for the next second's tick regardless of chain state
                // If we would wait less than 50ms, wait for the whole second.
                int64_t ntp_microseconds = golos::time::now().time_since_epoch().count();
                int64_t next_microseconds = 1000000 - (ntp_microseconds % 1000000);
                if (next_microseconds < 50000) { // we must sleep for at least 50ms
                    next_microseconds += 1000000;
                }

                production_timer_.expires_from_now( posix_time::microseconds(next_microseconds) );
                production_timer_.async_wait( [this](const system::error_code &) { block_production_loop(); } );
            }

            block_production_condition::block_production_condition_enum witness_plugin::impl::block_production_loop() {
                if (fc::time_point::now() < fc::time_point(STEEMIT_GENESIS_TIME)) {
                    wlog("waiting until genesis time to produce block: ${t}", ("t", STEEMIT_GENESIS_TIME));
                    schedule_production_loop();
                    return block_production_condition::wait_for_genesis;
                }

                block_production_condition::block_production_condition_enum result;
                fc::mutable_variant_object capture;
                try {
                    result = maybe_produce_block(capture);
                }
                catch (const fc::canceled_exception &) {
                    //We're trying to exit. Go ahead and let this one out.
                    throw;
                }
                catch (const golos::chain::unknown_hardfork_exception &e) {
                    // Hit a hardfork that the current node know nothing about, stop production and inform user
                    elog("${e}\nNode may be out of date...", ("e", e.to_detail_string()));
                    throw;
                }
                catch (const fc::exception &e) {
                    elog("Got exception while generating block:\n${e}", ("e", e.to_detail_string()));
                    result = block_production_condition::exception_producing_block;
                }

                switch (result) {
                    case block_production_condition::produced:
                        ilog("Generated block #${n} with timestamp ${t} at time ${c} by ${w}", (capture));
                        break;
                    case block_production_condition::not_synced:
                        // This log-record is commented, because it outputs very often
                        // ilog("Not producing block because production is disabled until we receive a recent block (see: --enable-stale-production)");
                        break;
                    case block_production_condition::not_my_turn:
                        // This log-record is commented, because it outputs very often
                        // ilog("Not producing block because it isn't my turn");
                        break;
                    case block_production_condition::not_time_yet:
                        // This log-record is commented, because it outputs very often
                        // ilog("Not producing block because slot has not yet arrived");
                        break;
                    case block_production_condition::no_private_key:
                        ilog("Not producing block for ${scheduled_witness} because I don't have the private key for ${scheduled_key}",
                             (capture));
                        break;
                    case block_production_condition::low_participation:
                        elog("Not producing block because node appears to be on a minority fork with only ${pct}% witness participation",
                             (capture));
                        break;
                    case block_production_condition::lag:
                        elog("Not producing block because node didn't wake up within 500ms of the slot time.");
                        break;
                    case block_production_condition::consecutive:
                        elog("Not producing block because the last block was generated by the same witness.\nThis node is probably disconnected from the network so block production has been disabled.\nDisable this check with --allow-consecutive option.");
                        break;
                    case block_production_condition::exception_producing_block:
                        elog("Failure when producing block with no transactions");
                        break;
                    case block_production_condition::wait_for_genesis:
                        break;
                }

                schedule_production_loop();
                return result;
            }

            block_production_condition::block_production_condition_enum witness_plugin::impl::maybe_produce_block(fc::mutable_variant_object &capture) {
                auto &db = database();
                fc::time_point now_fine = golos::time::now();
                fc::time_point_sec now = now_fine + fc::microseconds(500000);

                // If the next block production opportunity is in the present or future, we're synced.
                if (!_production_enabled) {
                    if (db.get_slot_time(1) >= now) {
                        _production_enabled = true;
                    } else {
                        return block_production_condition::not_synced;
                    }
                }

                // is anyone scheduled to produce now or one second in the future?
                uint32_t slot = db.get_slot_at_time(now);
                if (slot == 0) {
                    capture("next_time", db.get_slot_time(1));
                    return block_production_condition::not_time_yet;
                }

                //
                // this assert should not fail, because now <= db.head_block_time()
                // should have resulted in slot == 0.
                //
                // if this assert triggers, there is a serious bug in get_slot_at_time()
                // which would result in allowing a later block to have a timestamp
                // less than or equal to the previous block
                //
                assert(now > db.head_block_time());

                string scheduled_witness = db.get_scheduled_witness(slot);
                // we must control the witness scheduled to produce the next block.
                if (_witnesses.find(scheduled_witness) == _witnesses.end()) {
                    capture("scheduled_witness", scheduled_witness);
                    return block_production_condition::not_my_turn;
                }

                const auto &witness_by_name = db.get_index<golos::chain::witness_index>().indices().get<golos::chain::by_name>();
                auto itr = witness_by_name.find(scheduled_witness);

                fc::time_point_sec scheduled_time = db.get_slot_time(slot);
                golos::protocol::public_key_type scheduled_key = itr->signing_key;
                auto private_key_itr = _private_keys.find(scheduled_key);

                if (private_key_itr == _private_keys.end()) {
                    capture("scheduled_witness", scheduled_witness);
                    capture("scheduled_key", scheduled_key);
                    return block_production_condition::no_private_key;
                }

                uint32_t prate = db.witness_participation_rate();
                if (prate < _required_witness_participation) {
                    capture("pct", uint32_t(100 * uint64_t(prate) / STEEMIT_1_PERCENT));
                    return block_production_condition::low_participation;
                }

                if (llabs((scheduled_time - now).count()) > fc::milliseconds(500).count()) {
                    capture("scheduled_time", scheduled_time)("now", now);
                    return block_production_condition::lag;
                }

                int retry = 0;
                do {
                    try {
                        // TODO: the same thread as used in chain-plugin,
                        //       but in the future it should refactored to calling of a chain-plugin function
                        auto block = db.generate_block(
                                scheduled_time,
                                scheduled_witness,
                                private_key_itr->second,
                                _production_skip_flags
                        );
                        capture("n", block.block_num())("t", block.timestamp)("c", now)("w", scheduled_witness);
                        p2p().broadcast_block(block);

                        return block_production_condition::produced;
                    }
                    catch (fc::exception &e) {
                        elog("${e}", ("e", e.to_detail_string()));
                        elog("Clearing pending transactions and attempting again");
                        db.clear_pending();
                        retry++;
                    }
                } while (retry < 2);

                return block_production_condition::exception_producing_block;
            }

            void witness_plugin::impl::on_applied_block(const golos::protocol::signed_block &b) {
                if (_warn_miner && b.block_num() % 3 == 0) {
                    wlog("Miners are not supported in HF28. Please remove following parameters from config: mining-threads, miner-account-creation-fee, miner-maximum-block-size, miner-sbd-interest-rate");
                }
            }
        }
    }
}

