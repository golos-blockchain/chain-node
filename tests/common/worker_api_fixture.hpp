#include <golos/plugins/worker_api/worker_api_plugin.hpp>
#include "worker_fixture.hpp"

using golos::chain::worker_fixture;

struct worker_api_fixture : public worker_fixture {
    worker_api_fixture() : worker_fixture(true, [&]() {
        database_fixture::initialize<golos::plugins::worker_api::worker_api_plugin>();
        open_database();
        startup();
    }) {
    }
};
