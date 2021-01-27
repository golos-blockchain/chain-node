#include <golos/plugins/mongo_db/mongo_db_state.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/proposal_object.hpp>
#include <golos/plugins/social_network/social_network.hpp>

#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/value_context.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <appbase/plugin.hpp>

#include <boost/algorithm/string.hpp>

namespace golos {
namespace plugins {
namespace mongo_db {

    using bsoncxx::builder::stream::array;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::open_document;
    using bsoncxx::builder::stream::close_document;

    using golos::chain::by_account;
    using golos::chain::decline_voting_rights_request_index;

    state_writer::state_writer(db_map& bmi_to_add, const signed_block& block) :
        db_(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()),
        state_block(block),
        all_docs(bmi_to_add) {
    }

    named_document state_writer::create_document(const std::string& name,
            const std::string& key, const std::string& keyval) {
        named_document doc;
        doc.collection_name = name;
        doc.key = key;
        doc.keyval = keyval;
        doc.is_removal = false;
        return doc;
    }

    named_document state_writer::create_removal_document(const std::string& name,
            const std::string& key, const std::string& keyval) {
        named_document doc;
        doc.collection_name = name;
        doc.key = key;
        doc.keyval = keyval;
        doc.is_removal = true;
        return doc;
    }

    bool state_writer::format_comment(const std::string& auth, const std::string& perm) {
        try {
            auto& comment = db_.get_comment(auth, perm);
            auto oid = std::string(auth).append("/").append(perm);
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("comment_object", "_id", oid_hash);
            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "removed", false);

            format_value(body, "block_num", state_block.block_num());

            format_value(body, "author", auth);
            format_value(body, "permlink", perm);
            format_value(body, "abs_rshares", comment.abs_rshares);

            format_value(body, "allow_curation_rewards", comment.allow_curation_rewards);
            format_value(body, "allow_replies", comment.allow_replies);
            format_value(body, "allow_votes", comment.allow_votes);
            format_value(body, "cashout_time", comment.cashout_time);
            format_value(body, "children", comment.children);
            format_value(body, "children_abs_rshares", comment.children_abs_rshares);
            format_value(body, "children_rshares2", comment.children_rshares2);
            format_value(body, "created", comment.created);
            format_value(body, "depth", comment.depth);
            format_value(body, "last_payout", comment.last_payout);
            format_value(body, "max_accepted_payout", comment.max_accepted_payout);
            format_value(body, "max_cashout_time", comment.max_cashout_time);
            format_value(body, "net_rshares", comment.net_rshares);
            format_value(body, "net_votes", comment.net_votes);
            format_value(body, "parent_author", comment.parent_author);
            format_value(body, "parent_permlink", comment.parent_permlink);
            format_value(body, "percent_steem_dollars", comment.percent_steem_dollars);
            format_value(body, "reward_weight", comment.reward_weight);
            // format_value(body, "total_vote_weight", comment.total_vote_weight);
            format_value(body, "vote_rshares", comment.vote_rshares);

            if (!comment.beneficiaries.empty()) {
                array ben_array;
                for (auto& b: comment.beneficiaries) {
                    document tmp;
                    format_value(tmp, "account", b.account);
                    format_value(tmp, "weight", b.weight);
                    ben_array << tmp;
                }
                body << "beneficiaries" << ben_array;
            }

            std::string comment_mode;
            switch (comment.mode) {
                case not_set:
                    comment_mode = "not_set";
                    break;
                case first_payout:
                    comment_mode = "first_payout";
                    break;
                case second_payout:
                    comment_mode = "second_payout";
                    break;
                case archived:
                    comment_mode = "archived";
                    break;
            }

            format_value(body, "mode", comment_mode);

            if (db_.has_index<golos::plugins::social_network::comment_content_index>()) {
                const auto& con_idx = db_.get_index<golos::plugins::social_network::comment_content_index>().indices().get<golos::plugins::social_network::by_comment>();
                auto con_itr = con_idx.find(comment.id);
                if (con_itr != con_idx.end()) {
                    format_value(body, "title", con_itr->title);
                    format_value(body, "body", con_itr->body);
                    format_json(body, "json_metadata", con_itr->json_metadata);
                }
            }

            if (db_.has_index<golos::plugins::social_network::comment_last_update_index>()) {
                const auto& clu_idx = db_.get_index<golos::plugins::social_network::comment_last_update_index>().indices().get<golos::plugins::social_network::by_comment>();
                auto clu_itr = clu_idx.find(comment.id);
                if (clu_itr != clu_idx.end()) {
                    format_value(body, "active", clu_itr->active);
                    format_value(body, "last_update", clu_itr->last_update);
                }
            }

            if (db_.has_index<golos::plugins::social_network::comment_reward_index>()) {
                const auto& cr_idx = db_.get_index<golos::plugins::social_network::comment_reward_index>().indices().get<golos::plugins::social_network::by_comment>();
                auto cr_itr = cr_idx.find(comment.id);
                if (cr_itr != cr_idx.end()) {
                    format_value(body, "author_rewards", cr_itr->author_rewards);
                    format_value(body, "author_gbg_payout", cr_itr->author_gbg_payout_value);
                    format_value(body, "author_golos_payout", cr_itr->author_golos_payout_value);
                    format_value(body, "author_gests_payout", cr_itr->author_gests_payout_value);
                    format_value(body, "beneficiary_payout", cr_itr->beneficiary_payout_value);
                    format_value(body, "beneficiary_gests_payout", cr_itr->beneficiary_gests_payout_value);
                    format_value(body, "curator_payout", cr_itr->curator_payout_value);
                    format_value(body, "curator_gests_payout", cr_itr->curator_gests_payout_value);
                    format_value(body, "total_payout", cr_itr->total_payout_value);
                } else {
                    format_value(body, "author_rewards", 0);
                    format_value(body, "author_gbg_payout", asset(0, SBD_SYMBOL));
                    format_value(body, "author_golos_payout", asset(0, STEEM_SYMBOL));
                    format_value(body, "author_gests_payout", asset(0, VESTS_SYMBOL));
                    format_value(body, "beneficiary_payout", asset(0, SBD_SYMBOL));
                    format_value(body, "beneficiary_gests_payout", asset(0, VESTS_SYMBOL));
                    format_value(body, "curator_payout", asset(0, SBD_SYMBOL));
                    format_value(body, "curator_gests_payout", asset(0, VESTS_SYMBOL));
                    format_value(body, "total_payout", asset(0, SBD_SYMBOL));
                }
            }

            std::string category, root_oid;
            if (comment.parent_author == STEEMIT_ROOT_POST_PARENT) {
                category = to_string(comment.parent_permlink);
                root_oid = oid;
            } else {
                auto& root_comment = db_.get<comment_object, by_id>(comment.root_comment);
                category = to_string(root_comment.parent_permlink);
                root_oid = std::string(root_comment.author).append("/").append(root_comment.permlink.c_str());
            }
            format_value(body, "category", category);
            format_oid(body, "root_comment", root_oid);
            document root_comment_index;
            root_comment_index << "root_comment" << 1;
            doc.indexes_to_create.push_back(std::move(root_comment_index));

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));

            return true;
        }
//        catch (fc::exception& ex) {
//            ilog("MongoDB operations fc::exception during formatting comment. ${e}", ("e", ex.what()));
//        }
        catch (...) {
            // ilog("Unknown exception during formatting comment.");
            return false;
        }
    }

    void state_writer::format_account(const account_object& account) {
        try {
            auto oid = account.name;
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("account_object", "_id", oid_hash);
            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "name", account.name);
            format_value(body, "memo_key", std::string(account.memo_key));
            format_value(body, "proxy", account.proxy);

            format_value(body, "last_account_update", account.last_account_update);

            format_value(body, "created", account.created);
            format_value(body, "mined", account.mined);
            format_value(body, "owner_challenged", account.owner_challenged);
            format_value(body, "active_challenged", account.active_challenged);
            format_value(body, "last_owner_proved", account.last_owner_proved);
            format_value(body, "last_active_proved", account.last_active_proved);
            format_value(body, "recovery_account", account.recovery_account);
            format_value(body, "reset_account", account.reset_account);
            format_value(body, "last_account_recovery", account.last_account_recovery);
            format_value(body, "comment_count", account.comment_count);
            format_value(body, "lifetime_vote_count", account.lifetime_vote_count);
            format_value(body, "post_count", account.post_count);

            format_value(body, "can_vote", account.can_vote);
            format_value(body, "voting_power", account.voting_power);
            format_value(body, "last_vote_time", account.last_vote_time);

            format_value(body, "balance", account.balance);
            format_value(body, "savings_balance", account.savings_balance);

            format_value(body, "sbd_balance", account.sbd_balance);
            format_value(body, "sbd_seconds", account.sbd_seconds);
            format_value(body, "sbd_seconds_last_update", account.sbd_seconds_last_update);
            format_value(body, "sbd_last_interest_payment", account.sbd_last_interest_payment);

            format_value(body, "savings_sbd_balance", account.savings_sbd_balance);
            format_value(body, "savings_sbd_seconds", account.savings_sbd_seconds);
            format_value(body, "savings_sbd_seconds_last_update", account.savings_sbd_seconds_last_update);
            format_value(body, "savings_sbd_last_interest_payment", account.savings_sbd_last_interest_payment);

            format_value(body, "savings_withdraw_requests", account.savings_withdraw_requests);

            format_value(body, "benefaction_rewards", account.benefaction_rewards);
            format_value(body, "curation_rewards", account.curation_rewards);
            format_value(body, "delegation_rewards", account.delegation_rewards);
            format_value(body, "posting_rewards", account.posting_rewards);

            format_value(body, "vesting_shares", account.vesting_shares);
            format_value(body, "delegated_vesting_shares", account.delegated_vesting_shares);
            format_value(body, "received_vesting_shares", account.received_vesting_shares);

            format_value(body, "vesting_withdraw_rate", account.vesting_withdraw_rate);
            format_value(body, "next_vesting_withdrawal", account.next_vesting_withdrawal);
            format_value(body, "withdrawn", account.withdrawn);
            format_value(body, "to_withdraw", account.to_withdraw);
            format_value(body, "withdraw_routes", account.withdraw_routes);

            if (account.proxied_vsf_votes.size() != 0) {
                array ben_array;
                for (auto& b: account.proxied_vsf_votes) {
                    ben_array << b;
                }
                body << "proxied_vsf_votes" << ben_array;
            }

            format_value(body, "witnesses_voted_for", account.witnesses_voted_for);

            format_value(body, "last_comment", account.last_comment);
            format_value(body, "last_post", account.last_post);

            format_value(body, "referrer_account", account.referrer_account);
            format_value(body, "referrer_interest_rate", account.referrer_interest_rate);
            format_value(body, "referral_end_date", account.referral_end_date);
            format_value(body, "referral_break_fee", account.referral_break_fee);

            auto account_metadata = db_.find<account_metadata_object, by_account>(account.name);
            if (account_metadata != nullptr) {
                format_json(body, "json_metadata", account_metadata->json_metadata);
            }

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));
        }
        catch (...) {
            // ilog("Unknown exception during formatting account.");
        }
    }

    void state_writer::format_account(const std::string& name) {
        try {
            auto& account = db_.get_account(name);
            format_account(account);
        }
        catch (...) {
            // ilog("Unknown exception during formatting account.");
        }
    }

    void state_writer::format_account_authority(const account_name_type& account_name) {
        try {
            auto& account_authority = db_.get<account_authority_object, by_account>(account_name);

            auto oid = account_authority.account;
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("account_authority_object", "_id", oid_hash);
            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "account", account_authority.account);

            auto owner_keys = account_authority.owner.get_keys();
            if (!owner_keys.empty()) {
                array keys_array;
                for (auto& key: owner_keys) {
                    keys_array << std::string(key);
                }
                body << "owner" << keys_array;
            }

            auto active_keys = account_authority.active.get_keys();
            if (!active_keys.empty()) {
                array keys_array;
                for (auto& key: active_keys) {
                    keys_array << std::string(key);
                }
                body << "active" << keys_array;
            }

            auto posting_keys = account_authority.posting.get_keys();
            if (!posting_keys.empty()) {
                array keys_array;
                for (auto& key: posting_keys) {
                    keys_array << std::string(key);
                }
                body << "posting" << keys_array;
            }

            format_value(body, "last_owner_update", account_authority.last_owner_update);

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));

        }
        catch (...) {
            // ilog("Unknown exception during formatting account authority.");
        }
    }

    void state_writer::format_account_bandwidth(const account_name_type& account, const bandwidth_type& type) {
        try {
            auto band = db_.find<account_bandwidth_object, by_account_bandwidth_type>(
                std::make_tuple(account, type));
            if (band == nullptr) {
                return;
            }

            std::string band_type;
            switch (type) {
                case post:
                    band_type = "post";
                    break;
                case forum:
                    band_type = "forum";
                    break;
                case market:
                    band_type = "market";
                    break;
                case custom_json:
                    band_type = "custom_json";
                    break;
            }

            auto oid = std::string(band->account).append("/").append(band_type);
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("account_bandwidth_object", "_id", oid_hash);
            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "account", band->account);
            format_value(body, "type", band_type);
            format_value(body, "average_bandwidth", band->average_bandwidth);
            format_value(body, "lifetime_bandwidth", band->lifetime_bandwidth);
            format_value(body, "last_bandwidth_update", band->last_bandwidth_update);

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));

        }
        catch (...) {
            // ilog("Unknown exception during formatting account bandwidth.");
        }
    }

    void state_writer::format_witness(const witness_object& witness) {
        try {
            auto oid = witness.owner;
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("witness_object", "_id", oid_hash);
            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "owner", oid);
            format_value(body, "created", witness.created);
            format_value(body, "url", witness.url); //shared_str
            format_value(body, "total_missed", witness.total_missed);
            format_value(body, "last_aslot", witness.last_aslot);
            format_value(body, "last_confirmed_block_num", witness.last_confirmed_block_num);

            format_value(body, "pow_worker", witness.pow_worker);

            format_value(body, "signing_key", std::string(witness.signing_key));

            // format_value(body, "props", witness.props); - ignored because empty
            format_value(body, "sbd_exchange_rate", witness.sbd_exchange_rate);
            format_value(body, "last_sbd_exchange_update", witness.last_sbd_exchange_update);

            format_value(body, "votes", witness.votes);
            format_value(body, "schedule", witness.schedule);

            format_value(body, "virtual_last_update", witness.virtual_last_update);
            format_value(body, "virtual_position", witness.virtual_position);
            format_value(body, "virtual_scheduled_time", witness.virtual_scheduled_time);

            format_value(body, "last_work", witness.last_work.str());

            format_value(body, "running_version", std::string(witness.running_version));

            format_value(body, "hardfork_version_vote", std::string(witness.hardfork_version_vote));
            format_value(body, "hardfork_time_vote", witness.hardfork_time_vote);

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));

        }
        catch (...) {
            // ilog("Unknown exception during formatting witness.");
        }
    }

    void state_writer::format_witness(const account_name_type& owner) {
        try {
            auto& witness = db_.get_witness(owner);
            format_witness(witness);
        }
        catch (...) {
            // ilog("Unknown exception during formatting witness.");
        }
    }

    void state_writer::format_vesting_delegation_object(const vesting_delegation_object& delegation) {
        try {
            auto oid = std::string(delegation.delegator).append("/").append(std::string(delegation.delegatee));
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("vesting_delegation_object", "_id", oid_hash);
            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "delegator", delegation.delegator);
            format_value(body, "delegatee", delegation.delegatee);
            format_value(body, "vesting_shares", delegation.vesting_shares);
            format_value(body, "interest_rate", delegation.interest_rate);
            format_value(body, "min_delegation_time", delegation.min_delegation_time);
            format_value(body, "timestamp", state_block.timestamp);

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));

        }
        catch (...) {
            // ilog("Unknown exception during formatting vesting delegation object.");
        }
    }

    void state_writer::format_vesting_delegation_object(const account_name_type& delegator, const account_name_type& delegatee) {
        try {
            auto delegation = db_.find<vesting_delegation_object, by_delegation>(std::make_tuple(delegator, delegatee));
            if (delegation == nullptr) {
                return;
            }
            format_vesting_delegation_object(*delegation);
        }
        catch (...) {
            // ilog("Unknown exception during formatting vesting delegation object.");
        }
    }

    void state_writer::format_escrow(const escrow_object& escrow) {
        try {
            auto oid = std::string(escrow.from).append("/").append(std::to_string(escrow.escrow_id));
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("escrow_object", "_id", oid_hash);

            document from_index;
            from_index << "from" << 1;
            doc.indexes_to_create.push_back(std::move(from_index));

            document to_index;
            to_index << "to" << 1;
            doc.indexes_to_create.push_back(std::move(to_index));

            document agent_index;
            agent_index << "agent" << 1;
            doc.indexes_to_create.push_back(std::move(agent_index));

            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "removed", false);
            format_value(body, "escrow_id", escrow.escrow_id);
            format_oid(body, "from", escrow.from);
            format_oid(body, "to", escrow.to);
            format_oid(body, "agent", escrow.agent);
            format_value(body, "ratification_deadline", escrow.ratification_deadline);
            format_value(body, "escrow_expiration", escrow.escrow_expiration);
            format_value(body, "sbd_balance", escrow.sbd_balance);
            format_value(body, "steem_balance", escrow.steem_balance);
            format_value(body, "pending_fee", escrow.pending_fee);
            format_value(body, "to_approved", escrow.to_approved);
            format_value(body, "agent_approved", escrow.agent_approved);
            format_value(body, "disputed", escrow.disputed);
            format_value(body, "timestamp", state_block.timestamp);

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));
        }
        catch (...) {
            // ilog("Unknown exception during formatting escrow.");
        }
    }

    void state_writer::format_escrow(const account_name_type& name, uint32_t escrow_id) {
        try {
            auto& escrow = db_.get_escrow(name, escrow_id);
            format_escrow(escrow);
        }
        catch (...) {
            // ilog("Unknown exception during formatting escrow.");
        }
    }

    template <typename F, typename S>
    void remove_existing(F& first, const S& second) {
        auto src = std::move(first);
        std::set_difference(
            src.begin(), src.end(),
            second.begin(), second.end(),
            std::inserter(first, first.begin()));
    }

    void state_writer::format_proposal(const proposal_object& proposal) {
        try {
            auto oid = std::string(proposal.author).append("/").append(std::string(proposal.title.c_str()));
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("proposal_object", "_id", oid_hash);
            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "removed", false);
            format_value(body, "author", proposal.author);
            format_value(body, "title", proposal.title);

            format_value(body, "memo", proposal.memo);
            format_value(body, "timestamp", state_block.timestamp);

            if (proposal.review_period_time.valid()) {
                format_value(body, "review_period_time", *(proposal.review_period_time));
            }

            if (!proposal.proposed_operations.empty()) {
                array ops_array;
                auto operations = proposal.operations();
                for (auto& op: operations) {
                    ops_array << fc::json::to_string(op);
                }
                body << "proposed_operations" << ops_array;
            }

            format_array_value(body, "required_active_approvals", proposal.required_active_approvals,
                [] (const account_name_type& item) -> std::string { return std::string(item); });

            format_array_value(body, "available_active_approvals", proposal.available_active_approvals,
                [] (const account_name_type& item) -> std::string { return std::string(item); });

            format_array_value(body, "required_owner_approvals", proposal.required_owner_approvals,
                [] (const account_name_type& item) -> std::string { return std::string(item); });

            format_array_value(body, "available_owner_approvals", proposal.available_owner_approvals,
                [] (const account_name_type& item) -> std::string { return std::string(item); });

            format_array_value(body, "required_posting_approvals", proposal.required_posting_approvals,
                [] (const account_name_type& item) -> std::string { return std::string(item); });

            format_array_value(body, "available_posting_approvals", proposal.available_posting_approvals,
                [] (const account_name_type& item) -> std::string { return std::string(item); });

            format_array_value(body, "available_key_approvals", proposal.available_key_approvals,
                [] (const public_key_type& item) -> std::string { return std::string(item); });

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));
        }
        catch (...) {
            // ilog("Unknown exception during formatting proposal object.");
        }
    }

    void state_writer::format_proposal(const account_name_type& author, const std::string& title) {
        try {
            auto& proposal = db_.get_proposal(author, title);
            format_proposal(proposal);
        }
        catch (...) {
            // ilog("Unknown exception during formatting proposal object.");
        }
    }

    void state_writer::format_required_approval(const required_approval_object& reqapp,
            const account_name_type& proposal_author, const std::string& proposal_title) {
        try {
            auto proposal_oid = std::string(proposal_author).append("/").append(proposal_title);

            auto oid = std::string(proposal_oid).append("/")
                 .append(std::string(reqapp.account));
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("required_approval_object", "_id", oid_hash);

            document proposal_index;
            proposal_index << "proposal" << 1;
            doc.indexes_to_create.push_back(std::move(proposal_index));

            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "removed", false);
            format_value(body, "account", reqapp.account);
            format_oid(body, "proposal", proposal_oid);

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));
        } catch (...) {

        }
    }

    void state_writer::format_liquidity_reward_balance(const liquidity_reward_balance_object& lrbo,
        const account_name_type& owner) {
        try {
            auto oid = std::string(owner);
            auto oid_hash = hash_oid(oid);

            auto doc = create_document("liquidity_reward_balance_object", "_id", oid_hash);

            // We don't need any 'owner_id' because oid is owner

            auto& body = doc.doc;

            body << "$set" << open_document;

            format_oid(body, oid);

            format_value(body, "owner", owner);
            format_value(body, "steem_volume", lrbo.steem_volume);
            format_value(body, "sbd_volume", lrbo.sbd_volume);
            //format_value(body, "weight", lrbo.weight); // weight not exported now
            format_value(body, "last_update", lrbo.last_update);

            body << close_document;

            bmi_insert_or_replace(all_docs, std::move(doc));
        } catch (...) {
            //
        }
    }

    void state_writer::format_liquidity_reward_balance(const account_name_type& owner) {
        try {
            auto& obj_owner = db_.get_account(owner);
            const auto& ridx = db_.get_index<liquidity_reward_balance_index>().indices().get<by_owner>();
            auto itr = ridx.find(obj_owner.id);
            if (ridx.end() != itr) {
                format_liquidity_reward_balance(*itr, owner);
            }
        }
        catch (...) {
            //
        }
    }

    auto state_writer::operator()(const comment_operation& op) -> result_type {
        format_comment(op.author, op.permlink);
    }

    void state_writer::write_global_property_object(const dynamic_global_property_object& dgpo, bool history) {
        try {
            std::string oid;
            if (history) {
                oid = state_block.timestamp;
            } else {
                oid = MONGO_ID_SINGLE;
            }
            auto oid_hash = hash_oid(oid);

            auto doc = create_document(std::string("dynamic_global_property_object") + 
                 (history ? "_history" : ""), "_id", oid_hash);
            auto& body = doc.doc;

            if (!history) {
                body << "$set" << open_document;
            }

            format_oid(body, oid);

            if (history) {
                format_value(body, "timestamp", state_block.timestamp);
            }
            format_value(body, "head_block_number", dgpo.head_block_number);
            format_value(body, "head_block_id", dgpo.head_block_id.str());
            format_value(body, "time", dgpo.time);
            format_value(body, "current_witness", dgpo.current_witness);

            format_value(body, "total_pow", dgpo.total_pow);

            format_value(body, "num_pow_witnesses", dgpo.num_pow_witnesses);

            format_value(body, "virtual_supply", dgpo.virtual_supply);
            format_value(body, "current_supply", dgpo.current_supply);
            format_value(body, "confidential_supply", dgpo.confidential_supply);
            format_value(body, "current_sbd_supply", dgpo.current_sbd_supply);
            format_value(body, "confidential_sbd_supply", dgpo.confidential_sbd_supply);
            format_value(body, "total_vesting_fund_steem", dgpo.total_vesting_fund_steem);
            format_value(body, "total_vesting_shares", dgpo.total_vesting_shares);
            format_value(body, "total_reward_fund_steem", dgpo.total_reward_fund_steem);
            format_value(body, "total_reward_shares2", dgpo.total_reward_shares2);

            format_value(body, "sbd_interest_rate", dgpo.sbd_interest_rate);

            format_value(body, "sbd_print_rate", dgpo.sbd_print_rate);

            format_value(body, "average_block_size", dgpo.average_block_size);

            format_value(body, "maximum_block_size", dgpo.maximum_block_size);

            format_value(body, "current_aslot", dgpo.current_aslot);

            format_value(body, "recent_slots_filled", dgpo.head_block_number);
            format_value(body, "participation_count", dgpo.participation_count);

            format_value(body, "last_irreversible_block_num", dgpo.last_irreversible_block_num);

            format_value(body, "max_virtual_bandwidth", dgpo.max_virtual_bandwidth);

            format_value(body, "current_reserve_ratio", dgpo.current_reserve_ratio);

            if (db_.has_hardfork(STEEMIT_HARDFORK_0_22__76)) {
                format_value(body, "vote_regeneration_per_day", db_.get_witness_schedule_object().median_props.vote_regeneration_per_day);
            } else {
                format_value(body, "vote_regeneration_per_day", STEEMIT_VOTE_REGENERATION_PER_DAY_PRE_HF_22);
            }

            if (!history) {
                body << close_document;
            }

            bmi_insert_or_replace(all_docs, std::move(doc));
        }
        catch (...) {
            // ilog("Unknown exception during writing global property object.");
        }
    }

    void state_writer::write_witness_schedule_object(const witness_schedule_object& wso, bool history) {
        try {
            std::string oid;
            if (history) {
                oid = state_block.timestamp;
            } else {
                oid = MONGO_ID_SINGLE;
            }
            auto oid_hash = hash_oid(oid);

            auto doc = create_document(std::string("witness_schedule_object") + 
                 (history ? "_history" : ""), "_id", oid_hash);
            auto& body = doc.doc;

            if (!history) {
                body << "$set" << open_document;
            }

            format_oid(body, oid);

            if (history) {
                format_value(body, "timestamp", state_block.timestamp);
            }
            format_value(body, "current_virtual_time", wso.current_virtual_time);
            format_value(body, "next_shuffle_block_num", wso.next_shuffle_block_num);
            format_array_value(body, "current_shuffled_witnesses", wso.current_shuffled_witnesses,
                [] (const account_name_type& item) -> std::string { return std::string(item); });
            format_value(body, "num_scheduled_witnesses", wso.num_scheduled_witnesses);
            format_value(body, "top19_weight", wso.current_virtual_time);
            format_value(body, "timeshare_weight", wso.current_virtual_time);
            format_value(body, "miner_weight", wso.current_virtual_time);
            format_value(body, "witness_pay_normalization_factor", wso.current_virtual_time);
            //format_value(body, "median_props", ... // Skipped because it is still empty
            format_value(body, "majority_version", wso.current_virtual_time);

            if (!history) {
                body << close_document;
            }

            bmi_insert_or_replace(all_docs, std::move(doc));
        }
        catch (...) {
            // ilog("Unknown exception during writing witness schedule object.");
        }
    }

}}}