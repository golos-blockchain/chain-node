#include <openssl/md5.h>

#include <boost/algorithm/string.hpp>
#include <boost/iostreams/device/mapped_file.hpp>

#include <golos/protocol/steem_operations.hpp>

#include <golos/chain/block_summary_object.hpp>
#include <golos/chain/compound.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/database_exceptions.hpp>
#include <golos/chain/db_with.hpp>
#include <golos/chain/freezing_utils.hpp>
#include <golos/chain/hf_actions.hpp>
#include <golos/chain/evaluator_registry.hpp>
#include <golos/chain/index.hpp>
#include <golos/chain/snapshot_state.hpp>
#include <golos/chain/steem_evaluator.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/comment_bill.hpp>
#include <golos/chain/transaction_object.hpp>
#include <golos/chain/shared_db_merkle.hpp>
#include <golos/chain/operation_notification.hpp>
#include <golos/chain/proposal_object.hpp>
#include <golos/chain/curation_info.hpp>

#include <fc/smart_ref_impl.hpp>

#include <fc/container/deque.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>

#include <appbase/application.hpp>
#include <csignal>
#include <cerrno>
#include <cstring>

#define VIRTUAL_SCHEDULE_LAP_LENGTH  ( fc::uint128_t(uint64_t(-1)) )
#define VIRTUAL_SCHEDULE_LAP_LENGTH2 ( fc::uint128_t::max_value() )

namespace golos { namespace chain {

struct object_schema_repr {
    std::pair<uint16_t, uint16_t> space_type;
    std::string type;
};

struct operation_schema_repr {
    std::string id;
    std::string type;
};

struct db_schema {
    std::map<std::string, std::string> types;
    std::vector<object_schema_repr> object_types;
    std::string operation_type;
    std::vector<operation_schema_repr> custom_operation_types;
};

} } // golos::chain

FC_REFLECT((golos::chain::object_schema_repr), (space_type)(type))
FC_REFLECT((golos::chain::operation_schema_repr), (id)(type))
FC_REFLECT((golos::chain::db_schema), (types)(object_types)(operation_type)(custom_operation_types))


namespace golos { namespace chain {

        using std::sig_atomic_t;
        using boost::container::flat_set;

        inline u256 to256(const fc::uint128_t &t) {
            u256 v(t.hi);
            v <<= 64;
            v += t.lo;
            return v;
        }

        class signal_guard {
            struct sigaction old_hup_action, old_int_action, old_term_action;

            static volatile sig_atomic_t is_interrupted;

            bool is_restored = true;

            static inline void throw_exception();

            public:
                inline signal_guard();

                inline ~signal_guard();

                void setup();

                void restore();

                static inline sig_atomic_t get_is_interrupted() noexcept;
        };

        volatile sig_atomic_t signal_guard::is_interrupted = false;

        inline void signal_guard::throw_exception() {
            FC_THROW_EXCEPTION(database_signal_exception,
                               "Error setup signal handler: ${e}.",
                               ("e", strerror(errno)));
        }

        inline signal_guard::signal_guard() {
            setup();
        }

        inline signal_guard::~signal_guard() {
            try {
                restore();
            }
            FC_CAPTURE_AND_LOG(())
        }

        void signal_guard::setup() {
            if (is_restored) {
                struct sigaction new_action;

                new_action.sa_handler = [](int signum) {
                    is_interrupted = true;
                };
                new_action.sa_flags = SA_RESTART;

                if (sigemptyset(&new_action.sa_mask) == -1) {
                    throw_exception();
                }

                if (sigaction(SIGHUP, &new_action, &old_hup_action) == -1) {
                    throw_exception();
                }

                if (sigaction(SIGINT, &new_action, &old_int_action) == -1) {
                    throw_exception();
                }

                if (sigaction(SIGTERM, &new_action, &old_term_action) == -1) {
                    throw_exception();
                }

                is_restored = false;
            }
        }

        void signal_guard::restore() {
            if (!is_restored) {
                if (sigaction(SIGHUP, &old_hup_action, nullptr) == -1) {
                    throw_exception();
                }

                if (sigaction(SIGINT, &old_int_action, nullptr) == -1) {
                    throw_exception();
                }

                if (sigaction(SIGTERM, &old_term_action, nullptr) == -1) {
                    throw_exception();
                }

                is_interrupted = false;
                is_restored = true;
            }
        }

        inline sig_atomic_t signal_guard::get_is_interrupted() noexcept {
            return is_interrupted;
        }

        class database_impl {
        public:
            database_impl(database &self);

            database &_self;
            evaluator_registry<operation> _evaluator_registry;
        };

        database_impl::database_impl(database &self)
                : _self(self), _evaluator_registry(self) {
        }

        database::database()
                : _my(new database_impl(*this)) {
        }

        database::~database() {
            clear_pending();
        }

        void database::open(const fc::path &data_dir, const fc::path &shared_mem_dir, uint64_t initial_supply, uint64_t shared_file_size, uint32_t chainbase_flags) {
            try {
                auto start = fc::time_point::now();
                wlog("Start opening database. Please wait, don't break application...");

                init_schema();
                chainbase::database::open(shared_mem_dir, chainbase_flags, shared_file_size);

                initialize_indexes();
                initialize_evaluators();

                auto end = fc::time_point::now();
                wlog("Done opening database, elapsed time ${t} sec", ("t", double((end - start).count()) / 1000000.0));

                if (chainbase_flags & chainbase::database::read_write && _init_block_log) {
                    start = fc::time_point::now();
                    wlog("Start opening block log. Please wait, don't break application...");

                    if (!find<dynamic_global_property_object>()) {
                        with_strong_write_lock([&]() {
                            init_genesis(initial_supply);
                        });
                    }

                    _block_log.open(data_dir / "block_log");

                    // Rewind all undo state. This should return us to the state at the last irreversible block.
                    with_strong_write_lock([&]() {
                        undo_all();
                    });

                    if (revision() != head_block_num()) {
                        with_strong_read_lock([&]() {
                            init_hardforks(); // Writes to local state, but reads from db
                        });

                        FC_THROW_EXCEPTION(database_revision_exception,
                                           "Chainbase revision does not match head block num, "
                                           "current revision is ${rev}, "
                                           "current head block num is ${head}",
                                           ("rev", revision())
                                           ("head", head_block_num()));
                    }

                    if (head_block_num()) {
                        auto head_block = _block_log.read_block_by_num(head_block_num());
                        // This assertion should be caught and a reindex should occur
                        FC_ASSERT(head_block.valid() && head_block->id() ==
                                                        head_block_id(), "Chain state does not match block log. Please reindex blockchain.");

                        _fork_db.start_block(*head_block);
                    }
                    end = fc::time_point::now();
                    wlog("Done opening block log, elapsed time ${t} sec", ("t", double((end - start).count()) / 1000000.0));
                }

                with_strong_read_lock([&]() {
                    init_hardforks(); // Writes to local state, but reads from db
                });

            }
            FC_CAPTURE_LOG_AND_RETHROW((data_dir)(shared_mem_dir)(shared_file_size))
        }

        void database::reindex(const fc::path &data_dir, const fc::path &shared_mem_dir, uint32_t from_block_num, uint64_t shared_file_size, bool validate_during_replay) {
            try {
                set_reindexing(true);

                signal_guard sg;
                _fork_db.reset();    // override effect of _fork_db.start_block() call in open()

                auto start = fc::time_point::now();
                GOLOS_ASSERT(_block_log.head(), block_log_exception, "No blocks in block log. Cannot reindex an empty chain.");

                ilog("Replaying blocks...");

                uint64_t skip_flags =
                        skip_block_size_check |
                        skip_witness_signature |
                        skip_transaction_signatures |
                        skip_transaction_dupe_check |
                        skip_tapos_check |
                        skip_merkle_check |
                        skip_witness_schedule_check |
                        skip_authority_check |
                        skip_validate_operations | /// no need to validate operations
                        skip_validate_invariants |
                        skip_block_log;
                if (validate_during_replay) {
                    skip_flags = skip_validate_invariants |
                        skip_block_log;
                }

                with_strong_write_lock([&]() {
                    auto cur_block_num = from_block_num;
                    auto last_block_num = _block_log.head()->block_num();
                    auto last_block_pos = _block_log.get_block_pos(last_block_num);
                    int last_reindex_percent = 0;

                    set_reserved_memory(1024*1024*1024); // protect from memory fragmentations ...
                    while (cur_block_num < last_block_num) {
                        if (signal_guard::get_is_interrupted()) {
                            return;
                        }

                        auto end = fc::time_point::now();
                        auto cur_block_pos = _block_log.get_block_pos(cur_block_num);
                        auto cur_block = *_block_log.read_block_by_num(cur_block_num);

                        auto reindex_percent = cur_block_pos * 100 / last_block_pos;
                        if (reindex_percent - last_reindex_percent >= 1) {
                            std::cerr
                                << "   " << reindex_percent << "%   "
                                << cur_block_num << " of " << last_block_num
                                << "   ("  << (free_memory() / (1024 * 1024)) << "M free"
                                << ", elapsed " << double((end - start).count()) / 1000000.0 << " sec)\n";

                            last_reindex_percent = reindex_percent;
                        }

                        apply_block(cur_block, skip_flags);

                        if (cur_block_num % 1000 == 0) {
                            set_revision(head_block_num());
                        }

                        check_free_memory(true, cur_block_num);
                        cur_block_num++;
                    }

                    auto cur_block = *_block_log.read_block_by_num(cur_block_num);
                    apply_block(cur_block, skip_flags);
                    set_reserved_memory(0);
                    set_revision(head_block_num());
                });

                set_reindexing(false);

                if (signal_guard::get_is_interrupted()) {
                    sg.restore();

                    appbase::app().quit();
                }

                if (_block_log.head()->block_num()) {
                    _fork_db.start_block(*_block_log.head());
                }
                auto end = fc::time_point::now();
                ilog("Done reindexing, elapsed time: ${t} sec", ("t",
                        double((end - start).count()) / 1000000.0));
            }
            FC_CAPTURE_AND_RETHROW((data_dir)(shared_mem_dir))

        }

        void database::set_min_free_shared_memory_size(size_t value) {
            _min_free_shared_memory_size = value;
        }

        void database::set_inc_shared_memory_size(size_t value) {
            _inc_shared_memory_size = value;
        }

        void database::set_block_num_check_free_size(uint32_t value) {
            _block_num_check_free_memory = value;
        }

        void database::set_init_block_log(bool init_block_log) {
            _init_block_log = init_block_log;
        }

        void database::set_store_account_metadata(store_metadata_modes store_account_metadata) {
            _store_account_metadata = store_account_metadata;
        }

        void database::set_accounts_to_store_metadata(const std::vector<std::string>& accounts_to_store_metadata) {
            _accounts_to_store_metadata = accounts_to_store_metadata;
        }

        bool database::store_metadata_for_account(const std::string& name) const {
            if (_store_account_metadata != store_metadata_for_listed) {
                return _store_account_metadata == store_metadata_for_all;
            }
            auto& v = _accounts_to_store_metadata;
            return std::find(v.begin(), v.end(), name) != v.end();
        }

        void database::set_store_asset_metadata(bool store_asset_metadata) {
            _store_asset_metadata = store_asset_metadata;
        }

        bool database::store_asset_metadata() const {
            return _store_asset_metadata;
        }

        void database::set_store_memo_in_savings_withdraws(bool store_memo_in_savings_withdraws) {
            _store_memo_in_savings_withdraws = store_memo_in_savings_withdraws;
        }

        bool database::store_memo_in_savings_withdraws() const {
            return _store_memo_in_savings_withdraws;
        }

        void database::set_store_evaluator_events(bool store_evaluator_events) {
            _store_evaluator_events = store_evaluator_events;
        }

        bool database::store_evaluator_events() const {
            return _store_evaluator_events;
        }

        void database::set_store_comment_extras(bool store_comment_extras) {
            _store_comment_extras = store_comment_extras;
        }

        bool database::store_comment_extras() const {
            return _store_comment_extras;
        }

        void database::set_skip_virtual_ops() {
            _skip_virtual_ops = true;
        }

        bool database::_resize(uint32_t current_block_num) {
            if (_inc_shared_memory_size == 0) {
                elog("Auto-scaling of shared file size is not configured!. Do it immediately!");
                return false;
            }

            uint64_t max_mem = max_memory();

            size_t new_max = max_mem + _inc_shared_memory_size;
            wlog(
                "Memory is almost full on block ${block}, increasing to ${mem}M",
                ("block", current_block_num)("mem", new_max / (1024 * 1024)));
            resize(new_max);

            uint64_t free_mem = free_memory();
            uint64_t reserved_mem = reserved_memory();

            if (free_mem > reserved_mem) {
                free_mem -= reserved_mem;
            }

            uint32_t free_mb = uint32_t(free_mem / (1024 * 1024));
            uint32_t reserved_mb = uint32_t(reserved_mem / (1024 * 1024));
            wlog("Free memory is now ${free}M (${reserved}M)", ("free", free_mb)("reserved", reserved_mb));
            _last_free_gb_printed = free_mb / 1024;
            return true;
        }

        void database::check_free_memory(bool skip_print, uint32_t current_block_num) {
            if (0 != current_block_num % _block_num_check_free_memory) {
                return;
            }

            uint64_t reserved_mem = reserved_memory();
            uint64_t free_mem = free_memory();

            if (free_mem > reserved_mem) {
                free_mem -= reserved_mem;
            } else {
                set_reserved_memory(0);
            }

            if (_inc_shared_memory_size != 0 && _min_free_shared_memory_size != 0 &&
                free_mem < _min_free_shared_memory_size
            ) {
                _resize(current_block_num);
            } else if (!skip_print && _inc_shared_memory_size == 0 && _min_free_shared_memory_size == 0) {
                uint32_t free_gb = uint32_t(free_mem / (1024 * 1024 * 1024));
                if ((free_gb < _last_free_gb_printed) || (free_gb > _last_free_gb_printed + 1)) {
                    ilog(
                        "Free memory is now ${n}G. Current block number: ${block}",
                        ("n", free_gb)("block", current_block_num));
                    _last_free_gb_printed = free_gb;
                }

                if (free_gb == 0) {
                    uint32_t free_mb = uint32_t(free_mem / (1024 * 1024));
                    if (free_mb <= (_is_testing ? 2 : 500) && current_block_num % 10 == 0) {
                        elog("Free memory is now ${n}M. Increase shared file size immediately!", ("n", free_mb));
                    }
                }
            }
        }

        void database::wipe(const fc::path &data_dir, const fc::path &shared_mem_dir, bool include_blocks) {
            close();
            chainbase::database::wipe(shared_mem_dir);
            if (include_blocks) {
                fc::remove_all(data_dir / "block_log");
                fc::remove_all(data_dir / "block_log.index");
            }
        }

        void database::close(bool rewind) {
            try {
                // Since pop_block() will move tx's in the popped blocks into pending,
                // we have to clear_pending() after we're done popping to get a clean
                // DB state (issue #336).
                clear_pending();

                chainbase::database::flush();
                chainbase::database::close();

                _block_log.close();

                _fork_db.reset();
            }
            FC_CAPTURE_AND_RETHROW()
        }

        bool database::is_known_block(const block_id_type &id) const {
            try {
                return fetch_block_by_id(id).valid();
            } FC_CAPTURE_AND_RETHROW()
        }

/**
 * Only return true *if* the transaction has not expired or been invalidated. If this
 * method is called with a VERY old transaction we will return false, they should
 * query things by blocks if they are that old.
 */
        bool database::is_known_transaction(const transaction_id_type &id) const {
            try {
                const auto &trx_idx = get_index<transaction_index>().indices().get<by_trx_id>();
                return trx_idx.find(id) != trx_idx.end();
            } FC_CAPTURE_AND_RETHROW()
        }

        block_id_type database::find_block_id_for_num(uint32_t block_num) const {
            try {
                if (block_num == 0) {
                    return block_id_type();
                }

                // Reversible blocks are *usually* in the TAPOS buffer. Since this
                // is the fastest check, we do it first.
                block_summary_id_type bsid = block_num & 0xFFFF;
                const block_summary_object *bs = find<block_summary_object, by_id>(bsid);
                if (bs != nullptr) {
                    if (protocol::block_header::num_from_id(bs->block_id) ==
                        block_num) {
                        return bs->block_id;
                    }
                }

                // Next we query the block log. Irreversible blocks are here.

                auto b = _block_log.read_block_by_num(block_num);
                if (b.valid()) {
                    return b->id();
                }

                // Finally we query the fork DB.
                shared_ptr<fork_item> fitem = _fork_db.fetch_block_on_main_branch_by_number(block_num);
                if (fitem) {
                    return fitem->id;
                }

                return block_id_type();
            }
            FC_CAPTURE_AND_RETHROW((block_num))
        }

        block_id_type database::get_block_id_for_num(uint32_t block_num) const {
            block_id_type bid = find_block_id_for_num(block_num);
            FC_ASSERT(bid != block_id_type());
            return bid;
        }

        optional<signed_block> database::fetch_block_by_id(const block_id_type &id) const {
            try {
                auto b = _fork_db.fetch_block(id);
                if (!b) {
                    auto tmp = _block_log.read_block_by_num(protocol::block_header::num_from_id(id));

                    if (tmp && tmp->id() == id) {
                        return tmp;
                    }

                    tmp.reset();
                    return tmp;
                }

                return b->data;
            } FC_CAPTURE_AND_RETHROW()
        }

        optional<signed_block> database::fetch_block_by_number(uint32_t block_num) const {
            try {
                optional<signed_block> b;

                auto results = _fork_db.fetch_block_by_number(block_num);
                if (results.size() == 1) {
                    b = results[0]->data;
                } else {
                    b = _block_log.read_block_by_num(block_num);
                }

                return b;
            } FC_LOG_AND_RETHROW()
        }

        const signed_transaction database::get_recent_transaction(const transaction_id_type &trx_id) const {
            try {
                auto &index = get_index<transaction_index>().indices().get<by_trx_id>();
                auto itr = index.find(trx_id);
                FC_ASSERT(itr != index.end());
                signed_transaction trx;
                fc::raw::unpack(itr->packed_trx, trx);
                return trx;;
            } FC_CAPTURE_AND_RETHROW()
        }

        std::vector<block_id_type> database::get_block_ids_on_fork(block_id_type head_of_fork) const {
            try {
                pair<fork_database::branch_type, fork_database::branch_type> branches = _fork_db.fetch_branch_from(head_block_id(), head_of_fork);
                if (!((branches.first.back()->previous_id() ==
                       branches.second.back()->previous_id()))) {
                    edump((head_of_fork)
                            (head_block_id())
                            (branches.first.size())
                            (branches.second.size()));
                    assert(branches.first.back()->previous_id() ==
                           branches.second.back()->previous_id());
                }
                std::vector<block_id_type> result;
                for (const item_ptr &fork_block : branches.second) {
                    result.emplace_back(fork_block->id);
                }
                result.emplace_back(branches.first.back()->previous_id());
                return result;
            } FC_CAPTURE_AND_RETHROW()
        }

        chain_id_type database::get_chain_id() const {
            return STEEMIT_CHAIN_ID;
        }

        void database::throw_if_exists_limit_order(const account_name_type& owner, uint32_t id) const {
            if (nullptr != find_limit_order(owner, id)) {
                GOLOS_THROW_OBJECT_ALREADY_EXIST("limit_order", fc::mutable_variant_object()("account",owner)("order_id",id));
            }
        }

        void database::throw_if_exists_account(const account_name_type& account) const {
            if (nullptr != find_account(account)) {
                GOLOS_THROW_OBJECT_ALREADY_EXIST("account", account);
            }
        }

        const witness_object& database::get_witness(const account_name_type& name) const {
            try {
                return get<witness_object, by_name>(name);
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_MISSING_OBJECT("witness", name);
            } FC_CAPTURE_AND_RETHROW((name))
        }

        const witness_object* database::find_witness(const account_name_type& name) const {
            return find<witness_object, by_name>(name);
        }

        const account_object& database::get_account(const account_name_type& name) const {
            try {
                return get<account_object, by_name>(name);
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_MISSING_OBJECT("account", name);
            }
            FC_CAPTURE_AND_RETHROW((name))
        }

        const account_object* database::find_account(const account_name_type& name) const {
            return find<account_object, by_name>(name);
        }

        share_type database::get_account_reputation(const account_name_type& name) const {
            const auto* acc = find_account(name);
            if (!acc) return 0;
            return acc->reputation;
        }

        int database::is_blocking(const account_name_type& account, const account_name_type& blocking) const {
            if (!has_hardfork(STEEMIT_HARDFORK_0_27__185))
                return 0;
            auto& idx = get_index<account_blocking_index, by_account>();
            auto itr = idx.find(std::make_tuple(account, blocking));
            if (itr != idx.end()) {
                return 1;
            } else {
                auto& acc = get_account(account);
                if (acc.do_not_bother) {
                    auto rep = get_account_reputation(blocking);
                    if (rep < 27800000000000) { // low edge of 65 reputation
                        return 2;
                    }
                }
            }
            return 0;
        }

        void database::check_no_blocking(const account_name_type& account, const account_name_type& blocking, bool can_bypass) {
            if (!has_hardfork(STEEMIT_HARDFORK_0_27__185))
                return;
            int blocked = is_blocking(account, blocking);
            if (blocked) {
                if (blocked == 1) {
                    GOLOS_CHECK_VALUE(can_bypass,
                        "You are blocked by user (@${account}), and cannot bypass it with money in that case",
                        ("account", account));
                } else {
                    GOLOS_CHECK_VALUE(can_bypass,
                        "User (@${account}) tells to do not bother their, and you cannot bypass it with money in that case",
                        ("account", account));
                }

                const auto& median_props = get_witness_schedule_object().median_props;
                const auto& payer  = get_account(blocking);

                if (blocked == 1) {
                    GOLOS_CHECK_VALUE(payer.tip_balance >= median_props.unwanted_operation_cost,
                        "You are blocked by user (@${account}), so you need at least ${amount} of TIP balance",
                        ("account", account)
                        ("amount", median_props.unwanted_operation_cost));
                } else {
                    GOLOS_CHECK_VALUE(payer.tip_balance >= median_props.unwanted_operation_cost,
                        "User (@${account}) tells to do not bother their, so you need at least ${amount} of TIP balance",
                        ("account", account)
                        ("amount", median_props.unwanted_operation_cost));
                }
                modify(payer, [&](auto& payer) {
                    payer.tip_balance -= median_props.unwanted_operation_cost;
                });
                modify(get_account(account), [&](auto& account) {
                    account.tip_balance += median_props.unwanted_operation_cost;
                });
                push_event(unwanted_cost_operation(account, blocking, median_props.unwanted_operation_cost, "", false));
            }
        }

        const hashlink_type database::make_hashlink_shstr(const shared_string& permlink) const {
            return fc::hash64(permlink.c_str(), permlink.size());
        }

        const hashlink_type database::make_hashlink(const std::string& permlink) const {
            return fc::hash64(permlink.c_str(), permlink.size());
        }

        const comment_object& database::get_comment(const account_name_type &author, hashlink_type hashlink) const {
            try {
                return get<comment_object, by_hashlink>(boost::make_tuple(author, hashlink));
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_MISSING_OBJECT("comment", fc::mutable_variant_object()("account", author)("hashlink", hashlink));
            } FC_CAPTURE_AND_RETHROW((author)(hashlink))
        }

        const comment_object* database::find_comment(const account_name_type &author, hashlink_type hashlink) const {
            return find<comment_object, by_hashlink>(boost::make_tuple(author, hashlink));
        }

        const comment_object &database::get_comment_by_perm(const account_name_type &author, const shared_string &permlink) const {
            auto hashlink = make_hashlink_shstr(permlink);
            return get_comment(author, hashlink);
        }

        const comment_object *database::find_comment_by_perm(const account_name_type &author, const shared_string &permlink) const {
            auto hashlink = make_hashlink_shstr(permlink);
            return find_comment(author, hashlink);
        }

        const comment_object &database::get_comment_by_perm(const account_name_type &author, const string &permlink) const {
            auto hashlink = make_hashlink(permlink);
            return get_comment(author, hashlink);
        }

        const comment_object *database::find_comment_by_perm(const account_name_type &author, const string &permlink) const {
            auto hashlink = make_hashlink(permlink);
            return find_comment(author, hashlink);
        }

        const comment_object &database::get_comment(const comment_id_type &comment_id) const {
            try {
                return get<comment_object, by_id>(comment_id);
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_MISSING_OBJECT("comment", comment_id);
            } FC_CAPTURE_AND_RETHROW((comment_id))
        }

        const comment_extras_object& database::get_extras(const account_name_type& author, hashlink_type hashlink) const {
            try {
                return get<comment_extras_object, by_hashlink>(boost::make_tuple(author, hashlink));
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_MISSING_OBJECT("comment_extras", fc::mutable_variant_object()("author", author)("hashlink", hashlink));
            } FC_CAPTURE_AND_RETHROW((author)(hashlink))
        }

        const comment_extras_object* database::find_extras(const account_name_type& author, hashlink_type hashlink) const {
            return find<comment_extras_object, by_hashlink>(boost::make_tuple(author, hashlink));
        }

        const escrow_object &database::get_escrow(const account_name_type &name, uint32_t escrow_id) const {
            try {
                return get<escrow_object, by_from_id>(boost::make_tuple(name, escrow_id));
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_MISSING_OBJECT("escrow", fc::mutable_variant_object()("account", name)("escrow", escrow_id));
            } FC_CAPTURE_AND_RETHROW((name)(escrow_id))
        }

        const escrow_object *database::find_escrow(const account_name_type &name, uint32_t escrow_id) const {
            return find<escrow_object, by_from_id>(boost::make_tuple(name, escrow_id));
        }

        const limit_order_object &database::get_limit_order(const account_name_type &name, uint32_t orderid) const {
            try {
                if (!has_hardfork(STEEMIT_HARDFORK_0_6__127)) {
                    orderid = orderid & 0x0000FFFF;
                }

                return get<limit_order_object, by_account>(boost::make_tuple(name, orderid));
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_MISSING_OBJECT("limit_order", fc::mutable_variant_object()("account",name)("order_id", orderid));
            } FC_CAPTURE_AND_RETHROW((name)(orderid))
        }

        const limit_order_object *database::find_limit_order(const account_name_type &name, uint32_t orderid) const {
            if (!has_hardfork(STEEMIT_HARDFORK_0_6__127)) {
                orderid = orderid & 0x0000FFFF;
            }

            return find<limit_order_object, by_account>(boost::make_tuple(name, orderid));
        }

        const convert_request_object& database::get_convert_request(const account_name_type& name, uint32_t id) const {
            try {
                return get<convert_request_object, by_owner>(boost::make_tuple(name, id));
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_MISSING_OBJECT("convert_request", fc::mutable_variant_object()("account",name)("request_id", id));
            } FC_CAPTURE_AND_RETHROW((name)(id))
        }

        const convert_request_object* database::find_convert_request(const account_name_type& name, uint32_t id) const {
            return find<convert_request_object, by_owner>(boost::make_tuple(name, id));
        }

        void database::throw_if_exists_convert_request(const account_name_type &name, uint32_t id) const {
            if (nullptr != find_convert_request(name, id)) {
                GOLOS_THROW_OBJECT_ALREADY_EXIST("convert_request", fc::mutable_variant_object()("account",name)("request_id",id));
            }
        }

        const savings_withdraw_object& database::get_savings_withdraw(const account_name_type& owner, uint32_t request_id) const {
            try {
                return get<savings_withdraw_object, by_from_rid>(boost::make_tuple(owner, request_id));
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_MISSING_OBJECT("savings_withdraw", fc::mutable_variant_object()("account",owner)("request_id", request_id));
            } FC_CAPTURE_AND_RETHROW((owner)(request_id))
        }

        const account_authority_object &database::get_authority(const account_name_type &name) const {
            try {
                return get<account_authority_object, by_account>(name);
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_MISSING_OBJECT("authority", name);
            } FC_CAPTURE_AND_RETHROW((name))
        }

        const savings_withdraw_object* database::find_savings_withdraw(const account_name_type& owner, uint32_t request_id) const {
            return find<savings_withdraw_object, by_from_rid>(boost::make_tuple(owner, request_id));
        }

        const invite_object* database::find_invite(const public_key_type& invite_key) const {
            return find<invite_object, by_invite_key>(invite_key);
        }

        void database::throw_if_exists_invite(const public_key_type& invite_key) const {
            if (nullptr != find_invite(invite_key)) {
                GOLOS_THROW_OBJECT_ALREADY_EXIST("invite", invite_key);
            }
        }

        const asset_object* database::find_asset(const asset_symbol_type& symbol) const {
            return find<asset_object, by_symbol>(symbol);
        }

        void database::throw_if_exists_asset(const asset_symbol_type& symbol) const {
            GOLOS_CHECK_VALUE(symbol != STEEM_SYMBOL
                && symbol != SBD_SYMBOL
                && symbol != VESTS_SYMBOL
                && symbol != STMD_SYMBOL,
                "Symbol already used for internal asset.");
            if (nullptr != find_asset(symbol)) {
                GOLOS_THROW_OBJECT_ALREADY_EXIST("asset", symbol);
            }
        }

        const asset_object* database::find_asset(const std::string& symbol_name) const {
            return find<asset_object, by_symbol_name>(symbol_name);
        }

        void database::throw_if_exists_asset(const std::string& symbol_name) const {
            GOLOS_CHECK_VALUE(symbol_name != "GOLOS"
                && symbol_name != "GBG"
                && symbol_name != "GESTS"
                && symbol_name != "STMD",
                "Symbol already used for internal asset.");
            if (nullptr != find_asset(symbol_name)) {
                GOLOS_THROW_OBJECT_ALREADY_EXIST("asset", symbol_name);
            }
        }

        asset_symbol_type database::symbol_from_str(std::string str, bool allow_empty) const {
            boost::to_upper(str);
            asset_symbol_type sym = 0;
            if (str == "GOLOS") {
                sym = STEEM_SYMBOL;
            } else if (str == "GBG") {
                sym = SBD_SYMBOL;
            } else if (!allow_empty || str != "") {
                sym = get_asset(str).symbol();
            }
            return sym;
        }

        const donate_object& database::get_donate(const donate_object_id_type& donate_id) const {
            try {
                return get<donate_object, by_id>(donate_id);
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_MISSING_OBJECT("donate", donate_id);
            } FC_CAPTURE_AND_RETHROW((donate_id))
        }

        const invite_object& database::get_invite(const public_key_type& invite_key) const {
            try {
                return get<invite_object, by_invite_key>(invite_key);
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_MISSING_OBJECT("invite", invite_key);
            } FC_CAPTURE_AND_RETHROW((invite_key))
        }

        const asset_object& database::get_asset(const asset_symbol_type& symbol) const {
            try {
                return get<asset_object, by_symbol>(symbol);
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_MISSING_OBJECT("asset", symbol);
            } FC_CAPTURE_AND_RETHROW((symbol))
        }

        const asset_object& database::get_asset(const std::string& symbol_name, account_name_type creator) const {
            if (creator != account_name_type()) {
                try {
                    return get<asset_object, by_creator_symbol_name>(std::make_tuple(creator, symbol_name));
                } catch(const std::out_of_range& e) {
                    GOLOS_THROW_MISSING_OBJECT("asset", fc::mutable_variant_object()("symbol_name", symbol_name)("creator", creator));
                } FC_CAPTURE_AND_RETHROW((symbol_name)(creator))
            } else {
                try {
                    return get<asset_object, by_symbol_name>(symbol_name);
                } catch(const std::out_of_range& e) {
                    GOLOS_THROW_MISSING_OBJECT("asset", symbol_name);
                } FC_CAPTURE_AND_RETHROW((symbol_name))
            }
        }

        account_balance_object database::get_or_default_account_balance(const account_name_type& account, const asset_symbol_type& symbol) const {
            const auto& idx = get_index<account_balance_index, by_symbol_account>();
            auto itr = idx.find(std::make_tuple(symbol, account));
            if (itr != idx.end()) {
                return *itr;
            } else {
                const auto& sym_idx = get_index<asset_index, by_symbol>();
                auto sym_itr = sym_idx.find(symbol);
                GOLOS_CHECK_VALUE(sym_itr != sym_idx.end(), "invalid symbol");

                account_balance_object def;
                def.account = account;
                def.balance = asset(0, symbol);
                def.tip_balance = asset(0, symbol);
                def.market_balance = asset(0, symbol);
                return def;
            }
        }

        void database::adjust_account_balance(const account_name_type& account, const asset& delta, const asset& delta_tip, asset delta_market) {
            const auto& idx = get_index<account_balance_index, by_symbol_account>();
            auto itr = idx.find(std::make_tuple(delta.symbol, account));
            if (itr != idx.end()) {
                modify(*itr, [&](auto& o) {
                    o.balance += delta;
                    o.tip_balance += delta_tip;
                    if (delta_market.amount != 0) {
                        o.market_balance += delta_market;
                    }
                });
            } else {
                const auto& sym_idx = get_index<asset_index, by_symbol>();
                auto sym_itr = sym_idx.find(delta.symbol);
                GOLOS_CHECK_VALUE(sym_itr != sym_idx.end(), "invalid symbol");

                create<account_balance_object>([&](auto& o) {
                    o.account = account;
                    o.balance = delta;
                    o.tip_balance = delta_tip;
                    if (delta_market.amount != 0) {
                        o.market_balance = delta_market;
                    } else {
                        o.market_balance = asset(0, delta.symbol);
                    }
                });
            }
        }

        void database::update_pair_depth(asset base, asset quote) {
            const auto* p = find<market_pair_object, by_base_quote>(std::make_tuple(base.symbol, quote.symbol));
            if (p) {
                auto new_base = p->base_depth + base;
                auto new_quote = p->quote_depth + quote;
                if (!new_base.amount.value && !new_quote.amount.value) {
                    remove(*p);
                } else {
                    modify(*p, [&](auto& p) {
                        p.base_depth = new_base;
                        p.quote_depth = new_quote;
                    });
                }
            } else {
                create<market_pair_object>([&](auto& p) {
                    p.base_depth = base;
                    p.quote_depth = quote;
                });
            }
        }

        void database::update_asset_marketed(asset_symbol_type symbol) {
            FC_ASSERT(symbol != VESTS_SYMBOL && symbol != STMD_SYMBOL);
            if (symbol == STEEM_SYMBOL || symbol == SBD_SYMBOL)
                return;
            const auto& obj = get_asset(symbol);
            modify(obj, [&](auto& o) {
                o.marketed = head_block_time();
            });
        }

        const dynamic_global_property_object& database::get_dynamic_global_properties() const {
            try {
                return get<dynamic_global_property_object>();
            } catch(const std::out_of_range& e) {
                GOLOS_THROW_INTERNAL_ERROR("Missing dynamic_global_properties");
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::throw_if_exists_savings_withdraw(const account_name_type& owner, uint32_t request_id) const {
            if (nullptr != find_savings_withdraw(owner, request_id)) {
                GOLOS_THROW_OBJECT_ALREADY_EXIST("savings_withdraw",
                    fc::mutable_variant_object()("owner",owner)("request_id",request_id));
            }
        }

        const feed_history_object &database::get_feed_history() const {
            try {
                return get<feed_history_object>();
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_INTERNAL_ERROR("Missing feed_history");
            } FC_CAPTURE_AND_RETHROW()
        }

        const witness_schedule_object &database::get_witness_schedule_object() const {
            try {
                return get<witness_schedule_object>();
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_INTERNAL_ERROR("Missing witness_schedule");
            } FC_CAPTURE_AND_RETHROW()
        }

        const hardfork_property_object &database::get_hardfork_property_object() const {
            try {
                return get<hardfork_property_object>();
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_INTERNAL_ERROR("Missing hardfork_property");
            } FC_CAPTURE_AND_RETHROW()
        }

        const block_summary_object &database::get_block_summary(const block_summary_id_type &ref_block_num) const {
            try {
                return get<block_summary_object>(ref_block_num);
            } catch(const std::out_of_range &e) {
                GOLOS_THROW_MISSING_OBJECT("block_summary", ref_block_num);
            } FC_CAPTURE_AND_RETHROW()
        }

        const time_point_sec database::calculate_discussion_payout_time(const comment_object &comment) const {
            if (has_hardfork(STEEMIT_HARDFORK_0_17__431) || comment.parent_author == STEEMIT_ROOT_POST_PARENT) {
                return comment.cashout_time;
            } else {
                return get_comment(comment.root_comment).cashout_time;
            }
        }

        void database::pay_fee(const account_object &account, asset fee) {
            FC_ASSERT(fee.amount >= 0); /// NOTE if this fails then validate() on some operation is probably wrong
            if (fee.amount == 0) {
                return;
            }

            FC_ASSERT(account.balance >= fee);
            adjust_balance(account, -fee);
            adjust_supply(-fee);
        }

        bool database::update_account_bandwidth(const dynamic_global_property_object& props, const account_object &a, uint32_t trx_size, const bandwidth_type type) {
            bool has_bandwidth = true;

            if (props.total_vesting_shares.amount > 0) {
                auto band = find<account_bandwidth_object, by_account_bandwidth_type>(boost::make_tuple(a.name, type));
                if (band == nullptr) {
                    band = &create<account_bandwidth_object>([&](account_bandwidth_object &b) {
                        b.account = a.name;
                        b.type = type;
                    });
                }

                share_type new_bandwidth;
                share_type trx_bandwidth = trx_size * STEEMIT_BANDWIDTH_PRECISION;
                auto delta_time = (head_block_time() - band->last_bandwidth_update).to_seconds();
                if (delta_time > STEEMIT_BANDWIDTH_AVERAGE_WINDOW_SECONDS) {
                    new_bandwidth = 0;
                } else {
                    new_bandwidth = (
                            ((STEEMIT_BANDWIDTH_AVERAGE_WINDOW_SECONDS -
                              delta_time) *
                             fc::uint128_t(band->average_bandwidth.value))
                            /
                            STEEMIT_BANDWIDTH_AVERAGE_WINDOW_SECONDS).to_uint64();
                }

                new_bandwidth += trx_bandwidth;

                modify(*band, [&](account_bandwidth_object &b) {
                    b.average_bandwidth = new_bandwidth;
                    b.lifetime_bandwidth += trx_bandwidth;
                    b.last_bandwidth_update = head_block_time();
                });

                fc::uint128_t account_vshares(a.effective_vesting_shares().amount.value);
                fc::uint128_t total_vshares(props.total_vesting_shares.amount.value);
                fc::uint128_t account_average_bandwidth(band->average_bandwidth.value);
                fc::uint128_t max_virtual_bandwidth(props.max_virtual_bandwidth);

                has_bandwidth = (account_vshares * max_virtual_bandwidth) > (account_average_bandwidth * total_vshares);

                if (is_producing())
                    GOLOS_CHECK_LOGIC(has_bandwidth, 
                        logic_exception::account_exceeded_bandwidth_per_vestring_share,
                        "Account exceeded maximum allowed bandwidth per vesting share.",
                        ("account_vshares", account_vshares)
                        ("account_average_bandwidth", account_average_bandwidth)
                        ("max_virtual_bandwidth", max_virtual_bandwidth)
                        ("total_vesting_shares", total_vshares));
            }

            return has_bandwidth;
        }

        void database::check_negrep_posting_bandwidth(const account_object& acc,
            const std::string& target_type,
            const std::string& id1, const std::string& id2,
            const std::string& id3, const std::string& id4) {
            auto now = head_block_time();

            bool hf27 = has_hardfork(STEEMIT_HARDFORK_0_27);

            if (!has_hardfork(STEEMIT_HARDFORK_0_26__168)
                || acc.reputation >= 0) {
                modify(acc, [&](auto& a) {
                    a.last_posting_action = now;
                });
                return;
            }

            if (hf27) {
                const auto& median_props = get_witness_schedule_object().median_props;
                auto fee = median_props.unlimit_operation_cost;
                GOLOS_CHECK_VALUE(acc.tip_balance >= fee,
                    "You are have negative reputation, so you need to pay ${amount} of TIP balance",
                    ("amount", fee));
                modify(get_account(STEEMIT_NULL_ACCOUNT), [&](auto& a) {
                    a.tip_balance += fee;
                });
                modify(acc, [&](auto& a) {
                    a.tip_balance -= fee;
                    a.last_posting_action = now;
                });
                push_event(unlimit_cost_operation(
                    acc.name, fee, "negrep", target_type,
                    id1, id2, id3, id4
                ));
                return;
            }

            const auto& mprops = get_witness_schedule_object().median_props;

            auto consumption = mprops.negrep_posting_window / mprops.negrep_posting_per_window;

            auto elapsed_minutes = (now - acc.last_posting_action).to_seconds() / 60;

            auto regenerated_capacity = std::min(uint32_t(mprops.negrep_posting_window), uint32_t(elapsed_minutes));
            auto current_capacity = std::min(uint16_t(acc.negrep_posting_capacity + regenerated_capacity), mprops.negrep_posting_window);

            GOLOS_CHECK_BANDWIDTH(current_capacity + 1, consumption,
                bandwidth_exception::negrep_posting_bandwidth,
                "Can only comment/post/vote ${negrep_posting_per_window} times in ${negrep_posting_window} minutes.",
                ("negrep_posting_per_window", mprops.negrep_posting_per_window)("negrep_posting_window", mprops.negrep_posting_window));

            modify(acc, [&](auto& a) {
                a.negrep_posting_capacity = current_capacity - consumption;
                a.last_posting_action = now;
            });
        }

        uint32_t database::witness_participation_rate() const {
            const dynamic_global_property_object &dpo = get_dynamic_global_properties();
            return uint64_t(STEEMIT_100_PERCENT) *
                   dpo.recent_slots_filled.popcount() / 128;
        }

        void database::add_checkpoints(const flat_map<uint32_t, block_id_type> &checkpts) {
            for (const auto &i : checkpts) {
                _checkpoints[i.first] = i.second;
            }
        }

        bool database::before_last_checkpoint() const {
            return (_checkpoints.size() > 0) &&
                   (_checkpoints.rbegin()->first >= head_block_num());
        }

        std::pair<bool, int64_t> database::chain_status() const {
            time_point_sec now = fc::time_point::now();
            auto head = head_block_time();

            std::pair<bool, int64_t> res;
            res.second = int64_t(now.sec_since_epoch()) - head.sec_since_epoch();
            res.first = res.second <= 90;
            return res;
        }

        uint32_t database::validate_block(const signed_block& new_block, uint32_t skip) {
            uint32_t validate_block_steps =
                skip_merkle_check |
                skip_block_size_check;

            if ((skip & validate_block_steps) != validate_block_steps) {
                with_strong_read_lock([&](){
                    _validate_block(new_block, skip);
                });

                skip |= validate_block_steps;

                // it's tempting but very dangerous to check transaction signatures here
                //   because block can contain transactions with changing of authorizity
                //   and state can too contain changes of authorizity
            }

            return skip;
        }

        void database::_validate_block(const signed_block& new_block, uint32_t skip) {
            uint32_t new_block_num = new_block.block_num();

            if (!(skip & skip_merkle_check)) {
                auto merkle_root = new_block.calculate_merkle_root();

                try {
                    FC_ASSERT(
                        new_block.transaction_merkle_root == merkle_root,
                        "Merkle check failed",
                        ("next_block.transaction_merkle_root", new_block.transaction_merkle_root)
                        ("calc", merkle_root)
                        ("next_block", new_block)
                        ("id", new_block.id()));
                } catch (fc::assert_exception &e) {
                    const auto &merkle_map = get_shared_db_merkle();
                    auto itr = merkle_map.find(new_block_num);

                    if (itr == merkle_map.end() || itr->second != merkle_root) {
                        throw e;
                    }
                }
            }

            if (!(skip & skip_block_size_check)) {
                const auto &gprops = get_dynamic_global_properties();
                auto block_size = fc::raw::pack_size(new_block);
                if (has_hardfork(STEEMIT_HARDFORK_0_12)) {
                    FC_ASSERT(
                        block_size <= gprops.maximum_block_size,
                        "Block Size is too Big",
                        ("next_block_num", new_block_num)
                        ("block_size", block_size)
                        ("max", gprops.maximum_block_size));
                }
            }
        }

       /**
        * Push block "may fail" in which case every partial change is unwound. After
        * push block is successful the block is appended to the chain database on disk.
        *
        * @return true if we switched forks as a result of this push.
        */
        bool database::push_block(const signed_block &new_block, uint32_t skip) {
            //fc::time_point begin_time = fc::time_point::now();

            bool result;
            with_strong_write_lock([&]() {
                detail::without_pending_transactions(*this, skip, std::move(_pending_tx), [&]() {
                    try {
                        result = _push_block(new_block, skip);
                        check_free_memory(false, new_block.block_num());
                    } catch (const fc::exception &e) {
                        auto msg = std::string(e.what());
                        // TODO: there is no easy way to catch boost::interprocess::bad_alloc
                        if (msg.find("boost::interprocess::bad_alloc") == msg.npos) {
                            throw e;
                        }
                        wlog("Receive bad_alloc exception. Forcing to resize shared memory file.");
                        set_reserved_memory(free_memory());
                        if (!_resize(new_block.block_num())) {
                            throw e;
                        }
                        result = _push_block(new_block, skip);
                    }
                });
            });

            //fc::time_point end_time = fc::time_point::now();
            //fc::microseconds dt = end_time - begin_time;
            //if( ( new_block.block_num() % 10000 ) == 0 )
            //   ilog( "push_block ${b} took ${t} microseconds", ("b", new_block.block_num())("t", dt.count()) );
            return result;
        }

        void database::_maybe_warn_multiple_production(uint32_t height) const {
            auto blocks = _fork_db.fetch_block_by_number(height);
            if (blocks.size() > 1) {
                vector<std::pair<account_name_type, fc::time_point_sec>> witness_time_pairs;
                for (const auto &b : blocks) {
                    witness_time_pairs.push_back(std::make_pair(b->data.witness, b->data.timestamp));
                }

                ilog(
                    "Encountered block num collision at block ${n} due to a fork, witnesses are: ${w}",
                    ("n", height)("w", witness_time_pairs));
            }
            return;
        }

        bool database::_push_block(const signed_block &new_block, uint32_t skip) {
            try {
                if (!(skip & skip_fork_db)) {
                    shared_ptr<fork_item> new_head = _fork_db.push_block(new_block);
                    _maybe_warn_multiple_production(new_head->num);
                    //If the head block from the longest chain does not build off of the current head, we need to switch forks.
                    if (new_head->data.previous != head_block_id()) {
                        //If the newly pushed block is the same height as head, we get head back in new_head
                        //Only switch forks if new_head is actually higher than head
                        if (new_head->data.block_num() > head_block_num()) {
                            // wlog( "Switching to fork: ${id}", ("id",new_head->data.id()) );
                            auto branches = _fork_db.fetch_branch_from(new_head->data.id(), head_block_id());

                            // pop blocks until we hit the forked block
                            while (head_block_id() !=
                                   branches.second.back()->data.previous) {
                                pop_block();
                            }

                            // push all blocks on the new fork
                            for (auto ritr = branches.first.rbegin();
                                 ritr != branches.first.rend(); ++ritr) {
                                // ilog( "pushing blocks from fork ${n} ${id}", ("n",(*ritr)->data.block_num())("id",(*ritr)->data.id()) );
                                optional<fc::exception> except;
                                try {
                                    auto session = start_undo_session();
                                    apply_block((*ritr)->data, skip);
                                    session.push();
                                }
                                catch (const fc::exception &e) {
                                    except = e;
                                }
                                if (except) {
                                    // wlog( "exception thrown while switching forks ${e}", ("e",except->to_detail_string() ) );
                                    // remove the rest of branches.first from the fork_db, those blocks are invalid
                                    while (ritr != branches.first.rend()) {
                                        _fork_db.remove((*ritr)->data.id());
                                        ++ritr;
                                    }
                                    _fork_db.set_head(branches.second.front());

                                    // pop all blocks from the bad fork
                                    while (head_block_id() !=
                                           branches.second.back()->data.previous) {
                                        pop_block();
                                    }

                                    // restore all blocks from the good fork
                                    for (auto ritr = branches.second.rbegin();
                                         ritr !=
                                         branches.second.rend(); ++ritr) {
                                        auto session = start_undo_session();
                                        apply_block((*ritr)->data, skip);
                                        session.push();
                                    }
                                    throw *except;
                                }
                            }
                            return true;
                        } else {
                            return false;
                        }
                    }
                }

                try {
                    auto session = start_undo_session();
                    apply_block(new_block, skip);
                    session.push();
                }
                catch (const fc::exception &e) {
                    elog("Failed to push new block:\n${e}", ("e", e.to_detail_string()));
                    _fork_db.remove(new_block.id());
                    throw;
                }

                return false;
            } FC_CAPTURE_AND_RETHROW()
        }

       /**
        * Attempts to push the transaction into the pending queue
        *
        * When called to push a locally generated transaction, set the skip_block_size_check bit on the skip argument. This
        * will allow the transaction to be pushed even if it causes the pending block size to exceed the maximum block size.
        * Although the transaction will probably not propagate further now, as the peers are likely to have their pending
        * queues full as well, it will be kept in the queue to be propagated later when a new block flushes out the pending
        * queues.
        */
        void database::push_transaction(const signed_transaction &trx, uint32_t skip) {
            try {
                GOLOS_ASSERT(fc::raw::pack_size(trx) <= (get_dynamic_global_properties().maximum_block_size - 256),
                        golos::protocol::tx_too_long, "Transaction data is too long. Maximum transaction size ${max} bytes",
                        ("max",get_dynamic_global_properties().maximum_block_size - 256));
                with_weak_write_lock([&]() {
                    detail::with_producing(*this, [&]() {
                        _push_transaction(trx, skip);
                    });
                });
            }
            FC_CAPTURE_AND_RETHROW((trx))
        }

        void database::_push_transaction(const signed_transaction &trx, uint32_t skip) {
            // If this is the first transaction pushed after applying a block, start a new undo session.
            // This allows us to quickly rewind to the clean state of the head block, in case a new block arrives.
            if (!_pending_tx_session.valid()) {
                _pending_tx_session = start_undo_session();
            }

            // Create a temporary undo session as a child of _pending_tx_session.
            // The temporary session will be discarded by the destructor if
            // _apply_transaction fails. If we make it to merge(), we
            // apply the changes.

            auto temp_session = start_undo_session();
            _apply_transaction(trx, skip);
            _pending_tx.push_back(trx);

            notify_changed_objects();
            // The transaction applied successfully. Merge its changes into the pending block session.
            temp_session.squash();

            // notify anyone listening to pending transactions
            notify_on_pending_transaction(trx);
        }

        signed_block database::generate_block(
                fc::time_point_sec when,
                const account_name_type &witness_owner,
                const fc::ecc::private_key &block_signing_private_key,
                uint32_t skip /* = 0 */
        ) {
            signed_block result;
            try {
                result = _generate_block(when, witness_owner, block_signing_private_key, skip);
            }
            FC_CAPTURE_AND_RETHROW((witness_owner))
            return result;
        }


        signed_block database::_generate_block(
                fc::time_point_sec when,
                const account_name_type &witness_owner,
                const fc::ecc::private_key &block_signing_private_key,
                uint32_t skip
        ) {
            uint32_t slot_num = get_slot_at_time(when);
            FC_ASSERT(slot_num > 0);
            string scheduled_witness = get_scheduled_witness(slot_num);
            FC_ASSERT(scheduled_witness == witness_owner);

            const auto &witness_obj = get_witness(witness_owner);

            if (!(skip & skip_witness_signature))
                FC_ASSERT(witness_obj.signing_key ==
                          block_signing_private_key.get_public_key());

            static const size_t max_block_header_size =
                    fc::raw::pack_size(signed_block_header()) + 4;
            auto maximum_block_size = get_dynamic_global_properties().maximum_block_size; //STEEMIT_MAX_BLOCK_SIZE;
            size_t total_block_size = max_block_header_size;

            signed_block pending_block;

            with_strong_write_lock([&]() { detail::with_generating(*this, [&]() {
                //
                // The following code throws away existing pending_tx_session and
                // rebuilds it by re-applying pending transactions.
                //
                // This rebuild is necessary because pending transactions' validity
                // and semantics may have changed since they were received, because
                // time-based semantics are evaluated based on the current block
                // time. These changes can only be reflected in the database when
                // the value of the "when" variable is known, which means we need to
                // re-apply pending transactions in this method.
                //
                _pending_tx_session.reset();
                _pending_tx_session = start_undo_session();

                uint64_t postponed_tx_count = 0;
                // pop pending state (reset to head block state)
                for (const signed_transaction &tx : _pending_tx) {
                    // Only include transactions that have not expired yet for currently generating block,
                    // this should clear problem transactions and allow block production to continue

                    if (tx.expiration < when) {
                        continue;
                    }

                    uint64_t new_total_size =
                            total_block_size + fc::raw::pack_size(tx);

                    // postpone transaction if it would make block too big
                    if (new_total_size >= maximum_block_size) {
                        postponed_tx_count++;
                        continue;
                    }

                    try {
                        auto temp_session = start_undo_session();
                        _apply_transaction(tx, skip);
                        temp_session.squash();

                        total_block_size += fc::raw::pack_size(tx);
                        pending_block.transactions.push_back(tx);
                    }
                    catch (const fc::exception &e) {
                        // Do nothing, transaction will not be re-applied
                        //wlog( "Transaction was not processed while generating block due to ${e}", ("e", e) );
                        //wlog( "The transaction was ${t}", ("t", tx) );
                    }
                }
                if (postponed_tx_count > 0) {
                    wlog("Postponed ${n} transactions due to block size limit", ("n", postponed_tx_count));
                }

                _pending_tx_session.reset();
            }); });

            // We have temporarily broken the invariant that
            // _pending_tx_session is the result of applying _pending_tx, as
            // _pending_tx now consists of the set of postponed transactions.
            // However, the push_block() call below will re-create the
            // _pending_tx_session.

            pending_block.previous = head_block_id();
            pending_block.timestamp = when;
            pending_block.transaction_merkle_root = pending_block.calculate_merkle_root();
            pending_block.witness = witness_owner;
            if (has_hardfork(STEEMIT_HARDFORK_0_5__54)) {
                const auto &witness = get_witness(witness_owner);

                if (witness.running_version != STEEMIT_BLOCKCHAIN_VERSION) {
                    pending_block.extensions.insert(block_header_extensions(STEEMIT_BLOCKCHAIN_VERSION));
                }

                const auto &hfp = get_hardfork_property_object();

                if (hfp.current_hardfork_version <
                    STEEMIT_BLOCKCHAIN_HARDFORK_VERSION // Binary is newer hardfork than has been applied
                    && (witness.hardfork_version_vote !=
                        _hardfork_versions[hfp.last_hardfork + 1] ||
                        witness.hardfork_time_vote !=
                        _hardfork_times[hfp.last_hardfork +
                                        1])) // Witness vote does not match binary configuration
                {
                    // Make vote match binary configuration
                    pending_block.extensions.insert(block_header_extensions(hardfork_version_vote(_hardfork_versions[
                            hfp.last_hardfork + 1], _hardfork_times[
                            hfp.last_hardfork + 1])));
                } else if (hfp.current_hardfork_version ==
                           STEEMIT_BLOCKCHAIN_HARDFORK_VERSION // Binary does not know of a new hardfork
                           && witness.hardfork_version_vote >
                              STEEMIT_BLOCKCHAIN_HARDFORK_VERSION) // Voting for hardfork in the future, that we do not know of...
                {
                    // Make vote match binary configuration. This is vote to not apply the new hardfork.
                    pending_block.extensions.insert(block_header_extensions(hardfork_version_vote(_hardfork_versions[hfp.last_hardfork], _hardfork_times[hfp.last_hardfork])));
                }
            }

            if (!(skip & skip_witness_signature)) {
                pending_block.sign(block_signing_private_key);
            }

            // TODO: Move this to _push_block() so session is restored.
            if (!(skip & skip_block_size_check)) {
                FC_ASSERT(fc::raw::pack_size(pending_block) <= STEEMIT_MAX_BLOCK_SIZE);
            }

            push_block(pending_block, skip);

            return pending_block;
        }

/**
 * Removes the most recent block from the database and
 * undoes any changes it made.
 */
        void database::pop_block() {
            try {
                _pending_tx_session.reset();
                auto head_id = head_block_id();

                /// save the head block so we can recover its transactions
                optional<signed_block> head_block = fetch_block_by_id(head_id);
                GOLOS_ASSERT(head_block.valid(), pop_empty_chain, "there are no blocks to pop");

                _fork_db.pop_block();
                undo();

                _popped_tx.insert(_popped_tx.begin(), head_block->transactions.begin(), head_block->transactions.end());

            }
            FC_CAPTURE_AND_RETHROW()
        }

        void database::clear_pending() {
            try {
                assert((_pending_tx.size() == 0) ||
                       _pending_tx_session.valid());
                _pending_tx.clear();
                _pending_tx_session.reset();
            }
            FC_CAPTURE_AND_RETHROW()
        }

        void database::enable_plugins_on_push_transaction(bool value) {
            _enable_plugins_on_push_transaction = value;
        }

        void database::notify_pre_apply_operation(operation_notification &note) {
            note.trx_id = _current_trx_id;
            note.block = _current_block_num;
            note.trx_in_block = _current_trx_in_block;
            note.op_in_trx = _current_op_in_trx;

            if (!is_producing() || _enable_plugins_on_push_transaction) {
                STEEMIT_TRY_NOTIFY(pre_apply_operation, note);
            }
        }

        void database::notify_post_apply_operation(const operation_notification &note) {
            if (!is_producing() || _enable_plugins_on_push_transaction) {
                STEEMIT_TRY_NOTIFY(post_apply_operation, note);
            }
        }

        const void database::push_virtual_operation(const operation &op, bool force) {
            if (!force && _skip_virtual_ops ) {
                return;
            }

            FC_ASSERT(is_virtual_operation(op));
            operation_notification note(op);
            ++_current_virtual_op;
            note.virtual_op = _current_virtual_op;
            notify_pre_apply_operation(note);
            notify_post_apply_operation(note);
        }

        void database::notify_applied_block(const signed_block &block) {
            STEEMIT_TRY_NOTIFY(applied_block, block)
        }

        void database::notify_on_pending_transaction(const signed_transaction &tx) {
            STEEMIT_TRY_NOTIFY(on_pending_transaction, tx)
        }

        void database::notify_on_applied_transaction(const signed_transaction &tx) {
            STEEMIT_TRY_NOTIFY(on_applied_transaction, tx)
        }

        bool database::can_push_events() {
            return _store_evaluator_events && !is_generating() && !is_producing();
        }

        void database::push_event(const operation& op) {
            if (!can_push_events())
                return;

            create<event_object>([&](auto& o) {
                const auto size = fc::raw::pack_size(op);
                o.serialized_op.resize(size);
                fc::datastream<char*> ds(o.serialized_op.data(), size);
                fc::raw::pack(ds, op);
            });
        }

        void database::process_events() { try {
            if (!_store_evaluator_events)
                return;

            int processed = 0;
            const auto& idx = get_index<event_index, by_id>();
            auto itr = idx.begin();
            for (; processed < 500 && itr != idx.end(); ++processed) {
                operation op = fc::raw::unpack<operation>(itr->serialized_op);
                push_virtual_operation(op);

                const auto& current = *itr;
                ++itr;
                remove(current);
            }
            // if (itr != idx.end()) {
            //     wlog("process_events scheduled some events to next block");
            // }
        } FC_CAPTURE_AND_RETHROW() }

        account_name_type database::get_scheduled_witness(uint32_t slot_num) const {
            const dynamic_global_property_object &dpo = get_dynamic_global_properties();
            const witness_schedule_object &wso = get_witness_schedule_object();
            uint64_t current_aslot = dpo.current_aslot + slot_num;
            return wso.current_shuffled_witnesses[current_aslot %
                                                  wso.num_scheduled_witnesses];
        }

        fc::time_point_sec database::get_slot_time(uint32_t slot_num) const {
            if (slot_num == 0) {
                return fc::time_point_sec();
            }

            auto interval = STEEMIT_BLOCK_INTERVAL;
            const dynamic_global_property_object &dpo = get_dynamic_global_properties();

            if (head_block_num() == 0) {
                // n.b. first block is at genesis_time plus one block interval
                fc::time_point_sec genesis_time = dpo.time;
                return genesis_time + slot_num * interval;
            }

            int64_t head_block_abs_slot = head_block_time().sec_since_epoch() / interval;
            fc::time_point_sec head_slot_time(head_block_abs_slot * interval);

            // "slot 0" is head_slot_time
            // "slot 1" is head_slot_time,
            //   plus maint interval if head block is a maint block
            //   plus block interval if head block is not a maint block
            return head_slot_time + (slot_num * interval);
        }

        uint32_t database::get_slot_at_time(fc::time_point_sec when) const {
            fc::time_point_sec first_slot_time = get_slot_time(1);
            if (when < first_slot_time) {
                return 0;
            }
            return (when - first_slot_time).to_seconds() /
                   STEEMIT_BLOCK_INTERVAL + 1;
        }

/**
 *  Converts STEEM into sbd and adds it to to_account while reducing the STEEM supply
 *  by STEEM and increasing the sbd supply by the specified amount.
 */
        std::pair<asset, asset> database::create_sbd(const account_object &to_account, asset steem) {
            std::pair<asset, asset> assets(asset(0, SBD_SYMBOL), asset(0, STEEM_SYMBOL));

            try {
                if (steem.amount == 0) {
                    return assets;
                }

                const auto &median_price = get_feed_history().current_median_history;
                const auto &gpo = get_dynamic_global_properties();

                if (!median_price.is_null()) {
                    auto to_sbd = (gpo.sbd_print_rate * steem.amount) /
                                  STEEMIT_100_PERCENT;
                    auto to_steem = steem.amount - to_sbd;

                    auto sbd = asset(to_sbd, STEEM_SYMBOL) * median_price;

                    adjust_balance(to_account, sbd);
                    adjust_balance(to_account, asset(to_steem, STEEM_SYMBOL));
                    adjust_supply(asset(-to_sbd, STEEM_SYMBOL));
                    adjust_supply(sbd);
                    assets.first = sbd;
                    assets.second = to_steem;
                } else {
                    adjust_balance(to_account, steem);
                    assets.second = steem;
                }
            }
            FC_CAPTURE_LOG_AND_RETHROW((to_account.name)(steem))

            return assets;
        }

/**
 * @param to_account - the account to receive the new vesting shares
 * @param STEEM - STEEM to be converted to vesting shares
 */
        asset database::create_vesting(const account_object &to_account, asset steem) {
            try {
                const auto &cprops = get_dynamic_global_properties();

                /**
                 *  The ratio of total_vesting_shares / total_vesting_fund_steem should not
                 *  change as the result of the user adding funds
                 *
                 *  V / C = (V+Vn) / (C+Cn)
                 *
                 *  Simplifies to Vn = (V * Cn ) / C
                 *
                 *  If Cn equals o.amount, then we must solve for Vn to know how many new vesting shares
                 *  the user should receive.
                 *
                 *  128 bit math is requred due to multiplying of 64 bit numbers. This is done in asset and price.
                 */
                asset new_vesting = steem * cprops.get_vesting_share_price();

                modify(to_account, [&](account_object &to) {
                    to.vesting_shares += new_vesting;
                });

                modify(cprops, [&](dynamic_global_property_object &props) {
                    props.total_vesting_fund_steem += steem;
                    props.total_vesting_shares += new_vesting;
                });

                adjust_proxied_witness_votes(to_account, new_vesting.amount);

                return new_vesting;
            }
            FC_CAPTURE_AND_RETHROW((to_account.name)(steem))
        }

        fc::sha256 database::get_pow_target() const {
            const auto &dgp = get_dynamic_global_properties();
            fc::sha256 target;
            target._hash[0] = -1;
            target._hash[1] = -1;
            target._hash[2] = -1;
            target._hash[3] = -1;
            target = target >> ((dgp.num_pow_witnesses / 4) + 4);
            return target;
        }

        uint32_t database::get_pow_summary_target() const {
            const dynamic_global_property_object &dgp = get_dynamic_global_properties();
            if (dgp.num_pow_witnesses >= 1004) {
                return 0;
            }

            if (has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                return (0xFE00 - 0x0040 * dgp.num_pow_witnesses) << 0x10;
            } else {
                return (0xFC00 - 0x0040 * dgp.num_pow_witnesses) << 0x10;
            }
        }

        bool database::is_transit_enabled() const {
            return
                has_hardfork(STEEMIT_HARDFORK_0_21__1348) &&
                get_dynamic_global_properties().transit_block_num != std::numeric_limits<uint32_t>::max();
        }

        void database::process_transit_to_cyberway(const signed_block& b, uint32_t skip) {
            if (!has_hardfork(STEEMIT_HARDFORK_0_21__1348)) {
                return;
            }

            const auto& gpo = get_dynamic_global_properties();

            if (!is_transit_enabled()) {
                vector<account_name_type> transit_witnesses;
                transit_witnesses.reserve(STEEMIT_MAX_WITNESSES);

                uint32_t i;
                const auto& wso = get_witness_schedule_object();
                for (i = 0; i < wso.num_scheduled_witnesses; ++i) {
                    auto& witness = get_witness(wso.current_shuffled_witnesses[i]);
                    if (witness.schedule != witness_object::top19) {
                        //skip
                    } else if (witness.transit_to_cyberway_vote != STEEMIT_GENESIS_TIME) {
                        transit_witnesses.push_back(witness.owner);
                    }
                }

                if (transit_witnesses.size() >= STEEMIT_TRANSIT_REQUIRED_WITNESSES) {
                    modify(gpo, [&](auto& o) {
                        i = 0;
                        o.transit_block_num = b.block_num();
                        for (auto owner: transit_witnesses) {
                            o.transit_witnesses[i++] = owner;
                        }
                    });
                    liberate_golos_classic();
                } else {
                    return;
                }
            }
        }

        void database::check_witness_idleness(bool periodically) { try {
            if (periodically) {
                if (head_block_num() % GOLOS_WITNESS_IDLENESS_CHECK_INTERVAL != 0) return;

                if (!has_hardfork(STEEMIT_HARDFORK_0_22__77)) return;
            }

            uint32_t top_count = periodically ? GOLOS_WITNESS_IDLENESS_TOP_COUNT : UINT32_MAX;

            const auto& median_props = get_witness_schedule_object().median_props;
            const auto old_block_num = head_block_num() - (median_props.witness_idleness_time / STEEMIT_BLOCK_INTERVAL);

            std::vector<witness_id_type> witnesses_to_clear;
            uint32_t i = 0;
            const auto& widx = get_index<witness_index, by_vote_name>();
            for (auto itr = widx.begin(); itr != widx.end() && i < top_count; ++itr, ++i) {
                if (itr->last_confirmed_block_num == 0 || itr->last_confirmed_block_num > old_block_num) continue;
                witnesses_to_clear.push_back(itr->id);
            }

            const auto& vidx = get_index<witness_vote_index, by_witness_account>();
            for (auto& witn : witnesses_to_clear) {
                auto vitr = vidx.lower_bound(std::make_tuple(witn, account_id_type()));
                while (vitr != vidx.end() && vitr->witness == witn) {
                    const auto& voter = get(vitr->account);
                    const auto witness_vote_weight = voter.witness_vote_weight();

                    auto old_delta = witness_vote_weight / voter.witnesses_voted_for;
                    auto new_delta = witness_vote_weight / std::max(uint16_t(voter.witnesses_voted_for-1), uint16_t(1));

                    adjust_witness_votes(voter, -old_delta + new_delta);
                    adjust_witness_vote(voter, get(witn), -new_delta);

                    modify(voter, [&](auto& a) {
                        a.witnesses_voted_for--;
                    });

                    const auto& current = *vitr;
                    ++vitr;
                    remove(current);
                }
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::check_account_idleness() { try {
            if (head_block_num() % GOLOS_ACCOUNT_IDLENESS_CHECK_INTERVAL != 0) return;

            if (!has_hardfork(STEEMIT_HARDFORK_0_22__78)) return;

            const auto now = head_block_time();
            const auto& median_props = get_witness_schedule_object().median_props;
            const auto old_time = now - median_props.account_idleness_time;

            const auto& vdo_idx = get_index<vesting_delegation_index, by_delegation>();

            auto has_hf23 = has_hardfork(STEEMIT_HARDFORK_0_23__104);
            auto has_hf25 = has_hardfork(STEEMIT_HARDFORK_0_25__122);
            auto has_hf26 = has_hardfork(STEEMIT_HARDFORK_0_26__157);

            auto min_vs = 30000000000;

            auto process_acc = [&](auto& acc) { try {
                if (!acc.vesting_shares.amount.value || acc.to_withdraw.value) return;

                auto acc_vs = acc.effective_vesting_shares().amount.value;
                if (has_hf23 && acc_vs < 30000000) return;

                auto vdo_itr = vdo_idx.lower_bound(acc.name);
                while (vdo_itr != vdo_idx.end() && vdo_itr->delegator == acc.name) {
                    modify(acc, [&](auto& a) {
                        a.delegate_vs(-vdo_itr->vesting_shares, vdo_itr->is_emission);
                    });
                    modify(get_account(vdo_itr->delegatee), [&](auto& a) {
                        a.receive_vs(-vdo_itr->vesting_shares, vdo_itr->is_emission);
                    });
                    push_virtual_operation(return_vesting_delegation_operation(vdo_itr->delegator, vdo_itr->vesting_shares));

                    const auto& current = *vdo_itr;
                    ++vdo_itr;
                    remove(current);
                }

                auto to_withdraw = acc.vesting_shares.amount.value;
                if (has_hf25) {
                    to_withdraw = acc_vs - min_vs;
                    if (to_withdraw <= 0) return;
                    to_withdraw = std::min(to_withdraw, acc.vesting_shares.amount.value);
                }
                modify(acc, [&](auto& a) {
                    if (has_hf26) {
                        a.vesting_withdraw_rate = asset(to_withdraw / STEEMIT_VESTING_WITHDRAW_INTERVALS, VESTS_SYMBOL);
                    } else if (has_hf23) {
                        a.vesting_withdraw_rate = asset(to_withdraw / STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_26, VESTS_SYMBOL);
                    } else{
                        a.vesting_withdraw_rate = asset(to_withdraw / STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_23, VESTS_SYMBOL);
                    }
                    if (a.vesting_withdraw_rate.amount == 0)
                        a.vesting_withdraw_rate.amount = 1;
                    a.next_vesting_withdrawal = now + fc::seconds(STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS);
                    a.to_withdraw = to_withdraw;
                    a.withdrawn = 0;
                });
            } FC_CAPTURE_AND_RETHROW((acc.name)) };

            if (has_hardfork(STEEMIT_HARDFORK_0_27)) {
                const auto& idx = get_index<account_index, by_proved>();
                auto itr = idx.begin();
                for (; itr != idx.end() && !itr->frozen; ++itr) {
                    if (itr->last_active_operation > old_time) continue;
                    process_acc(*itr);
                }
            } else {
                const auto& idx = get_index<account_index, by_last_active_operation>();
                auto itr = idx.lower_bound(old_time);
                for (; itr != idx.end(); ++itr) {
                    process_acc(*itr);
                }
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::check_claim_idleness() { try {
            if (head_block_num() % GOLOS_CLAIM_IDLENESS_CHECK_INTERVAL != 0) return;

            if (!has_hardfork(STEEMIT_HARDFORK_0_23__83)) return;
            if (has_hardfork(STEEMIT_HARDFORK_0_27__202)) return;

            const auto now = head_block_time();
            const auto& median_props = get_witness_schedule_object().median_props;
            const auto old_time = now - median_props.claim_idleness_time;

            asset to_workers(0, STEEM_SYMBOL);

            const auto& idx = get_index<account_index, by_last_claim>();
            auto itr = idx.lower_bound(old_time);
            for (; itr != idx.end() && itr->last_claim > fc::time_point_sec::min(); ++itr) {
                if (itr->accumulative_balance.amount == 0) continue;
                to_workers += itr->accumulative_balance;
                modify(*itr, [&](auto& a) {
                    a.accumulative_balance = asset(0, STEEM_SYMBOL);
                });
            }

            if (to_workers.amount != 0) {
                adjust_balance(get_account(STEEMIT_WORKER_POOL_ACCOUNT), to_workers);
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::update_witness_windows_sec_to_min() {
            const auto& widx = get_index<witness_index, by_vote_name>();
            int num_updated = 0;
            for (auto itr = widx.begin();
                    itr != widx.end() && num_updated < 50;
                    ++itr, ++num_updated) {
                modify(*itr, [&](auto& w) {
                    w.props.hf26_windows_sec_to_min();
                });
            }

            modify(get_witness_schedule_object(), [&](auto& wso) {
                wso.median_props.hf26_windows_sec_to_min();
            });
        }

        void database::update_witness_schedule4() { try {
            vector<account_name_type> active_witnesses;
            active_witnesses.reserve(STEEMIT_MAX_WITNESSES);

            /// Add the highest voted witnesses
            flat_set<witness_id_type> selected_voted;
            selected_voted.reserve(STEEMIT_MAX_VOTED_WITNESSES);

            const auto &widx = get_index<witness_index>().indices().get<by_vote_name>();
            for (auto itr = widx.begin();
                 itr != widx.end() &&
                 selected_voted.size() < STEEMIT_MAX_VOTED_WITNESSES;
                 ++itr) {
                if (has_hardfork(STEEMIT_HARDFORK_0_14__278) &&
                    (itr->signing_key == public_key_type())) {
                    continue;
                }
                selected_voted.insert(itr->id);
                active_witnesses.push_back(itr->owner);
                modify(*itr, [&](witness_object &wo) { wo.schedule = witness_object::top19; });
            }

            auto num_elected = active_witnesses.size();

            /// Add miners from the top of the mining queue
            flat_set<witness_id_type> selected_miners;

            if (!has_hardfork(STEEMIT_HARDFORK_0_28__216)) {
                selected_miners.reserve(STEEMIT_MAX_MINER_WITNESSES);
                const auto &gprops = get_dynamic_global_properties();
                const auto &pow_idx = get_index<witness_index>().indices().get<by_pow>();
                auto mitr = pow_idx.upper_bound(0);
                while (mitr != pow_idx.end() &&
                       selected_miners.size() < STEEMIT_MAX_MINER_WITNESSES) {
                    // Only consider a miner who is not a top voted witness
                    if (selected_voted.find(mitr->id) == selected_voted.end()) {
                        // Only consider a miner who has a valid block signing key
                        if (!(has_hardfork(STEEMIT_HARDFORK_0_14__278) &&
                              get_witness(mitr->owner).signing_key ==
                              public_key_type())) {
                            selected_miners.insert(mitr->id);
                            active_witnesses.push_back(mitr->owner);
                            modify(*mitr, [&](witness_object &wo) { wo.schedule = witness_object::miner; });
                        }
                    }
                    // Remove processed miner from the queue
                    auto itr = mitr;
                    ++mitr;
                    modify(*itr, [&](witness_object &wit) {
                        wit.pow_worker = 0;
                    });
                    modify(gprops, [&](dynamic_global_property_object &obj) {
                        obj.num_pow_witnesses--;
                    });
                }
            }

            auto num_miners = selected_miners.size();

            /// Add the running witnesses in the lead
            const witness_schedule_object &wso = get_witness_schedule_object();
            fc::uint128_t new_virtual_time = wso.current_virtual_time;
            const auto &schedule_idx = get_index<witness_index>().indices().get<by_schedule_time>();
            auto sitr = schedule_idx.begin();
            vector<decltype(sitr)> processed_witnesses;
            for (auto witness_count =
                    selected_voted.size() + selected_miners.size();
                 sitr != schedule_idx.end() &&
                 witness_count < STEEMIT_MAX_WITNESSES;
                 ++sitr) {
                new_virtual_time = sitr->virtual_scheduled_time; /// everyone advances to at least this time
                processed_witnesses.push_back(sitr);

                if (has_hardfork(STEEMIT_HARDFORK_0_14__278) &&
                    sitr->signing_key == public_key_type()) {
                    continue;
                } /// skip witnesses without a valid block signing key

                if (selected_miners.find(sitr->id) == selected_miners.end()
                    && selected_voted.find(sitr->id) == selected_voted.end()) {
                    active_witnesses.push_back(sitr->owner);
                    modify(*sitr, [&](witness_object &wo) { wo.schedule = witness_object::timeshare; });
                    ++witness_count;
                }
            }

            auto num_timeshare =
                    active_witnesses.size() - num_miners - num_elected;

            /// Update virtual schedule of processed witnesses
            bool reset_virtual_time = false;
            for (auto itr = processed_witnesses.begin();
                 itr != processed_witnesses.end(); ++itr) {
                auto new_virtual_scheduled_time = new_virtual_time +
                                                  VIRTUAL_SCHEDULE_LAP_LENGTH2 /
                                                  ((*itr)->votes.value + 1);
                if (new_virtual_scheduled_time < new_virtual_time) {
                    reset_virtual_time = true; /// overflow
                    break;
                }
                modify(*(*itr), [&](witness_object &wo) {
                    wo.virtual_position = fc::uint128_t();
                    wo.virtual_last_update = new_virtual_time;
                    wo.virtual_scheduled_time = new_virtual_scheduled_time;
                });
            }
            if (reset_virtual_time) {
                new_virtual_time = fc::uint128_t();
                reset_virtual_schedule_time();
            }
#ifndef STEEMIT_BUILD_TESTNET
            size_t expected_active_witnesses = std::min(size_t(STEEMIT_MAX_WITNESSES), widx.size());
            if (head_block_num() > 14400) {
                FC_ASSERT(active_witnesses.size() ==
                          expected_active_witnesses, "number of active witnesses does not equal expected_active_witnesses=${expected_active_witnesses}",
                        ("active_witnesses.size()", active_witnesses.size())("STEEMIT_MAX_WITNESSES", STEEMIT_MAX_WITNESSES)("expected_active_witnesses", expected_active_witnesses));
            }
#endif

            auto majority_version = wso.majority_version;

            if (has_hardfork(STEEMIT_HARDFORK_0_5__54)) {
                flat_map<version, uint32_t, std::greater<version>> witness_versions;
                flat_map<std::tuple<hardfork_version, time_point_sec>, uint32_t> hardfork_version_votes;

                for (uint32_t i = 0; i < wso.num_scheduled_witnesses; i++) {
                    auto witness = get_witness(wso.current_shuffled_witnesses[i]);
                    if (witness_versions.find(witness.running_version) ==
                        witness_versions.end()) {
                        witness_versions[witness.running_version] = 1;
                    } else {
                        witness_versions[witness.running_version] += 1;
                    }

                    auto version_vote = std::make_tuple(witness.hardfork_version_vote, witness.hardfork_time_vote);
                    if (hardfork_version_votes.find(version_vote) ==
                        hardfork_version_votes.end()) {
                        hardfork_version_votes[version_vote] = 1;
                    } else {
                        hardfork_version_votes[version_vote] += 1;
                    }
                }

                int witnesses_on_version = 0;
                auto ver_itr = witness_versions.begin();

                // The map should be sorted highest version to smallest, so we iterate until we hit the majority of witnesses on at least this version
                while (ver_itr != witness_versions.end()) {
                    witnesses_on_version += ver_itr->second;

                    if (witnesses_on_version >=
                        STEEMIT_HARDFORK_REQUIRED_WITNESSES) {
                        majority_version = ver_itr->first;
                        break;
                    }

                    ++ver_itr;
                }

                auto hf_itr = hardfork_version_votes.begin();

                while (hf_itr != hardfork_version_votes.end()) {
                    if (hf_itr->second >= STEEMIT_HARDFORK_REQUIRED_WITNESSES) {
                        const auto &hfp = get_hardfork_property_object();
                        if (hfp.next_hardfork != std::get<0>(hf_itr->first) ||
                            hfp.next_hardfork_time !=
                            std::get<1>(hf_itr->first)) {

                            modify(hfp, [&](hardfork_property_object &hpo) {
                                hpo.next_hardfork = std::get<0>(hf_itr->first);
                                hpo.next_hardfork_time = std::get<1>(hf_itr->first);
                            });
                        }
                        break;
                    }

                    ++hf_itr;
                }

                // We no longer have a majority
                if (hf_itr == hardfork_version_votes.end()) {
                    modify(get_hardfork_property_object(), [&](hardfork_property_object &hpo) {
                        hpo.next_hardfork = hpo.current_hardfork_version;
                    });
                }
            }

            assert(num_elected + num_miners + num_timeshare ==
                   active_witnesses.size());

            modify(wso, [&](witness_schedule_object &_wso) {
                // active witnesses has exactly STEEMIT_MAX_WITNESSES elements, asserted above
                for (size_t i = 0; i < active_witnesses.size(); i++) {
                    _wso.current_shuffled_witnesses[i] = active_witnesses[i];
                }

                for (size_t i = active_witnesses.size();
                     i < STEEMIT_MAX_WITNESSES; i++) {
                    _wso.current_shuffled_witnesses[i] = account_name_type();
                }

                _wso.num_scheduled_witnesses = std::max<uint8_t>(active_witnesses.size(), 1);
                _wso.witness_pay_normalization_factor =
                        _wso.top19_weight * num_elected
                        + _wso.miner_weight * num_miners
                        + _wso.timeshare_weight * num_timeshare;

                /// shuffle current shuffled witnesses
                auto now_hi =
                        uint64_t(head_block_time().sec_since_epoch()) << 32;
                for (uint32_t i = 0; i < _wso.num_scheduled_witnesses; ++i) {
                    /// High performance random generator
                    /// http://xorshift.di.unimi.it/
                    uint64_t k = now_hi + uint64_t(i) * 2685821657736338717ULL;
                    k ^= (k >> 12);
                    k ^= (k << 25);
                    k ^= (k >> 27);
                    k *= 2685821657736338717ULL;

                    uint32_t jmax = _wso.num_scheduled_witnesses - i;
                    uint32_t j = i + k % jmax;
                    std::swap(_wso.current_shuffled_witnesses[i],
                            _wso.current_shuffled_witnesses[j]);
                }

                _wso.current_virtual_time = new_virtual_time;
                _wso.next_shuffle_block_num =
                        head_block_num() + _wso.num_scheduled_witnesses;
                _wso.majority_version = majority_version;
            });

            update_median_witness_props();
        } FC_CAPTURE_AND_RETHROW() }


/**
 *
 *  See @ref witness_object::virtual_last_update
 */
        void database::update_witness_schedule() {
            if ((head_block_num() % STEEMIT_MAX_WITNESSES) ==
                0) //wso.next_shuffle_block_num )
            {
                if (has_hardfork(STEEMIT_HARDFORK_0_4)) {
                    update_witness_schedule4();
                    return;
                }

                const auto &props = get_dynamic_global_properties();
                const witness_schedule_object &wso = get_witness_schedule_object();


                vector<account_name_type> active_witnesses;
                active_witnesses.reserve(STEEMIT_MAX_WITNESSES);

                fc::uint128_t new_virtual_time;

                /// only use vote based scheduling after the first 1M STEEM is created or if there is no POW queued
                if (props.num_pow_witnesses == 0 ||
                    head_block_num() > STEEMIT_START_MINER_VOTING_BLOCK) {
                    const auto &widx = get_index<witness_index>().indices().get<by_vote_name>();

                    for (auto itr = widx.begin(); itr != widx.end() &&
                                                  (active_witnesses.size() <
                                                   (STEEMIT_MAX_WITNESSES -
                                                    2)); ++itr) {
                        if (itr->pow_worker) {
                            continue;
                        }

                        active_witnesses.push_back(itr->owner);

                        /// don't consider the top 19 for the purpose of virtual time scheduling
                        modify(*itr, [&](witness_object &wo) {
                            wo.virtual_scheduled_time = fc::uint128_t::max_value();
                        });
                    }

                    /// add the virtual scheduled witness, reseeting their position to 0 and their time to completion

                    const auto &schedule_idx = get_index<witness_index>().indices().get<by_schedule_time>();
                    auto sitr = schedule_idx.begin();
                    while (sitr != schedule_idx.end() && sitr->pow_worker) {
                        ++sitr;
                    }

                    if (sitr != schedule_idx.end()) {
                        active_witnesses.push_back(sitr->owner);
                        modify(*sitr, [&](witness_object &wo) {
                            wo.virtual_position = fc::uint128_t();
                            new_virtual_time = wo.virtual_scheduled_time; /// everyone advances to this time

                            /// extra cautious sanity check... we should never end up here if witnesses are
                            /// properly voted on. TODO: remove this line if it is not triggered and therefore
                            /// the code path is unreachable.
                            if (new_virtual_time == fc::uint128_t::max_value()) {
                                new_virtual_time = fc::uint128_t();
                            }

                            /// this witness will produce again here
                            if (has_hardfork(STEEMIT_HARDFORK_0_2)) {
                                wo.virtual_scheduled_time +=
                                        VIRTUAL_SCHEDULE_LAP_LENGTH2 /
                                        (wo.votes.value + 1);
                            } else {
                                wo.virtual_scheduled_time +=
                                        VIRTUAL_SCHEDULE_LAP_LENGTH /
                                        (wo.votes.value + 1);
                            }
                        });
                    }
                }

                /// Add the next POW witness to the active set if there is one...
                const auto &pow_idx = get_index<witness_index>().indices().get<by_pow>();

                auto itr = pow_idx.upper_bound(0);
                /// if there is more than 1 POW witness, then pop the first one from the queue...
                if (props.num_pow_witnesses > STEEMIT_MAX_WITNESSES) {
                    if (itr != pow_idx.end()) {
                        modify(*itr, [&](witness_object &wit) {
                            wit.pow_worker = 0;
                        });
                        modify(get_dynamic_global_properties(), [&](dynamic_global_property_object &obj) {
                            obj.num_pow_witnesses--;
                        });
                    }
                }

                /// add all of the pow witnesses to the round until voting takes over, then only add one per round
                itr = pow_idx.upper_bound(0);
                while (itr != pow_idx.end()) {
                    active_witnesses.push_back(itr->owner);

                    if (head_block_num() > STEEMIT_START_MINER_VOTING_BLOCK ||
                        active_witnesses.size() >= STEEMIT_MAX_WITNESSES) {
                        break;
                    }
                    ++itr;
                }

                modify(wso, [&](witness_schedule_object &_wso) {
                    /*
         _wso.current_shuffled_witnesses.clear();
         _wso.current_shuffled_witnesses.reserve( active_witnesses.size() );

         for( const string& w : active_witnesses )
            _wso.current_shuffled_witnesses.push_back( w );
            */
                    // active witnesses has exactly STEEMIT_MAX_WITNESSES elements, asserted above
                    for (size_t i = 0; i < active_witnesses.size(); i++) {
                        _wso.current_shuffled_witnesses[i] = active_witnesses[i];
                    }

                    for (size_t i = active_witnesses.size();
                         i < STEEMIT_MAX_WITNESSES; i++) {
                        _wso.current_shuffled_witnesses[i] = account_name_type();
                    }

                    _wso.num_scheduled_witnesses = std::max<uint8_t>(active_witnesses.size(), 1);

                    //idump( (_wso.current_shuffled_witnesses)(active_witnesses.size()) );

                    auto now_hi =
                            uint64_t(head_block_time().sec_since_epoch()) << 32;
                    for (uint32_t i = 0;
                         i < _wso.num_scheduled_witnesses; ++i) {
                        /// High performance random generator
                        /// http://xorshift.di.unimi.it/
                        uint64_t k =
                                now_hi + uint64_t(i) * 2685821657736338717ULL;
                        k ^= (k >> 12);
                        k ^= (k << 25);
                        k ^= (k >> 27);
                        k *= 2685821657736338717ULL;

                        uint32_t jmax = _wso.num_scheduled_witnesses - i;
                        uint32_t j = i + k % jmax;
                        std::swap(_wso.current_shuffled_witnesses[i],
                                _wso.current_shuffled_witnesses[j]);
                    }

                    if (props.num_pow_witnesses == 0 ||
                        head_block_num() > STEEMIT_START_MINER_VOTING_BLOCK) {
                        _wso.current_virtual_time = new_virtual_time;
                    }

                    _wso.next_shuffle_block_num =
                            head_block_num() + _wso.num_scheduled_witnesses;
                });
                update_median_witness_props();
            }
        }

        void database::update_median_witness_props() { try {
            const witness_schedule_object& wso = get_witness_schedule_object();

            /// fetch all witness objects
            vector<const witness_object*> active;
            active.reserve(wso.num_scheduled_witnesses);
            for (int i = 0; i < wso.num_scheduled_witnesses; i++) {
                active.push_back(&get_witness(wso.current_shuffled_witnesses[i]));
            }

            chain_properties_30 median_props;

            auto median = active.size() / 2;

            auto calc_median = [&](auto&& param) {
                std::nth_element(
                    active.begin(), active.begin() + median, active.end(),
                    [&](const auto* a, const auto* b) {
                        return a->props.*param < b->props.*param;
                    }
                );
                median_props.*param = active[median]->props.*param;
            };

            auto calc_median_battery = [&](auto&& window, auto&& items) {
                std::nth_element(
                    active.begin(), active.begin() + median, active.end(),
                    [&](const auto* a, const auto* b) {
                        auto a_consumption = a->props.*window / a->props.*items;
                        auto b_consumption = b->props.*window / b->props.*items;
                        return std::tie(a_consumption, a->props.*items) < std::tie(b_consumption, b->props.*items);
                    }
                );
                median_props.*window = active[median]->props.*window;
                median_props.*items = active[median]->props.*items;
            };

            auto calc_median_min_max = [&](auto&& min, auto&& max) {
                std::nth_element(
                    active.begin(), active.begin() + median, active.end(),
                    [&](const auto* a, const auto* b) {
                        return std::tie(a->props.*min, a->props.*max) < std::tie(b->props.*min, b->props.*max);
                    }
                );
                median_props.*min = active[median]->props.*min;
                median_props.*max = active[median]->props.*max;
            };

            calc_median(&chain_properties_17::account_creation_fee);
            calc_median(&chain_properties_17::maximum_block_size);
            calc_median(&chain_properties_17::sbd_interest_rate);
            calc_median(&chain_properties_18::create_account_min_golos_fee);
            calc_median(&chain_properties_18::create_account_min_delegation);
            calc_median(&chain_properties_18::create_account_delegation_time);
            calc_median(&chain_properties_18::min_delegation);
            calc_median(&chain_properties_19::auction_window_size);
            calc_median(&chain_properties_19::max_referral_interest_rate);
            calc_median(&chain_properties_19::max_referral_term_sec);
            calc_median(&chain_properties_19::min_referral_break_fee);
            calc_median(&chain_properties_19::max_referral_break_fee);
            calc_median_battery(&chain_properties_19::posts_window, &chain_properties_19::posts_per_window);
            calc_median_battery(&chain_properties_19::comments_window, &chain_properties_19::comments_per_window);
            calc_median_battery(&chain_properties_19::votes_window, &chain_properties_19::votes_per_window);
            calc_median(&chain_properties_19::max_delegated_vesting_interest_rate);
            calc_median(&chain_properties_19::custom_ops_bandwidth_multiplier);
            calc_median_min_max(&chain_properties_19::min_curation_percent, &chain_properties_19::max_curation_percent);
            calc_median(&chain_properties_19::curation_reward_curve);
            calc_median(&chain_properties_19::allow_distribute_auction_reward);
            calc_median(&chain_properties_19::allow_return_auction_reward_to_fund);
            calc_median(&chain_properties_22::worker_reward_percent);
            calc_median(&chain_properties_22::witness_reward_percent);
            calc_median(&chain_properties_22::vesting_reward_percent);
            calc_median(&chain_properties_22::worker_request_creation_fee);
            calc_median(&chain_properties_22::worker_request_approve_min_percent);
            calc_median(&chain_properties_22::sbd_debt_convert_rate);
            calc_median(&chain_properties_22::vote_regeneration_per_day);
            calc_median(&chain_properties_22::witness_skipping_reset_time);
            calc_median(&chain_properties_22::witness_idleness_time);
            calc_median(&chain_properties_22::account_idleness_time);
            calc_median(&chain_properties_23::claim_idleness_time);
            calc_median(&chain_properties_23::min_invite_balance);
            calc_median(&chain_properties_24::asset_creation_fee);
            calc_median(&chain_properties_24::invite_transfer_interval_sec);
            calc_median(&chain_properties_26::convert_fee_percent);
            calc_median(&chain_properties_26::min_golos_power_to_curate);

            if (has_hardfork(STEEMIT_HARDFORK_0_26__163)) {
                // Now these parameters are unused
                calc_median(&chain_properties_26::worker_reward_percent);
                calc_median(&chain_properties_26::witness_reward_percent);
                calc_median(&chain_properties_26::vesting_reward_percent);
            } else {
                if (has_hardfork(STEEMIT_HARDFORK_0_23)) {
                    #define COPY_ALL_MEDIAN(FROM, TO, FIELD) \
                        std::nth_element( \
                            FROM.begin(), FROM.begin() + FROM.size() / 2, FROM.end(), \
                            [&](const auto* a, const auto* b) { \
                                return a->props.FIELD < b->props.FIELD; \
                            } \
                        ); \
                        vector<const witness_object*> TO; \
                        std::copy_if(FROM.begin(), FROM.end(), back_inserter(TO), [&](auto& el) { return el->props.FIELD == FROM[FROM.size() / 2]->props.FIELD; });

                    COPY_ALL_MEDIAN(active, act_worker, worker_reward_percent);
                    COPY_ALL_MEDIAN(act_worker, act_witness, witness_reward_percent);
                    std::nth_element(
                        act_witness.begin(), act_witness.begin() + act_witness.size() / 2, act_witness.end(),
                        [&](const auto* a, const auto* b) { \
                            return a->props.vesting_reward_percent < b->props.vesting_reward_percent;
                        }
                    );
                    median_props.worker_reward_percent = act_witness[act_witness.size() / 2]->props.worker_reward_percent;
                    median_props.witness_reward_percent = act_witness[act_witness.size() / 2]->props.witness_reward_percent;
                    median_props.vesting_reward_percent = act_witness[act_witness.size() / 2]->props.vesting_reward_percent;
                } else {
                    std::nth_element(
                        active.begin(), active.begin() + median, active.end(),
                        [&](const auto* a, const auto* b) {
                            return std::tie(a->props.worker_reward_percent, a->props.witness_reward_percent, a->props.vesting_reward_percent)
                                < std::tie(b->props.worker_reward_percent, b->props.witness_reward_percent, b->props.vesting_reward_percent);
                        }
                    );
                    median_props.worker_reward_percent = active[median]->props.worker_reward_percent;
                    median_props.witness_reward_percent = active[median]->props.witness_reward_percent;
                    median_props.vesting_reward_percent = active[median]->props.vesting_reward_percent;
                }
            }

            calc_median(&chain_properties_26::worker_emission_percent);
            calc_median(&chain_properties_26::vesting_of_remain_percent);
            if (has_hardfork(STEEMIT_HARDFORK_0_27)) {
                calc_median(&chain_properties_26::negrep_posting_window);
                calc_median(&chain_properties_26::negrep_posting_per_window);
            } else {
                calc_median_battery(&chain_properties_26::negrep_posting_window, &chain_properties_26::negrep_posting_per_window);
            }
            calc_median(&chain_properties_27::unwanted_operation_cost);
            calc_median(&chain_properties_27::unlimit_operation_cost);
            calc_median(&chain_properties_28::min_golos_power_to_emission);
            calc_median(&chain_properties_29::nft_issue_cost);
            calc_median(&chain_properties_30::private_group_golos_power);
            calc_median(&chain_properties_30::private_group_cost);

            const auto& dynamic_global_properties = get_dynamic_global_properties();

            modify(wso, [&](witness_schedule_object &_wso) {
                _wso.median_props = median_props;
            });

            modify(dynamic_global_properties, [&](dynamic_global_property_object& dgpo) {
                dgpo.maximum_block_size = median_props.maximum_block_size;
                dgpo.sbd_interest_rate = median_props.sbd_interest_rate;
                dgpo.custom_ops_bandwidth_multiplier = median_props.custom_ops_bandwidth_multiplier;
            });
        } FC_CAPTURE_AND_RETHROW() }

        void database::adjust_proxied_witness_votes(const account_object &a,
                const std::array<share_type,
                        STEEMIT_MAX_PROXY_RECURSION_DEPTH + 1> &delta,
                int depth) {
            if (a.proxy != STEEMIT_PROXY_TO_SELF_ACCOUNT) {
                /// nested proxies are not supported, vote will not propagate
                if (depth >= STEEMIT_MAX_PROXY_RECURSION_DEPTH) {
                    return;
                }

                const auto &proxy = get_account(a.proxy);

                modify(proxy, [&](account_object &a) {
                    for (int i = STEEMIT_MAX_PROXY_RECURSION_DEPTH - depth - 1;
                         i >= 0; --i) {
                        a.proxied_vsf_votes[i + depth] += delta[i];
                    }
                });

                adjust_proxied_witness_votes(proxy, delta, depth + 1);
            } else {
                share_type total_delta = 0;
                for (int i = STEEMIT_MAX_PROXY_RECURSION_DEPTH - depth;
                     i >= 0; --i) {
                    total_delta += delta[i];
                }
                if (has_hardfork(STEEMIT_HARDFORK_0_22__68)) {
                    total_delta /= std::max(a.witnesses_voted_for, uint16_t(1));
                }
                adjust_witness_votes(a, total_delta);
            }
        }

        void database::adjust_proxied_witness_votes(const account_object &a, share_type delta, int depth) {
            if (a.proxy != STEEMIT_PROXY_TO_SELF_ACCOUNT) {
                /// nested proxies are not supported, vote will not propagate
                if (depth >= STEEMIT_MAX_PROXY_RECURSION_DEPTH) {
                    return;
                }

                const auto &proxy = get_account(a.proxy);

                modify(proxy, [&](account_object &a) {
                    a.proxied_vsf_votes[depth] += delta;
                });

                adjust_proxied_witness_votes(proxy, delta, depth + 1);
            } else {
                if (has_hardfork(STEEMIT_HARDFORK_0_22__68)) {
                    delta /= std::max(a.witnesses_voted_for, uint16_t(1));
                }
                adjust_witness_votes(a, delta);
            }
        }

        void database::adjust_witness_votes(const account_object &a, share_type delta) {
            const auto &vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
            auto itr = vidx.lower_bound(boost::make_tuple(a.id, witness_id_type()));
            while (itr != vidx.end() && itr->account == a.id) {
                adjust_witness_vote(a, get(itr->witness), delta);
                ++itr;
            }
        }

        void database::adjust_witness_vote(const account_object& a, const witness_object& witness, share_type delta, bool overwrite) {
            const witness_schedule_object &wso = get_witness_schedule_object();
            modify(witness, [&](witness_object &w) {
                auto delta_pos = w.votes.value * (wso.current_virtual_time -
                                                  w.virtual_last_update);
                w.virtual_position += delta_pos;

                w.virtual_last_update = wso.current_virtual_time;
                // With staked voting it can be negative because of inaccuracy of integer division. It should be >= 0.
                w.votes = std::max(w.votes + delta, share_type(0));
                FC_ASSERT(w.votes <=
                          get_dynamic_global_properties().total_vesting_shares.amount, "", ("w.votes", w.votes)("props", get_dynamic_global_properties().total_vesting_shares));

                if (has_hardfork(STEEMIT_HARDFORK_0_2)) {
                    w.virtual_scheduled_time = w.virtual_last_update +
                                               (VIRTUAL_SCHEDULE_LAP_LENGTH2 -
                                                w.virtual_position) /
                                               (w.votes.value + 1);
                } else {
                    w.virtual_scheduled_time = w.virtual_last_update +
                                               (VIRTUAL_SCHEDULE_LAP_LENGTH -
                                                w.virtual_position) /
                                               (w.votes.value + 1);
                }

                /** witnesses with a low number of votes could overflow the time field and end up with a scheduled time in the past */
                if (has_hardfork(STEEMIT_HARDFORK_0_4)) {
                    if (w.virtual_scheduled_time < wso.current_virtual_time) {
                        w.virtual_scheduled_time = fc::uint128_t::max_value();
                    }
                }
            });

            const auto& vidx = get_index<witness_vote_index, by_account_witness>();
            auto vitr = vidx.find(std::make_tuple(a.id, witness.id));
            if (vitr != vidx.end()) {
                modify(*vitr, [&](auto& v) {
                    if (overwrite)
                        v.rshares = 0;
                    v.rshares = std::max(v.rshares + delta, share_type(0));
                });
            }
        }

        void database::clear_witness_votes(const account_object &a) {
            const auto &vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
            auto itr = vidx.lower_bound(boost::make_tuple(a.id, witness_id_type()));
            while (itr != vidx.end() && itr->account == a.id) {
                const auto &current = *itr;
                ++itr;
                remove(current);
            }

            if (has_hardfork(STEEMIT_HARDFORK_0_6__104)) { // TODO: this check can be removed after hard fork
                modify(a, [&](account_object &acc) {
                    acc.witnesses_voted_for = 0;
                });
            }
        }

        void database::clear_null_account_balance() { try {
            if (!has_hardfork(STEEMIT_HARDFORK_0_14__327)) {
                return;
            }

            const auto &null_account = get_account(STEEMIT_NULL_ACCOUNT);
            asset total_steem(0, STEEM_SYMBOL);
            asset total_sbd(0, SBD_SYMBOL);

            if (null_account.balance.amount > 0) {
                total_steem += null_account.balance;
                adjust_balance(null_account, -null_account.balance);
            }

            if (null_account.savings_balance.amount > 0) {
                total_steem += null_account.savings_balance;
                adjust_savings_balance(null_account, -null_account.savings_balance);
            }

            if (null_account.sbd_balance.amount > 0) {
                total_sbd += null_account.sbd_balance;
                adjust_balance(null_account, -null_account.sbd_balance);
            }

            if (null_account.savings_sbd_balance.amount > 0) {
                total_sbd += null_account.savings_sbd_balance;
                adjust_savings_balance(null_account, -null_account.savings_sbd_balance);
            }

            if (null_account.vesting_shares.amount > 0) {
                const auto &gpo = get_dynamic_global_properties();
                auto converted_steem = null_account.vesting_shares *
                                       gpo.get_vesting_share_price();

                modify(gpo, [&](dynamic_global_property_object &g) {
                    g.total_vesting_shares -= null_account.vesting_shares;
                    g.total_vesting_fund_steem -= converted_steem;
                });

                modify(null_account, [&](account_object &a) {
                    a.vesting_shares.amount = 0;
                });

                total_steem += converted_steem;
            }

            bool hf27 = has_hardfork(STEEMIT_HARDFORK_0_27__207);

            if (hf27 && null_account.tip_balance.amount > 0) {
                total_steem += null_account.tip_balance;
                modify(null_account, [&](account_object &a) {
                    a.tip_balance.amount = 0;
                });
            }

            if (total_steem.amount > 0) {
                adjust_supply(-total_steem);
            }

            if (total_sbd.amount > 0) {
                adjust_supply(-total_sbd);
            }

            if (has_hardfork(STEEMIT_HARDFORK_0_24__95)) {
                const auto& idx = get_index<account_balance_index, by_account_symbol>();
                auto itr = idx.lower_bound(null_account.name);
                for (; itr != idx.end() && itr->account == null_account.name; ++itr) {
                    modify(get_asset(itr->balance.symbol), [&](auto& a) {
                        a.supply -= itr->balance;
                        if (hf27) {
                            a.supply -= itr->tip_balance;
                        }
                    });
                    modify(*itr, [&](auto& a) {
                        a.balance = asset(0, a.balance.symbol);
                        if (hf27) {
                            a.tip_balance = asset(0, a.balance.symbol);
                        }
                    });
                }
            }
        } FC_CAPTURE_AND_RETHROW() }

/**
 * This method recursively tallies children_rshares2 for this post plus all of its parents,
 * TODO: this method can be skipped for validation-only nodes
 */
        void database::adjust_rshares2(const comment_object &c, fc::uint128_t old_rshares2, fc::uint128_t new_rshares2) {
            const auto* extras = find_extras(c.author, c.hashlink);
            if (extras) {
                modify(*extras, [&](auto& ex) {
                    ex.children_rshares2 -= old_rshares2;
                    ex.children_rshares2 += new_rshares2;
                });
            }
            if (c.depth) {
                adjust_rshares2(get_comment(c.parent_author, c.parent_hashlink), old_rshares2, new_rshares2);
            } else {
                const auto &cprops = get_dynamic_global_properties();
                modify(cprops, [&](dynamic_global_property_object &p) {
                    p.total_reward_shares2 -= old_rshares2;
                    p.total_reward_shares2 += new_rshares2;
                });
            }
        }

        authority database::update_owner_authority(const account_object &account, const authority &owner_authority) {
            if (head_block_num() >=
                STEEMIT_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM) {
                create<owner_authority_history_object>([&](owner_authority_history_object &hist) {
                    hist.account = account.name;
                    hist.previous_owner_authority = get_authority(account.name).owner;
                    hist.last_valid_time = head_block_time();
                });
            }

            const auto& auth = get_authority(account.name);
            authority old(auth.owner);
            modify(auth, [&](account_authority_object& auth) {
                auth.owner = owner_authority;
                auth.last_owner_update = head_block_time();
            });
            return old;
        }

        void database::process_vesting_withdrawals() { try {
            const auto &widx = get_index<account_index>().indices().get<by_next_vesting_withdrawal>();
            const auto &didx = get_index<withdraw_vesting_route_index>().indices().get<by_withdraw_route>();
            auto current = widx.begin();

            const auto &cprops = get_dynamic_global_properties();

            while (current != widx.end() && current->next_vesting_withdrawal <= head_block_time()) {
                const auto &from_account = *current;
                ++current;

                /**
                 *  Let T = total tokens in vesting fund
                 *  Let V = total vesting shares
                 *  Let v = total vesting shares being cashed out
                 *
                 *  The user may withdraw vT / V tokens
                 */
                share_type to_withdraw;
                if (from_account.to_withdraw - from_account.withdrawn <
                    from_account.vesting_withdraw_rate.amount) {
                    to_withdraw = std::min(
                        from_account.vesting_shares.amount,
                        from_account.to_withdraw % from_account.vesting_withdraw_rate.amount).value;
                } else {
                    to_withdraw = std::min(
                        from_account.vesting_shares.amount,
                        from_account.vesting_withdraw_rate.amount).value;
                }

                if (to_withdraw < 0) {
                    to_withdraw = 0;
                } else if (has_hardfork(STEEMIT_HARDFORK_0_28__215)) {
                    auto remainder = from_account.withdrawn + to_withdraw;
                    remainder = from_account.to_withdraw - remainder;
                    if (remainder > 0 && remainder < from_account.vesting_withdraw_rate.amount) {
                        to_withdraw += remainder;
                    }
                }

                share_type vests_deposited_as_steem = 0;
                share_type vests_deposited_as_vests = 0;
                asset total_steem_converted = asset(0, STEEM_SYMBOL);

                // Do two passes, the first for vests, the second for steem. Try to maintain as much accuracy for vests as possible.
                for (auto itr = didx.upper_bound(boost::make_tuple(from_account.id, account_id_type()));
                     itr != didx.end() && itr->from_account == from_account.id;
                     ++itr
                ) {
                    if (itr->auto_vest) {
                        share_type to_deposit = (
                                (fc::uint128_t(to_withdraw.value) * itr->percent) /
                                STEEMIT_100_PERCENT).to_uint64();
                        vests_deposited_as_vests += to_deposit;

                        if (to_deposit > 0) {
                            const auto &to_account = get(itr->to_account);

                            modify(to_account, [&](account_object &a) {
                                a.vesting_shares.amount += to_deposit;
                            });

                            adjust_proxied_witness_votes(to_account, to_deposit);

                            push_virtual_operation(
                                fill_vesting_withdraw_operation(
                                    from_account.name, to_account.name,
                                    asset(to_deposit, VESTS_SYMBOL), asset(to_deposit, VESTS_SYMBOL)));
                        }
                    }
                }

                for (auto itr = didx.upper_bound(boost::make_tuple(from_account.id, account_id_type()));
                     itr != didx.end() && itr->from_account == from_account.id;
                     ++itr
                ) {
                    if (!itr->auto_vest) {
                        const auto &to_account = get(itr->to_account);

                        share_type to_deposit = (
                                (fc::uint128_t(to_withdraw.value) * itr->percent) /
                                STEEMIT_100_PERCENT).to_uint64();
                        vests_deposited_as_steem += to_deposit;
                        auto converted_steem = asset(to_deposit, VESTS_SYMBOL) * cprops.get_vesting_share_price();
                        total_steem_converted += converted_steem;

                        if (to_deposit > 0) {
                            modify(to_account, [&](account_object &a) {
                                a.balance += converted_steem;
                            });

                            modify(cprops, [&](dynamic_global_property_object &o) {
                                o.total_vesting_fund_steem -= converted_steem;
                                o.total_vesting_shares.amount -= to_deposit;
                            });

                            push_virtual_operation(
                                fill_vesting_withdraw_operation(
                                    from_account.name, to_account.name,
                                    asset(to_deposit, VESTS_SYMBOL), converted_steem));
                        }
                    }
                }

                share_type to_convert = to_withdraw - vests_deposited_as_steem - vests_deposited_as_vests;

                if (to_convert < 0) {
                    to_convert = 0;
                }

                auto converted_steem = asset(to_convert, VESTS_SYMBOL) * cprops.get_vesting_share_price();

                bool add_fix_me = false;

                modify(from_account, [&](account_object &a) {
                    a.vesting_shares.amount -= to_withdraw;
                    a.balance += converted_steem;
                    a.withdrawn += to_withdraw;

                    if (a.withdrawn >= a.to_withdraw || a.vesting_shares.amount == 0) {
                        a.vesting_withdraw_rate.amount = 0;
                        a.next_vesting_withdrawal = fc::time_point_sec::maximum();

                        if (has_hardfork(STEEMIT_HARDFORK_0_19__971)) {
                            a.withdrawn = 0;
                            a.to_withdraw = 0;
                        } else if (a.to_withdraw.value) {
                            add_fix_me = true;
                        }
                    } else {
                        a.next_vesting_withdrawal += fc::seconds(STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS);
                    }
                });

                if (add_fix_me) {
                    const auto* fm = find<fix_me_object, by_account>(from_account.id);
                    if (!fm) {
                        create<fix_me_object>([&](auto& fm) {
                            fm.account = from_account.id;
                        });
                    }
                }

                modify(cprops, [&](dynamic_global_property_object &o) {
                    o.total_vesting_fund_steem -= converted_steem;
                    o.total_vesting_shares.amount -= to_convert;
                });

                if (to_withdraw > 0) {
                    adjust_proxied_witness_votes(from_account, -to_withdraw);
                    push_virtual_operation(
                        fill_vesting_withdraw_operation(
                            from_account.name, from_account.name,
                            asset(to_withdraw, VESTS_SYMBOL), converted_steem));
                }
            }
        } FC_CAPTURE_AND_RETHROW() }

        uint64_t database::pay_delegators(const account_object& delegatee, const comment_vote_object& cvo, uint64_t claim) {
            uint64_t delegators_reward = 0;
            const auto& vdo_idx = get_index<vesting_delegation_index>().indices().get<by_received>();
            for (auto& dvir : cvo.delegator_vote_interest_rates) {
                if (dvir.interest_rate > STEEMIT_100_PERCENT) {
                    wlog("Skip bad delegator's interest rate: ${r}", ("r", dvir.interest_rate));
                    continue;
                }
                auto delegator_claim = claim * dvir.interest_rate / STEEMIT_100_PERCENT;
                if (delegator_claim == 0) {
                    continue;
                }

                delegators_reward += delegator_claim;

                const auto& delegator = get_account(dvir.account);
                asset delegator_vesting = create_vesting(delegator, asset(delegator_claim, STEEM_SYMBOL));
                if (dvir.payout_strategy == to_delegated_vesting) {
                    auto vdo_itr = vdo_idx.find(std::make_tuple(delegatee.name, dvir.account));
                    if (vdo_itr != vdo_idx.end()) {
                        modify(delegator, [&](account_object& a) {
                            a.delegated_vesting_shares += delegator_vesting;
                        });
                        modify(delegatee, [&](account_object& a) {
                            a.received_vesting_shares += delegator_vesting;
                        });
                        modify(*vdo_itr, [&](vesting_delegation_object& o) {
                            o.vesting_shares += delegator_vesting;
                        });
                    }
                }

                push_virtual_operation(delegation_reward_operation(delegator.name, delegatee.name, dvir.payout_strategy, delegator_vesting, asset(delegator_claim, STEEM_SYMBOL)));

                modify(delegator, [&](account_object& a) {
                    a.delegation_rewards += delegator_claim;
                });
            }
            return delegators_reward;
        }

        uint64_t database::pay_curator(const comment_vote_object& cvo, const uint64_t& claim, const comment_curation_info& c, share_type& back_to_fund) {
            const auto& voter = get(cvo.voter);
            auto voter_claim = claim;

            if (voter.effective_vesting_shares() < c.min_vesting_shares_to_curate) {
                back_to_fund += voter_claim;
                return 0;
            }

            if (has_hardfork(STEEMIT_HARDFORK_0_19__756)) {
                voter_claim -= pay_delegators(voter, cvo, claim);
            }

            auto voter_reward = create_vesting(voter, asset(voter_claim, STEEM_SYMBOL));

            push_virtual_operation(curation_reward_operation(voter.name, voter_reward, c.comment.author, c.comment.hashlink, asset(voter_claim, STEEM_SYMBOL)));

            modify(voter, [&](auto& a) {
                a.curation_rewards += voter_claim;
            });

            return voter_claim;
        }
/**
 *  This method will iterate through all comment_vote_objects and give them
 *  (max_rewards * weight) / c.total_vote_weight.
 *
 *  @returns unclaimed rewards.
 */
        share_type database::pay_curators(const comment_curation_info& c, share_type max_rewards, share_type& actual_rewards) {
            try {
                share_type unclaimed_rewards = max_rewards;

                const auto& cbl = get_comment_bill(c.comment.id);

                if (c.total_vote_weight > 0 && cbl.allow_curation_rewards) {
                    share_type back_to_fund = 0;
                    uint128_t total_weight(c.total_vote_weight);
                    uint128_t auction_window_reward = uint128_t(max_rewards.value) * c.auction_window_weight / total_weight;

                    // memorize the top weight account after auction window
                    auto heaviest_itr = c.vote_list.end();

                    for (auto itr = c.vote_list.begin(); c.vote_list.end() != itr; ++itr) {
                        uint128_t weight(itr->weight);
                        uint64_t claim = ((max_rewards.value * weight) / total_weight).to_uint64();
                        // to_curators case
                        if (cbl.auction_window_reward_destination == protocol::to_curators &&
                            itr->vote->auction_time == cbl.auction_window_size
                        ) {
                            if (c.votes_after_auction_window_weight) {
                                claim += ((auction_window_reward * weight) / c.votes_after_auction_window_weight).to_uint64();
                            }
                            if (heaviest_itr == c.vote_list.end()) {
                                // as votes are sorted in non-decreasing order
                                // this if will work once or won't work at all
                                heaviest_itr = itr;
                                continue;
                            }
                        }

                        if (claim > 0) { // min_amt is non-zero satoshis
                            unclaimed_rewards -= claim;
                            actual_rewards += pay_curator(*itr->vote, claim, c, back_to_fund);
                        } else {
                            break;
                        }
                    }
                    if (cbl.auction_window_reward_destination == protocol::to_curators && heaviest_itr != c.vote_list.end()) {
                        // pay needed claim + rest unclaimed tokens (close to zero value) to curator with greates weight
                        // BTW: it has to be unclaimed_rewards.value not heaviest_vote_after_auw_weight + unclaimed_rewards.value, coz
                        //      unclaimed_rewards already contains this.
                        actual_rewards = pay_curator(*heaviest_itr->vote, unclaimed_rewards.value, c, back_to_fund);
                        unclaimed_rewards = 0;
                    }
                    if (back_to_fund != 0) {
                        modify(get_dynamic_global_properties(), [&](dynamic_global_property_object &props) {
                            props.total_reward_fund_steem += asset(back_to_fund, STEEM_SYMBOL);
                        });
                    }
                }

                if (!cbl.allow_curation_rewards) {
                    modify(get_dynamic_global_properties(), [&](dynamic_global_property_object &props) {
                        props.total_reward_fund_steem += unclaimed_rewards;
                    });

                    unclaimed_rewards = 0;
                }
                // Case: auction window destination is reward fund or there are not curator which can get the auw reward
                else if (cbl.auction_window_reward_destination != protocol::to_author && unclaimed_rewards > 0) {
                    push_virtual_operation(auction_window_reward_operation(asset(unclaimed_rewards, STEEM_SYMBOL), c.comment.author, c.comment.hashlink));
                    modify(get_dynamic_global_properties(), [&](dynamic_global_property_object &props) {
                        props.total_reward_fund_steem += asset(unclaimed_rewards, STEEM_SYMBOL);
                    });
                    unclaimed_rewards = 0;
                }

                return unclaimed_rewards;
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::cashout_comment_helper(const comment_object &comment) {
            protocol::curation_curve curve = comment.curation_reward_curve;
            try {
                const comment_bill_object* cblo = nullptr;
                if (comment.net_rshares > 0) {
                    cblo = find_comment_bill(comment.id);

                    uint128_t reward_tokens = uint128_t(
                         claim_rshare_reward(
                             comment.net_rshares,
                             comment.reward_weight,
                             to_steem(asset(cblo->bill.max_accepted_payout, SBD_SYMBOL))));

                    const account_object* author = nullptr;

                    bool should_pay = false;
                    if (reward_tokens > 0) {
                        author = &get_account(comment.author);
                        if (has_hardfork(STEEMIT_HARDFORK_0_27__204) && author->reputation < 0) {
                            modify(get_dynamic_global_properties(), [&](dynamic_global_property_object& props) {
                                props.total_reward_fund_steem += asset(reward_tokens.to_uint64(), STEEM_SYMBOL);
                            });
                        } else {
                            should_pay = true;
                        }
                    }

                    asset total_payout;
                    if (should_pay) {
                        share_type curation_tokens = ((reward_tokens *
                                                      cblo->bill.curation_rewards_percent) /
                                                      STEEMIT_100_PERCENT).to_uint64();

                        share_type author_tokens = reward_tokens.to_uint64() - curation_tokens;

                        share_type total_curator = 0;

                        comment_curation_info curation_info(*this, comment, false);
                        curve = curation_info.curve;
                        author_tokens += pay_curators(curation_info, curation_tokens, total_curator);

                        share_type total_beneficiary = 0;

                        for (auto &b : cblo->beneficiaries) {
                            auto benefactor_tokens = (author_tokens * b.weight) / STEEMIT_100_PERCENT;
                            auto vest_created = create_vesting(get_account(b.account), benefactor_tokens);
                            push_virtual_operation(
                                comment_benefactor_reward_operation(
                                    b.account, comment.author, comment.hashlink, vest_created, asset(benefactor_tokens, STEEM_SYMBOL)));
                            modify(get_account(b.account), [&](account_object& a) {
                                a.benefaction_rewards += benefactor_tokens;
                            });
                            total_beneficiary += benefactor_tokens;
                        }

                        author_tokens -= total_beneficiary;

                        auto sbd_steem = (author_tokens * cblo->bill.percent_steem_dollars) / (2 * STEEMIT_100_PERCENT);
                        auto vesting_steem = author_tokens - sbd_steem;

                        auto vest_created = create_vesting(*author, vesting_steem);
                        auto sbd_payout = create_sbd(*author, asset(0, STEEM_SYMBOL));
                        auto sbd_side_payout = create_sbd(*author, asset(0, STEEM_SYMBOL));
                        if (has_hardfork(STEEMIT_HARDFORK_0_23__84)) {
                            sbd_side_payout = create_sbd(get_account(STEEMIT_WORKER_POOL_ACCOUNT), sbd_steem);
                        } else {
                            sbd_payout = create_sbd(*author, sbd_steem);
                        }

                        // stats only.. TODO: Move to plugin...
                        total_payout = to_sbd(asset(reward_tokens.to_uint64(), STEEM_SYMBOL));
                        total_payout -= sbd_side_payout.first;

                        push_virtual_operation(author_reward_operation(comment.author, comment.hashlink, sbd_payout.first, sbd_payout.second, vest_created, asset(sbd_steem, STEEM_SYMBOL), asset(vesting_steem, STEEM_SYMBOL)));
                        push_virtual_operation(comment_reward_operation(comment.author, comment.hashlink, total_payout));

                        modify(get_account(comment.author), [&](account_object &a) {
                            a.posting_rewards += author_tokens;
                        });

                        auto author_golos = asset(author_tokens, STEEM_SYMBOL);
                        auto benefactor_golos = asset(total_beneficiary, STEEM_SYMBOL);
                        auto curator_golos = asset(total_curator, STEEM_SYMBOL);
                        push_virtual_operation(total_comment_reward_operation(comment.author, comment.hashlink, author_golos, benefactor_golos, curator_golos, comment.net_rshares.value));
                    }

                    fc::uint128_t old_rshares2 = calculate_vshares(comment.net_rshares.value);
                    adjust_rshares2(comment, old_rshares2, 0);
                }

                modify(comment, [&](comment_object &c) {
                    /**
                    * A payout is only made for positive rshares, negative rshares hang around
                    * for the next time this post might get an upvote.
                    */
                    if (c.net_rshares > 0) {
                        c.net_rshares = 0;
                    }
                    c.children_abs_rshares = 0;
                    c.abs_rshares = 0;
                    c.vote_rshares = 0;
                    c.max_cashout_time = fc::time_point_sec::maximum();
                    c.curation_reward_curve = curve;

                    if (has_hardfork(STEEMIT_HARDFORK_0_17__431)) {
                        c.cashout_time = fc::time_point_sec::maximum();
                    } else if (c.parent_author == STEEMIT_ROOT_POST_PARENT) {
                        if (c.last_payout == fc::time_point_sec::min()) {
                            c.cashout_time = head_block_time() + STEEMIT_SECOND_CASHOUT_WINDOW;
                        } else {
                            c.cashout_time = fc::time_point_sec::maximum();
                        }
                    }

                    if (calculate_discussion_payout_time(c) == fc::time_point_sec::maximum()) {
                        c.mode = archived;
                    } else {
                        c.mode = second_payout;
                    }

                    c.last_payout = head_block_time();
                });

                push_virtual_operation(comment_payout_update_operation(comment.author, comment.hashlink));

                if (comment.mode == archived) {
                    const auto& vote_idx = get_index<comment_vote_index>().indices().get<by_comment_voter>();
                    auto vote_itr = vote_idx.lower_bound(comment.id);
                    while (vote_itr != vote_idx.end() && vote_itr->comment == comment.id) {
                        const auto& cur_vote = *vote_itr;
                        ++vote_itr;
                        modify(cur_vote, [&](comment_vote_object& cvo) {
                            cvo.num_changes = (-1) - (cvo.num_changes); // mark vote that it's ready to be removed (archived comment)
                        });
                    }

                    if (!cblo) cblo = find_comment_bill(comment.id);
                    if (cblo) {
                        remove(*cblo);
                    }
                }
            } FC_CAPTURE_AND_RETHROW((comment.author)(comment.hashlink)(comment.created))
        }

        void database::process_comment_cashout() { try {
            /// don't allow any content to get paid out until the website is ready to launch
            /// and people have had a week to start posting. The first cashout will be the biggest because it
            /// will represent 2+ months of rewards.
//            if (!has_hardfork(STEEMIT_FIRST_CASHOUT_TIME)) {
//                return;
//            }

            if (head_block_time() <= STEEMIT_FIRST_CASHOUT_TIME) {
                return;
            }

            int count = 0;
            const auto &cidx = get_index<comment_index>().indices().get<by_cashout_time>();
            const auto &com_by_root = get_index<comment_index>().indices().get<by_root>();
            const bool has_hardfork_0_17__431 = has_hardfork(STEEMIT_HARDFORK_0_17__431);
            const auto block_time = head_block_time();

            auto current = cidx.begin();
            while (current != cidx.end() && current->cashout_time <= block_time) {
                if (has_hardfork_0_17__431) {
                    cashout_comment_helper(*current);
                } else {
                    auto itr = com_by_root.lower_bound(current->root_comment);
                    while (itr != com_by_root.end() && itr->root_comment == current->root_comment) {
                        const auto &comment = *itr;
                        ++itr;
                        cashout_comment_helper(comment);
                        ++count;
                    }
                }
                current = cidx.begin();
            }
        } FC_CAPTURE_AND_RETHROW() }

       /**
        *  At a start overall the network has an inflation rate of 15.15% of virtual golos per year.
        *  Each year the inflation rate is reduced by 0.42% and stops at 0.95% of virtual golos per year in 33 years.
        *
        *  Before HF22:
        *  66.67% of inflation is directed to content reward pool
        *  26.67% of inflation is directed to vesting fund
        *  6.66% of inflation is directed to witness pay
        *
        *  After HF22:
        *  15.00% of inflation (voted by witnesses) is directed to worker pay
        *  10.00% of inflation (voted by witnesses) is directed to witness pay
        *  10.00% of inflation (voted by witnesses) is directed to vesting fund
        *  65.00% of inflation is directed to content reward pool
        *
        *  This method pays out vesting, reward shares, witnesses and workers every block.
        */
        void database::process_funds() { try {
            const auto& wso = get_witness_schedule_object();
            const auto& props = get_dynamic_global_properties();
            const auto& cwit = get_witness(props.current_witness);
            const auto& wacc = get_account(cwit.owner);
            asset producer_reward;

            if (has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                /**
                 * Inflation rate starts from 15,15% and reduced to 0.95% at a rate of 0.01% every 250k blocks.
                 * At block 178,704,000 (16 years) have a 8.00% instantaneous inflation rate. This narrowing will take
                 * approximately 33 years and will complete on block 357,408,000.
                 */
                int64_t start_inflation_rate = int64_t(STEEMIT_INFLATION_RATE_START_PERCENT);
                int64_t inflation_rate_adjustment = int64_t(head_block_num() / STEEMIT_INFLATION_NARROWING_PERIOD);
                int64_t inflation_rate_floor = int64_t(STEEMIT_INFLATION_RATE_STOP_PERCENT);

                // below subtraction cannot underflow int64_t because inflation_rate_adjustment is <2^32
                int64_t current_inflation_rate =
                    std::max(start_inflation_rate - inflation_rate_adjustment,
                             inflation_rate_floor);

                auto new_steem =
                        (props.virtual_supply.amount * current_inflation_rate) /
                        (int64_t(STEEMIT_100_PERCENT) * int64_t(STEEMIT_BLOCKS_PER_YEAR));
                auto content_reward =
                        (new_steem * STEEMIT_CONTENT_REWARD_PERCENT_PRE_HF22) /
                        STEEMIT_100_PERCENT; /// 66.67% to content creator
                auto vesting_reward =
                        (new_steem * STEEMIT_VESTING_FUND_PERCENT_PRE_HF22) /
                        STEEMIT_100_PERCENT; /// 26.67% to vesting fund
                auto witness_reward = new_steem - content_reward - vesting_reward; /// Remaining 6.66% to witness pay

                share_type worker_reward = 0;
                if (has_hardfork(STEEMIT_HARDFORK_0_26__163)) {
                    worker_reward = new_steem * wso.median_props.worker_emission_percent / STEEMIT_100_PERCENT;
                    witness_reward = new_steem * uint16_t(GOLOS_WITNESS_EMISSION_PERCENT) / STEEMIT_100_PERCENT;
                    auto remain = new_steem - worker_reward - witness_reward;
                    vesting_reward = remain * wso.median_props.vesting_of_remain_percent / STEEMIT_100_PERCENT;
                    content_reward = remain - vesting_reward;
                } else if (has_hardfork(STEEMIT_HARDFORK_0_22__8)) {
                    worker_reward = new_steem * wso.median_props.worker_reward_percent / STEEMIT_100_PERCENT;
                    witness_reward = new_steem * wso.median_props.witness_reward_percent / STEEMIT_100_PERCENT;
                    vesting_reward = new_steem * wso.median_props.vesting_reward_percent / STEEMIT_100_PERCENT;
                    content_reward = new_steem - worker_reward - witness_reward - vesting_reward;
                }

                witness_reward *= STEEMIT_MAX_WITNESSES;

                if (cwit.schedule == witness_object::timeshare) {
                    witness_reward *= wso.timeshare_weight;
                } else if (cwit.schedule == witness_object::miner) {
                    witness_reward *= wso.miner_weight;
                } else if (cwit.schedule == witness_object::top19) {
                    witness_reward *= wso.top19_weight;
                } else {
                    wlog("Encountered unknown witness type for witness: ${w}", ("w", cwit.owner));
                }

                witness_reward /= wso.witness_pay_normalization_factor;

                new_steem = content_reward + vesting_reward + witness_reward;

                if (worker_reward != 0) {
                    new_steem += worker_reward;
                    adjust_balance(get_account(STEEMIT_WORKER_POOL_ACCOUNT), asset(worker_reward, STEEM_SYMBOL));
                }

                modify(props, [&](dynamic_global_property_object &p) {
                    if (has_hardfork(STEEMIT_HARDFORK_0_23__83)) {
                        p.accumulative_balance += asset(vesting_reward, STEEM_SYMBOL);
                        p.accumulative_emission_per_day = asset(vesting_reward, STEEM_SYMBOL) * STEEMIT_BLOCKS_PER_DAY;
                    } else {
                        p.total_vesting_fund_steem += asset(vesting_reward, STEEM_SYMBOL);
                    }
                    p.total_reward_fund_steem += asset(content_reward, STEEM_SYMBOL);
                    p.current_supply += asset(new_steem, STEEM_SYMBOL);
                    p.virtual_supply += asset(new_steem, STEEM_SYMBOL);
                });

                producer_reward = create_vesting(wacc, asset(witness_reward, STEEM_SYMBOL));
            } else {
                auto witness_pay = get_producer_reward();
                /// pay witness in vesting shares
                if (props.head_block_number >= STEEMIT_START_MINER_VOTING_BLOCK || (wacc.vesting_shares.amount == 0)) {
                    producer_reward = create_vesting(wacc, witness_pay);
                } else {
                    modify(wacc, [&](account_object &a) {
                        a.balance += witness_pay;
                    });
                }

                auto content_reward = get_content_reward();
                auto curate_reward = get_curation_reward();
                auto vesting_reward = content_reward + curate_reward + witness_pay;
                content_reward = content_reward + curate_reward;

                if (props.head_block_number < STEEMIT_START_VESTING_BLOCK) {
                    vesting_reward.amount = 0;
                } else {
                    vesting_reward.amount.value *= 9;
                }

                modify(props, [&](dynamic_global_property_object& p) {
                    p.total_vesting_fund_steem += vesting_reward;
                    p.total_reward_fund_steem += content_reward;
                    p.current_supply += content_reward + witness_pay + vesting_reward;
                    p.virtual_supply += content_reward + witness_pay + vesting_reward;
                });
            }

            if (producer_reward.amount.value)
                push_virtual_operation(producer_reward_operation(wacc.name, producer_reward));
        } FC_CAPTURE_AND_RETHROW() }

        std::pair<asset, asset> database::get_min_gp_to_emission() const {
            asset min_golos{0, STEEM_SYMBOL};
            asset min_golos_power_to_emission{0, VESTS_SYMBOL};
            if (has_hardfork(STEEMIT_HARDFORK_0_28__218)) {
                const auto& gbg_median = get_feed_history().current_median_history;
                if (!gbg_median.is_null()) {
                    auto min_gbg = get_witness_schedule_object().median_props.min_golos_power_to_emission;
                    min_golos = min_gbg * gbg_median;
                    min_golos_power_to_emission = min_golos * get_dynamic_global_properties().get_vesting_share_price();
                }
            }
            return std::make_pair(min_golos_power_to_emission, min_golos);
        }

        std::pair<asset, asset> database::get_min_gp_for_groups() const {
            asset min_golos{0, STEEM_SYMBOL};
            asset min_golos_power{0, VESTS_SYMBOL};
            if (has_hardfork(STEEMIT_HARDFORK_0_30)) {
                const auto& gbg_median = get_feed_history().current_median_history;
                if (!gbg_median.is_null()) {
                    auto min_gbg = get_witness_schedule_object().median_props.private_group_golos_power;
                    min_golos = min_gbg * gbg_median;
                    min_golos_power = min_golos * get_dynamic_global_properties().get_vesting_share_price();
                }
            }
            return std::make_pair(min_golos_power, min_golos);
        }

        void database::process_accumulative_distributions() { try {
            if (!has_hardfork(STEEMIT_HARDFORK_0_23__83)) return;

            const auto& props = get_dynamic_global_properties();

            bool has_hf27 = has_hardfork(STEEMIT_HARDFORK_0_27__202);
            if (has_hf27) {
                if (head_block_num() % GOLOS_ACCUM_DISTRIBUTION_INTERVAL != 0) return;
            } else {
                if (head_block_num() % GOLOS_ACCUM_DISTRIBUTION_INTERVAL_HF26 != 0) return;
            }

            bool has_hf28 = has_hardfork(STEEMIT_HARDFORK_0_28__218);
            auto min_golos_power_to_emission = get_min_gp_to_emission().first;

            auto distributed_sum = asset(0, STEEM_SYMBOL);

            auto process_acc = [&](const auto& acc) { try {
                auto stake = asset((uint128_t(props.accumulative_balance.amount.value) * acc.emission_vesting_shares().amount.value / props.total_vesting_shares.amount.value).to_uint64(), STEEM_SYMBOL);
                modify(acc, [&](auto& a) {
                    if (has_hf27 && (!has_hf28 || acc.vesting_shares >= min_golos_power_to_emission)) {
                        a.tip_balance += stake;
                    } else {
                        a.accumulative_balance += stake;
                    }
                });
                distributed_sum += stake;
            } FC_CAPTURE_AND_RETHROW((acc.name)) };

            if (!has_hf27) {
                const auto& acc_idx = get_index<account_index, by_vesting_shares>();
                auto acc_itr = acc_idx.upper_bound(asset(0, VESTS_SYMBOL));
                for (; acc_itr != acc_idx.end(); acc_itr++) {
                    process_acc(*acc_itr);
                }
            } else {
                const auto& acc_idx = get_index<account_index, by_proved>();
                auto acc_itr = acc_idx.begin();
                for (; acc_itr != acc_idx.end() && !acc_itr->frozen; acc_itr++) {
                    if (acc_itr->emission_vesting_shares().amount == 0) continue;
                    process_acc(*acc_itr);
                }
            }

            modify(props, [&](auto& props) {
                props.accumulative_remainder += (props.accumulative_balance - distributed_sum);
                props.accumulative_balance = asset(0, STEEM_SYMBOL);
            });

            if (props.accumulative_remainder.amount.value) {
                push_virtual_operation(accumulative_remainder_operation(props.accumulative_remainder));
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::auto_claim_accumulatives() { try {
            if (!has_hardfork(STEEMIT_HARDFORK_0_27__202)) return;
            if (has_hardfork(STEEMIT_HARDFORK_0_28__218)) return;

            const auto& acc_idx = get_index<account_index, by_accumulative>();
            auto acc_itr = acc_idx.begin();
            auto i = 0;
            for (; acc_itr != acc_idx.end() && acc_itr->accumulative_balance.amount != 0 && i < 100;
                ++i) {
                const auto& acc = *acc_itr;
                ++acc_itr;
                modify(acc, [&](auto& acnt) {
                    acnt.tip_balance += acnt.accumulative_balance;
                    acnt.accumulative_balance.amount = 0;
                });
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::process_savings_withdraws() { try {
            const auto &idx = get_index<savings_withdraw_index>().indices().get<by_complete_from_rid>();
            auto itr = idx.begin();
            while (itr != idx.end()) {
                if (itr->complete > head_block_time()) {
                    break;
                }
                adjust_balance(get_account(itr->to), itr->amount);

                modify(get_account(itr->from), [&](account_object &a) {
                    a.savings_withdraw_requests--;
                });

                push_virtual_operation(fill_transfer_from_savings_operation(itr->from, itr->to, itr->amount, itr->request_id, to_string(itr->memo)));

                remove(*itr);
                itr = idx.begin();
            }
        } FC_CAPTURE_AND_RETHROW() }

        asset database::get_liquidity_reward() const {
            if (has_hardfork(STEEMIT_HARDFORK_0_12__178)) {
                return asset(0, STEEM_SYMBOL);
            }

            const auto &props = get_dynamic_global_properties();
            static_assert(STEEMIT_LIQUIDITY_REWARD_PERIOD_SEC ==
                          60 * 60, "this code assumes a 1 hour time interval");
            asset percent(protocol::calc_percent_reward_per_hour<STEEMIT_LIQUIDITY_APR_PERCENT>(props.virtual_supply.amount), STEEM_SYMBOL);
            return std::max(percent, STEEMIT_MIN_LIQUIDITY_REWARD);
        }

        asset database::get_content_reward() const {
            const auto &props = get_dynamic_global_properties();
            auto reward = asset(255, STEEM_SYMBOL);
            static_assert(STEEMIT_BLOCK_INTERVAL ==
                          3, "this code assumes a 3-second time interval");
            if (props.head_block_number > STEEMIT_START_VESTING_BLOCK) {
                asset percent(protocol::calc_percent_reward_per_block<STEEMIT_CONTENT_APR_PERCENT>(props.virtual_supply.amount), STEEM_SYMBOL);
                reward = std::max(percent, STEEMIT_MIN_CONTENT_REWARD);
            }

            return reward;
        }

        asset database::get_curation_reward() const {
            const auto &props = get_dynamic_global_properties();
            auto reward = asset(85, STEEM_SYMBOL);
            static_assert(STEEMIT_BLOCK_INTERVAL ==
                          3, "this code assumes a 3-second time interval");
            if (props.head_block_number > STEEMIT_START_VESTING_BLOCK) {
                asset percent(protocol::calc_percent_reward_per_block<STEEMIT_CURATE_APR_PERCENT>(props.virtual_supply.amount), STEEM_SYMBOL);
                reward = std::max(percent, STEEMIT_MIN_CURATE_REWARD);
            }

            return reward;
        }

        asset database::get_producer_reward() const {
            static_assert(STEEMIT_BLOCK_INTERVAL == 3, "this code assumes a 3-second time interval");
            const auto& props = get_dynamic_global_properties();
            asset percent(protocol::calc_percent_reward_per_block<STEEMIT_PRODUCER_APR_PERCENT>(props.virtual_supply.amount), STEEM_SYMBOL);

            // why this needed? the method called only before hf16
            auto min_reward = has_hardfork(STEEMIT_HARDFORK_0_16) ? STEEMIT_MIN_PRODUCER_REWARD : STEEMIT_MIN_PRODUCER_REWARD_PRE_HF_16;
            auto pay = std::max(percent, min_reward);
            return pay;
        }

        asset database::get_pow_reward() const {
            const auto &props = get_dynamic_global_properties();

#ifndef STEEMIT_BUILD_TESTNET
            /// 0 block rewards until at least STEEMIT_MAX_WITNESSES have produced a POW
            if (props.num_pow_witnesses < STEEMIT_MAX_WITNESSES &&
                props.head_block_number < STEEMIT_START_VESTING_BLOCK) {
                return asset(0, STEEM_SYMBOL);
            }
#endif

            static_assert(STEEMIT_BLOCK_INTERVAL ==
                          3, "this code assumes a 3-second time interval");
//            static_assert(STEEMIT_MAX_WITNESSES ==
//                          21, "this code assumes 21 per round");
            asset percent(calc_percent_reward_per_round<STEEMIT_POW_APR_PERCENT>(props.virtual_supply.amount), STEEM_SYMBOL);

            if (has_hardfork(STEEMIT_HARDFORK_0_16)) {
                return std::max(percent, STEEMIT_MIN_POW_REWARD);
            } else {
                return std::max(percent, STEEMIT_MIN_POW_REWARD_PRE_HF_16);
            }
        }


        void database::pay_liquidity_reward() { try {
#ifdef STEEMIT_BUILD_TESTNET
            if (!liquidity_rewards_enabled) {
                return;
            }
#endif

            if ((head_block_num() % STEEMIT_LIQUIDITY_REWARD_BLOCKS) == 0) {
                auto reward = get_liquidity_reward();

                if (reward.amount == 0) {
                    return;
                }

                const auto &ridx = get_index<liquidity_reward_balance_index>().indices().get<by_volume_weight>();
                auto itr = ridx.begin();
                if (itr != ridx.end() && itr->volume_weight() > 0) {
                    adjust_supply(reward, true);
                    adjust_balance(get(itr->owner), reward);
                    modify(*itr, [&](liquidity_reward_balance_object &obj) {
                        obj.steem_volume = 0;
                        obj.sbd_volume = 0;
                        obj.last_update = head_block_time();
                        obj.weight = 0;
                    });

                    push_virtual_operation(liquidity_reward_operation(get(itr->owner).name, reward));
                }
            }
        } FC_CAPTURE_AND_RETHROW() }

        uint128_t database::get_content_constant_s() const {
            return uint128_t(uint64_t(2000000000000ll)); // looking good for posters
        }

        inline uint128_t calculate_vshares_linear(uint128_t rshares) {
            return rshares;
        }

        inline const uint128_t calculate_vshares_quadratic(uint128_t rshares, uint128_t s) {
            return (rshares + s) * (rshares + s) - s * s;
        }

        uint128_t database::calculate_vshares(uint128_t rshares) const {
            if (has_hardfork(STEEMIT_HARDFORK_0_17__433)) {
                return calculate_vshares_linear(rshares);
            } else {
                return calculate_vshares_quadratic(rshares, get_content_constant_s());
            }
        }

/**
 *  Iterates over all conversion requests with a conversion date before
 *  the head block time and then converts them to/from steem/sbd at the
 *  current median price feed history price times the premium
 */
        void database::process_conversions() { try {
            auto now = head_block_time();
            const auto& request_by_date = get_index<convert_request_index, by_conversion_date>();
            auto itr = request_by_date.begin();

            const auto& fhistory = get_feed_history();
            if (fhistory.current_median_history.is_null()) {
                return;
            }

            asset net_sbd(0, SBD_SYMBOL);
            asset net_steem(0, STEEM_SYMBOL);

            while (itr != request_by_date.end() &&
                   itr->conversion_date <= now) {
                const auto& user = get_account(itr->owner);

                auto clean_amount = itr->amount - itr->fee;

                auto amount_to_issue =
                        clean_amount * fhistory.current_median_history;

                adjust_balance(user, amount_to_issue);

                if (itr->amount.symbol == STEEM_SYMBOL) {
                    net_sbd -= amount_to_issue;
                    net_steem -= itr->amount;
                } else {
                    net_sbd += itr->amount;
                    net_steem += amount_to_issue;
                }

                push_virtual_operation(fill_convert_request_operation(user.name, itr->requestid, itr->amount, amount_to_issue, itr->fee));

                remove(*itr);
                itr = request_by_date.begin();
            }

            auto converted_sbd = asset(0, STEEM_SYMBOL);
            if (net_sbd.amount < 0) {
                converted_sbd = (-net_sbd) * fhistory.current_median_history;
                converted_sbd = -converted_sbd;
            } else {
                converted_sbd = net_sbd * fhistory.current_median_history;
            }

            const auto& props = get_dynamic_global_properties();
            modify(props, [&](dynamic_global_property_object& p) {
                p.current_supply += net_steem;
                p.current_sbd_supply -= net_sbd;
                p.virtual_supply += net_steem;
                p.virtual_supply -= converted_sbd;
            });
        } FC_CAPTURE_AND_RETHROW() }

        void database::process_sbd_debt_conversions() { try {
            if (!has_hardfork(STEEMIT_HARDFORK_0_22__64) || (head_block_num() % STEEMIT_SBD_DEBT_CONVERT_INTERVAL != 0)) return;

            const auto& median_props = get_witness_schedule_object().median_props;
            if (!median_props.sbd_debt_convert_rate) return;

            const auto& median_history = get_feed_history().current_median_history;
            if (median_history.is_null()) return;

            const auto& props = get_dynamic_global_properties();
            if (props.sbd_debt_percent < STEEMIT_SBD_DEBT_CONVERT_THRESHOLD) return;

            if (has_hardfork(STEEMIT_HARDFORK_0_26__159)) {
                const auto& orders = get_index<limit_order_index, by_symbol>();
                for (auto itr = orders.find(SBD_SYMBOL);
                        itr != orders.end() && itr->symbol == SBD_SYMBOL; ) {
                    const auto& val = *itr;
                    ++itr;
                    cancel_order(val);
                }
            } else {
                const auto& orders = get_index<limit_order_index>().indices();
                for (auto itr = orders.begin(); itr != orders.end(); ) {
                    cancel_order(*itr);
                    itr = orders.begin();
                }
            }

            asset net_sbd(0, SBD_SYMBOL);
            asset net_steem(0, STEEM_SYMBOL);

            std::set<account_name_type> processed;

            const auto& idx = get_index<account_index, by_sbd>();
            for (auto itr = idx.begin(); itr != idx.end(); ) {
                const auto& acc = *itr;
                if (!acc.sbd_balance.amount.value && !acc.savings_sbd_balance.amount.value) break;
                ++itr;

                if (!processed.insert(acc.name).second) continue;

                auto sbd = asset(
                    (uint128_t(acc.sbd_balance.amount) * median_props.sbd_debt_convert_rate / STEEMIT_100_PERCENT).to_uint64(), SBD_SYMBOL);
                auto savings_sbd = asset(
                    (uint128_t(acc.savings_sbd_balance.amount) * median_props.sbd_debt_convert_rate / STEEMIT_100_PERCENT).to_uint64(), SBD_SYMBOL);
                if (!sbd.amount.value && !savings_sbd.amount.value) continue;

                asset steem(0, STEEM_SYMBOL);
                if (sbd.amount.value) {
                    auto steem = sbd * median_history;
                    adjust_balance(acc, -sbd);
                    adjust_balance(acc, steem);
                    net_sbd += sbd;
                    net_steem += steem;
                }

                asset savings_steem(0, STEEM_SYMBOL);
                if (savings_sbd.amount.value) {
                    auto savings_steem = savings_sbd * median_history;
                    adjust_savings_balance(acc, -savings_sbd);
                    adjust_savings_balance(acc, savings_steem);
                    net_sbd += savings_sbd;
                    net_steem += savings_steem;
                }

                push_virtual_operation(convert_sbd_debt_operation(acc.name, sbd, steem, savings_sbd, savings_steem));
            }

            const auto& escrows = get_index<escrow_index>().indices();
            for (auto itr = escrows.begin(); itr != escrows.end(); ++itr) {
                if (!itr->sbd_balance.amount.value && (itr->pending_fee.symbol != SBD_SYMBOL || !itr->pending_fee.amount.value)) continue;

                if (itr->sbd_balance.amount.value) {
                    auto sbd = asset(
                        (uint128_t(itr->sbd_balance.amount) * median_props.sbd_debt_convert_rate / STEEMIT_100_PERCENT).to_uint64(), SBD_SYMBOL);
                    auto steem = sbd * median_history;

                    modify(*itr, [&](auto& e) {
                        e.sbd_balance -= sbd;
                        e.steem_balance += steem;
                    });

                    net_sbd += sbd;
                    net_steem += steem;
                }

                if (itr->pending_fee.symbol == SBD_SYMBOL && itr->pending_fee.amount.value) {
                    auto fee_sbd = asset(
                        (uint128_t(itr->pending_fee.amount) * median_props.sbd_debt_convert_rate / STEEMIT_100_PERCENT).to_uint64(), SBD_SYMBOL);
                    auto fee_steem = fee_sbd * median_history;

                    modify(*itr, [&](auto& e) {
                        e.pending_fee -= fee_sbd;
                    });

                    adjust_balance(get_account(itr->agent), fee_steem);

                    net_sbd += fee_sbd;
                    net_steem += fee_steem;
                }
            }

            const auto& withdraws = get_index<savings_withdraw_index>().indices();
            for (auto itr = withdraws.begin(); itr != withdraws.end(); ++itr) {
                if (itr->amount.symbol != SBD_SYMBOL || !itr->amount.amount.value) continue;

                auto sbd = asset(
                    (uint128_t(itr->amount.amount) * median_props.sbd_debt_convert_rate / STEEMIT_100_PERCENT).to_uint64(), SBD_SYMBOL);
                auto steem = sbd * median_history;

                adjust_balance(get_account(itr->to), steem);

                if (itr->amount == sbd) {
                    remove(*itr);

                    modify(get_account(itr->from), [&](account_object& a) {
                        a.savings_withdraw_requests--;
                    });
                } else {
                    modify(*itr, [&](auto& w) {
                        w.amount -= sbd;
                    });
                }

                net_sbd += sbd;
                net_steem += steem;
            }

            modify(props, [&](auto& p) {
                p.current_supply += net_steem;
                p.current_sbd_supply -= net_sbd;
                p.virtual_supply += net_steem;
                p.virtual_supply -= net_sbd * median_history;
            });
        } FC_CAPTURE_AND_RETHROW() }

        asset database::to_sbd(const asset &steem) const {
            FC_ASSERT(steem.symbol == STEEM_SYMBOL);
            const auto &feed_history = get_feed_history();
            if (feed_history.current_median_history.is_null()) {
                return asset(0, SBD_SYMBOL);
            }

            return steem * feed_history.current_median_history;
        }

        asset database::to_steem(const asset &sbd) const {
            FC_ASSERT(sbd.symbol == SBD_SYMBOL);
            const auto &feed_history = get_feed_history();
            if (feed_history.current_median_history.is_null()) {
                return asset(0, STEEM_SYMBOL);
            }

            return sbd * feed_history.current_median_history;
        }

/**
 *  This method reduces the rshare^2 supply and returns the number of tokens are
 *  redeemed.
 */
        share_type database::claim_rshare_reward(share_type rshares, uint16_t reward_weight, asset max_steem) {
        try {
                FC_ASSERT(rshares > 0);

                const auto &props = get_dynamic_global_properties();

                u256 rs(rshares.value);
                u256 rf(props.total_reward_fund_steem.amount.value);
                u256 total_rshares2 = to256(props.total_reward_shares2);

                u256 rs2 = to256(calculate_vshares(rshares.value));
                rs2 = (rs2 * reward_weight) / STEEMIT_100_PERCENT;

                u256 payout_u256 = (rf * rs2) / total_rshares2;
                FC_ASSERT(payout_u256 <=
                          u256(uint64_t(std::numeric_limits<int64_t>::max())));
                uint64_t payout = static_cast< uint64_t >( payout_u256 );

                asset sbd_payout_value = to_sbd(asset(payout, STEEM_SYMBOL));

                if (sbd_payout_value < STEEMIT_MIN_PAYOUT_SBD) {
                    payout = 0;
                }

                payout = std::min(payout, uint64_t(max_steem.amount.value));

                modify(props, [&](dynamic_global_property_object &p) {
                    p.total_reward_fund_steem.amount -= payout;
                });

                return payout;
            } FC_CAPTURE_AND_RETHROW((rshares)(max_steem))
        }

        void database::account_recovery_processing() { try {
            // Clear expired recovery requests
            const auto& rec_req_idx = get_index<account_recovery_request_index>().indices().get<by_expiration>();
            auto rec_req = rec_req_idx.begin();

            while (rec_req != rec_req_idx.end() && rec_req->expires <= head_block_time()) {
                remove(*rec_req);
                rec_req = rec_req_idx.begin();
            }

            // Clear invalid historical authorities
            const auto& hist_idx = get_index<owner_authority_history_index>().indices(); //by id
            auto hist = hist_idx.begin();

            while (
                hist != hist_idx.end() &&
                time_point_sec(hist->last_valid_time + STEEMIT_OWNER_AUTH_RECOVERY_PERIOD) < head_block_time()
            ) {
                remove(*hist);
                hist = hist_idx.begin();
            }

            // Apply effective recovery_account changes
            const auto& change_req_idx = get_index<change_recovery_account_request_index>().indices().get<by_effective_date>();
            auto change_req = change_req_idx.begin();

            while (change_req != change_req_idx.end() && change_req->effective_on <= head_block_time()) {
                modify(get_account(change_req->account_to_recover), [&](account_object& a) {
                    a.recovery_account = change_req->recovery_account;
                });
                remove(*change_req);
                change_req = change_req_idx.begin();
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::expire_escrow_ratification() { try {
            const auto &escrow_idx = get_index<escrow_index>().indices().get<by_ratification_deadline>();
            auto escrow_itr = escrow_idx.lower_bound(false);

            while (escrow_itr != escrow_idx.end() &&
                   !escrow_itr->is_approved() &&
                   escrow_itr->ratification_deadline <= head_block_time()) {
                const auto &old_escrow = *escrow_itr;
                ++escrow_itr;

                const auto &from_account = get_account(old_escrow.from);
                adjust_balance(from_account, old_escrow.steem_balance);
                adjust_balance(from_account, old_escrow.sbd_balance);
                adjust_balance(from_account, old_escrow.pending_fee);

                remove(old_escrow);
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::process_decline_voting_rights() { try {
            const auto &request_idx = get_index<decline_voting_rights_request_index>().indices().get<by_effective_date>();
            auto itr = request_idx.begin();

            while (itr != request_idx.end() &&
                   itr->effective_date <= head_block_time()) {
                const auto &account = get(itr->account);

                /// remove all current votes
                std::array<share_type,
                        STEEMIT_MAX_PROXY_RECURSION_DEPTH + 1> delta;
                delta[0] = -account.vesting_shares.amount;
                for (int i = 0; i < STEEMIT_MAX_PROXY_RECURSION_DEPTH; ++i) {
                    delta[i + 1] = -account.proxied_vsf_votes[i];
                }
                adjust_proxied_witness_votes(account, delta);

                clear_witness_votes(account);

                modify(get(itr->account), [&](account_object &a) {
                    a.can_vote = false;
                    a.proxy = STEEMIT_PROXY_TO_SELF_ACCOUNT;
                });

                remove(*itr);
                itr = request_idx.begin();
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::fix_recovery_accounts() {
            const auto& idx = get_index<account_index, by_proved>();
            auto itr = idx.begin();
            for (; itr != idx.end() && !itr->frozen; ++itr) {
                if (itr->recovery_account == "gc-regfund") {
                    modify(*itr, [&](auto& acc) {
                        acc.recovery_account = STEEMIT_RECOVERY_ACCOUNT;
                    });
                }
            }
        }

        void database::process_account_freezing() { try {
            if (!has_hardfork(STEEMIT_HARDFORK_0_27__203)) return;

            freezing_utils fru(*this);
            if (fru.hf_long_ago) {
                return;
            }

            auto now = head_block_time();

            const auto& idx = get_index<account_index, by_proved>();
            auto itr = idx.begin();
            auto i = 0;
            for (; itr != idx.end() && !itr->frozen && itr->proved_hf < fru.hardfork && i < 100; ++i) {
                const auto& acc = *itr;
                ++itr;

                bool inactive = fru.is_inactive(acc);

                modify(acc, [&](auto& a) {
                    a.proved_hf = fru.hardfork;
                    a.frozen = inactive;
                });

                if (!inactive) continue;

                auto& auth = get_authority(acc.name);
                create<account_freeze_object>([&](auto& r) {
                    r.account = acc.name;
                    r.owner = auth.owner;
                    r.active = auth.active;
                    r.posting = auth.posting;
                    r.frozen = now;
                    r.hardfork = fru.hardfork;
                });

                modify(auth, [&](auto& auth) {
                    auth.posting = authority();
                    auth.active = authority();
                    auth.owner = authority();

                    auth.posting.weight_threshold = 1;
                    auth.active.weight_threshold = 1;
                    auth.owner.weight_threshold = 1;
                });
            }

            if (fru.hf_ago_ended_now) {
                fix_recovery_accounts();
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::process_gbg_payments() { try {
            if (!has_hardfork(STEEMIT_HARDFORK_0_28__219)) return;

            if (head_block_num() % (STEEMIT_SBD_INTEREST_COMPOUND_INTERVAL_SEC / 3) != 0) {
                return;
            }

            const auto& bidx = get_index<account_index, by_savings>();

            for (auto itr = bidx.begin();
                    itr != bidx.end() && itr->savings_sbd_balance.amount > 0; ) {
                const auto& acc = *itr;
                ++itr;

                modify(acc, [&](auto& acc) {
                    acc.savings_sbd_seconds +=
                            fc::uint128_t(acc.savings_sbd_balance.amount.value) *
                            (head_block_time() -
                             acc.savings_sbd_seconds_last_update).to_seconds();
                    acc.savings_sbd_seconds_last_update = head_block_time();
                });
            }

            const auto& idx = get_index<account_index, by_savings_seconds>();

            for (auto itr = idx.begin(); itr != idx.end() && itr->savings_sbd_seconds > 0; ) {
                const auto& acc = *itr;
                ++itr;

                auto min_vests = get_min_gp_to_emission().first;
                if (acc.vesting_shares < min_vests) {
                    continue;
                }

                modify(acc, [&](auto& acc) {
                    pay_savings_interest(acc);
                });
            }
        } FC_CAPTURE_AND_RETHROW() }

        time_point_sec database::head_block_time() const {
            return get_dynamic_global_properties().time;
        }

        uint32_t database::head_block_num() const {
            return get_dynamic_global_properties().head_block_number;
        }

        block_id_type database::head_block_id() const {
            return get_dynamic_global_properties().head_block_id;
        }

        uint32_t database::last_non_undoable_block_num() const {
            return get_dynamic_global_properties().last_irreversible_block_num;
        }

        void database::initialize_evaluators() {
            _my->_evaluator_registry.register_evaluator<vote_evaluator>();
            _my->_evaluator_registry.register_evaluator<comment_evaluator>();
            _my->_evaluator_registry.register_evaluator<comment_options_evaluator>();
            _my->_evaluator_registry.register_evaluator<delete_comment_evaluator>();
            _my->_evaluator_registry.register_evaluator<transfer_evaluator>();
            _my->_evaluator_registry.register_evaluator<transfer_to_vesting_evaluator>();
            _my->_evaluator_registry.register_evaluator<withdraw_vesting_evaluator>();
            _my->_evaluator_registry.register_evaluator<set_withdraw_vesting_route_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_create_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_update_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_metadata_evaluator>();
            _my->_evaluator_registry.register_evaluator<witness_update_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_witness_vote_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_witness_proxy_evaluator>();
            _my->_evaluator_registry.register_evaluator<custom_evaluator>();
            _my->_evaluator_registry.register_evaluator<custom_binary_evaluator>();
            _my->_evaluator_registry.register_evaluator<custom_json_evaluator>();
            _my->_evaluator_registry.register_evaluator<pow_evaluator>();
            _my->_evaluator_registry.register_evaluator<pow2_evaluator>();
            _my->_evaluator_registry.register_evaluator<report_over_production_evaluator>();
            _my->_evaluator_registry.register_evaluator<feed_publish_evaluator>();
            _my->_evaluator_registry.register_evaluator<convert_evaluator>();
            _my->_evaluator_registry.register_evaluator<limit_order_create_evaluator>();
            _my->_evaluator_registry.register_evaluator<limit_order_create2_evaluator>();
            _my->_evaluator_registry.register_evaluator<limit_order_cancel_evaluator>();
            _my->_evaluator_registry.register_evaluator<challenge_authority_evaluator>();
            _my->_evaluator_registry.register_evaluator<prove_authority_evaluator>();
            _my->_evaluator_registry.register_evaluator<request_account_recovery_evaluator>();
            _my->_evaluator_registry.register_evaluator<recover_account_evaluator>();
            _my->_evaluator_registry.register_evaluator<change_recovery_account_evaluator>();
            _my->_evaluator_registry.register_evaluator<escrow_transfer_evaluator>();
            _my->_evaluator_registry.register_evaluator<escrow_approve_evaluator>();
            _my->_evaluator_registry.register_evaluator<escrow_dispute_evaluator>();
            _my->_evaluator_registry.register_evaluator<escrow_release_evaluator>();
            _my->_evaluator_registry.register_evaluator<transfer_to_savings_evaluator>();
            _my->_evaluator_registry.register_evaluator<transfer_from_savings_evaluator>();
            _my->_evaluator_registry.register_evaluator<cancel_transfer_from_savings_evaluator>();
            _my->_evaluator_registry.register_evaluator<decline_voting_rights_evaluator>();
            _my->_evaluator_registry.register_evaluator<reset_account_evaluator>();
            _my->_evaluator_registry.register_evaluator<set_reset_account_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_create_with_delegation_evaluator>();
            _my->_evaluator_registry.register_evaluator<delegate_vesting_shares_evaluator>();
            _my->_evaluator_registry.register_evaluator<delegate_vesting_shares_with_interest_evaluator>();
            _my->_evaluator_registry.register_evaluator<reject_vesting_shares_delegation_evaluator>();
            _my->_evaluator_registry.register_evaluator<transit_to_cyberway_evaluator>();
            _my->_evaluator_registry.register_evaluator<proposal_create_evaluator>();
            _my->_evaluator_registry.register_evaluator<proposal_update_evaluator>();
            _my->_evaluator_registry.register_evaluator<proposal_delete_evaluator>();
            _my->_evaluator_registry.register_evaluator<chain_properties_update_evaluator>();
            _my->_evaluator_registry.register_evaluator<break_free_referral_evaluator>();
            _my->_evaluator_registry.register_evaluator<worker_request_evaluator>();
            _my->_evaluator_registry.register_evaluator<worker_request_delete_evaluator>();
            _my->_evaluator_registry.register_evaluator<worker_request_vote_evaluator>();
            _my->_evaluator_registry.register_evaluator<claim_evaluator>();
            _my->_evaluator_registry.register_evaluator<donate_evaluator>();
            _my->_evaluator_registry.register_evaluator<transfer_from_tip_evaluator>();
            _my->_evaluator_registry.register_evaluator<transfer_to_tip_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_create_with_invite_evaluator>();
            _my->_evaluator_registry.register_evaluator<invite_evaluator>();
            _my->_evaluator_registry.register_evaluator<invite_claim_evaluator>();
            _my->_evaluator_registry.register_evaluator<asset_create_evaluator>();
            _my->_evaluator_registry.register_evaluator<asset_update_evaluator>();
            _my->_evaluator_registry.register_evaluator<asset_issue_evaluator>();
            _my->_evaluator_registry.register_evaluator<asset_transfer_evaluator>();
            _my->_evaluator_registry.register_evaluator<override_transfer_evaluator>();
            _my->_evaluator_registry.register_evaluator<invite_donate_evaluator>();
            _my->_evaluator_registry.register_evaluator<invite_transfer_evaluator>();
            _my->_evaluator_registry.register_evaluator<limit_order_cancel_ex_evaluator>();
            _my->_evaluator_registry.register_evaluator<account_setup_evaluator>();
            _my->_evaluator_registry.register_evaluator<paid_subscription_create_evaluator>();
            _my->_evaluator_registry.register_evaluator<paid_subscription_update_evaluator>();
            _my->_evaluator_registry.register_evaluator<paid_subscription_delete_evaluator>();
            _my->_evaluator_registry.register_evaluator<paid_subscription_transfer_evaluator>();
            _my->_evaluator_registry.register_evaluator<paid_subscription_cancel_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_collection_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_collection_delete_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_issue_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_transfer_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_sell_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_buy_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_cancel_order_evaluator>();
            _my->_evaluator_registry.register_evaluator<nft_auction_evaluator>();
        }

        void database::set_custom_operation_interpreter(const std::string &id, std::shared_ptr<custom_operation_interpreter> registry) {
            bool inserted = _custom_operation_interpreters.emplace(id, registry).second;
            // This assert triggering means we're mis-configured (multiple registrations of custom JSON evaluator for same ID)
            FC_ASSERT(inserted);
        }

        std::shared_ptr<custom_operation_interpreter> database::get_custom_json_evaluator(const std::string &id) {
            auto it = _custom_operation_interpreters.find(id);
            if (it != _custom_operation_interpreters.end()) {
                return it->second;
            }
            return std::shared_ptr<custom_operation_interpreter>();
        }

        void database::initialize_indexes() {
            add_core_index<dynamic_global_property_index>(*this);
            add_core_index<account_index>(*this);
            add_core_index<account_authority_index>(*this);
            add_core_index<account_freeze_index>(*this);
            add_core_index<account_bandwidth_index>(*this);
            add_core_index<witness_index>(*this);
            add_core_index<transaction_index>(*this);
            add_core_index<block_summary_index>(*this);
            add_core_index<witness_schedule_index>(*this);
            add_core_index<comment_index>(*this);
            add_core_index<comment_app_index>(*this);
            add_core_index<comment_bill_index>(*this);
            add_core_index<comment_extras_index>(*this);
            add_core_index<comment_vote_index>(*this);
            add_core_index<witness_vote_index>(*this);
            add_core_index<limit_order_index>(*this);
            add_core_index<feed_history_index>(*this);
            add_core_index<convert_request_index>(*this);
            add_core_index<liquidity_reward_balance_index>(*this);
            add_core_index<hardfork_property_index>(*this);
            add_core_index<withdraw_vesting_route_index>(*this);
            add_core_index<owner_authority_history_index>(*this);
            add_core_index<account_recovery_request_index>(*this);
            add_core_index<change_recovery_account_request_index>(*this);
            add_core_index<escrow_index>(*this);
            add_core_index<savings_withdraw_index>(*this);
            add_core_index<decline_voting_rights_request_index>(*this);
            add_core_index<vesting_delegation_index>(*this);
            add_core_index<vesting_delegation_expiration_index>(*this);
            add_core_index<account_metadata_index>(*this);
            add_core_index<proposal_index>(*this);
            add_core_index<required_approval_index>(*this);
            add_core_index<worker_request_index>(*this);
            add_core_index<worker_request_vote_index>(*this);
            add_core_index<donate_index>(*this);
            add_core_index<invite_index>(*this);
            add_core_index<asset_index>(*this);
            add_core_index<market_pair_index>(*this);
            add_core_index<fix_me_index>(*this);
            add_core_index<account_balance_index>(*this);
            add_core_index<event_index>(*this);
            add_core_index<account_blocking_index>(*this);
            add_core_index<paid_subscription_index>(*this);
            add_core_index<paid_subscriber_index>(*this);
            add_core_index<nft_collection_index>(*this);
            add_core_index<nft_index>(*this);
            add_core_index<nft_order_index>(*this);
            add_core_index<nft_bet_index>(*this);

            _plugin_index_signal();
        }

        const std::string &database::get_json_schema() const {
            return _json_schema;
        }

        void database::init_schema() {
            /*done_adding_indexes();

   db_schema ds;

   std::vector< std::shared_ptr< abstract_schema > > schema_list;

   std::vector< object_schema > object_schemas;
   get_object_schemas( object_schemas );

   for( const object_schema& oschema : object_schemas )
   {
      ds.object_types.emplace_back();
      ds.object_types.back().space_type.first = oschema.space_id;
      ds.object_types.back().space_type.second = oschema.type_id;
      oschema.schema->get_name( ds.object_types.back().type );
      schema_list.push_back( oschema.schema );
   }

   std::shared_ptr< abstract_schema > operation_schema = get_schema_for_type< operation >();
   operation_schema->get_name( ds.operation_type );
   schema_list.push_back( operation_schema );

   for( const std::pair< std::string, std::shared_ptr< custom_operation_interpreter > >& p : _custom_operation_interpreters )
   {
      ds.custom_operation_types.emplace_back();
      ds.custom_operation_types.back().id = p.first;
      schema_list.push_back( p.second->get_operation_schema() );
      schema_list.back()->get_name( ds.custom_operation_types.back().type );
   }

   golos::db::add_dependent_schemas( schema_list );
   std::sort( schema_list.begin(), schema_list.end(),
      []( const std::shared_ptr< abstract_schema >& a,
          const std::shared_ptr< abstract_schema >& b )
      {
         return a->id < b->id;
      } );
   auto new_end = std::unique( schema_list.begin(), schema_list.end(),
      []( const std::shared_ptr< abstract_schema >& a,
          const std::shared_ptr< abstract_schema >& b )
      {
         return a->id == b->id;
      } );
   schema_list.erase( new_end, schema_list.end() );

   for( std::shared_ptr< abstract_schema >& s : schema_list )
   {
      std::string tname;
      s->get_name( tname );
      FC_ASSERT( ds.types.find( tname ) == ds.types.end(), "types with different ID's found for name ${tname}", ("tname", tname) );
      std::string ss;
      s->get_str_schema( ss );
      ds.types.emplace( tname, ss );
   }

   _json_schema = fc::json::to_string( ds );
   return;*/
        }

        void database::init_genesis(uint64_t init_supply) {
            try {
                // Create blockchain accounts
                public_key_type init_public_key(STEEMIT_INIT_PUBLIC_KEY);

                create<account_object>([&](account_object &a) {
                    a.name = STEEMIT_MINER_ACCOUNT;
                });

                if (store_metadata_for_account(STEEMIT_MINER_ACCOUNT)) {
                    create<account_metadata_object>([&](account_metadata_object& m) {
                        m.account = STEEMIT_MINER_ACCOUNT;
                    });
                }

                create<account_authority_object>([&](account_authority_object &auth) {
                    auth.account = STEEMIT_MINER_ACCOUNT;
                    auth.owner.weight_threshold = 1;
                    auth.active.weight_threshold = 1;
                });

                create<account_object>([&](account_object &a) {
                    a.name = STEEMIT_NULL_ACCOUNT;
                });
                
                if (store_metadata_for_account(STEEMIT_NULL_ACCOUNT)) {
                    create<account_metadata_object>([&](account_metadata_object& m) {
                        m.account = STEEMIT_NULL_ACCOUNT;
                    });
                }
                
                create<account_authority_object>([&](account_authority_object &auth) {
                    auth.account = STEEMIT_NULL_ACCOUNT;
                    auth.owner.weight_threshold = 1;
                    auth.active.weight_threshold = 1;
                });

                create<account_object>([&](account_object &a) {
                    a.name = STEEMIT_TEMP_ACCOUNT;
                });

                if (store_metadata_for_account(STEEMIT_TEMP_ACCOUNT)) {
                    create<account_metadata_object>([&](account_metadata_object& m) {
                        m.account = STEEMIT_TEMP_ACCOUNT;
                    });
                }

                create<account_authority_object>([&](account_authority_object &auth) {
                    auth.account = STEEMIT_TEMP_ACCOUNT;
                    auth.owner.weight_threshold = 0;
                    auth.active.weight_threshold = 0;
                });

                for (int i = 0; i < STEEMIT_NUM_INIT_MINERS; ++i) {
                    const auto& name = STEEMIT_INIT_MINER_NAME + (i ? fc::to_string(i) : std::string());
                    create<account_object>([&](account_object &a) {
                        a.name = name;
                        a.memo_key = init_public_key;
                        a.balance = asset(i ? 0 : init_supply, STEEM_SYMBOL);
                    });

                    if (store_metadata_for_account(name)) {
                        create<account_metadata_object>([&](account_metadata_object& m) {
                            m.account = name;
                        });
                    }

                    create<account_authority_object>([&](account_authority_object &auth) {
                        auth.account = name;
                        auth.owner.add_authority(init_public_key, 1);
                        auth.owner.weight_threshold = 1;
                        auth.active = auth.owner;
                        auth.posting = auth.active;
                    });
                    create<witness_object>([&](witness_object &w) {
                        w.owner = name;
                        w.signing_key = init_public_key;
                        w.schedule = witness_object::miner;
                    });
                }

                create<dynamic_global_property_object>([&](dynamic_global_property_object &p) {
                    p.current_witness = STEEMIT_INIT_MINER_NAME;
                    p.time = STEEMIT_GENESIS_TIME;
                    p.recent_slots_filled = fc::uint128_t::max_value();
                    p.participation_count = 128;
                    p.current_supply = asset(init_supply, STEEM_SYMBOL);
                    p.virtual_supply = p.current_supply;
                    p.maximum_block_size = STEEMIT_MAX_BLOCK_SIZE;
                });
                auto snapshot_path = string("./snapshot5392323.json");
                auto snapshot_file = fc::path(snapshot_path);
                auto snapshot_exists = fc::exists(snapshot_file);

#ifdef STEEMIT_BUILD_TESTNET
                // Note: snapshot is not exist (not copied) in default TESTNET config
                if (snapshot_exists) {
#else
                FC_ASSERT(snapshot_exists, "Snapshot file '${file}' was not found.", ("file", snapshot_file));
#endif

                std::cout << "Initializing state from snapshot file: "
                          << snapshot_file.generic_string() << "\n";

#ifndef STEEMIT_BUILD_TESTNET
                unsigned char digest[MD5_DIGEST_LENGTH];
                char snapshot_checksum[] = "081b0149f0b2a570ae76b663090cfb0c";
                char md5hash[33];
                boost::iostreams::mapped_file_source src(snapshot_path);
                MD5((unsigned char *)src.data(), src.size(), (unsigned char *)&digest);
                for (int i = 0; i < 16; i++) {
                    sprintf(&md5hash[i * 2], "%02x", (unsigned int)digest[i]);
                }
                FC_ASSERT(memcmp(md5hash, snapshot_checksum, 32) ==
                          0, "Checksum of snapshot [${h}] is not equal [${s}]", ("h", md5hash)("s", snapshot_checksum));
#else
                share_type snapshot_supply = int64_t( init_supply );
                share_type initial_vesting = int64_t( 0 );
#endif

                snapshot_state snapshot = fc::json::from_file(snapshot_file).as<snapshot_state>();
                for (account_summary &account : snapshot.accounts) {
                    create<account_object>([&](account_object& a) {
                        a.name = account.name;
                        a.memo_key = account.keys.memo_key;
                        a.recovery_account = STEEMIT_INIT_MINER_NAME;
#ifdef STEEMIT_BUILD_TESTNET
                        for (auto& amount : account.balances.assets) {
                            if (amount.symbol == STEEM_SYMBOL) {
                                a.balance.amount = amount.amount;
                            } else if (amount.symbol == SBD_SYMBOL) {
                                a.sbd_balance.amount = amount.amount;
                            } else if (amount.symbol == VESTS_SYMBOL) {
                                a.vesting_shares.amount = amount.amount;
                            } else {
                                FC_ASSERT(false, "Unknown asset in snapshot");
                            }
                        }
                        //assume price of GOLOS = GBG, just for testnet
                        snapshot_supply -= a.balance.amount + a.sbd_balance.amount + a.vesting_shares.amount;
                        initial_vesting += a.vesting_shares.amount;
                        elog("initial_vesting = ${initial_vesting}, add = ${vests}",("initial_vesting",initial_vesting)("vests",a.vesting_shares.amount));
#endif
                    });

                    if (store_metadata_for_account(account.name)) {
                        create<account_metadata_object>([&](account_metadata_object& m) {
                            m.account = account.name;
                            m.json_metadata = "{created_at: 'GENESIS'}";
                        });
                    }
                    
                    create<account_authority_object>([&](account_authority_object& auth) {
                        auth.account = account.name;
                        auth.owner.weight_threshold = 1;
                        auth.owner = account.keys.owner_key;
                        auth.active = account.keys.active_key;
                        auth.posting = account.keys.posting_key;
                    });
                }
                std::cout << "Imported " << snapshot.accounts.size()
                          << " accounts from " << snapshot_file.generic_string()
                          << ".\n";

#ifdef STEEMIT_BUILD_TESTNET

                    const auto& initiator = get_account( STEEMIT_INIT_MINER_NAME );
                    modify( initiator, [&]( account_object& a )
                    {
                        a.balance  = asset( snapshot_supply, STEEM_SYMBOL );
                        elog("ajust balance to ${amount}",("amount", a.balance.amount));
                    } );
                    const auto &gpo = get_dynamic_global_properties();
                    modify(gpo, [&](dynamic_global_property_object &p) {
                        p.total_vesting_fund_steem = asset(initial_vesting, STEEM_SYMBOL);
                        p.total_vesting_shares = asset(initial_vesting, VESTS_SYMBOL);
                        elog("ajust gpo vests to ${amount}",("amount", initial_vesting));
                    });
                }
#endif

                // Nothing to do
                create<feed_history_object>([&](feed_history_object &o) {});
                for (int i = 0; i < 0x10000; i++) {
                    create<block_summary_object>([&](block_summary_object &) {});
                }
                create<hardfork_property_object>([&](hardfork_property_object &hpo) {
                    hpo.processed_hardforks.push_back(STEEMIT_GENESIS_TIME);
                });

                // Create witness scheduler
                create<witness_schedule_object>([&](witness_schedule_object &wso) {
                    wso.current_shuffled_witnesses[0] = STEEMIT_INIT_MINER_NAME;
                });
            }
            FC_CAPTURE_AND_RETHROW()
        }

        uint32_t database::validate_transaction(const signed_transaction &trx, uint32_t skip) {
            const uint32_t validate_transaction_steps =
                skip_authority_check |
                skip_transaction_signatures |
                skip_validate_operations |
                skip_tapos_check;

            // in case of multi-thread application, it's allow to validate transaction in read-thread
            if ((skip & validate_transaction_steps) != validate_transaction_steps) {
                // this method can be used only for push_transaction(),
                //  because such transactions only added to pending list,
                //  and they will be rechecked on block generation
                auto validate_action = [&]() {
                    _validate_transaction(trx, skip);
                };

                if (!(skip & skip_database_locking)) {
                    with_weak_read_lock(std::move(validate_action));
                } else {
                    validate_action();
                }

                skip |= validate_transaction_steps;
            }

            if (!(skip & skip_apply_transaction)) {
                auto apply_action = [&]() {
                    auto session = start_undo_session();
                    _apply_transaction(trx, skip);
                    session.undo();
                };

                if (!(skip & skip_database_locking)) {
                    with_weak_write_lock(std::move(apply_action));
                } else {
                    apply_action();
                }
            }

            return skip;
        }

        void database::_validate_transaction(const signed_transaction &trx, uint32_t skip) {
            if (!(skip & skip_validate_operations)) {   /* issue #505 explains why this skip_flag is disabled */
                trx.validate();
            }

            if (!(skip & (skip_transaction_signatures | skip_authority_check))) {
                const chain_id_type &chain_id = STEEMIT_CHAIN_ID;

                auto get_active = [&](const account_name_type& name) {
                    return authority(get_authority(name).active);
                };

                auto get_owner = [&](const account_name_type& name) {
                    return authority(get_authority(name).owner);
                };

                auto get_posting = [&](const account_name_type& name) {
                    return authority(get_authority(name).posting);
                };

                try {
                    trx.verify_authority(chain_id, get_active, get_owner, get_posting, STEEMIT_MAX_SIG_CHECK_DEPTH);
                }
                catch (protocol::tx_missing_active_auth &e) {
                    if (get_shared_db_merkle().find(head_block_num() + 1) == get_shared_db_merkle().end()) {
                        throw e;
                    }
                }
            }

            //Skip all manner of expiration and TaPoS checking if we're on block 1;
            // It's impossible that the transaction is expired, and TaPoS makes no sense as no blocks exist.
            if (BOOST_LIKELY(head_block_num() > 0)) {
                if (!(skip & skip_tapos_check)) {
                    const auto &tapos_block_summary = get_block_summary(trx.ref_block_num);
                    //Verify TaPoS block summary has correct ID prefix,
                    //   and that this block's time is not past the expiration
                    GOLOS_ASSERT(trx.ref_block_prefix == tapos_block_summary.block_id._hash[1],
                        tx_invalid_field, "Transaction field ${field} has invalid value",
                        ("field","ref_block_prefix")
                        ("trx.ref_block_prefix", trx.ref_block_prefix)
                        ("tapos_block_summary", tapos_block_summary.block_id._hash[1]));
                }

                fc::time_point_sec now = head_block_time();
                fc::time_point_sec maximum = now + fc::seconds(STEEMIT_MAX_TIME_UNTIL_EXPIRATION);

                GOLOS_ASSERT(trx.expiration <= maximum, 
                    tx_invalid_field, "Transaction field ${field} has invalid value",
                    ("field", "expiration")
                    ("trx.expiration", trx.expiration)
                    ("now", now)("maximum", maximum));

                // Simple solution to pending trx bug when now == trx.expiration
                if (is_producing() || has_hardfork(STEEMIT_HARDFORK_0_9)) {
                    GOLOS_ASSERT(now < trx.expiration, tx_expired,
                            "Transaction is expired. Now ${now}, expired ${expired}", 
                            ("now", now)("expired", trx.expiration-1)
                            ("trx.expiration", trx.expiration));
                }

                GOLOS_ASSERT(now <= trx.expiration, tx_expired,
                        "Transaction is expired. Now ${now}, expired ${expired}", 
                        ("now", now)("expired", trx.expiration)
                        ("trx.expiration", trx.expiration));
            }
        }

        void database::notify_changed_objects() {
            try {
                /*vector< golos::chainbase::generic_id > ids;
      get_changed_ids( ids );
      STEEMIT_TRY_NOTIFY( changed_objects, ids )*/
                /*
      if( _undo_db.enabled() )
      {
         const auto& head_undo = _undo_db.head();
         vector<object_id_type> changed_ids;  changed_ids.reserve(head_undo.old_values.size());
         for( const auto& item : head_undo.old_values ) changed_ids.push_back(item.first);
         for( const auto& item : head_undo.new_ids ) changed_ids.push_back(item);
         vector<const object*> removed;
         removed.reserve( head_undo.removed.size() );
         for( const auto& item : head_undo.removed )
         {
            changed_ids.push_back( item.first );
            removed.emplace_back( item.second.get() );
         }
         STEEMIT_TRY_NOTIFY( changed_objects, changed_ids )
      }
      */
            }
            FC_CAPTURE_AND_RETHROW()

        }

        void database::set_flush_interval(uint32_t flush_blocks) {
            _flush_blocks = flush_blocks;
            _next_flush_block = 0;
        }

        const block_log &database::get_block_log() const {
            return _block_log;
        }

//////////////////// private methods ////////////////////

        void database::apply_block(const signed_block &next_block, uint32_t skip) {
            try {
                //fc::time_point begin_time = fc::time_point::now();

                auto block_num = next_block.block_num();
                if (_checkpoints.size() &&
                    _checkpoints.rbegin()->second != block_id_type()) {
                    auto itr = _checkpoints.find(block_num);
                    if (itr != _checkpoints.end())
                        FC_ASSERT(next_block.id() ==
                                  itr->second, "Block did not match checkpoint", ("checkpoint", *itr)("block_id", next_block.id()));

                    if (_checkpoints.rbegin()->first >= block_num) {
                        skip = skip_witness_signature
                               | skip_transaction_signatures
                               | skip_transaction_dupe_check
                               | skip_fork_db
                               | skip_block_size_check
                               | skip_tapos_check
                               | skip_authority_check
                               /* | skip_merkle_check While blockchain is being downloaded, txs need to be validated against block headers */
                               | skip_undo_history_check
                               | skip_witness_schedule_check
                               | skip_validate_operations
                               | skip_validate_invariants;
                    }
                }

                _apply_block(next_block, skip);

                /*try
   {
   /// check invariants
   if( is_producing() || !( skip & skip_validate_invariants ) )
      validate_invariants();
   }
   FC_CAPTURE_AND_RETHROW( (next_block) );*/

                //fc::time_point end_time = fc::time_point::now();
                //fc::microseconds dt = end_time - begin_time;
                if (_flush_blocks != 0) {
                    if (_next_flush_block == 0) {
                        uint32_t lep = block_num + 1 + _flush_blocks * 9 / 10;
                        uint32_t rep = block_num + 1 + _flush_blocks;

                        // use time_point::now() as RNG source to pick block randomly between lep and rep
                        uint32_t span = rep - lep;
                        uint32_t x = lep;
                        if (span > 0) {
                            uint64_t now = uint64_t(fc::time_point::now().time_since_epoch().count());
                            x += now % span;
                        }
                        _next_flush_block = x;
//                        ilog("Next flush scheduled at block ${b}", ("b", x));
                    }

                    if (_next_flush_block == block_num) {
                        _next_flush_block = 0;
//                        ilog("Flushing database shared memory at block ${b}", ("b", block_num));
                        chainbase::database::flush();
                    }
                }

            } FC_CAPTURE_AND_RETHROW((next_block))
        }

        void database::_apply_block(const signed_block &next_block, uint32_t skip) {
            try {
                uint32_t next_block_num = next_block.block_num();
                const auto &gprops = get_dynamic_global_properties();
                //block_id_type next_block_id = next_block.id();

                _validate_block(next_block, skip);

                const witness_object &signing_witness = validate_block_header(skip, next_block);

                _current_block_num = next_block_num;
                _current_trx_in_block = 0;
                _current_virtual_op = 0;

                /// modify current witness so transaction evaluators can know who included the transaction,
                /// this is mostly for POW operations which must pay the current_witness
                modify(gprops, [&](dynamic_global_property_object &dgp) {
                    dgp.current_witness = next_block.witness;
                });

                /// parse witness version reporting
                process_header_extensions(next_block);

                if (has_hardfork(STEEMIT_HARDFORK_0_5__54)) // Cannot remove after hardfork
                {
                    const auto &witness = get_witness(next_block.witness);
                    const auto &hardfork_state = get_hardfork_property_object();
                    FC_ASSERT(witness.running_version >=
                              hardfork_state.current_hardfork_version,
                            "Block produced by witness that is not running current hardfork",
                            ("witness", witness)("next_block.witness", next_block.witness)("hardfork_state", hardfork_state)
                    );
                }

                for (const auto &trx : next_block.transactions) {
                    /* We do not need to push the undo state for each transaction
                     * because they either all apply and are valid or the
                     * entire block fails to apply.  We only need an "undo" state
                     * for transactions when validating broadcast transactions or
                     * when building a block.
                     */
                    apply_transaction(trx, skip);
                    ++_current_trx_in_block;
                }

                _current_trx_in_block = -1;
                _current_op_in_trx = 0;
                _current_virtual_op = 0;

                update_global_dynamic_data(next_block, skip);
                update_signing_witness(signing_witness, next_block);

                update_last_irreversible_block(skip);

                create_block_summary(next_block);

                clear_expired_proposals();
                clear_expired_transactions();
                clear_expired_orders();
                clear_expired_delegations();

                check_witness_idleness();
                update_witness_schedule();

                update_median_feed();
                update_virtual_supply();

                clear_null_account_balance();
                process_funds();
                process_accumulative_distributions();
                auto_claim_accumulatives();
                process_conversions();
                process_sbd_debt_conversions();
                process_comment_cashout();
                process_worker_votes();
                process_worker_cashout();
                check_account_idleness();
                check_claim_idleness();
                process_events();
                process_vesting_withdrawals();
                process_savings_withdraws();
                pay_liquidity_reward();
                update_virtual_supply();

                account_recovery_processing();
                expire_escrow_ratification();
                process_decline_voting_rights();

                process_hardforks();

                process_account_freezing();

                process_gbg_payments();

                process_paid_subscribers();

                process_nft_bets();

                // notify observers that the block has been applied
                notify_applied_block(next_block);

                process_transit_to_cyberway(next_block, skip);

                notify_changed_objects();

                if (has_hardfork(STEEMIT_HARDFORK_0_30)) {
                    hf_actions hf_act(*this);
                    hf_act.fix_vesting_withdrawals();
                }

            } FC_CAPTURE_LOG_AND_RETHROW((next_block.block_num()))
        }

        void database::process_header_extensions(const signed_block &next_block) {
            auto itr = next_block.extensions.begin();

            while (itr != next_block.extensions.end()) {
                switch (itr->which()) {
                    case 0: // void_t
                        break;
                    case 1: // version
                    {
                        auto reported_version = itr->get<version>();
                        const auto &signing_witness = get_witness(next_block.witness);
                        //idump( (next_block.witness)(signing_witness.running_version)(reported_version) );

                        if (reported_version !=
                            signing_witness.running_version) {
                            modify(signing_witness, [&](witness_object &wo) {
                                wo.running_version = reported_version;
                            });
                        }
                        break;
                    }
                    case 2: // hardfork_version vote
                    {
                        auto hfv = itr->get<hardfork_version_vote>();
                        const auto &signing_witness = get_witness(next_block.witness);
                        //idump( (next_block.witness)(signing_witness.running_version)(hfv) );

                        if (hfv.hf_version !=
                            signing_witness.hardfork_version_vote ||
                            hfv.hf_time != signing_witness.hardfork_time_vote) {
                            modify(signing_witness, [&](witness_object &wo) {
                                wo.hardfork_version_vote = hfv.hf_version;
                                wo.hardfork_time_vote = hfv.hf_time;
                            });
                        }

                        break;
                    }
                    default:
                        FC_ASSERT(false, "Unknown extension in block header");
                }

                ++itr;
            }
        }


        void database::update_median_feed() {
            try {
                if ((head_block_num() % STEEMIT_FEED_INTERVAL_BLOCKS) != 0) {
                    return;
                }

                auto now = head_block_time();
                const witness_schedule_object &wso = get_witness_schedule_object();
                vector<price> feeds;
                feeds.reserve(wso.num_scheduled_witnesses);
                for (int i = 0; i < wso.num_scheduled_witnesses; i++) {
                    const auto &wit = get_witness(wso.current_shuffled_witnesses[i]);

                    if (has_hardfork(STEEMIT_HARDFORK_0_18__220)) {
                        if ( now < wit.last_sbd_exchange_update + STEEMIT_MAX_FEED_AGE && !wit.sbd_exchange_rate.is_null() ) {
                            feeds.push_back(wit.sbd_exchange_rate);
                        }
                    }
                    else {
                        if ( wit.last_sbd_exchange_update < now + STEEMIT_MAX_FEED_AGE && !wit.sbd_exchange_rate.is_null() ) {

                            feeds.push_back(wit.sbd_exchange_rate);
                        }
                    }
                }

                if (feeds.size() >= STEEMIT_MIN_FEEDS) {
                    std::sort(feeds.begin(), feeds.end());
                    auto median_feed = feeds[feeds.size() / 2];

                    bool is_forced_min_price = false;
                    const auto &gpo = get_dynamic_global_properties();

                    modify(get_feed_history(), [&](feed_history_object &fho) {
                        fho.price_history.push_back(median_feed);
                        size_t steem_feed_history_window = STEEMIT_FEED_HISTORY_WINDOW_PRE_HF_16;
                        if (has_hardfork(STEEMIT_HARDFORK_0_16__551)) {
                            steem_feed_history_window = STEEMIT_FEED_HISTORY_WINDOW;
                        }

                        if (fho.price_history.size() >
                            steem_feed_history_window) {
                            fho.price_history.pop_front();
                        }

                        if (fho.price_history.size()) {
                            std::deque<price> copy;
                            for (auto i : fho.price_history) {
                                copy.push_back(i);
                            }

                            std::sort(copy.begin(), copy.end()); /// TODO: use nth_item
                            fho.witness_median_history = copy[copy.size() / 2];
                            fho.current_median_history = fho.witness_median_history;

#ifdef STEEMIT_BUILD_TESTNET
                            if (skip_price_feed_limit_check) {
                                return;
                            }
#endif
                            if (has_hardfork(STEEMIT_HARDFORK_0_14__230)) {
                                price min_price(asset(10 * gpo.current_sbd_supply.amount, SBD_SYMBOL), gpo.current_supply); // This price limits SBD to 10% market cap

                                if (!has_hardfork(STEEMIT_HARDFORK_0_29)) {
                                    min_price = price(asset(9 * gpo.current_sbd_supply.amount, SBD_SYMBOL), gpo.current_supply);
                                }

                                is_forced_min_price = min_price > fho.current_median_history;
                                if (is_forced_min_price) {
                                    fho.current_median_history = min_price;
                                }
                            }
                        }
                    });

                    if (has_hardfork(STEEMIT_HARDFORK_0_29)) {
                        modify(gpo, [&](auto& mutable_gpo) {mutable_gpo.is_forced_min_price = is_forced_min_price;});
                    } else {
                        if (is_forced_min_price) {
                            modify(gpo, [&](auto& mutable_gpo) {mutable_gpo.is_forced_min_price = is_forced_min_price;});
                        }
                    }
                }
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::apply_transaction(const signed_transaction &trx, uint32_t skip) {
            _apply_transaction(trx, skip);
            notify_on_applied_transaction(trx);
        }

        void database::_apply_transaction(const signed_transaction &trx, uint32_t skip) {
            try {
                _current_trx_id = trx.id();
                _current_virtual_op = 0;

                auto &trx_idx = get_index<transaction_index>();
                auto trx_id = trx.id();
                // idump((trx_id)(skip&skip_transaction_dupe_check));
                if (!(skip & skip_transaction_dupe_check) &&
                          trx_idx.indices().get<by_trx_id>().find(trx_id) != trx_idx.indices().get<by_trx_id>().end()) {
                    FC_THROW_EXCEPTION(tx_duplicate_transaction,
                          "Duplicate transaction check failed", ("trx_ix", trx_id));
                }

                _validate_transaction(trx, skip);

                flat_set<account_name_type> required;
                vector<authority> other;
                trx.get_required_authorities(required, required, required, other);

                auto trx_size = fc::raw::pack_size(trx);

                const auto& props = get_dynamic_global_properties();

                for (const auto& auth : required) {
                    const auto& acnt = get_account(auth);
                    update_account_bandwidth(props, acnt, trx_size, bandwidth_type::forum);
                    for (const auto& op : trx.operations) {
                        if (is_market_operation(op)) {
                            update_account_bandwidth(props, acnt, trx_size * 10, bandwidth_type::market);
                            break;
                        } else if (has_hardfork(STEEMIT_HARDFORK_0_19__924) && is_custom_json_operation(op)) {
                            update_account_bandwidth(props, acnt, trx_size * props.custom_ops_bandwidth_multiplier, bandwidth_type::custom_json);
                            break;
                        }
                    }

                    const auto now = head_block_time();
                    for (const auto& op : trx.operations) {
                        if (is_active_operation(op)) {
                            modify(acnt, [&](auto& a) {
                                a.last_active_operation = now;
                            });
                            break;
                        }
                    }
                }

                //Insert transaction into unique transactions database.
                if (!(skip & skip_transaction_dupe_check)) {
                    create<transaction_object>([&](transaction_object &transaction) {
                        transaction.trx_id = trx_id;
                        transaction.expiration = trx.expiration;
                        fc::raw::pack(transaction.packed_trx, trx);
                    });
                }

                //Finally process the operations
                _current_op_in_trx = 0;
                for (const auto &op : trx.operations) {
                    try {
                        try {
                            apply_operation(op);
                            ++_current_op_in_trx;
                        } catch(const fc::exception& e) {
                            FC_THROW_EXCEPTION(tx_invalid_operation,
                                    "Invalid operation ${index} in transaction: ${errmsg}",
                                    ("index", _current_op_in_trx)
                                    ("errmsg", e.to_string())
                                    ("error", e));
                        } 
                    } FC_CAPTURE_AND_RETHROW((op));
                }
                _current_trx_id = transaction_id_type();

            } FC_CAPTURE_AND_RETHROW((trx))
        }

        void database::apply_operation(const operation &op, bool is_virtual /* false */) {
            operation_notification note(op);
            if (is_virtual) {
                ++_current_virtual_op;
                note.virtual_op = _current_virtual_op;
            }
            notify_pre_apply_operation(note);
            _my->_evaluator_registry.get_evaluator(op).apply(op);
            notify_post_apply_operation(note);
        }

        const witness_object &database::validate_block_header(uint32_t skip, const signed_block &next_block) const {
            try {
                FC_ASSERT(head_block_id() ==
                          next_block.previous, "", ("head_block_id", head_block_id())("next.prev", next_block.previous));
                FC_ASSERT(head_block_time() <
                          next_block.timestamp, "", ("head_block_time", head_block_time())("next", next_block.timestamp)("blocknum", next_block.block_num()));
                const witness_object &witness = get_witness(next_block.witness);

                if (!(skip & skip_witness_signature))
                    FC_ASSERT(next_block.validate_signee(witness.signing_key));

                if (!(skip & skip_witness_schedule_check)) {
                    uint32_t slot_num = get_slot_at_time(next_block.timestamp);
                    FC_ASSERT(slot_num > 0);

                    auto num = next_block.block_num();
                    // 2023-01-21, 2023-01-31
                    // 65693821 is exact
                    if (num < 65693821 || num > 65893852) {
                        string scheduled_witness = get_scheduled_witness(slot_num);

                        FC_ASSERT(witness.owner ==
                            scheduled_witness, "Witness produced block at wrong time",
                            ("block witness", next_block.witness)("scheduled", scheduled_witness)("slot_num", slot_num));
                    }
                }

                return witness;
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::create_block_summary(const signed_block &next_block) {
            try {
                block_summary_id_type sid(next_block.block_num() & 0xffff);
                modify(get_block_summary(sid), [&](block_summary_object &p) {
                    p.block_id = next_block.id();
                });
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::update_global_dynamic_data(const signed_block &b, uint32_t skip) {
            try {
                auto block_size = fc::raw::pack_size(b);
                const dynamic_global_property_object &_dgp =
                        get_dynamic_global_properties();

                uint32_t missed_blocks = 0;
                if (head_block_time() != fc::time_point_sec()) {
                    missed_blocks = get_slot_at_time(b.timestamp);
                    assert(missed_blocks != 0);
                    missed_blocks--;
                    for (uint32_t i = 0; i < missed_blocks; ++i) {
                        const auto &witness_missed = get_witness(get_scheduled_witness(
                                i + 1));
                        if (witness_missed.owner != b.witness) {
                            const auto& median_props = get_witness_schedule_object().median_props;
                            auto reset_blocks = median_props.witness_skipping_reset_time / STEEMIT_BLOCK_INTERVAL;

                            modify(witness_missed, [&](witness_object &w) {
                                w.total_missed++;
                                if (is_transit_enabled()) {
                                    //hack, in order to switch off missing delates faster
                                    if (head_block_num() - _dgp.transit_block_num < 210) {
                                        if(head_block_num() - w.last_confirmed_block_num > 30) {
                                            w.signing_key = public_key_type();
                                            push_virtual_operation(shutdown_witness_operation(w.owner));
                                        }
                                    } else if ((has_hardfork(STEEMIT_HARDFORK_0_22__79)
                                            && (head_block_num() - w.last_confirmed_block_num > reset_blocks))
                                            || (head_block_num() - w.last_confirmed_block_num > STEEMIT_BLOCKS_PER_DAY)) {
                                        w.signing_key = public_key_type();
                                        push_virtual_operation(shutdown_witness_operation(w.owner));
                                    }
                                } else if (has_hardfork(STEEMIT_HARDFORK_0_22__79)) {
                                    if (head_block_num() - w.last_confirmed_block_num > reset_blocks) {
                                        w.signing_key = public_key_type();
                                        push_virtual_operation(shutdown_witness_operation(w.owner));
                                    }
                                } else if (has_hardfork(STEEMIT_HARDFORK_0_14__278)) {
                                    if (head_block_num() - w.last_confirmed_block_num > STEEMIT_BLOCKS_PER_DAY) {
                                        w.signing_key = public_key_type();
                                        push_virtual_operation(shutdown_witness_operation(w.owner));
                                    }
                                }
                            });
                        }
                    }
                }

                // dynamic global properties updating
                modify(_dgp, [&](dynamic_global_property_object &dgp) {
                    // This is constant time assuming 100% participation. It is O(B) otherwise (B = Num blocks between update)
                    for (uint32_t i = 0; i < missed_blocks + 1; i++) {
                        dgp.participation_count -= dgp.recent_slots_filled.hi & 0x8000000000000000ULL ? 1 : 0;
                        dgp.recent_slots_filled = (dgp.recent_slots_filled << 1) + (i == 0 ? 1 : 0);
                        dgp.participation_count += (i == 0 ? 1 : 0);
                    }

                    dgp.head_block_number = b.block_num();
                    dgp.head_block_id = b.id();
                    dgp.time = b.timestamp;
                    dgp.current_aslot += missed_blocks + 1;
                    dgp.average_block_size =
                            (99 * dgp.average_block_size + block_size) / 100;

       /*
       *  About once per minute the average network use is consulted and used to adjust
       *  the reserve ratio. Anything above 25% usage (since STEEMIT_HARDFORK_0_12__179)
       *  reduces the ratio by half which should instantly bring the network from 50% to
       *  25% use unless the demand comes from users who have surplus capacity. In other
       *  words, a 50% reduction in reserve ratio does not result in a 50% reduction in
       *  usage, it will only impact users who where attempting to use more than 50% of
       *  their capacity.
       *
       *  When the reserve ratio is at its max (check STEEMIT_MAX_RESERVE_RATIO) a 50%
       *  reduction will take 3 to 4 days to return back to maximum. When it is at its
       *  minimum it will return back to its prior level in just a few minutes.
       *
       *  If the network reserve ratio falls under 100 then it is probably time to
       *  increase the capacity of the network.
       */
                    if (dgp.head_block_number % 20 == 0) {
                        if ((!has_hardfork(STEEMIT_HARDFORK_0_12__179) &&
                             dgp.average_block_size >
                             dgp.maximum_block_size / 2) ||
                            (has_hardfork(STEEMIT_HARDFORK_0_12__179) &&
                             dgp.average_block_size >
                             dgp.maximum_block_size / 4)) {
                            dgp.current_reserve_ratio /= 2; /// exponential back up
                        } else { /// linear growth... not much fine grain control near full capacity
                            dgp.current_reserve_ratio++;
                        }

                        if (has_hardfork(STEEMIT_HARDFORK_0_2) &&
                            dgp.current_reserve_ratio >
                            STEEMIT_MAX_RESERVE_RATIO) {
                            dgp.current_reserve_ratio = STEEMIT_MAX_RESERVE_RATIO;
                        }
                    }
                    dgp.max_virtual_bandwidth = (dgp.maximum_block_size *
                                                 dgp.current_reserve_ratio *
                                                 STEEMIT_BANDWIDTH_PRECISION *
                                                 STEEMIT_BANDWIDTH_AVERAGE_WINDOW_SECONDS) /
                                                STEEMIT_BLOCK_INTERVAL;
                });

                if (!(skip & skip_undo_history_check)) {
                    GOLOS_ASSERT(
                        _dgp.head_block_number - _dgp.last_irreversible_block_num < STEEMIT_MAX_UNDO_HISTORY,
                        undo_database_exception,
                        "The database does not have enough undo history to support a blockchain with so many missed blocks. "
                        "Please add a checkpoint if you would like to continue applying blocks beyond this point.",
                        ("last_irreversible_block_num", _dgp.last_irreversible_block_num)
                        ("head", _dgp.head_block_number)
                        ("max_undo", STEEMIT_MAX_UNDO_HISTORY));
                }
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::update_virtual_supply() {
            try {
                modify(get_dynamic_global_properties(), [&](dynamic_global_property_object &dgp) {
                    dgp.virtual_supply = dgp.current_supply
                                         +
                                         (get_feed_history().current_median_history.is_null()
                                          ? asset(0, STEEM_SYMBOL) :
                                          dgp.current_sbd_supply *
                                          get_feed_history().current_median_history);

                    auto median_price = get_feed_history().current_median_history;

                    if (!median_price.is_null() &&
                        has_hardfork(STEEMIT_HARDFORK_0_14__230)) {
                        auto percent_sbd = uint16_t((
                                (fc::uint128_t((dgp.current_sbd_supply *
                                                get_feed_history().current_median_history).amount.value) *
                                 STEEMIT_100_PERCENT)
                                / dgp.virtual_supply.amount.value).to_uint64());

                        if (percent_sbd <= STEEMIT_SBD_START_PERCENT) {
                            dgp.sbd_print_rate = STEEMIT_100_PERCENT;
                        } else if (percent_sbd >= STEEMIT_SBD_STOP_PERCENT) {
                            dgp.sbd_print_rate = 0;
                        } else {
                            dgp.sbd_print_rate =
                                    ((STEEMIT_SBD_STOP_PERCENT - percent_sbd) *
                                     STEEMIT_100_PERCENT) /
                                    (STEEMIT_SBD_STOP_PERCENT -
                                     STEEMIT_SBD_START_PERCENT);
                        }

                        if (has_hardfork(STEEMIT_HARDFORK_0_22__64)) {
                            auto supply = dgp.virtual_supply;
                            if (has_hardfork(STEEMIT_HARDFORK_0_29)) {
                                supply = dgp.current_supply;
                            }
                            dgp.sbd_debt_percent = uint16_t((
                                (fc::uint128_t((dgp.current_sbd_supply *
                                                get_feed_history().witness_median_history).amount.value) *
                                 STEEMIT_100_PERCENT)
                                / supply.amount.value).to_uint64());
                        }
                    }
                });
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::update_signing_witness(const witness_object &signing_witness, const signed_block &new_block) {
            try {
                const dynamic_global_property_object &dpo = get_dynamic_global_properties();
                uint64_t new_block_aslot = dpo.current_aslot +
                                           get_slot_at_time(new_block.timestamp);

                modify(signing_witness, [&](witness_object &_wit) {
                    _wit.last_aslot = new_block_aslot;
                    _wit.last_confirmed_block_num = new_block.block_num();
                });
            } FC_CAPTURE_AND_RETHROW()
        }

        void database::update_last_irreversible_block(uint32_t skip) {
            try {
                const dynamic_global_property_object &dpo = get_dynamic_global_properties();

                /**
    * Prior to voting taking over, we must be more conservative...
    *
    */
                if (head_block_num() < STEEMIT_START_MINER_VOTING_BLOCK) {
                    modify(dpo, [&](dynamic_global_property_object &_dpo) {
                        if (head_block_num() > STEEMIT_MAX_WITNESSES) {
                            _dpo.last_irreversible_block_num =
                                    head_block_num() - STEEMIT_MAX_WITNESSES;
                        }
                    });
                } else {
                    const witness_schedule_object &wso = get_witness_schedule_object();

                    vector<const witness_object *> wit_objs;
                    wit_objs.reserve(wso.num_scheduled_witnesses);
                    for (int i = 0; i < wso.num_scheduled_witnesses; i++) {
                        wit_objs.push_back(&get_witness(wso.current_shuffled_witnesses[i]));
                    }

                    static_assert(STEEMIT_IRREVERSIBLE_THRESHOLD >
                                  0, "irreversible threshold must be nonzero");

                    // 1 1 1 2 2 2 2 2 2 2 -> 2     .7*10 = 7
                    // 1 1 1 1 1 1 1 2 2 2 -> 1
                    // 3 3 3 3 3 3 3 3 3 3 -> 3

                    size_t offset = ((STEEMIT_100_PERCENT -
                                      STEEMIT_IRREVERSIBLE_THRESHOLD) *
                                     wit_objs.size() / STEEMIT_100_PERCENT);

                    std::nth_element(wit_objs.begin(),
                            wit_objs.begin() + offset, wit_objs.end(),
                            [](const witness_object *a, const witness_object *b) {
                                return a->last_confirmed_block_num <
                                       b->last_confirmed_block_num;
                            });

                    uint32_t new_last_irreversible_block_num = wit_objs[offset]->last_confirmed_block_num;

                    if (new_last_irreversible_block_num > dpo.last_irreversible_block_num) {
                        //this is ok, because irreversible can grow in leaps, but transit should start on particular irreversible
                        if (is_transit_enabled() &&
                            dpo.transit_block_num > dpo.last_irreversible_block_num &&
                            dpo.transit_block_num <= new_last_irreversible_block_num
                        ) {
                            new_last_irreversible_block_num = dpo.transit_block_num;
                        }

                        modify(dpo, [&](dynamic_global_property_object &_dpo) {
                            _dpo.last_irreversible_block_num = new_last_irreversible_block_num;
                        });
                    }
                }

                commit(dpo.last_irreversible_block_num);

                if (!(skip & skip_block_log)) {
                    // output to block log based on new last irreverisible block num
                    const auto &tmp_head = _block_log.head();
                    uint64_t log_head_num = 0;

                    if (tmp_head) {
                        log_head_num = tmp_head->block_num();
                    }

                    if (log_head_num < dpo.last_irreversible_block_num) {
                        while (log_head_num < dpo.last_irreversible_block_num) {
                            std::shared_ptr<fork_item> block = _fork_db.fetch_block_on_main_branch_by_number(
                                    log_head_num + 1);
                            FC_ASSERT(block, "Current fork in the fork database does not contain the last_irreversible_block");
                            _block_log.append(block->data);
                            log_head_num++;
                        }

                        _block_log.flush();
                    }
                }

                _fork_db.set_max_size(dpo.head_block_number -
                                      dpo.last_irreversible_block_num + 1);
            } FC_CAPTURE_AND_RETHROW()
        }


        bool database::apply_order(const limit_order_object &new_order_object) {
            auto order_id = new_order_object.id;

            const auto &limit_price_idx = get_index<limit_order_index>().indices().get<by_price>();

            auto max_price = ~new_order_object.sell_price;
            auto limit_itr = limit_price_idx.lower_bound(max_price.max());
            auto limit_end = limit_price_idx.upper_bound(max_price);

            bool finished = false;
            while (!finished && limit_itr != limit_end) {
                auto old_limit_itr = limit_itr;
                ++limit_itr;
                // match returns 2 when only the old order was fully filled. In this case, we keep matching; otherwise, we stop.
                finished = (
                        match(new_order_object, *old_limit_itr, old_limit_itr->sell_price) &
                        0x1);
            }

            return find<limit_order_object>(order_id) == nullptr;
        }

        int database::match(const limit_order_object &new_order, const limit_order_object &old_order, const price &match_price) {
            assert(new_order.sell_price.quote.symbol ==
                   old_order.sell_price.base.symbol);
            assert(new_order.sell_price.base.symbol ==
                   old_order.sell_price.quote.symbol);
            assert(new_order.for_sale > 0 && old_order.for_sale > 0);
            assert(match_price.quote.symbol ==
                   new_order.sell_price.base.symbol);
            assert(match_price.base.symbol == old_order.sell_price.base.symbol);

            auto new_order_for_sale = new_order.amount_for_sale();
            auto old_order_for_sale = old_order.amount_for_sale();

            asset new_order_pays, new_order_receives, old_order_pays, old_order_receives;

            if (new_order_for_sale <= old_order_for_sale * match_price) {
                old_order_receives = new_order_for_sale;
                new_order_receives = new_order_for_sale * match_price;
            } else {
                //This line once read: assert( old_order_for_sale < new_order_for_sale * match_price );
                //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
                //Although new_order_for_sale is greater than old_order_for_sale * match_price, old_order_for_sale == new_order_for_sale * match_price
                //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
                new_order_receives = old_order_for_sale;
                old_order_receives = old_order_for_sale * match_price;
            }

            old_order_pays = new_order_receives;
            new_order_pays = old_order_receives;

            assert(new_order_pays == new_order.amount_for_sale() ||
                   old_order_pays == old_order.amount_for_sale());

            auto age = head_block_time() - old_order.created;
            if (!has_hardfork(STEEMIT_HARDFORK_0_12__178) &&
                ((age >= STEEMIT_MIN_LIQUIDITY_REWARD_PERIOD_SEC &&
                  !has_hardfork(STEEMIT_HARDFORK_0_10__149)) ||
                 (age >= STEEMIT_MIN_LIQUIDITY_REWARD_PERIOD_SEC_HF10 &&
                  has_hardfork(STEEMIT_HARDFORK_0_10__149)))) {
                if (old_order_receives.symbol == STEEM_SYMBOL) {
                    adjust_liquidity_reward(get_account(old_order.seller), old_order_receives, false);
                    adjust_liquidity_reward(get_account(new_order.seller), -old_order_receives, false);
                } else {
                    adjust_liquidity_reward(get_account(old_order.seller), new_order_receives, true);
                    adjust_liquidity_reward(get_account(new_order.seller), -new_order_receives, true);
                }
            }

            auto has_hf25 = has_hardfork(STEEMIT_HARDFORK_0_25);

            auto subtract_fee = [&](asset& order_receives, asset& fee, account_name_type& fee_receiver) {
                if (order_receives.symbol == STEEM_SYMBOL || order_receives.symbol == SBD_SYMBOL) {
                    return;
                }
                const auto& ast = get_asset(order_receives.symbol);
                if (ast.fee_percent != 0 && order_receives.amount != 0) {
                    fee = asset(
                        (uint128_t(order_receives.amount.value) * ast.fee_percent / STEEMIT_100_PERCENT).to_uint64(),
                        order_receives.symbol);
                    if (has_hf25) fee.amount = std::max(fee.amount, share_type(1));
                    adjust_balance(get_account(ast.creator), fee);
                    order_receives -= fee;
                    fee_receiver = ast.creator;
                }
            };

            asset new_trade_fee(0, new_order_receives.symbol);
            account_name_type new_trade_fee_receiver;

            asset old_trade_fee(0, old_order_receives.symbol);
            account_name_type old_trade_fee_receiver;

            if (has_hardfork(STEEMIT_HARDFORK_0_24__95)) {
                subtract_fee(new_order_receives, new_trade_fee, new_trade_fee_receiver);
                if (has_hf25) {
                    subtract_fee(old_order_receives, old_trade_fee, old_trade_fee_receiver);
                }
            }

            push_virtual_operation(fill_order_operation(
                new_order.seller, new_order.orderid, new_order_pays, new_trade_fee, new_trade_fee_receiver,
                old_order.seller, old_order.orderid, old_order_pays, old_trade_fee, old_trade_fee_receiver, old_order.sell_price));

            int result = 0;
            result |= fill_order(new_order, new_order_pays, new_order_receives);
            result |= fill_order(old_order, old_order_pays, old_order_receives)
                    << 1;

            assert(result != 0);
            return result;
        }


        void database::adjust_liquidity_reward(const account_object &owner, const asset &volume, bool is_sdb) {
            const auto &ridx = get_index<liquidity_reward_balance_index>().indices().get<by_owner>();
            auto itr = ridx.find(owner.id);
            if (itr != ridx.end()) {
                modify<liquidity_reward_balance_object>(*itr, [&](liquidity_reward_balance_object &r) {
                    if (head_block_time() - r.last_update >=
                        STEEMIT_LIQUIDITY_TIMEOUT_SEC) {
                        r.sbd_volume = 0;
                        r.steem_volume = 0;
                        r.weight = 0;
                    }

                    if (is_sdb) {
                        r.sbd_volume += volume.amount.value;
                    } else {
                        r.steem_volume += volume.amount.value;
                    }

                    r.update_weight(has_hardfork(STEEMIT_HARDFORK_0_10__141));
                    r.last_update = head_block_time();
                });
            } else {
                create<liquidity_reward_balance_object>([&](liquidity_reward_balance_object &r) {
                    r.owner = owner.id;
                    if (is_sdb) {
                        r.sbd_volume = volume.amount.value;
                    } else {
                        r.steem_volume = volume.amount.value;
                    }

                    r.update_weight(has_hardfork(STEEMIT_HARDFORK_0_9__141));
                    r.last_update = head_block_time();
                });
            }
        }


        bool database::fill_order(const limit_order_object& order, const asset& pays, asset receives) {
            try {
                FC_ASSERT(order.amount_for_sale().symbol == pays.symbol);
                FC_ASSERT(pays.symbol != receives.symbol);

                const account_object &seller = get_account(order.seller);

                adjust_balance(seller, receives);
                adjust_market_balance(seller, -pays);

                if (pays == order.amount_for_sale()) {
                    update_pair_depth(-order.amount_for_sale(), -order.amount_to_receive());
                    remove(order);
                    return true;
                } else {
                    auto rec = order.amount_to_receive();
                    modify(order, [&](limit_order_object &b) {
                        b.for_sale -= pays.amount;
                    });
                    rec = rec - order.amount_to_receive();
                    update_pair_depth(-pays, -rec);
                    /**
          *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
          *  have hit the limit where the seller is asking for nothing in return. When this
          *  happens we must refund any balance back to the seller, it is too small to be
          *  sold at the sale price.
          */
                    if (order.amount_to_receive().amount == 0) {
                        cancel_order(order);
                        return true;
                    }
                    return false;
                }
            }
            FC_CAPTURE_AND_RETHROW((order)(pays)(receives))
        }

        void database::cancel_order(const limit_order_object &order) {
            const auto& owner = get_account(order.seller);
            adjust_balance(owner, order.amount_for_sale());
            adjust_market_balance(owner, -order.amount_for_sale());
            push_order_delete_event(order);
            update_pair_depth(-order.amount_for_sale(), -order.amount_to_receive());
            remove(order);
        }


        void database::clear_expired_transactions() { try {
            //Look for expired transactions in the deduplication list, and remove them.
            //Transactions must have expired by at least two forking windows in order to be removed.
            auto &transaction_idx = get_index<transaction_index>();
            const auto &dedupe_index = transaction_idx.indices().get<by_expiration>();
            while ((!dedupe_index.empty()) &&
                   (head_block_time() > dedupe_index.begin()->expiration)) {
                remove(*dedupe_index.begin());
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::clear_expired_orders() { try {
            auto now = head_block_time();
            const auto &orders_by_exp = get_index<limit_order_index>().indices().get<by_expiration>();
            auto itr = orders_by_exp.begin();
            while (itr != orders_by_exp.end() && itr->expiration < now) {
                cancel_order(*itr);
                itr = orders_by_exp.begin();
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::clear_expired_delegations() { try {
            auto now = head_block_time();
            const auto& delegations_by_exp = get_index<vesting_delegation_expiration_index, by_expiration>();
            auto itr = delegations_by_exp.begin();
            while (itr != delegations_by_exp.end() && itr->expiration < now) {
                modify(get_account(itr->delegator), [&](account_object& a) {
                    a.delegate_vs(-itr->vesting_shares, itr->is_emission);
                });
                push_virtual_operation(return_vesting_delegation_operation(itr->delegator, itr->vesting_shares));
                remove(*itr);
                itr = delegations_by_exp.begin();
            }
        } FC_CAPTURE_AND_RETHROW() }

        void database::adjust_balance(const account_object &a, const asset &delta) {
            if (delta.symbol == STEEM_SYMBOL) {
                modify(a, [&](auto& acnt) {acnt.balance += delta;});
            } else if (delta.symbol == SBD_SYMBOL) {
                adjust_sbd_balance(a, delta);
            } else {
                GOLOS_CHECK_VALUE(has_hardfork(STEEMIT_HARDFORK_0_24__95), "invalid symbol");
                adjust_account_balance(a.name, delta, asset(0, delta.symbol));
            }
        }

        void database::adjust_sbd_balance(const account_object &a, const asset &delta) {
            const auto& dynamic_global_properties = get_dynamic_global_properties();

            const bool is_fee_payment_enabled = (has_hardfork(STEEMIT_HARDFORK_0_19__952) && !dynamic_global_properties.is_forced_min_price) ||
                                                !has_hardfork(STEEMIT_HARDFORK_0_19__952);

            modify(a, [&](account_object& acnt) {

                if (!has_hardfork(STEEMIT_HARDFORK_0_26__131) && a.sbd_seconds_last_update != head_block_time()) {
                    acnt.sbd_seconds += fc::uint128_t(a.sbd_balance.amount.value) * (head_block_time() - a.sbd_seconds_last_update).to_seconds();

                    acnt.sbd_seconds_last_update = head_block_time();

                    if (is_fee_payment_enabled &&
                        acnt.sbd_seconds > 0 &&
                        (acnt.sbd_seconds_last_update - acnt.sbd_last_interest_payment).to_seconds() > STEEMIT_SBD_INTEREST_COMPOUND_INTERVAL_SEC) {

                        const auto interest = (acnt.sbd_seconds * dynamic_global_properties.sbd_interest_rate) / (STEEMIT_SECONDS_PER_YEAR * STEEMIT_100_PERCENT);

                        asset interest_paid(interest.to_uint64(), SBD_SYMBOL);

                        acnt.sbd_balance += interest_paid;
                        acnt.sbd_seconds = 0;
                        acnt.sbd_last_interest_payment = head_block_time();

                        push_virtual_operation(interest_operation(a.name, interest_paid));

                        modify(dynamic_global_properties, [&](dynamic_global_property_object &props) {
                            props.current_sbd_supply += interest_paid;
                            props.virtual_supply += interest_paid * get_feed_history().current_median_history;
                        });
                    }
                }
                acnt.sbd_balance += delta;
            });
        }

        void database::pay_savings_interest(account_object& acnt) { // call it inside modify()
            if (acnt.savings_sbd_seconds == 0) {
                return;
            }

            auto has_hf28 = has_hardfork(STEEMIT_HARDFORK_0_28__219);
            if (!has_hf28 && (acnt.savings_sbd_seconds_last_update -
                acnt.savings_sbd_last_interest_payment).to_seconds() <=
                STEEMIT_SBD_INTEREST_COMPOUND_INTERVAL_SEC) {
                return;
            }

            const auto& dgp = get_dynamic_global_properties();

            if (has_hf28 && dgp.is_forced_min_price) {
                return;
            }

            auto interest = acnt.savings_sbd_seconds /
                            STEEMIT_SECONDS_PER_YEAR;
            interest *= dgp.sbd_interest_rate;
            interest /= STEEMIT_100_PERCENT;
            asset interest_paid(interest.to_uint64(), SBD_SYMBOL);
            acnt.savings_sbd_balance += interest_paid;
            acnt.savings_sbd_seconds = 0;
            acnt.savings_sbd_last_interest_payment = head_block_time();

            push_virtual_operation(interest_operation(acnt.name, interest_paid));

            modify(dgp, [&](dynamic_global_property_object &props) {
                props.current_sbd_supply += interest_paid;
                props.virtual_supply += interest_paid *
                                        get_feed_history().current_median_history;
            });
        }

        void database::adjust_savings_balance(const account_object &a, const asset &delta) {
            modify(a, [&](account_object &acnt) {
                if (delta.symbol == STEEM_SYMBOL) {
                    acnt.savings_balance += delta;
                } else if (delta.symbol == SBD_SYMBOL) {
                    if (a.savings_sbd_seconds_last_update !=
                        head_block_time()) {
                        acnt.savings_sbd_seconds +=
                                fc::uint128_t(a.savings_sbd_balance.amount.value) *
                                (head_block_time() -
                                 a.savings_sbd_seconds_last_update).to_seconds();
                        acnt.savings_sbd_seconds_last_update = head_block_time();

                        if (!has_hardfork(STEEMIT_HARDFORK_0_28__219)) {
                            pay_savings_interest(acnt);
                        }
                    }
                    acnt.savings_sbd_balance += delta;
                } else {
                    GOLOS_CHECK_VALUE(false, "invalid symbol");
                }
            });
        }

        void database::adjust_market_balance(const account_object& a, const asset& delta) {
            if (delta.symbol == STEEM_SYMBOL) {
                modify(a, [&](auto& acnt) {acnt.market_balance += delta;});
            } else if (delta.symbol == SBD_SYMBOL) {
                modify(a, [&](auto& acnt) {acnt.market_sbd_balance += delta;});
            } else {
                GOLOS_CHECK_VALUE(has_hardfork(STEEMIT_HARDFORK_0_24__95), "invalid symbol");
                adjust_account_balance(a.name, asset(0, delta.symbol), asset(0, delta.symbol), delta);
            }
        }


        void database::adjust_supply(const asset &delta, bool adjust_vesting) {

            const auto &props = get_dynamic_global_properties();
            if (props.head_block_number < STEEMIT_BLOCKS_PER_DAY * 7) {
                adjust_vesting = false;
            }

            modify(props, [&](dynamic_global_property_object& props) {
                if (delta.symbol == STEEM_SYMBOL) {
                    asset new_vesting((adjust_vesting && delta.amount > 0) ?
                                      delta.amount * 9 : 0, STEEM_SYMBOL);
                    props.current_supply += delta + new_vesting;
                    props.virtual_supply += delta + new_vesting;
                    props.total_vesting_fund_steem += new_vesting;
                    assert(props.current_supply.amount.value >= 0);
                } else if (delta.symbol == SBD_SYMBOL) {
                    props.current_sbd_supply += delta;
                    props.virtual_supply = props.current_sbd_supply *
                                           get_feed_history().current_median_history +
                                           props.current_supply;
                    assert(props.current_sbd_supply.amount.value >= 0);
                } else {
                    GOLOS_CHECK_VALUE(false, "invalid symbol");
                }
            });
        }


        asset database::get_balance(const account_object& a, asset_symbol_type symbol) const {
            if (symbol == STEEM_SYMBOL) {
                return a.balance;
            } else if (symbol == SBD_SYMBOL) {
                return a.sbd_balance;
            } else {
                GOLOS_CHECK_VALUE(false, "invalid symbol");
            }
        }

        asset database::get_savings_balance(const account_object &a, asset_symbol_type symbol) const {
            if (symbol == STEEM_SYMBOL) {
                return a.savings_balance;
            } else if (symbol == SBD_SYMBOL) {
                return a.savings_sbd_balance;
            } else {
                GOLOS_CHECK_VALUE(false, "invalid symbol");
            }
        }

        void database::init_hardforks() {
            _hardfork_times[0] = fc::time_point_sec(STEEMIT_GENESIS_TIME);
            _hardfork_versions[0] = hardfork_version(0, 0);
            FC_ASSERT(STEEMIT_HARDFORK_0_1 == 1, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_1] = fc::time_point_sec(STEEMIT_HARDFORK_0_1_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_1] = STEEMIT_HARDFORK_0_1_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_2 == 2, "Invlaid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_2] = fc::time_point_sec(STEEMIT_HARDFORK_0_2_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_2] = STEEMIT_HARDFORK_0_2_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_3 == 3, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_3] = fc::time_point_sec(STEEMIT_HARDFORK_0_3_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_3] = STEEMIT_HARDFORK_0_3_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_4 == 4, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_4] = fc::time_point_sec(STEEMIT_HARDFORK_0_4_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_4] = STEEMIT_HARDFORK_0_4_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_5 == 5, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_5] = fc::time_point_sec(STEEMIT_HARDFORK_0_5_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_5] = STEEMIT_HARDFORK_0_5_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_6 == 6, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_6] = fc::time_point_sec(STEEMIT_HARDFORK_0_6_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_6] = STEEMIT_HARDFORK_0_6_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_7 == 7, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_7] = fc::time_point_sec(STEEMIT_HARDFORK_0_7_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_7] = STEEMIT_HARDFORK_0_7_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_8 == 8, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_8] = fc::time_point_sec(STEEMIT_HARDFORK_0_8_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_8] = STEEMIT_HARDFORK_0_8_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_9 == 9, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_9] = fc::time_point_sec(STEEMIT_HARDFORK_0_9_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_9] = STEEMIT_HARDFORK_0_9_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_10 == 10, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_10] = fc::time_point_sec(STEEMIT_HARDFORK_0_10_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_10] = STEEMIT_HARDFORK_0_10_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_11 == 11, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_11] = fc::time_point_sec(STEEMIT_HARDFORK_0_11_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_11] = STEEMIT_HARDFORK_0_11_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_12 == 12, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_12] = fc::time_point_sec(STEEMIT_HARDFORK_0_12_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_12] = STEEMIT_HARDFORK_0_12_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_13 == 13, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_13] = fc::time_point_sec(STEEMIT_HARDFORK_0_13_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_13] = STEEMIT_HARDFORK_0_13_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_14 == 14, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_14] = fc::time_point_sec(STEEMIT_HARDFORK_0_14_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_14] = STEEMIT_HARDFORK_0_14_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_15 == 15, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_15] = fc::time_point_sec(STEEMIT_HARDFORK_0_15_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_15] = STEEMIT_HARDFORK_0_15_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_16 == 16, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_16] = fc::time_point_sec(STEEMIT_HARDFORK_0_16_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_16] = STEEMIT_HARDFORK_0_16_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_17 == 17, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_17] = fc::time_point_sec(STEEMIT_HARDFORK_0_17_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_17] = STEEMIT_HARDFORK_0_17_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_18 == 18, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_18] = fc::time_point_sec(STEEMIT_HARDFORK_0_18_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_18] = STEEMIT_HARDFORK_0_18_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_19 == 19, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_19] = fc::time_point_sec(STEEMIT_HARDFORK_0_19_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_19] = STEEMIT_HARDFORK_0_19_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_20 == 20, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_20] = fc::time_point_sec(STEEMIT_HARDFORK_0_20_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_20] = STEEMIT_HARDFORK_0_20_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_21 == 21, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_21] = fc::time_point_sec(STEEMIT_HARDFORK_0_21_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_21] = STEEMIT_HARDFORK_0_21_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_22 == 22, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_22] = fc::time_point_sec(STEEMIT_HARDFORK_0_22_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_22] = STEEMIT_HARDFORK_0_22_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_23 == 23, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_23] = fc::time_point_sec(STEEMIT_HARDFORK_0_23_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_23] = STEEMIT_HARDFORK_0_23_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_24 == 24, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_24] = fc::time_point_sec(STEEMIT_HARDFORK_0_24_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_24] = STEEMIT_HARDFORK_0_24_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_25 == 25, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_25] = fc::time_point_sec(STEEMIT_HARDFORK_0_25_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_25] = STEEMIT_HARDFORK_0_25_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_26 == 26, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_26] = fc::time_point_sec(STEEMIT_HARDFORK_0_26_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_26] = STEEMIT_HARDFORK_0_26_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_27 == 27, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_27] = fc::time_point_sec(STEEMIT_HARDFORK_0_27_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_27] = STEEMIT_HARDFORK_0_27_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_28 == 28, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_28] = fc::time_point_sec(STEEMIT_HARDFORK_0_28_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_28] = STEEMIT_HARDFORK_0_28_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_29 == 29, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_29] = fc::time_point_sec(STEEMIT_HARDFORK_0_29_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_29] = STEEMIT_HARDFORK_0_29_VERSION;
            FC_ASSERT(STEEMIT_HARDFORK_0_30 == 30, "Invalid hardfork configuration");
            _hardfork_times[STEEMIT_HARDFORK_0_30] = fc::time_point_sec(STEEMIT_HARDFORK_0_30_TIME);
            _hardfork_versions[STEEMIT_HARDFORK_0_30] = STEEMIT_HARDFORK_0_30_VERSION;

            const auto &hardforks = get_hardfork_property_object();
            FC_ASSERT(
                hardforks.last_hardfork <= STEEMIT_NUM_HARDFORKS,
                "Chain knows of more hardforks than configuration",
                ("hardforks.last_hardfork", hardforks.last_hardfork)
                ("STEEMIT_NUM_HARDFORKS", STEEMIT_NUM_HARDFORKS));
            FC_ASSERT(
                _hardfork_versions[hardforks.last_hardfork] <= STEEMIT_BLOCKCHAIN_VERSION,
                "Blockchain version is older than last applied hardfork");
            FC_ASSERT(STEEMIT_BLOCKCHAIN_HARDFORK_VERSION == _hardfork_versions[STEEMIT_NUM_HARDFORKS]);
        }

        void database::reset_virtual_schedule_time() {
            const witness_schedule_object &wso = get_witness_schedule_object();
            modify(wso, [&](witness_schedule_object &o) {
                o.current_virtual_time = fc::uint128_t(); // reset it 0
            });

            const auto &idx = get_index<witness_index>().indices();
            for (const auto &witness : idx) {
                modify(witness, [&](witness_object &wobj) {
                    wobj.virtual_position = fc::uint128_t();
                    wobj.virtual_last_update = wso.current_virtual_time;
                    wobj.virtual_scheduled_time = VIRTUAL_SCHEDULE_LAP_LENGTH2 /
                                                  (wobj.votes.value + 1);
                });
            }
        }

        void database::process_hardforks() {
            try {
                // If there are upcoming hardforks and the next one is later, do nothing
                const auto &hardforks = get_hardfork_property_object();

                if (has_hardfork(STEEMIT_HARDFORK_0_5__54)) {
                    while (_hardfork_versions[hardforks.last_hardfork] <
                           hardforks.next_hardfork
                           &&
                           hardforks.next_hardfork_time <= head_block_time()) {
                        if (hardforks.last_hardfork < STEEMIT_NUM_HARDFORKS) {
                            apply_hardfork(hardforks.last_hardfork + 1);
                        } else {
                            throw unknown_hardfork_exception();
                        }
                    }
                } else {
                    while (hardforks.last_hardfork < STEEMIT_NUM_HARDFORKS
                           && _hardfork_times[hardforks.last_hardfork + 1] <=
                              head_block_time()
                           &&
                           hardforks.last_hardfork < STEEMIT_HARDFORK_0_5__54) {
                        apply_hardfork(hardforks.last_hardfork + 1);
                    }
                }
            }
            FC_CAPTURE_AND_RETHROW()
        }

        bool database::has_hardfork(uint32_t hardfork) const {
            return get_hardfork_property_object().processed_hardforks.size() >
                   hardfork;
        }

        void database::set_hardfork(uint32_t hardfork, bool apply_now) {
            auto const &hardforks = get_hardfork_property_object();

            for (uint32_t i = hardforks.last_hardfork + 1;
                 i <= hardfork && i <= STEEMIT_NUM_HARDFORKS; i++) {
                if (i <= STEEMIT_HARDFORK_0_5__54) {
                    _hardfork_times[i] = head_block_time();
                } else {
                    modify(hardforks, [&](hardfork_property_object &hpo) {
                        hpo.next_hardfork = _hardfork_versions[i];
                        hpo.next_hardfork_time = head_block_time();
                    });
                }

                if (apply_now) {
                    apply_hardfork(i);
                }
            }
        }

        void database::apply_hardfork(uint32_t hardfork) {
            if (_log_hardforks) {
                elog("HARDFORK ${hf} at block ${b}", ("hf", hardfork)("b", head_block_num()));
            }

            hf_actions hf_act(*this);

            switch (hardfork) {
                case STEEMIT_HARDFORK_0_1:
                    perform_vesting_share_split(10000);
#ifdef STEEMIT_BUILD_TESTNET
                {
                    custom_operation test_op;
                    string op_msg = "Testnet: Hardfork applied";
                    test_op.data = vector<char>(op_msg.begin(), op_msg.end());
                    test_op.required_auths.insert(STEEMIT_INIT_MINER_NAME);
                    operation op = test_op;   // we need the operation object to live to the end of this scope
                    operation_notification note(op);
                    notify_pre_apply_operation(note);
                    notify_post_apply_operation(note);
                }
                break;
#endif
                    break;
                case STEEMIT_HARDFORK_0_2:
                    retally_witness_votes();
                    break;
                case STEEMIT_HARDFORK_0_3:
                    retally_witness_votes();
                    break;
                case STEEMIT_HARDFORK_0_4:
                    reset_virtual_schedule_time();
                    break;
                case STEEMIT_HARDFORK_0_5:
                    break;
                case STEEMIT_HARDFORK_0_6:
                    retally_witness_vote_counts();
                    retally_comment_children();
                    break;
                case STEEMIT_HARDFORK_0_7:
                    break;
                case STEEMIT_HARDFORK_0_8:
                    retally_witness_vote_counts(true);
                    break;
                case STEEMIT_HARDFORK_0_9: {

                }
                    break;
                case STEEMIT_HARDFORK_0_10:
                    retally_liquidity_weight();
                    break;
                case STEEMIT_HARDFORK_0_11:
                    break;
                case STEEMIT_HARDFORK_0_12: {
                    const auto &comment_idx = get_index<comment_index>().indices();

                    for (auto itr = comment_idx.begin();
                         itr != comment_idx.end(); ++itr) {
                        // At the hardfork time, all new posts with no votes get their cashout time set to +12 hrs from head block time.
                        // All posts with a payout get their cashout time set to +30 days. This hardfork takes place within 30 days
                        // initial payout so we don't have to handle the case of posts that should be frozen that aren't
                        if (itr->parent_author == STEEMIT_ROOT_POST_PARENT) {
                            // Post has not been paid out and has no votes (cashout_time == 0 === net_rshares == 0, under current semmantics)
                            if (itr->last_payout == fc::time_point_sec::min() &&
                                itr->cashout_time ==
                                fc::time_point_sec::maximum()) {
                                modify(*itr, [&](comment_object &c) {
                                    c.cashout_time = head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17;
                                    c.mode = first_payout;
                                });
                            }
                                // Has been paid out, needs to be on second cashout window
                            else if (itr->last_payout > fc::time_point_sec()) {
                                modify(*itr, [&](comment_object &c) {
                                    c.cashout_time = c.last_payout + STEEMIT_SECOND_CASHOUT_WINDOW;
                                    c.mode = second_payout;
                                });
                            }
                        }
                    }

                    modify(get_authority(STEEMIT_MINER_ACCOUNT), [&](account_authority_object &auth) {
                        auth.posting = authority();
                        auth.posting.weight_threshold = 1;
                    });

                    modify(get_authority(STEEMIT_NULL_ACCOUNT), [&](account_authority_object &auth) {
                        auth.posting = authority();
                        auth.posting.weight_threshold = 1;
                    });

                    modify(get_authority(STEEMIT_TEMP_ACCOUNT), [&](account_authority_object &auth) {
                        auth.posting = authority();
                        auth.posting.weight_threshold = 1;
                    });
                }
                    break;
                case STEEMIT_HARDFORK_0_13:
                    break;
                case STEEMIT_HARDFORK_0_14:
                    break;
                case STEEMIT_HARDFORK_0_15:
                    break;
                case STEEMIT_HARDFORK_0_16:
                    modify(get_feed_history(), [&](feed_history_object &fho) {
                        while (fho.price_history.size() >
                               STEEMIT_FEED_HISTORY_WINDOW) {
                            fho.price_history.pop_front();
                        }
                    });

                    for (const std::string &acc : hardfork16::get_compromised_accounts()) {
                        const account_object *account = find_account(acc);
                        if (account == nullptr) {
                            continue;
                        }

                        update_owner_authority(*account, authority(1, public_key_type("GLS8hLtc7rC59Ed7uNVVTXtF578pJKQwMfdTvuzYLwUi8GkNTh5F6"), 1));

                        modify(get_authority(account->name), [&](account_authority_object &auth) {
                            auth.active = authority(1, public_key_type("GLS8hLtc7rC59Ed7uNVVTXtF578pJKQwMfdTvuzYLwUi8GkNTh5F6"), 1);
                            auth.posting = authority(1, public_key_type("GLS8hLtc7rC59Ed7uNVVTXtF578pJKQwMfdTvuzYLwUi8GkNTh5F6"), 1);
                        });
                    }
                    break;
                case STEEMIT_HARDFORK_0_17: {
                    /*
                     * For all current comments we will either keep their current cashout time, or extend it to 1 week
                     * after creation.
                     *
                     * We cannot do a simple iteration by cashout time because we are editting cashout time.
                     * More specifically, we will be adding an explicit cashout time to all comments with parents.
                     * To find all discussions that have not been paid out we fir iterate over posts by cashout time.
                     * Before the hardfork these are all root posts. Iterate over all of their children, adding each
                     * to a specific list. Next, update payout times for all discussions on the root post. This defines
                     * the min cashout time for each child in the discussion. Then iterate over the children and set
                     * their cashout time in a similar way, grabbing the root post as their inherent cashout time.
                     */
                    const auto &by_time_idx = get_index<comment_index, by_cashout_time>();
                    const auto &by_root_idx = get_index<comment_index, by_root>();
                    const auto max_cashout_time = head_block_time();

                    std::vector<comment_object::id_type> root_posts;
                    root_posts.reserve(STEEMIT_HF_17_NUM_POSTS);

                    for (const auto &comment: by_time_idx) {
                        if (comment.cashout_time == fc::time_point_sec::maximum()) {
                            break;
                        }
                        root_posts.push_back(comment.id);
                    }

                    for (const auto &id: root_posts) {
                        auto itr = by_root_idx.lower_bound(id);
                        for (; itr != by_root_idx.end() && itr->root_comment == id; ++itr) {
                            modify(*itr, [&](comment_object &c) {
                                // limit second cashout window to 1 week, or a current block time
                                c.cashout_time = std::max(c.created + STEEMIT_CASHOUT_WINDOW_SECONDS, max_cashout_time);
                            });

                            if (itr->net_rshares.value > 0) {
                                auto old_rshares2 = calculate_vshares_quadratic(
                                        itr->net_rshares.value, get_content_constant_s());
                                auto new_rshares2 = calculate_vshares_linear(itr->net_rshares.value);
                                adjust_rshares2(*itr, old_rshares2, new_rshares2);
                            }
                        }
                    }}
                    break;
                case STEEMIT_HARDFORK_0_18:
                    break;
                case STEEMIT_HARDFORK_0_19:
                    break;
                case STEEMIT_HARDFORK_0_20:
                    break;
                case STEEMIT_HARDFORK_0_21:
                    break;
                case STEEMIT_HARDFORK_0_22: {
                    hf_act.create_worker_pool();
                    retally_witness_votes_hf22();
                    check_witness_idleness(false);
                    } break;
                case STEEMIT_HARDFORK_0_23:
                    modify(get_account(STEEMIT_WORKER_POOL_ACCOUNT), [&](auto& acnt) {
                        acnt.recovery_account = account_name_type();
                    });
                    for (const auto& acnt : get_index<account_index>().indices()) {
                        const auto now = head_block_time();
                        modify(acnt, [&](auto& acnt) {
                            acnt.last_claim = now;
                        });
                    }
                    break;
                case STEEMIT_HARDFORK_0_24:
                    {
                        auto* bittrex = find_account("bittrex");
                        if (bittrex) {
                            auto& null = get_account(STEEMIT_NULL_ACCOUNT);
                            push_virtual_operation(internal_transfer_operation("bittrex", STEEMIT_NULL_ACCOUNT, bittrex->balance, "Burning tokens out of circulation"));
                            adjust_balance(null, bittrex->balance);
                            adjust_balance(*bittrex, -bittrex->balance);
                            push_virtual_operation(internal_transfer_operation("bittrex", STEEMIT_NULL_ACCOUNT, bittrex->sbd_balance, "Burning tokens out of circulation"));
                            adjust_balance(null, bittrex->sbd_balance);
                            adjust_balance(*bittrex, -bittrex->sbd_balance);
                        }
                    }
                    break;
                case STEEMIT_HARDFORK_0_25:
                    break;
                case STEEMIT_HARDFORK_0_26:
                    update_witness_windows_sec_to_min();
                    break;
                case STEEMIT_HARDFORK_0_27:
                    hf_act.create_registrator_account();
                    break;
                case STEEMIT_HARDFORK_0_28:
                    hf_act.convert_min_curate_golos_power();
                    break;
                case STEEMIT_HARDFORK_0_30:
                    hf_act.prepare_for_tests();
                    break;
                default:
                    break;
            }

            modify(get_hardfork_property_object(), [&](hardfork_property_object &hfp) {
                FC_ASSERT(hardfork == hfp.last_hardfork +
                                      1, "Hardfork being applied out of order", ("hardfork", hardfork)("hfp.last_hardfork", hfp.last_hardfork));
                FC_ASSERT(hfp.processed_hardforks.size() ==
                          hardfork, "Hardfork being applied out of order");
                hfp.processed_hardforks.push_back(_hardfork_times[hardfork]);
                hfp.last_hardfork = hardfork;
                hfp.current_hardfork_version = _hardfork_versions[hardfork];
                FC_ASSERT(hfp.processed_hardforks[hfp.last_hardfork] ==
                          _hardfork_times[hfp.last_hardfork], "Hardfork processing failed sanity check...");
                if (hardfork == STEEMIT_HARDFORK_0_27) {
                    hfp.hf27_applied_block = head_block_num();
                }
            });

            push_virtual_operation(hardfork_operation(hardfork), true);
        }

        void database::retally_liquidity_weight() {
            const auto &ridx = get_index<liquidity_reward_balance_index>().indices().get<by_owner>();
            for (const auto &i : ridx) {
                modify(i, [](liquidity_reward_balance_object &o) {
                    o.update_weight(true/*HAS HARDFORK10 if this method is called*/);
                });
            }
        }

/**
 * Verifies all supply invariantes check out
 */
        void database::validate_invariants() const {
            try {
                const auto &account_idx = get_index<account_index>().indices().get<by_name>();
                asset total_supply = asset(0, STEEM_SYMBOL);
                asset total_sbd = asset(0, SBD_SYMBOL);
                asset total_vesting = asset(0, VESTS_SYMBOL);
                share_type total_vsf_votes = share_type(0);

                auto gpo = get_dynamic_global_properties();

                /// verify no witness has too many votes
                const auto &witness_idx = get_index<witness_index>().indices();
                for (auto itr = witness_idx.begin();
                     itr != witness_idx.end(); ++itr)
                    FC_ASSERT(itr->votes <
                              gpo.total_vesting_shares.amount, "", ("itr", *itr));

                for (auto itr = account_idx.begin();
                     itr != account_idx.end(); ++itr) {
                    total_supply += itr->accumulative_balance;
                    total_supply += itr->balance;
                    total_supply += itr->savings_balance;
                    total_supply += itr->tip_balance;
                    total_sbd += itr->sbd_balance;
                    total_sbd += itr->savings_sbd_balance;
                    total_vesting += itr->vesting_shares;
                    total_vsf_votes += (itr->proxy ==
                                        STEEMIT_PROXY_TO_SELF_ACCOUNT ?
                                        itr->witness_vote_weight() :
                                        (STEEMIT_MAX_PROXY_RECURSION_DEPTH > 0 ?
                                         itr->proxied_vsf_votes[
                                                 STEEMIT_MAX_PROXY_RECURSION_DEPTH -
                                                 1] :
                                         itr->vesting_shares.amount));
                }

                const auto &convert_request_idx = get_index<convert_request_index>().indices();

                for (auto itr = convert_request_idx.begin();
                     itr != convert_request_idx.end(); ++itr) {
                    if (itr->amount.symbol == STEEM_SYMBOL) {
                        total_supply += itr->amount;
                    } else if (itr->amount.symbol == SBD_SYMBOL) {
                        total_sbd += itr->amount;
                    } else
                        FC_ASSERT(false, "Encountered illegal symbol in convert_request_object");
                }

                const auto &limit_order_idx = get_index<limit_order_index>().indices();

                for (auto itr = limit_order_idx.begin();
                     itr != limit_order_idx.end(); ++itr) {
                    if (itr->sell_price.base.symbol == STEEM_SYMBOL) {
                        total_supply += asset(itr->for_sale, STEEM_SYMBOL);
                    } else if (itr->sell_price.base.symbol == SBD_SYMBOL) {
                        total_sbd += asset(itr->for_sale, SBD_SYMBOL);
                    }
                }

                const auto &escrow_idx = get_index<escrow_index>().indices().get<by_id>();

                for (auto itr = escrow_idx.begin();
                     itr != escrow_idx.end(); ++itr) {
                    total_supply += itr->steem_balance;
                    total_sbd += itr->sbd_balance;

                    if (itr->pending_fee.symbol == STEEM_SYMBOL) {
                        total_supply += itr->pending_fee;
                    } else if (itr->pending_fee.symbol == SBD_SYMBOL) {
                        total_sbd += itr->pending_fee;
                    } else
                        FC_ASSERT(false, "found escrow pending fee that is not SBD or STEEM");
                }

                const auto &savings_withdraw_idx = get_index<savings_withdraw_index>().indices().get<by_id>();

                for (auto itr = savings_withdraw_idx.begin();
                     itr != savings_withdraw_idx.end(); ++itr) {
                    if (itr->amount.symbol == STEEM_SYMBOL) {
                        total_supply += itr->amount;
                    } else if (itr->amount.symbol == SBD_SYMBOL) {
                        total_sbd += itr->amount;
                    } else
                        FC_ASSERT(false, "found savings withdraw that is not SBD or STEEM");
                }

                const auto& psro_idx = get_index<paid_subscriber_index, by_id>();
                for (auto itr = psro_idx.begin();
                     itr != psro_idx.end(); ++itr) {
                    if (itr->prepaid.symbol == STEEM_SYMBOL) {
                        total_supply += itr->prepaid;
                    } else if (itr->prepaid.symbol == SBD_SYMBOL) {
                        total_sbd += itr->prepaid;
                    }
                }

                const auto& noo_idx = get_index<nft_order_index, by_id>();
                for (auto itr = noo_idx.begin();
                     itr != noo_idx.end(); ++itr) {
                    if (!itr->holds) continue;
                    if (itr->price.symbol == STEEM_SYMBOL) {
                        total_supply += itr->price;
                    } else if (itr->price.symbol == SBD_SYMBOL) {
                        total_sbd += itr->price;
                    }
                }

                const auto& nbo_idx = get_index<nft_bet_index, by_id>();
                for (auto itr = nbo_idx.begin();
                     itr != nbo_idx.end(); ++itr) {
                    if (itr->price.symbol == STEEM_SYMBOL) {
                        total_supply += itr->price;
                    } else if (itr->price.symbol == SBD_SYMBOL) {
                        total_sbd += itr->price;
                    }
                }

                fc::uint128_t total_rshares2;
                fc::uint128_t total_children_rshares2;

                const auto &comment_idx = get_index<comment_index>().indices();

                for (auto itr = comment_idx.begin();
                     itr != comment_idx.end(); ++itr) {
                    if (itr->net_rshares.value > 0) {
                        auto delta = calculate_vshares(itr->net_rshares.value);
                        total_rshares2 += delta;
                    }
                    if (itr->parent_author == STEEMIT_ROOT_POST_PARENT) {
                        const auto* ex = find_extras(itr->author, itr->hashlink);
                        if (ex) {
                            total_children_rshares2 += ex->children_rshares2;
                        }
                    }
                }

                total_supply += gpo.total_vesting_fund_steem +
                                gpo.accumulative_balance +
                                gpo.accumulative_remainder +
                                gpo.total_reward_fund_steem;

                FC_ASSERT(gpo.current_supply ==
                          total_supply, "", ("gpo.current_supply", gpo.current_supply)("total_supply", total_supply));
                FC_ASSERT(gpo.current_sbd_supply ==
                          total_sbd, "", ("gpo.current_sbd_supply", gpo.current_sbd_supply)("total_sbd", total_sbd));
                FC_ASSERT(gpo.total_vesting_shares ==
                          total_vesting, "", ("gpo.total_vesting_shares", gpo.total_vesting_shares)("total_vesting", total_vesting));
                FC_ASSERT(gpo.total_vesting_shares.amount ==
                          total_vsf_votes, "", ("total_vesting_shares", gpo.total_vesting_shares)("total_vsf_votes", total_vsf_votes));
                FC_ASSERT(gpo.total_reward_shares2 ==
                          total_rshares2, "", ("gpo.total", gpo.total_reward_shares2)("check.total", total_rshares2)("delta",
                        gpo.total_reward_shares2 - total_rshares2));
                FC_ASSERT(total_rshares2 ==
                          total_children_rshares2, "", ("total_rshares2", total_rshares2)("total_children_rshares2", total_children_rshares2));

                FC_ASSERT(gpo.virtual_supply >= gpo.current_supply);
                if (!get_feed_history().current_median_history.is_null()) {
                    FC_ASSERT(gpo.current_sbd_supply *
                              get_feed_history().current_median_history +
                              gpo.current_supply
                              ==
                              gpo.virtual_supply, "", ("gpo.current_sbd_supply", gpo.current_sbd_supply)("get_feed_history().current_median_history", get_feed_history().current_median_history)("gpo.current_supply", gpo.current_supply)("gpo.virtual_supply", gpo.virtual_supply));
                }
            }
            FC_CAPTURE_LOG_AND_RETHROW((head_block_num()));
        }

        void database::perform_vesting_share_split(uint32_t magnitude) {
            try {
                modify(get_dynamic_global_properties(), [&](dynamic_global_property_object &d) {
                    d.total_vesting_shares.amount *= magnitude;
                    d.total_reward_shares2 = 0;
                });

                // Need to update all VESTS in accounts and the total VESTS in the dgpo
                for (const auto &account : get_index<account_index>().indices()) {
                    modify(account, [&](account_object &a) {
                        a.vesting_shares.amount *= magnitude;
                        a.withdrawn *= magnitude;
                        a.to_withdraw *= magnitude;
                        a.vesting_withdraw_rate = asset(a.to_withdraw /
                                                        STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_16, VESTS_SYMBOL);
                        if (a.vesting_withdraw_rate.amount == 0) {
                            a.vesting_withdraw_rate.amount = 1;
                        }

                        for (uint32_t i = 0;
                             i < STEEMIT_MAX_PROXY_RECURSION_DEPTH; ++i) {
                            a.proxied_vsf_votes[i] *= magnitude;
                        }
                    });
                }

                const auto &comments = get_index<comment_index>().indices();
                for (const auto &comment : comments) {
                    modify(comment, [&](comment_object &c) {
                        c.net_rshares *= magnitude;
                        c.abs_rshares *= magnitude;
                        c.vote_rshares *= magnitude;
                    });
                    auto* extras = find_extras(comment.author, comment.hashlink);
                    if (extras) {
                        modify(*extras, [&](auto& ex) {
                            ex.children_rshares2 = 0;
                        });
                    }
                }

                for (const auto &c : comments) {
                    if (c.net_rshares.value > 0) {
                        adjust_rshares2(c, 0, calculate_vshares(c.net_rshares.value));
                    }
                }

            }
            FC_CAPTURE_AND_RETHROW()
        }

        void database::retally_comment_children() {
            const auto &cidx = get_index<comment_index>().indices();

            // Clear children counts
            for (auto itr = cidx.begin(); itr != cidx.end(); ++itr) {
                modify(*itr, [&](comment_object &c) {
                    c.children = 0;
                });
            }

            for (auto itr = cidx.begin(); itr != cidx.end(); ++itr) {
                if (itr->parent_author != STEEMIT_ROOT_POST_PARENT) {

                    const comment_object *parent = &get_comment(itr->parent_author, itr->parent_hashlink);
                    while (parent) {
                        modify(*parent, [&](comment_object &c) {
                            c.children++;
                        });

                        if (parent->parent_author != STEEMIT_ROOT_POST_PARENT) {
                            parent = &get_comment(parent->parent_author, parent->parent_hashlink);
                        } else {
                            parent = nullptr;
                        }
                    }

                }
            }
        }

        void database::retally_witness_votes() {
            const auto &witness_idx = get_index<witness_index>().indices();

            // Clear all witness votes
            for (auto itr = witness_idx.begin();
                 itr != witness_idx.end(); ++itr) {
                modify(*itr, [&](witness_object &w) {
                    w.votes = 0;
                    w.virtual_position = 0;
                });
            }

            const auto &account_idx = get_index<account_index>().indices();

            // Apply all existing votes by account
            for (auto itr = account_idx.begin();
                 itr != account_idx.end(); ++itr) {
                if (itr->proxy != STEEMIT_PROXY_TO_SELF_ACCOUNT) {
                    continue;
                }

                const auto &a = *itr;

                const auto &vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
                auto wit_itr = vidx.lower_bound(boost::make_tuple(a.id, witness_id_type()));
                while (wit_itr != vidx.end() && wit_itr->account == a.id) {
                    adjust_witness_vote(a, get(wit_itr->witness), a.witness_vote_weight(), true);
                    ++wit_itr;
                }
            }
        }

        void database::retally_witness_votes_hf22() {
            const auto& witness_idx = get_index<witness_index>().indices();

            // Clear all witness votes
            for (auto itr = witness_idx.begin(); itr != witness_idx.end(); ++itr) {
                modify(*itr, [&](auto& w) {
                    w.votes = 0;
                    w.virtual_position = 0;
                });
            }

            const auto& account_idx = get_index<account_index>().indices();
            const auto& vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();

            // Apply all existing votes by account
            for (auto itr = account_idx.begin(); itr != account_idx.end(); ++itr) {
                if (itr->proxy != STEEMIT_PROXY_TO_SELF_ACCOUNT) {
                    continue;
                }

                const auto& a = *itr;
                auto weight = a.witness_vote_weight() / std::max(a.witnesses_voted_for, uint16_t(1));

                auto wit_itr = vidx.lower_bound(std::make_tuple(a.id, witness_id_type()));
                while (wit_itr != vidx.end() && wit_itr->account == a.id) {
                    adjust_witness_vote(a, get(wit_itr->witness), weight, true);
                    ++wit_itr;
                }
            }
        }

        void database::retally_witness_vote_counts(bool force) {
            const auto &account_idx = get_index<account_index>().indices();

            // Check all existing votes by account
            for (auto itr = account_idx.begin();
                 itr != account_idx.end(); ++itr) {
                const auto &a = *itr;
                uint16_t witnesses_voted_for = 0;
                if (force || (a.proxy != STEEMIT_PROXY_TO_SELF_ACCOUNT)) {
                    const auto &vidx = get_index<witness_vote_index>().indices().get<by_account_witness>();
                    auto wit_itr = vidx.lower_bound(boost::make_tuple(a.id, witness_id_type()));
                    while (wit_itr != vidx.end() && wit_itr->account == a.id) {
                        ++witnesses_voted_for;
                        ++wit_itr;
                    }
                }
                if (a.witnesses_voted_for != witnesses_voted_for) {
                    modify(a, [&](account_object &account) {
                        account.witnesses_voted_for = witnesses_voted_for;
                    });
                }
            }
        }

        void database::transfer_gbg(const account_object &account, const account_object &receiver) {
            elog("------- transfer gbg -${sbd}/${ssbd} ${acc}", ("sbd", account.sbd_balance)("ssbd", account.savings_sbd_balance)("acc", account.sbd_balance));
            auto sbd_balance = account.sbd_balance;
            adjust_balance(account, -1 * account.sbd_balance);
            elog("------- reduce sbd_balance -${sbd} ${acc}", ("sbd", sbd_balance)("acc", account.sbd_balance));
            //GBG interest
            sbd_balance += account.sbd_balance;
            adjust_balance(account, -1 * account.sbd_balance);
            elog("------- reduce sbd_balance -${sbd} ${acc}", ("sbd", sbd_balance)("acc", account.sbd_balance));
            //GBG from savings
            sbd_balance += account.savings_sbd_balance;
            adjust_savings_balance(account, -1 * account.savings_sbd_balance);
            elog("------- reduce savings sbd_balance -${sbd} ${acc}", ("sbd", sbd_balance)("acc", account.savings_sbd_balance));
            //GBG interest from savings
            sbd_balance += account.savings_sbd_balance;
            adjust_savings_balance(account, -1 * account.savings_sbd_balance);
            elog("------- reduce savings sbd_balance -${sbd} ${acc}", ("sbd", sbd_balance)("acc", account.savings_sbd_balance));

            adjust_balance(receiver, sbd_balance);
            elog("---- add sbd balance receiver ${acc} - ${receiver}", ("acc", sbd_balance)("receiver", receiver.sbd_balance));
        }

        void database::transfer_golos(const account_object &account, const account_object &receiver) {
            elog("---- add balance receiver ${acc}/${sacc} - ${receiver}", ("acc", account.balance)("sacc", account.savings_balance)("receiver", receiver.balance));
            //GOLOS
            auto balance = account.balance;
            adjust_balance(account, -1 * account.balance);
            //GOLOS from savings
            balance += account.savings_balance;
            adjust_savings_balance(account, -1 * account.savings_balance);

            adjust_balance(receiver, balance);
            elog("---- after add balance receiver ${acc} - ${receiver}", ("acc", account.balance)("receiver", receiver.balance));
        }

        void database::terminate_vesting_activities(const account_object &account) {
            //return delegate
            {
                const auto& vdo_idx = get_index<vesting_delegation_index>().indices().get<by_delegation>();
                auto itr = vdo_idx.lower_bound(std::make_tuple(account.name, std::string()));

                while (itr != vdo_idx.end() && itr->delegator == account.name) {
                    modify(get_account(itr->delegatee), [&](account_object& a) {
                        a.received_vesting_shares -= itr->vesting_shares;
                    });

                    modify(account, [&](account_object& a) {
                        a.delegated_vesting_shares = asset(0, VESTS_SYMBOL);
                    });

                    elog("---- remove delegation ${acc} - ${vestings}", ("acc", itr->delegator)("vestings", itr->vesting_shares));
                    remove(*itr);
                    itr = vdo_idx.lower_bound(std::make_tuple(account.name, std::string()));
                }
            }
            //expired delegations
            {
                const auto& vdo_idx = get_index<vesting_delegation_expiration_index, by_account_expiration>();
                auto itr = vdo_idx.lower_bound(std::make_tuple(account.name, time_point_sec::min()));

                while (itr != vdo_idx.end() && itr->delegator == account.name) {
                    modify(account, [&](account_object& a) {
                        a.delegated_vesting_shares = asset(0, VESTS_SYMBOL);
                    });

                    elog("---- remove expiring delegation ${acc} - ${vestings}", ("acc", itr->delegator)("vestings", itr->vesting_shares));
                    remove(*itr);
                    itr = vdo_idx.lower_bound(std::make_tuple(account.name, time_point_sec::min()));
                }
            }

            //stop withdraw
            modify(account, [&](account_object &a) {
                a.vesting_withdraw_rate = asset(0, VESTS_SYMBOL);
                a.next_vesting_withdrawal = time_point_sec::maximum();
                a.to_withdraw = 0;
                a.withdrawn = 0;
            });

        }

        void database::transfer_vestings(const account_object &account, const account_object &receiver) {
            terminate_vesting_activities(account);

            auto vesting_shares = account.vesting_shares;

            modify(receiver, [&](account_object &a) {
                a.vesting_shares += vesting_shares;
            });

            modify(account, [&](account_object &a) {
                a.vesting_shares = asset(0, VESTS_SYMBOL);
            });

            elog("---- receiver's balance ${receiver} ${golos}", ("receiver", receiver.name)("golos", receiver.vesting_shares));
            adjust_proxied_witness_votes(account, -1 * vesting_shares.amount);
        }

        void database::clear_authority(const account_object &account) {
            const auto &account_auth = get_authority(account.name);
            modify(account_auth, [&](account_authority_object &auth) {
                auth.posting = authority();
                auth.active = authority();
                auth.owner = authority();

                auth.posting.weight_threshold = 1;
                auth.active.weight_threshold = 1;
                auth.owner.weight_threshold = 1;

            });

            modify(account, [&](account_object &acc) {
                acc.memo_key = public_key_type();
            });

            //stop witness
            const witness_object *witness = find_witness(account.name);
            if (witness) {
                modify(*witness, [&](witness_object &w) {
                    w.signing_key = public_key_type();
                });
            }
        }

        void database::freeze_account(const account_object &account, const account_object &receiver) {

            elog("freeze ${name}, receiver balance = ${rbalance}", ("name", account.name)("rbalance", receiver.balance));

            transfer_golos(account, receiver);
            transfer_gbg(account, receiver);
            transfer_vestings(account, receiver);

            clear_authority(account);
        }

        void database::set_gc_authority(const account_object &account) {
            clear_authority(account);
            
            const auto &account_auth = get_authority(account.name);
            modify(account_auth, [&](account_authority_object &auth) {
                for (const std::string &acc : liberation_hardfork::get_founders()) {	
                    auth.active.add_authorities(acc, 1);
                    auth.owner.add_authorities(acc, 1);
                }             
            });
            modify(account_auth, [&](account_authority_object &auth) {
                auth.active.weight_threshold = STEEMIT_HARDFORK_0_21_AUTHORITY_THRESHOLD;
                auth.owner.weight_threshold = STEEMIT_HARDFORK_0_21_AUTHORITY_THRESHOLD;
            });
        }

        void database::replace_recovery(const account_object &old_recovery, const account_object &new_recovery) {
            const auto &accounts_by_name = get_index<account_index>().indices().get<by_name>();

            for (auto itr = accounts_by_name.lower_bound(std::string());
                    itr != accounts_by_name.end(); ++itr) {
                const auto &account = *itr;
                if(account.recovery_account == old_recovery.name) {
                    modify(account, [&](account_object &acc) {
                        acc.recovery_account = new_recovery.name;
                    });
                }
            }

        }

        void database::liberate_golos_classic() {

            const account_object *check = find_account(liberation_hardfork::get_acc_regfund().beneficiary);
            if (check != nullptr) {
                const auto &acc_referendum = get_account(liberation_hardfork::get_acc_referendum());
                const auto &acc_regfund = get_account(liberation_hardfork::get_acc_regfund().beneficiary);
                const auto &acc_transit = get_account(liberation_hardfork::get_acc_transit().beneficiary);

                //transfer all funds from cf accounts to the new account referendum and remove authority

                for (const std::string &acc : liberation_hardfork::get_accounts_for_freeze()) {	
                    const auto &account = get_account(acc);
                    freeze_account(account, acc_referendum);
                    replace_recovery(account, acc_regfund);
                }             

                //Convert GESTS to GOLOS
                const auto &cprops = get_dynamic_global_properties();
                auto vesting_shares = acc_referendum.vesting_shares;
                auto converted_steem = vesting_shares * cprops.get_vesting_share_price();
                modify(acc_referendum, [&](account_object &a) {
                    a.balance += converted_steem;
                    a.vesting_shares -= vesting_shares;
                });

                modify(cprops, [&](dynamic_global_property_object &props) {
                    props.total_vesting_fund_steem -= converted_steem;
                    props.total_vesting_shares -= vesting_shares;
                });

                const auto &acc_worker = get_account(liberation_hardfork::get_acc_worker().beneficiary);
                const auto &acc_mm = get_account(liberation_hardfork::get_acc_mm().beneficiary);

                auto transfer_beneficiary = [&](const auto& reward, const auto& account) {
                    const auto amount = asset(reward.amount);
                    elog("---- transfer to ${acc} ${amount} ${ref}", ("acc", account.name)("amount", amount)("ref", acc_referendum.balance));
                    if(acc_referendum.balance.amount > amount.amount) {
                        adjust_balance(acc_referendum, -amount);
                        adjust_balance(account, amount);
                    }
                };

                transfer_beneficiary(liberation_hardfork::get_acc_worker(), acc_worker);
                transfer_beneficiary(liberation_hardfork::get_acc_mm(), acc_mm);
                transfer_beneficiary(liberation_hardfork::get_acc_regfund(), acc_regfund);
                transfer_beneficiary(liberation_hardfork::get_acc_transit(), acc_transit);

                set_gc_authority(acc_regfund);
                set_gc_authority(acc_transit);
                set_gc_authority(acc_referendum);
            }
        }

} } //golos::chain
