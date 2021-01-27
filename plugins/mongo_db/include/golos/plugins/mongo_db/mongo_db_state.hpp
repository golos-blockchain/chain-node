#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>

#include <golos/protocol/operations.hpp>
#include <golos/chain/database.hpp>

#include <golos/plugins/mongo_db/mongo_db_types.hpp>

namespace golos {
namespace plugins {
namespace mongo_db {

    class state_writer {
    public:
        using result_type = void;

        state_writer(db_map& bmi_to_add, const signed_block& block);

        template<class T>
        result_type operator()(const T& o) {}
        result_type operator()(const comment_operation& op);

        void write_global_property_object(const dynamic_global_property_object& dgpo, bool history);

        void write_witness_schedule_object(const witness_schedule_object& wso, bool history);

    private:
        database &db_;

        const signed_block &state_block;

        db_map &all_docs;

        bool format_comment(const std::string& auth, const std::string& perm);

        void format_account(const account_object& account);

        void format_account(const std::string& name);

        void format_account_authority(const account_name_type& account_name);

        void format_account_bandwidth(const account_name_type& account, const bandwidth_type& type);

        void format_witness(const witness_object& witness);

        void format_witness(const account_name_type& owner);

        void format_vesting_delegation_object(const vesting_delegation_object& delegation);

        void format_vesting_delegation_object(const account_name_type& delegator,
            const account_name_type& delegatee);

        void format_escrow(const escrow_object &escrow);

        void format_escrow(const account_name_type &name, uint32_t escrow_id);

        void format_global_property_object();

        void format_proposal(const proposal_object& proposal);

        void format_proposal(const account_name_type& author, const std::string& title);

        void format_required_approval(const required_approval_object& reqapp,
            const account_name_type& proposal_author, const std::string& proposal_title);

        void format_liquidity_reward_balance(const liquidity_reward_balance_object& lrbo,
            const account_name_type& owner);

        void format_liquidity_reward_balance(const account_name_type& owner);

        named_document create_document(const std::string& name,
            const std::string& key, const std::string& keyval);

        named_document create_removal_document(const std::string& name,
            const std::string& key, const std::string& keyval);
    };

}}} // golos::plugins::mongo_db
