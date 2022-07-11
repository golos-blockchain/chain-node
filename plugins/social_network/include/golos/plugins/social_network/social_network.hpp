#pragma once

#include <appbase/application.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/api/discussion.hpp>
#include <golos/plugins/follow/plugin.hpp>
#include <golos/api/account_vote.hpp>
#include <golos/api/vote_state.hpp>
#include <golos/api/discussion_helper.hpp>
#include <golos/plugins/social_network/social_network_types.hpp>

namespace golos { namespace plugins { namespace social_network {
    using plugins::json_rpc::msg_pack;
    using golos::api::discussion;
    using golos::api::opt_prefs;
    using golos::api::account_vote;
    using golos::api::vote_state;
    using namespace golos::chain;
    using golos::api::comment_api_object;

    using categorized_discussions = std::map<std::string,std::vector<discussion>>;

    DEFINE_API_ARGS(get_content,                  msg_pack, discussion)
    DEFINE_API_ARGS(get_content_replies,          msg_pack, std::vector<discussion>)
    DEFINE_API_ARGS(get_all_content_replies,      msg_pack, std::vector<discussion>)
    DEFINE_API_ARGS(get_account_votes,            msg_pack, std::vector<account_vote>)
    DEFINE_API_ARGS(get_active_votes,             msg_pack, std::vector<vote_state>)
    DEFINE_API_ARGS(get_donates,                  msg_pack, std::vector<donate_api_object>)
    DEFINE_API_ARGS(get_donates_for_targets,      msg_pack, std::vector<std::vector<donate_api_object>>)
    DEFINE_API_ARGS(get_replies_by_last_update,   msg_pack, std::vector<discussion>)
    DEFINE_API_ARGS(get_all_discussions_by_active, msg_pack, categorized_discussions)
    DEFINE_API_ARGS(get_last_reply,               msg_pack, discussion)

    class social_network final: public appbase::plugin<social_network> {
    public:
        APPBASE_PLUGIN_REQUIRES (
            (chain::plugin)
            (json_rpc::plugin)
        )

        DECLARE_API(
            (get_content)
            (get_content_replies)
            (get_all_content_replies)
            (get_account_votes)
            (get_active_votes)
            (get_donates)
            (get_donates_for_targets)
            (get_replies_by_last_update)
            (get_all_discussions_by_active)
            (get_last_reply)
        )

        social_network();
        ~social_network();

        void set_program_options(
            boost::program_options::options_description& cfg,
            boost::program_options::options_description& config_file_options
        ) override;

        static const std::string& name();

        void plugin_initialize(const boost::program_options::variables_map& options) override;

        void plugin_startup() override;
        void plugin_shutdown() override;

        comment_api_object create_comment_api_object(const comment_object& o) const;

        const comment_content_object& get_comment_content(const comment_id_type& comment) const ;
        const comment_content_object* find_comment_content(const comment_id_type& comment) const ;


    private:
        struct impl;
        std::unique_ptr<impl> pimpl;
    };

// Callback which is needed for correct work of discussion_helper
    void fill_comment_info(const golos::chain::database& db, const comment_object& co, comment_api_object& cao);
    std::string get_json_metadata(const golos::chain::database& db, const comment_object&);

} } } // golos::plugins::social_network
