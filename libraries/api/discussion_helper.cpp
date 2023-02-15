#include <golos/api/discussion_helper.hpp>
#include <golos/api/comment_api_object.hpp>
#include <golos/api/content_utils.hpp>
#include <golos/chain/account_object.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/chain/comment_app_helper.hpp>
#include <golos/chain/curation_info.hpp>
#include <fc/io/json.hpp>
#include <boost/algorithm/string.hpp>


namespace golos { namespace api {

     boost::multiprecision::uint256_t to256(const fc::uint128_t& t) {
        boost::multiprecision::uint256_t result(t.high_bits());
        result <<= 65;
        result += t.low_bits();
        return result;
    }

    struct discussion_helper::impl final {
    public:
        impl() = delete;
        impl(
            golos::chain::database& db,
            std::function<void(const golos::chain::database&, discussion&)> fill_promoted,
            std::function<void(const golos::chain::database&, const comment_object&, comment_api_object&)> fill_comment_info,
            bool fills_author_reputation = true)
            : database_(db),
              fill_promoted_(fill_promoted),
              fill_comment_info_(fill_comment_info),
              fills_author_reputation_(fills_author_reputation) {
        }
        ~impl() = default;

        discussion create_discussion(const std::string& author) const ;

        discussion create_discussion(const comment_object& o) const ;

        std::vector<vote_state> select_active_votes(
            const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
        ) const;

        std::vector<vote_state> select_active_votes(const comment_curation_info&, uint32_t limit, uint32_t offset) const;

        void set_pending_payout(discussion& d) const;

        void set_url(discussion& d) const;

        golos::chain::database& database() {
            return database_;
        }

        golos::chain::database& database() const {
            return database_;
        }

        comment_api_object create_comment_api_object(const comment_object& o) const;

        discussion get_discussion(const comment_object& c, uint32_t vote_limit, uint32_t offset, opt_prefs prefs = opt_prefs()) const;

        void fill_discussion(discussion& d, const comment_object& comment, uint32_t vote_limit, uint32_t offset, opt_prefs prefs = opt_prefs()) const;

        void fill_comment_api_object(const comment_object& o, comment_api_object& d) const;

    private:
        void distribute_auction_tokens(discussion& d, share_type& curator_tokens, share_type& author_tokens) const;

    private:
        golos::chain::database& database_;
        std::function<void(const golos::chain::database&, discussion&)> fill_promoted_;
        std::function<void(const golos::chain::database&, const comment_object&, comment_api_object&)> fill_comment_info_;
        bool fills_author_reputation_;
    };

// create_comment_api_object 
    comment_api_object discussion_helper::create_comment_api_object(const comment_object& o) const {
        return pimpl->create_comment_api_object(o);
    }

    comment_api_object discussion_helper::impl::create_comment_api_object(const comment_object& o) const {
        comment_api_object result;

        fill_comment_api_object(o, result);

        return result;
    }

// fill_comment_api_object 

    void discussion_helper::fill_comment_api_object(const comment_object& o, comment_api_object& d) const {
        pimpl->fill_comment_api_object(o, d);
    }

    void discussion_helper::impl::fill_comment_api_object(const comment_object& o, comment_api_object& d) const {
        d.id = o.id;
        d.parent_author = o.parent_author;
        d.parent_hashlink = o.parent_hashlink;
        d.author = o.author;
        d.hashlink = o.hashlink;
        d.created = o.created;
        d.last_payout = o.last_payout;
        d.depth = o.depth;
        d.children = o.children;
        d.net_rshares = o.net_rshares;
        d.abs_rshares = o.abs_rshares;
        d.vote_rshares = o.vote_rshares;
        d.children_abs_rshares = o.children_abs_rshares;
        d.cashout_time = o.cashout_time;
        d.max_cashout_time = o.max_cashout_time;
        d.reward_weight = o.reward_weight;
        d.net_votes = o.net_votes;
        d.mode = o.mode;
        d.root_comment = o.root_comment;
        d.allow_votes = o.allow_votes;

        const auto* cbl = database().find_comment_bill(o.id);

        if (cbl) {
            d.allow_curation_rewards = cbl->bill.allow_curation_rewards;
            d.curation_rewards_percent = cbl->bill.curation_rewards_percent;
            d.percent_steem_dollars = cbl->bill.percent_steem_dollars;
            d.max_accepted_payout = asset(cbl->bill.max_accepted_payout, SBD_SYMBOL);
            d.min_golos_power_to_curate = asset(cbl->bill.min_golos_power_to_curate, STEEM_SYMBOL);

            d.beneficiaries = vector<protocol::beneficiary_route_type>();
            for (auto& route : cbl->beneficiaries) {
                d.beneficiaries->push_back(route);
            }

            d.auction_window_reward_destination = cbl->bill.auction_window_reward_destination;
            d.auction_window_size = cbl->bill.auction_window_size;
        }

        if (fill_comment_info_) {
            fill_comment_info_(database(), o, d);
        }

        const auto* extras = database().find_extras(o.author, o.hashlink);
        if (extras) {
            d.permlink = to_string(extras->permlink);
            d.parent_permlink = to_string(extras->parent_permlink);
            if (o.parent_author == STEEMIT_ROOT_POST_PARENT) {
                d.category = to_string(extras->parent_permlink);
                d.root_author = d.author;
                d.root_permlink = d.permlink;
            } else {
                const auto& root = database().get<comment_object, by_id>(o.root_comment);
                const auto* root_extras = database().find_extras(root.author, root.hashlink);
                if (root_extras) {
                    d.category = to_string(root_extras->parent_permlink);
                    d.root_author = root.author;
                    d.root_permlink = to_string(root_extras->permlink);
                }
            }
            d.app = get_comment_app_by_id(database(), extras->app_id);
            d.has_worker_request = extras->has_worker_request;
            d.children_rshares2 = extras->children_rshares2;
        }
    }

// get_discussion
    discussion discussion_helper::impl::get_discussion(const comment_object& comment, uint32_t vote_limit, uint32_t offset, opt_prefs prefs) const {
        discussion d = create_discussion(comment);
        fill_discussion(d, comment, vote_limit, offset, prefs);
        return d;
    }

    discussion discussion_helper::get_discussion(const comment_object& c, uint32_t vote_limit, uint32_t offset, opt_prefs prefs) const {
        return pimpl->get_discussion(c, vote_limit, offset, prefs);
    }

    void discussion_helper::impl::fill_discussion(
        discussion& d, const comment_object& comment, uint32_t vote_limit, uint32_t offset, opt_prefs prefs
    ) const {
        set_url(d);

        if (fills_author_reputation_) {
            d.author_reputation = database_.get_account_reputation(d.author);
        }

        if (d.body.size() > 1024 * 128) {
            d.body = "body pruned due to size";
        }
        if (d.parent_author.size() > 0 && d.body.size() > 1024 * 16) {
            d.body = "comment pruned due to size";
        }

        d.active_votes_count = comment.total_votes;

        comment_curation_info c{database_, comment, true};

        d.curation_reward_curve = c.curve;
        d.total_vote_weight = c.total_vote_weight;
        d.auction_window_weight = c.auction_window_weight;
        d.votes_in_auction_window_weight = c.votes_in_auction_window_weight;
        d.active_votes = select_active_votes(c, vote_limit, offset);

        set_pending_payout(d);

        if (!!prefs) {
            auto bc = process_content_prefs(prefs, d.author, database_);
            if (!!bc) {
                d.bad = *bc;
            }
        }
    }

    void discussion_helper::fill_discussion(
        discussion& d, const comment_object& comment, uint32_t vote_limit, uint32_t offset, opt_prefs prefs
    ) const {
        pimpl->fill_discussion(d, comment, vote_limit, offset, prefs);
    }
//

// select_active_votes
    std::vector<vote_state> discussion_helper::impl::select_active_votes(
        const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
    ) const {
        const auto& comment = database_.get_comment_by_perm(author, permlink);
        comment_curation_info c{database_, comment, true};

        return select_active_votes(c, limit, offset);
    }

    std::vector<vote_state> discussion_helper::impl::select_active_votes(
        const comment_curation_info& c, uint32_t limit, uint32_t offset
    ) const {
        offset = std::min(offset, uint32_t(c.vote_list.size()));
        limit = std::min(limit, uint32_t(c.vote_list.size() - offset));

        if (limit == 0) {
            return {};
        }

        auto itr = c.vote_list.begin();
        std::advance(itr, offset);

        std::vector<vote_state> result;
        result.reserve(limit);

        for (; itr != c.vote_list.end() && result.size() < limit; ++itr) {
            const auto& vo = database().get(itr->vote->voter);
            vote_state vstate;
            vstate.voter = vo.name;
            vstate.weight = itr->weight;
            vstate.rshares = itr->vote->rshares;
            vstate.percent = itr->vote->vote_percent;
            vstate.time = itr->vote->last_update;
            vstate.reputation = vo.reputation;
            result.emplace_back(std::move(vstate));
        }
        return result;
    }

    std::vector<vote_state> discussion_helper::select_active_votes(
        const std::string& author, const std::string& permlink, uint32_t limit, uint32_t offset
    ) const {
        return pimpl->select_active_votes(author, permlink, limit, offset);
    }

//
// set_pending_payout

    void discussion_helper::impl::distribute_auction_tokens(discussion& d, share_type& curators_tokens, share_type& author_tokens) const {
        const auto auction_window_reward = curators_tokens.value * d.auction_window_weight / d.total_vote_weight;

        curators_tokens -= auction_window_reward;

        if (*d.auction_window_reward_destination == to_author) {
            author_tokens += auction_window_reward;
        } else if (*d.auction_window_reward_destination == to_curators &&
                   d.total_vote_weight != d.votes_in_auction_window_weight + d.auction_window_weight) {
            curators_tokens += auction_window_reward;
        }
    }

    void discussion_helper::impl::set_pending_payout(discussion& d) const {
        auto& db = database();

        fill_promoted_(db, d);

        if (!d.max_accepted_payout.valid()) {
            return;
        }

        if (d.total_vote_weight == 0) {
            return;
        }

        const auto& props = db.get_dynamic_global_properties();
        const auto& hist = db.get_feed_history();
        asset pot = props.total_reward_fund_steem;

        if (hist.current_median_history.is_null()) {
            return;
        }

        u256 total_r2 = to256(props.total_reward_shares2);

        if (props.total_reward_shares2 > 0) {
            auto vshares = db.calculate_vshares(d.net_rshares.value > 0 ? d.net_rshares.value : 0);

            u256 r2 = to256(vshares); //to256(abs_net_rshares);
            r2 = (r2 * d.reward_weight) / STEEMIT_100_PERCENT;
            r2 *= pot.amount.value;
            r2 /= total_r2;

            share_type reward_tokens = std::min(reward_tokens, db.to_steem(*d.max_accepted_payout).amount);

            share_type curation_tokens = reward_tokens * *d.curation_rewards_percent / STEEMIT_100_PERCENT;

            share_type author_tokens = reward_tokens - curation_tokens;

            if (*d.allow_curation_rewards) {
                distribute_auction_tokens(d, curation_tokens, author_tokens);

                d.pending_curator_payout_value = db.to_sbd(asset(curation_tokens, STEEM_SYMBOL));
                d.pending_curator_payout_gests_value = asset(curation_tokens, STEEM_SYMBOL) * props.get_vesting_share_price();
                d.pending_payout_value += d.pending_curator_payout_value;
            }

            uint32_t benefactor_weights = 0;
            for (auto &b : *d.beneficiaries) {
                benefactor_weights += b.weight;
            }

            if (benefactor_weights != 0) {
                auto total_beneficiary = (author_tokens * benefactor_weights) / STEEMIT_100_PERCENT;
                author_tokens -= total_beneficiary;
                d.pending_benefactor_payout_value = db.to_sbd(asset(total_beneficiary, STEEM_SYMBOL));
                d.pending_benefactor_payout_gests_value = (asset(total_beneficiary, STEEM_SYMBOL) * props.get_vesting_share_price());
                d.pending_payout_value += d.pending_benefactor_payout_value;
            }
            
            auto sbd_steem = (author_tokens * *d.percent_steem_dollars) / (2 * STEEMIT_100_PERCENT);
            auto vesting_steem = asset(author_tokens - sbd_steem, STEEM_SYMBOL);
            d.pending_author_payout_gests_value = vesting_steem * props.get_vesting_share_price();
            auto to_sbd = asset((props.sbd_print_rate * sbd_steem) / STEEMIT_100_PERCENT, STEEM_SYMBOL);
            auto to_steem = asset(sbd_steem, STEEM_SYMBOL) - to_sbd;

            d.pending_author_payout_golos_value = to_steem;
            d.pending_author_payout_gbg_value = db.to_sbd(to_sbd);
            d.pending_author_payout_value = d.pending_author_payout_gbg_value + db.to_sbd(to_steem + vesting_steem);
            d.pending_author_payout_in_golos = vesting_steem; // from HF23 there are no GOLOS and GBG
            d.pending_payout_value += d.pending_author_payout_value;

            // End of main calculation

            u256 tpp = to256(d.children_rshares2);
            tpp *= pot.amount.value;
            tpp /= total_r2;

            d.total_pending_payout_value = db.to_sbd(asset(static_cast<uint64_t>(tpp), pot.symbol));
        }
    }

//
// set_url
    void discussion_helper::impl::set_url(discussion& d) const {
        d.url = "/" + d.category + "/@" + d.root_author + "/" + d.root_permlink;

        if (d.root_comment != d.id) {
            d.url += "#@" + d.author + "/" + d.permlink;
        }
    }

//
// create_discussion
    discussion discussion_helper::impl::create_discussion(const std::string& author) const {
        auto dis = discussion();
        if (fills_author_reputation_) {
            dis.author_reputation = database_.get_account_reputation(dis.author);
        }
        dis.active = time_point_sec::min();
        dis.last_update = time_point_sec::min();
        return dis;
    }

    discussion discussion_helper::impl::create_discussion(const comment_object& o) const {
        discussion d;
        fill_comment_api_object(o, d);
        return d;
    }

    discussion discussion_helper::create_discussion(const std::string& author) const {
        return pimpl->create_discussion(author);
    }

    discussion discussion_helper::create_discussion(const comment_object& o) const {
        return pimpl->create_discussion(o);
    }

    discussion_helper::discussion_helper(
        golos::chain::database& db,
        std::function<void(const golos::chain::database&, discussion&)> fill_promoted,
        std::function<void(const database&, const comment_object &, comment_api_object&)> fill_comment_info,
        bool fills_author_reputation
    ) {
        pimpl = std::make_unique<impl>(db, fill_promoted, fill_comment_info,
            fills_author_reputation);
    }

    discussion_helper::~discussion_helper() {}

//
} } // golos::api
