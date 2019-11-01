#include <golos/plugins/worker_api/worker_api_plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/social_network/social_network.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/chain/index.hpp>
#include <golos/chain/operation_notification.hpp>
#include <appbase/application.hpp>

namespace golos { namespace plugins { namespace worker_api {

namespace bpo = boost::program_options;
using golos::api::discussion_helper;

struct post_operation_visitor {
    golos::chain::database& _db;
    worker_api_plugin& _plugin;

    post_operation_visitor(golos::chain::database& db, worker_api_plugin& plugin) : _db(db), _plugin(plugin) {
    }

    typedef void result_type;

    template<typename T>
    result_type operator()(const T&) const {
    }

    result_type operator()(const worker_request_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wtmo_idx = _db.get_index<worker_request_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(post.id);

        if (wtmo_itr != wtmo_idx.end()) { // Edit case
            _db.modify(*wtmo_itr, [&](worker_request_metadata_object& wtmo) {
                wtmo.modified = _db.head_block_time();
            });

            return;
        }

        // Create case

        _db.create<worker_request_metadata_object>([&](worker_request_metadata_object& wtmo) {
            wtmo.post = post.id;
            wtmo.net_rshares = post.net_rshares;
        });
    }

    result_type operator()(const worker_request_delete_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wtmo_idx = _db.get_index<worker_request_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(post.id);

        const auto* wto = _db.find_worker_request(post.id);
        if (wto) {
            return;
        }

        _db.remove(*wtmo_itr);
    }

    result_type operator()(const vote_operation& o) const {
        const auto& post = _db.get_comment(o.author, o.permlink);

        const auto& wtmo_idx = _db.get_index<worker_request_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(post.id);
        if (wtmo_itr != wtmo_idx.end()) {
            _db.modify(*wtmo_itr, [&](worker_request_metadata_object& wtmo) {
                wtmo.net_rshares = post.net_rshares;
            });
        }
    }

    result_type operator()(const worker_request_approve_operation& o) const {
        const auto& wto_post = _db.get_comment(o.author, o.permlink);
        const auto& wto = _db.get_worker_request(wto_post.id);

        const auto& wtmo_idx = _db.get_index<worker_request_metadata_index, by_post>();
        auto wtmo_itr = wtmo_idx.find(wto_post.id);

        auto approves = _db.count_worker_request_approves(wto_post.id);

        _db.modify(*wtmo_itr, [&](worker_request_metadata_object& wtmo) {
            wtmo.approves = approves[worker_request_approve_state::approve];
            wtmo.disapproves = approves[worker_request_approve_state::disapprove];

            if (wto.state == worker_request_state::payment) {
                wtmo.payment_beginning_time = wto.next_cashout_time;
            }
        });
    }
};

class worker_api_plugin::worker_api_plugin_impl final {
public:
    worker_api_plugin_impl(worker_api_plugin& plugin)
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
        helper = std::make_unique<discussion_helper>(
            _db,
            follow::fill_account_reputation,
            nullptr,
            golos::plugins::social_network::fill_comment_info
        );
    }

    void post_operation(const operation_notification& note, worker_api_plugin& self) {
        try {
            note.op.visit(post_operation_visitor(_db, self));
        } catch (const fc::assert_exception&) {
            if (_db.is_producing()) {
                throw;
            }
        }
    }

    template <typename DatabaseIndex, typename OrderIndex, bool ReverseSort, typename FillWorkerFields>
    void select_postbased_results_ordered(const auto& query, std::vector<auto>& result, FillWorkerFields&& fill_worker_fields, bool fill_posts);

    ~worker_api_plugin_impl() = default;

    golos::chain::database& _db;

    std::unique_ptr<discussion_helper> helper;
};

worker_api_plugin::worker_api_plugin() = default;

worker_api_plugin::~worker_api_plugin() = default;

const std::string& worker_api_plugin::name() {
    static std::string name = "worker_api";
    return name;
}

void worker_api_plugin::set_program_options(
    bpo::options_description& cli,
    bpo::options_description& cfg
) {
}

void worker_api_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
    ilog("Intializing account notes plugin");

    my = std::make_unique<worker_api_plugin::worker_api_plugin_impl>(*this);

    my->_db.post_apply_operation.connect([&](const operation_notification& note) {
        my->post_operation(note, *this);
    });

    add_plugin_index<worker_request_metadata_index>(my->_db);

    JSON_RPC_REGISTER_API(name())
}

void worker_api_plugin::plugin_startup() {
    ilog("Starting up worker api plugin");
}

void worker_api_plugin::plugin_shutdown() {
    ilog("Shutting down worker api plugin");
}

template <typename DatabaseIndex, typename OrderIndex, bool ReverseSort, typename FillWorkerFields>
void worker_api_plugin::worker_api_plugin_impl::select_postbased_results_ordered(const auto& query, std::vector<auto>& result, FillWorkerFields&& fill_worker_fields, bool fill_posts) {
    if (!_db.has_index<DatabaseIndex>()) {
        return;
    }

    if (query.has_author()) {
        GOLOS_CHECK_PARAM(fill_posts, {
            GOLOS_CHECK_VALUE(fill_posts, "Cannot filter by author without filling posts");
        });
    }

    _db.with_weak_read_lock([&]() {
        const auto& idx = _db.get_index<DatabaseIndex, OrderIndex>();
        auto itr = idx.begin();
        if (ReverseSort) {
            itr = idx.end();
        }

        const comment_object* post = nullptr;

        if (query.has_start()) {
            post = _db.find_comment(*query.start_author, *query.start_permlink);
            if (!post) {
                return;
            }

            const auto& post_idx = _db.get_index<DatabaseIndex, by_post>();
            const auto post_itr = post_idx.find(post->id);
            if (post_itr == post_idx.end()) {
                return;
            }
            itr = idx.iterator_to(*post_itr);
        }

        result.reserve(query.limit);

        auto handle = [&](auto obj) {
            comment_api_object ca;
            if (fill_posts) {
                if (!post) {
                    post = &_db.get_comment(itr->post);
                }

                if (!query.is_good_author(post->author)) {
                    return;
                }

                ca = helper->create_comment_api_object(*post);
                post = nullptr;
            }
            result.emplace_back(obj, ca);
            if (!fill_worker_fields(this, obj, result.back(), query)) {
                result.pop_back();
            }
        };

        if (ReverseSort) {
            auto ritr = boost::make_reverse_iterator(itr);
            for (; ritr != idx.rend() && result.size() < query.limit; ++ritr) {
                handle(*ritr);
            }
        } else {
            for (; itr != idx.end() && result.size() < query.limit; ++itr) {
                handle(*itr);
            }
        }
    });
}

// Api Defines

DEFINE_API(worker_api_plugin, get_worker_requests) {
    PLUGIN_API_VALIDATE_ARGS(
        (worker_request_query, query)
        (worker_request_sort, sort)
        (bool, fill_posts)
    )
    query.validate();

    std::vector<worker_request_api_object> result;

    auto wto_fill_worker_fields = [&](worker_api_plugin_impl* my, const worker_request_metadata_object& wtmo, worker_request_api_object& wto_api, const worker_request_query& query) -> bool {
        auto wto = my->_db.get_worker_request(wtmo.post);
        if (!query.is_good_state(wto.state)) {
            return false;
        }
        wto_api.fill_worker_request(wto);
        return true;
    };

    if (sort == worker_request_sort::by_created) {
        my->select_postbased_results_ordered<worker_request_metadata_index, by_id, true>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_request_sort::by_net_rshares) {
        my->select_postbased_results_ordered<worker_request_metadata_index, by_net_rshares, false>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_request_sort::by_approves) {
        my->select_postbased_results_ordered<worker_request_metadata_index, by_approves, false>(query, result, wto_fill_worker_fields, fill_posts);
    } else if (sort == worker_request_sort::by_disapproves) {
        my->select_postbased_results_ordered<worker_request_metadata_index, by_disapproves, false>(query, result, wto_fill_worker_fields, fill_posts);
    }

    return result;
}

} } } // golos::plugins::worker_api
